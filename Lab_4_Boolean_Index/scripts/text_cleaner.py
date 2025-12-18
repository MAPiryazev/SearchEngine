import re
import html

from bs4 import BeautifulSoup
import mwparserfromhell


_ws_re = re.compile(r"\s+")
_html_comment_re = re.compile(r"<!--.*?-->", re.DOTALL)
_ref_re = re.compile(r"<ref\b[^>]*>.*?</ref\s*>", re.IGNORECASE | re.DOTALL)
_ref_self_re = re.compile(r"<ref\b[^>]*/\s*>", re.IGNORECASE)
_gallery_re = re.compile(r"<gallery\b[^>]*>.*?</gallery\s*>", re.IGNORECASE | re.DOTALL)

_file_link_re = re.compile(r"\[\[(Файл|File|Image):[^\]]+\]\]", re.IGNORECASE)
_category_re = re.compile(r"\[\[(Категория|Category):[^\]]+\]\]", re.IGNORECASE)

_ext_link_re = re.compile(r"\[(https?://[^\s\]]+)\s*([^\]]*)\]")


def _norm_ws(s: str) -> str:
    return _ws_re.sub(" ", s).strip()


def clean_stackoverflow_html(raw_html: str, keep_code: bool = False) -> str:
    if not raw_html:
        return ""

    s = html.unescape(raw_html)
    soup = BeautifulSoup(s, "html.parser")

    if not keep_code:
        for t in soup.find_all(["pre", "code"]):
            t.decompose()

    text = soup.get_text(" ", strip=True)
    return _norm_ws(text)


def clean_wiki_wikitext(wikitext: str) -> str:
    if not wikitext:
        return ""

    s = html.unescape(wikitext)

    s = _html_comment_re.sub(" ", s)
    s = _gallery_re.sub(" ", s)
    s = _ref_re.sub(" ", s)
    s = _ref_self_re.sub(" ", s)

    s = _ext_link_re.sub(lambda m: (m.group(2) or " "), s)

    s = _file_link_re.sub(" ", s)
    s = _category_re.sub(" ", s)

    code = mwparserfromhell.parse(s)
    text = code.strip_code(normalize=True, collapse=True)
    return _norm_ws(text)
