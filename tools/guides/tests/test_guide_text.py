"""Tests for guide_text.py, on synthetic pages and a fake network only.

    python -m unittest discover -s tools/guides/tests
"""
from __future__ import annotations

import io
import json
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

GUIDES = pathlib.Path(__file__).resolve().parents[1]
FIXTURES = pathlib.Path(__file__).resolve().parent / "fixtures"
sys.path.insert(0, str(GUIDES))

import guide_text as gt  # noqa: E402

MANIFEST = {
    "raid": "test",
    "name": "Test Raid",
    "slugs": {
        "lord-marrowgar": {
            "sources": [{"tag": "wt25", "url": "https://tavern.example/guide/marrowgar/"}],
            "wowhead": "https://www.wowhead.com/wotlk/guide/raids/icecrown-citadel/lord-marrowgar-strategy",
        },
        "deathbringer-saurfang": {
            "sources": [{"tag": "ntb", "url": "https://blocked.example/saurfang/"}],
            "wowhead": "https://www.wowhead.com/wotlk/guide/raids/icecrown-citadel/deathbringer-saurfang-strategy",
        },
        "sister-svalna": {
            "sources": [{"tag": "blog", "url": "http://blog.example/2010/09/post.html",
                         "sections": ["Sister Svalna", "Not in the post"]}],
        },
        "sindragosa-gauntlet": {
            "sources": [{"tag": "blog", "url": "http://blog.example/2010/09/post.html",
                         "sections": ["Sindragosa's gauntlet"]}],
        },
        "wowhead-source": {
            "sources": [{"tag": "wh", "url": "https://www.wowhead.com/wotlk/guide/raids/icecrown-citadel/overview"}],
        },
        "blood-prince-trash": {"note": "no guide covers it"},
    },
}


def fixture(name: str) -> str:
    return (FIXTURES / name).read_text(encoding="utf-8")


def ok(body: str | bytes, content_type: str = "text/html; charset=utf-8") -> gt.Response:
    data = body.encode("utf-8") if isinstance(body, str) else body
    return gt.Response(200, {"content-type": content_type}, data, "OK")


class FakeWeb:
    def __init__(self, pages: dict[str, gt.Response]):
        self.pages = pages
        self.requests: list[str] = []

    def __call__(self, url: str, limit: int) -> gt.Response:
        self.requests.append(url)
        return self.pages.get(url, gt.Response(404, {}, b"", "Not Found"))


class FakeTime:
    def __init__(self):
        self.now = 1000.0
        self.sleeps: list[float] = []

    def clock(self) -> float:
        return self.now

    def sleep(self, seconds: float) -> None:
        self.sleeps.append(seconds)
        self.now += seconds


def fetcher_for(web: FakeWeb, clock: FakeTime | None = None) -> gt.Fetcher:
    clock = clock or FakeTime()
    return gt.Fetcher(get=web, sleep=clock.sleep, clock=clock.clock)


class CacheCase(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self._tmp.name)
        self.cache = self.root / "cache"
        self.manifest_path = self.root / "test.json"
        self.manifest_path.write_text(json.dumps(MANIFEST), encoding="utf-8")
        self.manifest = gt.load_manifest(self.manifest_path)

    def tearDown(self):
        self._tmp.cleanup()

    def run_main(self, *args: str, fetcher: gt.Fetcher | None = None) -> str:
        out = io.StringIO()
        argv = [args[0], "--manifest", str(self.manifest_path), "--cache", str(self.cache), *args[1:]]
        with mock.patch.object(sys, "stdout", out):
            self.assertEqual(gt.main(argv, fetcher=fetcher, today="2026-10-27"), 0)
        return out.getvalue()

    def text(self, slug: str, tag: str) -> str:
        return gt.text_path(self.cache, slug, tag).read_text(encoding="utf-8")


