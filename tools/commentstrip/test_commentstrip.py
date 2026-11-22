import pathlib
import tempfile
import unittest

import commentstrip as cs

HEADER = "/*\n * This file is part of the mod-playerbots module for AzerothCore.\n */\n"


def strip(text: str, keep_texts=frozenset(), keep_params=True) -> str:
    comments = cs.lex(text, "t")
    cs.classify(text, comments, set(keep_texts), keep_params)
    return cs.strip_text(text, [c for c in comments if not c.reason], "t")


class LexTest(unittest.TestCase):
    def test_slashes_in_string_are_code(self):
        self.assertEqual(strip('auto s = "http://x"; // y\n'), 'auto s = "http://x";\n')

    def test_escaped_quote_in_string(self):
        self.assertEqual(strip('auto s = "a\\"//b"; // y\n'), 'auto s = "a\\"//b";\n')

    def test_slash_char_literal(self):
        self.assertEqual(strip("char c = '/'; // y\n"), "char c = '/';\n")

    def test_raw_string_untouched(self):
        src = 'auto r = R"d(// not /* a comment)d";\n'
        self.assertEqual(strip(src), src)

    def test_digit_separator_fails(self):
        with self.assertRaises(cs.StripError):
            cs.lex("int a = 1'000; // x\n", "t")

    def test_continued_line_comment_fails(self):
        with self.assertRaises(cs.StripError):
            cs.lex("// x \\\nint a;\n", "t")


class StripTest(unittest.TestCase):
    def test_full_line_comment(self):
        self.assertEqual(strip("a();\n// x\nb();\n"), "a();\nb();\n")

    def test_trailing_comment(self):
        self.assertEqual(strip("int a = 1;  // x\n"), "int a = 1;\n")

    def test_multi_line_block(self):
        self.assertEqual(strip("a();\n/* x\n * y\n */\nb();\n"), "a();\nb();\n")

    def test_block_between_identifiers(self):
        self.assertEqual(strip("int/*x y*/a;\n"), "int a;\n")

    def test_block_before_code(self):
        self.assertEqual(strip("    /* x y */ foo();\n"), "    foo();\n")

    def test_param_name_kept_by_default(self):
        src = "bool A::Execute(Event /*event*/)\n"
        self.assertEqual(strip(src), src)

    def test_param_name_stripped_on_request(self):
        self.assertEqual(strip("bool A::Execute(Event /*event*/)\n", keep_params=False),
                         "bool A::Execute(Event)\n")
        self.assertEqual(strip("f(/*x*/ 1, /*y*/ 2);\n", keep_params=False), "f(1, 2);\n")

    def test_header_and_directive_kept(self):
        src = HEADER + "\nint a; // NOLINT\n"
        self.assertEqual(strip(src), src)

    def test_keep_text(self):
        src = "// keep in DATA_* order\nint a[3];\n"
        self.assertEqual(strip(src, {"keep in DATA_* order"}), src)


class BlankLineTest(unittest.TestCase):
    def test_banner_between_blanks(self):
        self.assertEqual(strip("a();\n\n// ---- x ----\n\nb();\n"), "a();\n\nb();\n")

    def test_after_open_brace(self):
        self.assertEqual(strip("{\n    // x\n\n    a();\n}\n"), "{\n    a();\n}\n")

    def test_before_close_brace(self):
        self.assertEqual(strip("{\n    a();\n\n    // x\n}\n"), "{\n    a();\n}\n")

    def test_end_of_file(self):
        self.assertEqual(strip("a();\n\n// x\n"), "a();\n")

    def test_existing_blank_kept(self):
        src = "a();\n\n// x\nb();\n"
        self.assertEqual(strip(src), "a();\n\nb();\n")

    def test_after_header(self):
        self.assertEqual(strip(HEADER + "\n// x\n\n#include \"a.h\"\n"), HEADER + "\n#include \"a.h\"\n")


class FileTest(unittest.TestCase):
    def run_strip(self, files: dict, *extra) -> tuple[int, pathlib.Path]:
        tmp = pathlib.Path(tempfile.mkdtemp())
        for name, data in files.items():
            (tmp / name).write_bytes(data)
        code = cs.main(["--root", str(tmp), "strip", *extra, "."])
        return code, tmp

    def test_crlf_kept(self):
        code, tmp = self.run_strip({"a.cpp": b"a();\r\n// x\r\nb(); // y\r\n"})
        self.assertEqual(code, 0)
        self.assertEqual((tmp / "a.cpp").read_bytes(), b"a();\r\nb();\r\n")

    def test_unmatched_keep_entry_fails(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        (tmp / "keep").mkdir()
        (tmp / "keep" / "lane.txt").write_text("a.cpp\tnot there\n", encoding="utf-8")
        code, root = self.run_strip({"a.cpp": b"// x\na();\n"}, "--keep", str(tmp / "keep"))
        self.assertEqual(code, 1)

    def test_matched_keep_entry_passes(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        (tmp / "keep").mkdir()
        (tmp / "keep" / "lane.txt").write_text("a.cpp\tx\n", encoding="utf-8")
        code, root = self.run_strip({"a.cpp": b"// x\na(); // y\n"}, "--keep", str(tmp / "keep"))
        self.assertEqual(code, 0)
        self.assertEqual((root / "a.cpp").read_bytes(), b"// x\na();\n")


class CheckTest(unittest.TestCase):
    def test_comment_and_whitespace_changes_are_equal(self):
        self.assertEqual(cs.code_tokens("int a = 1; // x\nf( a );\n", "t"),
                         cs.code_tokens("int a = 1;\r\nf(a);\r\n", "t"))

    def test_code_change_differs(self):
        self.assertNotEqual(cs.code_tokens("int a = 1; // x\n", "t"), cs.code_tokens("int a = 2;\n", "t"))

    def test_string_whitespace_counts(self):
        self.assertNotEqual(cs.code_tokens('auto s = "a b";\n', "t"), cs.code_tokens('auto s = "a  b";\n', "t"))


if __name__ == "__main__":
    unittest.main()
