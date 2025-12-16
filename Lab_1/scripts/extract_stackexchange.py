import json
import re
import sys
from pathlib import Path

from lxml import etree

sys.path.insert(0, str(Path(__file__).parent))
from utils.config import Config


_CODE_RE = re.compile(r"<code>.*?</code>", flags=re.DOTALL)
_PRE_RE = re.compile(r"<pre>.*?</pre>", flags=re.DOTALL)
_TAG_RE = re.compile(r"<[^>]+>")
_WS_RE = re.compile(r"\s+")
_HTML_ENTITIES = (
    ("&lt;", "<"),
    ("&gt;", ">"),
    ("&amp;", "&"),
    ("&quot;", '"'),
    ("&#39;", "'"),
)


def _clear_element(elem):
    elem.clear()
    while elem.getprevious() is not None:
        del elem.getparent()[0]


class StackExchangeParser:
    def __init__(self, posts_xml_path, output_path, max_docs=None, min_text_len=100):
        self.posts_xml_path = Path(posts_xml_path)
        self.output_path = Path(output_path)
        self.max_docs = max_docs
        self.min_text_len = min_text_len
        self.doc_count = 0

    def clean_html(self, text):
        if not text:
            return ""

        text = _CODE_RE.sub(" [код] ", text)
        text = _PRE_RE.sub(" [блок кода] ", text)
        text = _TAG_RE.sub("", text)

        for src, dst in _HTML_ENTITIES:
            text = text.replace(src, dst)

        return _WS_RE.sub(" ", text).strip()

    def extract_document(self, row_elem):
        if row_elem.get("PostTypeId") != "1":
            return None

        post_id = row_elem.get("Id")
        body = row_elem.get("Body")
        if not body:
            return None

        text = self.clean_html(body)
        if len(text) < self.min_text_len:
            return None

        title = row_elem.get("Title")
        full_text = f"{title}. {text}" if title else text

        tags = row_elem.get("Tags", "")
        score = int(row_elem.get("Score", "0"))
        views = int(row_elem.get("ViewCount", "0"))
        created = row_elem.get("CreationDate")

        self.doc_count += 1

        return {
            "id": self.doc_count,
            "external_id": post_id,
            "source": "stackexchange",
            "title": title or f"Вопрос {post_id}",
            "text": full_text,
            "meta": {
                "url": f"https://ru.stackoverflow.com/questions/{post_id}",
                "timestamp": created,
                "tags": tags,
                "score": score,
                "views": views,
                "raw_length_bytes": len(body.encode("utf-8")),
                "language": "ru",
            },
        }

    def parse(self):
        if not self.posts_xml_path.exists():
            print(f"Файл не найден: {self.posts_xml_path}")
            return

        self.output_path.parent.mkdir(parents=True, exist_ok=True)

        context = etree.iterparse(
            str(self.posts_xml_path),
            events=("end",),
            tag="row",
        )

        with open(self.output_path, "w", encoding="utf-8") as out:
            for _, elem in context:
                doc = self.extract_document(elem)
                if doc:
                    out.write(json.dumps(doc, ensure_ascii=False))
                    out.write("\n")

                    if self.doc_count % 1000 == 0:
                        print(f"Обработано: {self.doc_count}", end="\r")

                    if self.max_docs and self.doc_count >= self.max_docs:
                        break

                _clear_element(elem)

        print(f"\nГотово. Документов: {self.doc_count}")


def main():
    Config.create_dirs()

    posts_xml = Config.CORPUS_RAW_DIR / "stackexchange" / "Posts.xml"
    output_path = Config.CORPUS_EXTRACTED_DIR / "stackexchange" / "docs.jsonl"

    parser = StackExchangeParser(
        posts_xml_path=posts_xml,
        output_path=output_path,
        max_docs=Config.STACKEXCHANGE_MIN_DOCS,
    )
    parser.parse()


if __name__ == "__main__":
    main()