class WowheadSavedPageTest(CacheCase):
    def save(self, name: str, html: str) -> pathlib.Path:
        folder = self.cache / "wowhead"
        folder.mkdir(parents=True, exist_ok=True)
        path = folder / name
        path.write_text(html, encoding="utf-8")
        return path

    def test_converts_a_saved_page_found_by_its_canonical_link(self):
        self.save("whatever name.html", fixture("wowhead_saved.html"))
        assets = self.cache / "wowhead" / "Lord Marrowgar Strategy_files"
        assets.mkdir()
        (assets / "arena.png").write_bytes(b"\x89PNG")
        self.run_main("convert")
        text = self.text("lord-marrowgar", "wowhead")
        self.assertTrue(text.startswith("# Lord Marrowgar Strategy Guide\n"))
        self.assertIn("- Author: Fixture Author", text)
        self.assertIn("- Updated: 2023-05-06", text)
        self.assertIn("matched by canonical link", text)
        self.assertIn("\n## Overview\n", text)
        self.assertIn("\n### Tips for Tanks\n", text)
        self.assertIn("- Bone Spike Graveyard (spell 69057) pins a random player.", text)
        self.assertIn("\n  - Nested detail about the spike.", text)
        self.assertIn("Lord Marrowgar (npc 36612) carries Bryntroll (item 50415).", text)
        self.assertIn("- spell 69075 has no link text.", text)
        self.assertIn("**Saber Lash**", text)
        self.assertIn("[diagram: Where everyone stands](../wowhead/Lord%20Marrowgar%20Strategy_files/arena.png)", text)
        self.assertIn("Icon text stays.", text)
        for junk in ("tiny icon", "never shown", "must not appear", "Share this", "Guides"):
            self.assertNotIn(junk, text)
        index = (self.cache / "INDEX.md").read_text(encoding="utf-8")
        self.assertIn("| lord-marrowgar | wowhead |", index)
        self.assertIn("ok: saved page `whatever name.html`", index)
        self.assertIn("| deathbringer-saurfang | wowhead | https://www.wowhead.com/wotlk/guide/raids/icecrown-citadel/"
                      "deathbringer-saurfang-strategy | | missing: not saved yet | |", index)

    def test_saved_from_comment_identifies_a_page_without_canonical_link(self):
        html = fixture("wowhead_saved.html").replace('<link rel="canonical"', '<link rel="alternate"')
        slug, how = gt.identify_saved(self.manifest, html)
        self.assertEqual((slug, how), ("lord-marrowgar", "saved-from comment"))

    def test_title_is_the_fallback(self):
        html = ("<html><head><title>Deathbringer Saurfang Strategy Guide - WotLK Classic - Wowhead</title></head>"
                "<body><p>text</p></body></html>")
        self.assertEqual(gt.identify_saved(self.manifest, html), ("deathbringer-saurfang", "title"))
        slug, how = gt.identify_saved(self.manifest, "<html><head><title>Some other page</title></head></html>")
        self.assertIsNone(slug)
        self.assertIn("matched", how)

    def test_mhtml_save_is_read_from_its_html_part(self):
        url = MANIFEST["slugs"]["lord-marrowgar"]["wowhead"]
        html = "<html><head><title>Untitled</title></head><body><p>Caf=C3=A9 text</p></body></html>"
        mhtml = (f"From: <Saved by Blink>\r\nSnapshot-Content-Location: {url}\r\nSubject: x\r\nMIME-Version: 1.0\r\n"
                 'Content-Type: multipart/related;\r\n\ttype="text/html";\r\n\tboundary="----B"\r\n\r\n\r\n'
                 "------B\r\nContent-Type: text/html\r\nContent-Transfer-Encoding: quoted-printable\r\n"
                 f"Content-Location: {url}\r\n\r\n{html}\r\n\r\n------B\r\nContent-Type: image/png\r\n"
                 "Content-Transfer-Encoding: base64\r\nContent-Location: https://wow.zamimg.com/a.png\r\n\r\n"
                 "iVBORw0KGgo=\r\n\r\n------B--\r\n")
        self.save("page.mhtml", "").write_bytes(mhtml.encode("ascii"))
        pages = gt.scan_saved(self.manifest, self.cache)
        self.assertEqual([(p.slug, p.how) for p in pages], [("lord-marrowgar", "MHTML location")])
        self.assertIn("Café text", pages[0].text)

    def test_a_deleted_saved_page_leaves_no_text_behind(self):
        path = self.save("marrowgar.html", fixture("wowhead_saved.html"))
        self.run_main("convert")
        self.assertTrue(gt.text_path(self.cache, "lord-marrowgar", "wowhead").exists())
        path.unlink()
        self.run_main("convert")
        self.assertFalse(gt.text_path(self.cache, "lord-marrowgar", "wowhead").exists())
        index = (self.cache / "INDEX.md").read_text(encoding="utf-8")
        self.assertIn("lord-marrowgar-strategy | | missing: not saved yet | |", index)

    def test_comment_bylines_are_not_the_page_author(self):
        html = ("<html><head><title>t</title></head><body><article><p>" + "body text " * 30 + "</p></article>"
                "<div id='comments'><div class='comment-author vcard'><cite class='fn'>A Reader</cite></div>"
                "<time datetime='2011-01-01'>x</time></div></body></html>")
        info = gt.render_page(html, "https://plain.example/").info
        self.assertEqual((info.author, info.published), ("", ""))

    def test_unmatched_page_is_listed_in_the_index(self):
        self.save("random.html", "<html><head><title>Cooking tips</title></head><body>x</body></html>")
        out = self.run_main("convert")
        self.assertIn("unmatched saved page random.html", out)
        index = (self.cache / "INDEX.md").read_text(encoding="utf-8")
        self.assertIn("## Saved pages that matched no slug", index)
        self.assertIn("`random.html`", index)


