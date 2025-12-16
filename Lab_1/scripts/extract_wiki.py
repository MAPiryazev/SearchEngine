import bz2
import json
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from utils.config import Config


_WS_RE = re.compile(r"\s+")
_TMPL_RE = re.compile(r"\{\{[^}]+\}\}")
_FILE_RE = re.compile(r"\[\[(File|Файл|Image|Изображение):[^\]]+\]\]", flags=re.IGNORECASE)
_LINK_RE = re.compile(r"\[\[(?:[^|\]]*\|)?([^\]]+)\]\]")
_EXTLINK_RE = re.compile(r"\[https?://[^\]]+\]")
_TAG_RE = re.compile(r"<[^>]+>")
_QUOTES_RE = re.compile(r"'{2,}")


SKIP_PREFIXES = {
    "Википедия", "Wikipedia", "Обсуждение", "Участник",
    "Шаблон", "Template", "Файл", "File", "Категория", "Category",
    "Портал", "Проект", "Справка", "MediaWiki", "Модуль",
}


class WikiParser:
    def __init__(self, dump_path, output_path, max_docs=None, min_text_len=100):
        self.dump_path = Path(dump_path)
        self.output_path = Path(output_path)
        self.max_docs = max_docs
        self.min_text_len = min_text_len
        self.doc_count = 0
        self.ns = ""

    def clean_wikitext(self, text):
        if not text:
            return ""

        text = _TMPL_RE.sub("", text)
        text = _FILE_RE.sub("", text)
        text = _LINK_RE.sub(r"\1", text)
        text = _EXTLINK_RE.sub("", text)
        text = _TAG_RE.sub("", text)
        text = _QUOTES_RE.sub("", text)

        return _WS_RE.sub(" ", text).strip()

    def _detect_namespace(self, tag):
        m = re.match(r"\{.*\}", tag)
        return m.group(0) if m else ""

    def _find_text(self, parent, name):
        elem = parent.find(f"{self.ns}{name}")
        return elem.text if elem is not None else None

    def _should_skip_title(self, title):
        if ":" not in title:
            return False
        prefix = title.split(":", 1)[0]
        return prefix in SKIP_PREFIXES

    def _parse_page(self, page):
        title = self._find_text(page, "title")
        if not title or self._should_skip_title(title):
            return None

        if page.find(f"{self.ns}redirect") is not None:
            return None

        page_id = self._find_text(page, "id") or ""

        revision = page.find(f"{self.ns}revision")
        if revision is None:
            return None

        raw_text = self._find_text(revision, "text")
        if not raw_text:
            return None

        text = self.clean_wikitext(raw_text)
        if len(text) < self.min_text_len:
            return None

        timestamp = self._find_text(revision, "timestamp")

        self.doc_count += 1
        return {
            "id": self.doc_count,
            "external_id": page_id or str(self.doc_count),
            "source": "wiki",
            "title": title,
            "text": text,
            "meta": {
                "url": f'https://ru.wikipedia.org/wiki/{title.replace(" ", "_")}',
                "timestamp": timestamp,
                "raw_length_bytes": len(raw_text.encode("utf-8")),
                "language": "ru",
            },
        }

    def parse(self):
        if not self.dump_path.exists():
            print(f"Файл не найден: {self.dump_path}")
            return

        self.output_path.parent.mkdir(parents=True, exist_ok=True)

        with bz2.open(self.dump_path, "rb") as bz2_file, open(
            self.output_path, "w", encoding="utf-8", buffering=8192
        ) as out:
            context = ET.iterparse(bz2_file, events=("start", "end"))
            _, root = next(context)
            self.ns = self._detect_namespace(root.tag)

            for event, elem in context:
                if event != "end" or elem.tag != f"{self.ns}page":
                    continue

                doc = self._parse_page(elem)
                if doc:
                    out.write(json.dumps(doc, ensure_ascii=False))
                    out.write("\n")

                    if self.doc_count % 500 == 0:
                        print(f"Обработано: {self.doc_count}", end="\r")
                        out.flush()

                    if self.max_docs and self.doc_count >= self.max_docs:
                        break

                elem.clear()
                root.clear()

        print(f"\nГотово. Документов: {self.doc_count}")
        print(f"Файл: {self.output_path}")


def main():
    Config.create_dirs()

    dump_path = Config.CORPUS_RAW_DIR / "wiki" / "ruwiki-latest-pages-articles.xml.bz2"
    output_path = Config.CORPUS_EXTRACTED_DIR / "wiki" / "docs.jsonl"

    parser = WikiParser(
        dump_path=dump_path,
        output_path=output_path,
        max_docs=Config.WIKI_MIN_DOCS,
    )
    parser.parse()


if __name__ == "__main__":
    main()
