"""Тесты парсера Wikipedia."""

import pytest
from datetime import datetime
import xml.etree.ElementTree as ET


def create_wiki_page_xml(title, text, page_id='123'):
    """Создает XML элемент страницы Wikipedia."""
    ns = '{http://www.mediawiki.org/xml/export-0.10/}'
    
    page = ET.Element(f'{ns}page')
    
    title_elem = ET.SubElement(page, f'{ns}title')
    title_elem.text = title
    
    id_elem = ET.SubElement(page, f'{ns}id')
    id_elem.text = page_id
    
    revision = ET.SubElement(page, f'{ns}revision')
    text_elem = ET.SubElement(revision, f'{ns}text')
    text_elem.text = text
    
    timestamp_elem = ET.SubElement(revision, f'{ns}timestamp')
    timestamp_elem.text = '2024-01-01T00:00:00Z'
    
    return page, ns


class TestWikiPageProcessing:
    """Тесты обработки страниц Wikipedia."""
    
    def test_valid_page(self, temp_dir, test_config):
        from scripts.crawler import Crawler
        import yaml
        
        config_file = temp_dir / 'config.yaml'
        with open(config_file, 'w') as f:
            yaml.dump(test_config, f)
        
        crawler = Crawler(str(config_file))
        
        page, ns = create_wiki_page_xml(
            'Тестовая статья',
            'Это длинный текст тестовой статьи ' * 10
        )
        
        doc = crawler.process_wiki_page(page, ns)
        
        assert doc is not None
        assert doc['title'] == 'Тестовая статья'
        assert doc['source'] == 'wiki'
        assert 'url' in doc
        assert 'raw_html' in doc
    
    def test_redirect_page_filtered(self, temp_dir, test_config):
        from scripts.crawler import Crawler
        import yaml
        
        config_file = temp_dir / 'config.yaml'
        with open(config_file, 'w') as f:
            yaml.dump(test_config, f)
        
        crawler = Crawler(str(config_file))
        
        page, ns = create_wiki_page_xml('Редирект', 'Текст')
        redirect = ET.SubElement(page, f'{ns}redirect')
        redirect.set('title', 'Другая страница')
        
        doc = crawler.process_wiki_page(page, ns)
        
        assert doc is None
    
    def test_short_text_filtered(self, temp_dir, test_config):
        from scripts.crawler import Crawler
        import yaml
        
        config_file = temp_dir / 'config.yaml'
        with open(config_file, 'w') as f:
            yaml.dump(test_config, f)
        
        crawler = Crawler(str(config_file))
        
        page, ns = create_wiki_page_xml('Короткая', 'Короткий текст')
        doc = crawler.process_wiki_page(page, ns)
        
        assert doc is None
    
    def test_service_page_filtered(self, temp_dir, test_config):
        from scripts.crawler import Crawler
        import yaml
        
        config_file = temp_dir / 'config.yaml'
        with open(config_file, 'w') as f:
            yaml.dump(test_config, f)
        
        crawler = Crawler(str(config_file))
        
        service_titles = [
            'Википедия:Правила',
            'Шаблон:Инфобокс',
            'Файл:Image.jpg',
            'Категория:Тест'
        ]
        
        for title in service_titles:
            page, ns = create_wiki_page_xml(title, 'Текст ' * 50)
            doc = crawler.process_wiki_page(page, ns)
            assert doc is None