class SiteBodyTest(unittest.TestCase):
    def test_wordpress_article(self):
        rendered = gt.render_page(fixture("wordpress.html"), "https://tavern.example/guide/marrowgar/")
        text = rendered.markdown
        self.assertTrue(text.startswith("# Fixture Raid Guide\n"))
        self.assertIn("- Author: Tavern Writer", text)
        self.assertIn("- Updated: 2022-11-20", text)
        self.assertIn("- Published: 2022-10-01", text)
        self.assertIn("Blistering Cold (spell 70123) every so often", text)
        self.assertIn("[diagram: Raid positions](https://tavern.example/wp-content/uploads/positions.png)", text)
        self.assertIn("| Role | Job |\n|---|---|\n| Tank | Hold the boss \\| face away |\n| Healer | Heal |", text)
        self.assertIn("## Tips for Healers\n\nKeep everyone topped.", text)
        for junk in ("Contents entry", "Share this", "Related", "Another guide", "Reader comment", "Copyright",
                     "Home"):
            self.assertNotIn(junk, text)
        self.assertEqual(text.count("Fixture Raid Guide"), 1)

    def test_blogger_post(self):
        rendered = gt.render_page(fixture("blogger.html"), "http://blog.example/2010/09/post.html")
        text = rendered.markdown
        self.assertTrue(text.startswith("# A gauntlet walkthrough\n"))
        self.assertIn("- Author: Fixture Mage", text)
        self.assertNotIn("- Updated:", text)
        self.assertIn("- Published: 2010-09-18", text)
        self.assertIn("**Sister Svalna**\n\nShe throws spears at the raid.", text)
        self.assertIn("pillar until the spear lands.\nSecond line of the same paragraph.", text)
        self.assertIn("[diagram: Map of the hall](https://blogger.example/img/full-map.png)", text)
        self.assertNotIn("Fixture Blog", text.split("\n", 1)[1])
        self.assertNotIn("must not appear", text)
        self.assertNotIn("Posted by", text)

    def test_sections_cut_a_page_to_the_named_parts(self):
        rendered = gt.render_page(fixture("blogger.html"), "http://blog.example/2010/09/post.html",
                                  sections=["Sister Svalna", "Not in the post"])
        self.assertEqual(rendered.sections_found, ["Sister Svalna"])
        self.assertEqual(rendered.sections_missing, ["Not in the post"])
        self.assertIn("She throws spears", rendered.markdown)
        self.assertNotIn("Pull the spiders", rendered.markdown)
        self.assertNotIn("Opening words", rendered.markdown)
        self.assertIn("- Sections: Sister Svalna\n- Sections not found: Not in the post", rendered.markdown)
        whole = gt.render_page(fixture("blogger.html"), "http://blog.example/2010/09/post.html", sections=["Nothing"])
        self.assertIn("none found, whole page kept", whole.markdown)
        self.assertIn("Pull the spiders", whole.markdown)

    def test_generic_page_falls_back_to_the_densest_block(self):
        filler = " ".join(["word"] * 60)
        html = (f"<html><head><title>Plain</title></head><body><div class='top'><p>short</p></div>"
                f"<div class='content'><p>{filler}</p><p>{filler}</p></div></body></html>")
        rendered = gt.render_page(html, "https://plain.example/page")
        self.assertNotIn("short", rendered.markdown)
        self.assertEqual(rendered.markdown.count(filler), 2)


