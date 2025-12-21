"""Тесты парсера Stack Overflow."""

import pytest
from lxml import etree


def create_so_post_xml(post_id='1', post_type='1', title='Test', body='<p>Test body</p>' * 10):
    """Создает XML элемент поста Stack Overflow."""
    row = etree.Element('row')
    row.set('Id', post_id)
    row.set('PostTypeId', post_type)
    row.set('Title', title)
    row.set('Body', body)
    row.set('CreationDate', '2024-01-01T00:00:00.000')
    row.set('Score', '5')
    row.set('Tags', '<python><testing>')
    
    return row


class TestSOPostProcessing:
    """Тесты обработки постов Stack Overflow."""
    
    def test_valid_question(self, temp_dir, test_config):
        from scripts.crawler import Crawler
        import yaml
        
        config_file = temp_dir / 'config.yaml'
        with open(config_file, 'w') as f:
            yaml.dump(test_config, f)
        
        crawler = Crawler(str(config_file))
        
        post = create_so_post_xml(
            post_id='123',
            title='Как работает Python?',
            body='<p>Подробный текст вопроса</p>' * 10
        )
        
        doc = crawler.process_so_post(post)
        
        assert doc is not None
        assert doc['title'] == 'Как работает Python?'
        assert doc['source'] == 'stackoverflow'
        assert doc['external_id'] == '123'
        assert 'url' in doc
    
    def test_answer_filtered(self, temp_dir, test_config):
        from scripts.crawler import Crawler
        import yaml
        
        config_file = temp_dir / 'config.yaml'
        with open(config_file, 'w') as f:
            yaml.dump(test_config, f)
        
        crawler = Crawler(str(config_file))
        
        post = create_so_post_xml(post_type='2')
        doc = crawler.process_so_post(post)
        
        assert doc is None
    
    def test_short_body_filtered(self, temp_dir, test_config):
        from scripts.crawler import Crawler
        import yaml
        
        config_file = temp_dir / 'config.yaml'
        with open(config_file, 'w') as f:
            yaml.dump(test_config, f)
        
        crawler = Crawler(str(config_file))
        
        post = create_so_post_xml(body='<p>Short</p>')
        doc = crawler.process_so_post(post)
        
        assert doc is None
