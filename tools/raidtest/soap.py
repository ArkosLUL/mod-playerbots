#!/usr/bin/env python3
"""Run one GM command on the worldserver over SOAP.

    soap.py ".dc test status"

Credentials come from tools/raidtest/.env (SOAP_URL, SOAP_USER, SOAP_PASS) and nowhere else. The
account needs SEC_ADMINISTRATOR, below that the server answers 403.
"""
from __future__ import annotations

import base64
import http.client
import pathlib
import socket
import sys
import urllib.error
import urllib.request
import xml.etree.ElementTree as ET
from xml.sax.saxutils import escape

HERE = pathlib.Path(__file__).resolve().parent
ENV_FILE = HERE / ".env"
CREDENTIAL_KEYS = ("SOAP_URL", "SOAP_USER", "SOAP_PASS")

ENVELOPE = (
    '<?xml version="1.0" encoding="utf-8"?>'
    '<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/" '
    'xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/" '
    'xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance" '
    'xmlns:xsd="http://www.w3.org/1999/XMLSchema" xmlns:ns1="urn:AC">'
    "<SOAP-ENV:Body><ns1:executeCommand><command>{command}</command></ns1:executeCommand>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>"
)


class SoapError(Exception):
    """Anything that kept a command from returning a result. Messages never carry credentials."""


class SoapFault(SoapError):
    """The server ran the command, or refused it, and answered with a SOAP fault."""


class SoapAuthError(SoapError):
    """401 or 403: the account in .env is wrong or not an administrator."""


class SoapUnavailable(SoapError):
    """Nothing answered: the worldserver is down, restarting, or not listening yet."""


def read_env(path: pathlib.Path = ENV_FILE) -> dict[str, str]:
    """KEY=VALUE lines; blanks, # comments, `export ` and surrounding quotes are allowed."""
    if not path.is_file():
        raise SoapError(f"{path} does not exist; it needs {', '.join(CREDENTIAL_KEYS)}")
    values: dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, _, value = line.partition("=")
        key = key.strip()
        if key.startswith("export "):
            key = key[len("export "):].strip()
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "'\"":
            value = value[1:-1]
        values[key] = value
    return values


def credentials(path: pathlib.Path = ENV_FILE) -> tuple[str, str, str]:
    values = read_env(path)
    missing = [key for key in CREDENTIAL_KEYS if not values.get(key)]
    if missing:
        raise SoapError(f"{path} has no {', '.join(missing)}")
    return values["SOAP_URL"], values["SOAP_USER"], values["SOAP_PASS"]


def envelope(command: str) -> bytes:
    return ENVELOPE.format(command=escape(command)).encode("utf-8")


def _local(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def _find_text(root: ET.Element, name: str) -> str | None:
    for element in root.iter():
        if _local(element.tag) == name:
            return element.text or ""
    return None


def parse_response(body: bytes) -> str:
    """The command's output. A fault body raises SoapFault with the server's own message."""
    try:
        root = ET.fromstring(body)
    except ET.ParseError as exc:
        raise SoapError(f"unreadable SOAP response: {exc}") from None
    fault = _find_text(root, "faultstring")
    if fault is not None:
        raise SoapFault(fault.strip() or "SOAP fault with no message")
    result = _find_text(root, "result")
    if result is None:
        raise SoapError("SOAP response carries no result")
    return result


def execute(command: str, url: str, user: str, password: str, timeout: float = 60.0) -> str:
    token = base64.b64encode(f"{user}:{password}".encode("utf-8")).decode("ascii")
    request = urllib.request.Request(
        url,
        data=envelope(command),
        method="POST",
        headers={"Content-Type": "text/xml; charset=utf-8", "Authorization": f"Basic {token}"},
    )
    status = 200
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            body = response.read()
    except urllib.error.HTTPError as exc:
        status, body = exc.code, exc.read()
    except (urllib.error.URLError, http.client.HTTPException, ConnectionError, socket.timeout, TimeoutError) as exc:
        reason = getattr(exc, "reason", exc)
        raise SoapUnavailable(f"no SOAP answer from the worldserver: {reason}") from None

    if status in (401, 403):
        raise SoapAuthError(f"SOAP refused the account in .env (HTTP {status})")
    if status != 200:
        # gSOAP sends a fault as HTTP 500 with the fault envelope as its body
        if body:
            parse_response(body)
        raise SoapError(f"SOAP answered HTTP {status} with no fault")
    return parse_response(body)


class Client:
    """Credentials read once from .env, then one call per command."""

    def __init__(self, env_file: pathlib.Path = ENV_FILE, timeout: float = 60.0):
        self._url, self._user, self._password = credentials(env_file)
        self._timeout = timeout

    def __call__(self, command: str) -> str:
        return execute(command, self._url, self._user, self._password, self._timeout)

    def __repr__(self) -> str:
        return "soap.Client()"


def main(argv: list[str]) -> int:
    if len(argv) != 1:
        print('usage: soap.py "<command>"', file=sys.stderr)
        return 2
    try:
        print(Client()(argv[0]))
    except SoapError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