class NonAsciiTest(CacheCase):
    PAGE = ("<html><head><meta charset='{charset}'><title>Blood-Queen Lana’thel</title></head><body><article>"
            "<p>{body}</p></article></body></html>")
    BODY = "Lana’thel — Æther, über, Кровь " + "filler " * 40

    def test_utf8_and_declared_charsets_decode(self):
        utf8 = self.PAGE.format(charset="utf-8", body=self.BODY).encode("utf-8")
        self.assertIn("Æther", gt.decode_html(utf8))
        latin = self.PAGE.format(charset="windows-1252", body="café — naïve").encode("cp1252")
        self.assertIn("café — naïve", gt.decode_html(latin))
        self.assertIn("café", gt.decode_html(latin.replace(b"windows-1252", b"x-unknown"), "cp1252"))

    def test_text_is_written_as_utf8_and_printing_survives_a_cp1251_console(self):
        folder = self.cache / "wowhead"
        folder.mkdir(parents=True)
        (folder / "Æther notes.html").write_text(self.PAGE.format(charset="utf-8", body=self.BODY),
                                                      encoding="utf-8")
        raw = gt.raw_path(self.cache, self.manifest.sources[0])
        raw.parent.mkdir(parents=True)
        raw.write_bytes(self.PAGE.format(charset="utf-8", body=self.BODY).encode("utf-8"))
        buffer = io.BytesIO()
        console = io.TextIOWrapper(buffer, encoding="cp1251", errors="strict")
        with mock.patch.object(sys, "stdout", console):
            self.assertEqual(gt.main(["convert", "--manifest", str(self.manifest_path), "--cache", str(self.cache)]), 0)
            console.flush()
        self.assertIn("Æther notes.html", buffer.getvalue().decode("utf-8"))
        text = self.text("lord-marrowgar", "wt25")
        self.assertIn("# Blood-Queen Lana’thel", text)
        self.assertIn("Æther, über, Кровь", text)


