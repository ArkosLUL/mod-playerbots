"""SOAP transport: the envelope, the reply parsing, error mapping and the .env reader.

    python -m unittest discover -s tools/raidtest/tests
"""
from __future__ import annotations

import io
import pathlib
import sys
import tempfile
import unittest
import urllib.error
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

import soap  # noqa: E402

RESULT = (b'<?xml version="1.0" encoding="UTF-8"?><SOAP-ENV:Envelope '
          b'xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/" xmlns:ns1="urn:AC"><SOAP-ENV:Body>'
          b'<ns1:executeCommandResponse><result>no test runs active&#xD;\n</result>'
          b'</ns1:executeCommandResponse></SOAP-ENV:Body></SOAP-ENV:Envelope>')
FAULT = (b'<?xml version="1.0" encoding="UTF-8"?><SOAP-ENV:Envelope '
         b'xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"><SOAP-ENV:Body><SOAP-ENV:Fault>'
         b'<faultcode>SOAP-ENV:Client</faultcode><faultstring>There is no such command.</faultstring>'
         b'</SOAP-ENV:Fault></SOAP-ENV:Body></SOAP-ENV:Envelope>')
SECRET = "hunter2-very-secret"


class FakeResponse:
    def __init__(self, body: bytes):
        self.body = body

    def read(self) -> bytes:
        return self.body

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False


def http_error(code: int, body: bytes) -> urllib.error.HTTPError:
    return urllib.error.HTTPError("http://127.0.0.1:7878/", code, "error", {}, io.BytesIO(body))


class EnvelopeTest(unittest.TestCase):
    def test_command_is_escaped(self):
        body = soap.envelope('.dc test start a<b & "c"').decode("utf-8")
        self.assertIn("<command>.dc test start a&lt;b &amp; \"c\"</command>", body)
        self.assertIn('xmlns:ns1="urn:AC"', body)
        self.assertIn("<ns1:executeCommand>", body)


class ParseTest(unittest.TestCase):
    def test_result(self):
        self.assertEqual(soap.parse_response(RESULT), "no test runs active\r\n")

    def test_empty_result_is_empty_text(self):
        body = RESULT.replace(b"no test runs active&#xD;\n", b"")
        self.assertEqual(soap.parse_response(body), "")

    def test_fault_raises_with_server_message(self):
        with self.assertRaises(soap.SoapFault) as caught:
            soap.parse_response(FAULT)
        self.assertEqual(str(caught.exception), "There is no such command.")

    def test_garbage_is_an_error(self):
        with self.assertRaises(soap.SoapError):
            soap.parse_response(b"<html>not soap")
        with self.assertRaises(soap.SoapError):
            soap.parse_response(b"<a><b/></a>")


class ExecuteTest(unittest.TestCase):
    def call(self, outcome):
        target = "soap.urllib.request.urlopen"
        if isinstance(outcome, BaseException):
            patch = mock.patch(target, side_effect=outcome)
        else:
            patch = mock.patch(target, return_value=outcome)
        with patch as urlopen:
            try:
                return soap.execute(".dc test status", "http://127.0.0.1:7878/", "gm", SECRET), urlopen
            except soap.SoapError as exc:
                self.assertNotIn(SECRET, str(exc))
                raise

    def test_ok_sends_basic_auth(self):
        result, urlopen = self.call(FakeResponse(RESULT))
        self.assertEqual(result, "no test runs active\r\n")
        request = urlopen.call_args[0][0]
        self.assertEqual(request.get_method(), "POST")
        self.assertTrue(request.get_header("Authorization").startswith("Basic "))

    def test_fault_over_http_500(self):
        with self.assertRaises(soap.SoapFault):
            self.call(http_error(500, FAULT))

    def test_fault_over_http_200(self):
        with self.assertRaises(soap.SoapFault):
            self.call(FakeResponse(FAULT))

    def test_auth(self):
        for code in (401, 403):
            with self.assertRaises(soap.SoapAuthError):
                self.call(http_error(code, b""))

    def test_500_without_fault(self):
        with self.assertRaises(soap.SoapError) as caught:
            self.call(http_error(500, b""))
        self.assertNotIsInstance(caught.exception, soap.SoapFault)

    def test_down(self):
        for error in (urllib.error.URLError(ConnectionRefusedError(10061, "refused")), ConnectionResetError(),
                      TimeoutError()):
            with self.assertRaises(soap.SoapUnavailable):
                self.call(error)


class EnvTest(unittest.TestCase):
    def write(self, text: str) -> pathlib.Path:
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        path = pathlib.Path(folder.name) / ".env"
        path.write_text(text, encoding="utf-8")
        return path

    def test_reads_quotes_comments_and_export(self):
        path = self.write(f"# soap\nSOAP_URL=http://127.0.0.1:7878/\nexport SOAP_USER='gm'\n"
                          f'SOAP_PASS="{SECRET}"\n\nDC_RUNS_FILE=/x/dc_testruns.jsonl\n')
        self.assertEqual(soap.credentials(path), ("http://127.0.0.1:7878/", "gm", SECRET))
        self.assertEqual(soap.read_env(path)["DC_RUNS_FILE"], "/x/dc_testruns.jsonl")

    def test_missing_keys_name_the_keys_only(self):
        path = self.write(f"SOAP_URL=http://127.0.0.1:7878/\nSOAP_PASS={SECRET}\n")
        with self.assertRaises(soap.SoapError) as caught:
            soap.credentials(path)
        self.assertIn("SOAP_USER", str(caught.exception))
        self.assertNotIn(SECRET, str(caught.exception))

    def test_missing_file(self):
        with self.assertRaises(soap.SoapError):
            soap.credentials(pathlib.Path(tempfile.gettempdir()) / "no-such-dir-raidtest" / ".env")

    def test_client_repr_hides_credentials(self):
        path = self.write(f"SOAP_URL=http://127.0.0.1:7878/\nSOAP_USER=gm\nSOAP_PASS={SECRET}\n")
        self.assertNotIn(SECRET, repr(soap.Client(path)))


if __name__ == "__main__":
    unittest.main()
