import json
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).parent.parent / "scripts"))
from extract_wiki import WikiParser


class TestWikiParser:
    def test_clean_wikitext_removes_templates(self):
        parser = WikiParser(None, None)

        text = "Текст {{Шаблон|параметр=значение}} продолжение"
        result = parser.clean_wikitext(text)

        assert "{{" not in result
        assert "}}" not in result
        assert "Текст" in result
        assert "продолжение" in result

    def test_clean_wikitext_removes_file_links(self):
        parser = WikiParser(None, None)

        text = "Текст [[Файл:Image.jpg|thumb|Описание]] продолжение"
        result = parser.clean_wikitext(text)

        assert "Файл:" not in result
        assert "Image.jpg" not in result
        assert "Текст" in result
        assert "продолжение" in result

    def test_clean_wikitext_converts_wiki_links(self):
        parser = WikiParser(None, None)

        assert parser.clean_wikitext("[[Москва]]") == "Москва"
        assert parser.clean_wikitext("[[Москва|столица России]]") == "столица России"

    def test_clean_wikitext_removes_html(self):
        parser = WikiParser(None, None)

        text = "Текст <ref>Источник</ref> <div>блок</div> продолжение"
        result = parser.clean_wikitext(text)

        assert "<ref>" not in result
        assert "<div>" not in result
        assert "Текст" in result
        assert "продолжение" in result

    def test_clean_wikitext_normalizes_whitespace(self):
        parser = WikiParser(None, None)

        text = "Текст    с     множественными\n\n\nпробелами"
        result = parser.clean_wikitext(text)

        assert "    " not in result
        assert "\n\n" not in result
        assert result == "Текст с множественными пробелами"

    def test_clean_wikitext_empty_input(self):
        parser = WikiParser(None, None)

        assert parser.clean_wikitext("") == ""
        assert parser.clean_wikitext(None) == ""

    def test_parser_output_format(self, tmp_path):
        xml_content = """<?xml version="1.0" encoding="UTF-8"?>
<mediawiki xmlns="http://www.mediawiki.org/xml/export-0.11/">
  <page>
    <title>Тестовая статья</title>
    <ns>0</ns>
    <id>12345</id>
    <revision>
      <id>67890</id>
      <timestamp>2024-01-01T00:00:00Z</timestamp>
      <text>Это тестовая статья с достаточным количеством текста для прохождения фильтра минимальной длины. Здесь должно быть более ста символов чистого текста после обработки.</text>
    </revision>
  </page>
</mediawiki>
"""
        input_file = tmp_path / "test_dump.xml"
        output_file = tmp_path / "test_output.jsonl"

        input_file.write_text(xml_content, encoding="utf-8")

        parser = WikiParser(
            dump_path=input_file,
            output_path=output_file,
            max_docs=10,
        )
        parser.parse()

        assert output_file.exists()

        with open(output_file, "r", encoding="utf-8") as f:
            doc = json.loads(f.readline())

        required_fields = ["id", "external_id", "source", "title", "text", "meta"]
        for field in required_fields:
            assert field in doc

        assert doc["source"] == "wiki"
        assert doc["title"] == "Тестовая статья"
        assert "тестовая статья" in doc["text"].lower()
        assert doc["meta"]["language"] == "ru"


class TestWikiParserFilters:
    def test_filters_service_pages(self, tmp_path):
        xml_content = """<?xml version="1.0" encoding="UTF-8"?>
<mediawiki xmlns="http://www.mediawiki.org/xml/export-0.11/">
  <page>
    <title>Википедия:Правила</title>
    <ns>4</ns>
    <id>1</id>
    <revision>
      <text>Служебная страница с большим количеством текста для теста фильтрации служебных страниц Википедии.</text>
    </revision>
  </page>
</mediawiki>
"""
        input_file = tmp_path / "service_test.xml"
        output_file = tmp_path / "service_output.jsonl"

        input_file.write_text(xml_content, encoding="utf-8")

        parser = WikiParser(input_file, output_file, max_docs=10)
        parser.parse()

        assert not output_file.exists() or output_file.stat().st_size == 0

    def test_filters_redirects(self, tmp_path):
        xml_content = """<?xml version="1.0" encoding="UTF-8"?>
<mediawiki xmlns="http://www.mediawiki.org/xml/export-0.11/">
  <page>
    <title>Москва-река</title>
    <ns>0</ns>
    <id>999</id>
    <redirect title="Москва (река)" />
    <revision>
      <text>#REDIRECT [[Москва (река)]]</text>
    </revision>
  </page>
</mediawiki>
"""
        input_file = tmp_path / "redirect_test.xml"
        output_file = tmp_path / "redirect_output.jsonl"

        input_file.write_text(xml_content, encoding="utf-8")

        parser = WikiParser(input_file, output_file, max_docs=10)
        parser.parse()

        assert not output_file.exists() or output_file.stat().st_size == 0


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