class RobotsTest(unittest.TestCase):
    PAGE = "https://site.example/guide/"

    def test_robots_disallowing_claudebot_refuses_without_requesting_the_page(self):
        web = FakeWeb({"https://site.example/robots.txt": ok("User-agent: ClaudeBot\nDisallow: /\n", "text/plain")})
        outcome = fetcher_for(web).fetch(self.PAGE)
        self.assertEqual((outcome.status, outcome.reason), ("refused", "robots.txt disallows ClaudeBot"))
        self.assertEqual(web.requests, ["https://site.example/robots.txt"])

    def test_robots_disallowing_this_agent_refuses(self):
        robots = "User-agent: mod-playerbots-guide-reader\nDisallow: /guide/\n"
        web = FakeWeb({"https://site.example/robots.txt": ok(robots, "text/plain")})
        outcome = fetcher_for(web).fetch(self.PAGE)
        self.assertEqual(outcome.reason, f"robots.txt disallows {gt.USER_AGENT}")

    def test_crawl_delay_and_the_two_second_floor(self):
        robots = "User-agent: *\nCrawl-delay: 7\nDisallow: /admin/\n"
        web = FakeWeb({"https://site.example/robots.txt": ok(robots, "text/plain"),
                       self.PAGE: ok("<p>a</p>"), "https://site.example/other/": ok("<p>b</p>"),
                       "https://second.example/robots.txt": gt.Response(404, {}, b"", "Not Found"),
                       "https://second.example/a": ok("<p>c</p>"), "https://second.example/b": ok("<p>d</p>")})
        clock = FakeTime()
        fetcher = fetcher_for(web, clock)
        for url in (self.PAGE, "https://site.example/other/", "https://second.example/a", "https://second.example/b"):
            self.assertEqual(fetcher.fetch(url).status, "ok")
        self.assertEqual(clock.sleeps, [7.0, 7.0, 2.0, 2.0])
        self.assertEqual(fetcher.fetch(self.PAGE).status, "ok")
        self.assertEqual(len(web.requests), 6)

    def test_unavailable_robots_refuses(self):
        web = FakeWeb({"https://site.example/robots.txt": gt.Response(503, {}, b"", "Unavailable")})
        outcome = fetcher_for(web).fetch(self.PAGE)
        self.assertEqual(outcome.status, "refused")
        self.assertIn("robots.txt unavailable", outcome.reason)

    def test_403_stops_the_host_for_the_run(self):
        web = FakeWeb({"https://site.example/robots.txt": gt.Response(404, {}, b"", "Not Found"),
                       self.PAGE: gt.Response(403, {}, b"", "Forbidden"), "https://site.example/b": ok("<p>b</p>")})
        fetcher = fetcher_for(web)
        self.assertEqual(fetcher.fetch(self.PAGE).status, "refused")
        second = fetcher.fetch("https://site.example/b")
        self.assertEqual(second.status, "refused")
        self.assertIn("stopped earlier in this run (HTTP 403)", second.reason)
        self.assertNotIn("https://site.example/b", web.requests)

    def test_robots_403_stops_the_host(self):
        web = FakeWeb({"https://site.example/robots.txt": gt.Response(403, {}, b"", "Forbidden")})
        outcome = fetcher_for(web).fetch(self.PAGE)
        self.assertEqual(outcome.status, "refused")
        self.assertIn("HTTP 403", outcome.reason)
        self.assertEqual(web.requests, ["https://site.example/robots.txt"])


class WowheadRefusalTest(unittest.TestCase):
    def test_wowhead_is_refused_without_any_request(self):
        web = FakeWeb({})
        fetcher = fetcher_for(web)
        for url in ("https://www.wowhead.com/wotlk/guide/raids/icecrown-citadel/overview",
                    "http://wowhead.com/", "https://WOTLK.Wowhead.com./x", "https://wow.zamimg.com/images/a.png"):
            outcome = fetcher.fetch(url)
            self.assertEqual(outcome.status, "refused", url)
        self.assertEqual(web.requests, [])

    def test_a_redirect_to_wowhead_is_not_followed(self):
        web = FakeWeb({"https://site.example/robots.txt": gt.Response(404, {}, b"", "Not Found"),
                       "https://site.example/go": gt.Response(302, {"location": "https://www.wowhead.com/x"}, b"", "")})
        outcome = fetcher_for(web).fetch("https://site.example/go")
        self.assertEqual(outcome.status, "refused")
        self.assertIn("wowhead.com is never fetched", outcome.reason)
        self.assertEqual(web.requests, ["https://site.example/robots.txt", "https://site.example/go"])

    def test_banned_domains_do_not_catch_lookalikes(self):
        self.assertIsNone(gt.banned_reason("notwowhead.com"))
        self.assertIsNotNone(gt.banned_reason("classic.wowhead.com"))


