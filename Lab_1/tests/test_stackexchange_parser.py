import json
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).parent.parent / "scripts"))
from extract_stackexchange import StackExchangeParser


class TestStackExchangeParser:
    def test_clean_html_removes_tags(self):
        parser = StackExchangeParser(None, None)

        text = "<p>Текст в <strong>параграфе</strong> с тегами</p>"
        result = parser.clean_html(text)

        assert "<p>" not in result
        assert "<strong>" not in result
        assert "Текст в параграфе с тегами" in result

    def test_clean_html_removes_code_blocks(self):
        parser = StackExchangeParser(None, None)

        text = (
            "Текст <code>print('hello')</code> продолжение "
            "<pre>def foo():\n    pass</pre> конец"
        )
        result = parser.clean_html(text)

        assert "<code>" not in result
        assert "<pre>" not in result
        assert "print" not in result
        assert "[код]" in result
        assert "Текст" in result
        assert "продолжение" in result

    def test_clean_html_decodes_entities(self):
        parser = StackExchangeParser(None, None)

        text = 'Текст с &lt;тегами&gt; и &amp; символами &quot;кавычки&quot;'
        result = parser.clean_html(text)

        assert "&lt;" not in result
        assert "&gt;" not in result
        assert "&amp;" not in result
        assert "<тегами>" in result
        assert "& символами" in result

    def test_clean_html_empty_input(self):
        parser = StackExchangeParser(None, None)

        assert parser.clean_html("") == ""
        assert parser.clean_html(None) == ""

    def test_parser_output_format(self, tmp_path):
        xml_content = """<?xml version="1.0" encoding="utf-8"?>
<posts>
  <row Id="1" PostTypeId="1"
       Title="Как использовать Python для парсинга XML?"
       Body="&lt;p&gt;Мне нужно распарсить большой XML файл. Какая библиотека лучше?&lt;/p&gt;"
       Score="42" ViewCount="1000" AnswerCount="5"
       Tags="&lt;python&gt;&lt;xml&gt;&lt;parsing&gt;"
       CreationDate="2024-01-01T00:00:00.000" />
</posts>
"""
        input_file = tmp_path / "test_posts.xml"
        output_file = tmp_path / "test_output.jsonl"
        input_file.write_text(xml_content, encoding="utf-8")

        parser = StackExchangeParser(input_file, output_file, max_docs=10)
        parser.parse()

        assert output_file.exists()

        with open(output_file, "r", encoding="utf-8") as f:
            doc = json.loads(f.readline())

        required_fields = ["id", "external_id", "source", "title", "text", "meta"]
        for field in required_fields:
            assert field in doc

        assert doc["source"] == "stackoverflow"
        assert "Python" in doc["title"]
        assert "XML" in doc["title"]
        assert doc["meta"]["score"] == 42
        assert doc["meta"]["view_count"] == 1000
        assert doc["meta"]["language"] == "ru"

    def test_filters_answers(self, tmp_path):
        xml_content = """<?xml version="1.0" encoding="utf-8"?>
<posts>
  <row Id="2" PostTypeId="2" ParentId="1"
       Body="&lt;p&gt;Это ответ на вопрос с достаточным количеством текста для теста&lt;/p&gt;"
       Score="10" CreationDate="2024-01-01T00:00:00.000" />
</posts>
"""
        input_file = tmp_path / "answer_test.xml"
        output_file = tmp_path / "answer_output.jsonl"
        input_file.write_text(xml_content, encoding="utf-8")

        parser = StackExchangeParser(input_file, output_file, max_docs=10)
        parser.parse()

        assert not output_file.exists() or output_file.stat().st_size == 0

    def test_filters_short_posts(self, tmp_path):
        xml_content = """<?xml version="1.0" encoding="utf-8"?>
<posts>
  <row Id="3" PostTypeId="1"
       Title="Короткий вопрос"
       Body="&lt;p&gt;Мало текста&lt;/p&gt;"
       CreationDate="2024-01-01T00:00:00.000" />
</posts>
"""
        input_file = tmp_path / "short_test.xml"
        output_file = tmp_path / "short_output.jsonl"
        input_file.write_text(xml_content, encoding="utf-8")

        parser = StackExchangeParser(input_file, output_file, max_docs=10)
        parser.parse()

        assert not output_file.exists() or output_file.stat().st_size == 0


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