class FetchCommandTest(CacheCase):
    def web(self) -> FakeWeb:
        page = fixture("wordpress.html")
        return FakeWeb({
            "https://tavern.example/robots.txt": ok("User-agent: *\nDisallow: /wp-admin/\n", "text/plain"),
            "https://tavern.example/guide/marrowgar/": ok(page),
            "https://tavern.example/wp-content/uploads/positions.png": ok(b"\x89PNG", "image/png"),
            "https://blocked.example/robots.txt": ok("User-agent: ClaudeBot\nDisallow: /\n", "text/plain"),
            "http://blog.example/robots.txt": gt.Response(404, {}, b"", "Not Found"),
            "http://blog.example/2010/09/post.html": ok(fixture("blogger.html")),
            "https://blogger.example/robots.txt": gt.Response(404, {}, b"", "Not Found"),
            "https://blogger.example/img/full-map.png": ok(b"<html>", "text/html"),
        })

    def test_fetch_then_convert(self):
        web = self.web()
        clock = FakeTime()
        self.run_main("fetch", "--images", fetcher=fetcher_for(web, clock))
        self.assertEqual(web.requests.count("http://blog.example/2010/09/post.html"), 1)
        self.assertTrue(gt.raw_path(self.cache, gt.Source("lord-marrowgar", "wt25", "")).exists())
        self.assertTrue((self.cache / "raw" / "blog" / "sindragosa-gauntlet.html").exists())
        self.assertTrue((self.cache / "wowhead").is_dir())
        log = json.loads((self.cache / "fetch-log.json").read_text(encoding="utf-8"))
        self.assertEqual(log["lord-marrowgar|wt25"]["images"], {"ok": 1, "refused": 0, "error": 0})
        self.assertEqual(log["sister-svalna|blog"]["images"], {"ok": 0, "refused": 0, "error": 1})
        self.assertEqual(log["deathbringer-saurfang|ntb"]["reason"], "robots.txt disallows ClaudeBot")
        self.assertIn("wowhead.com is never fetched", log["wowhead-source|wh"]["reason"])
        self.run_main("convert")
        marrowgar = self.text("lord-marrowgar", "wt25")
        self.assertIn("- Fetched: 2026-10-27", marrowgar)
        self.assertRegex(marrowgar,
                         r"\[diagram: Raid positions\]\(\.\./img/wt25/lord-marrowgar/[0-9a-f]{10}-positions\.png\)")
        self.assertIn("Pull the spiders", self.text("sindragosa-gauntlet", "blog"))
        self.assertNotIn("She throws spears", self.text("sindragosa-gauntlet", "blog"))
        index = (self.cache / "INDEX.md").read_text(encoding="utf-8")
        self.assertIn("| lord-marrowgar | wt25 | https://tavern.example/guide/marrowgar/ | 2026-10-27 "
                      "| ok; images 1 ok |", index)
        self.assertIn("| deathbringer-saurfang | ntb | https://blocked.example/saurfang/ | 2026-10-27 | refused: "
                      "robots.txt disallows ClaudeBot | |", index)
        self.assertIn("ok; images 1 error; sections Sister Svalna; not found Not in the post |", index)
        self.assertIn("| blood-prince-trash | | | | missing: no guide covers it | |", index)

    def test_a_failed_refetch_keeps_the_earlier_copy(self):
        self.run_main("fetch", "--slug", "lord-marrowgar", fetcher=fetcher_for(self.web()))
        down = FakeWeb({"https://tavern.example/robots.txt": gt.Response(404, {}, b"", "Not Found"),
                        "https://tavern.example/guide/marrowgar/": gt.Response(500, {}, b"", "Server Error")})
        argv = ["fetch", "--manifest", str(self.manifest_path), "--cache", str(self.cache), "--slug", "lord-marrowgar"]
        with mock.patch.object(sys, "stdout", io.StringIO()):
            gt.main(argv, fetcher=fetcher_for(down), today="2026-10-28")
        log = json.loads((self.cache / "fetch-log.json").read_text(encoding="utf-8"))
        entry = log["lord-marrowgar|wt25"]
        self.assertEqual((entry["status"], entry["kept"]), ("error", "2026-10-27"))
        self.run_main("convert", "--slug", "lord-marrowgar")
        self.assertIn("- Fetched: 2026-10-27", self.text("lord-marrowgar", "wt25"))
        index = (self.cache / "INDEX.md").read_text(encoding="utf-8")
        self.assertIn("error: HTTP 500 Server Error (kept the copy from 2026-10-27)", index)

    def test_unknown_slug_and_bad_manifest_fail(self):
        err = io.StringIO()
        with mock.patch.object(sys, "stderr", err):
            code = gt.main(["convert", "--manifest", str(self.manifest_path), "--cache", str(self.cache),
                            "--slug", "nope"])
        self.assertEqual(code, 2)
        bad = self.root / "bad.json"
        bad.write_text(json.dumps({"raid": "x", "name": "X", "slugs": {"a": {"sources": [
            {"tag": "wowhead", "url": "https://a.example/"}]}}}), encoding="utf-8")
        with self.assertRaises(gt.ManifestError):
            gt.load_manifest(bad)


if __name__ == "__main__":
    unittest.main()
