"""Тесты основного функционала робота."""

import pytest
import yaml
from pathlib import Path
from datetime import datetime, timedelta
from scripts.crawler import Crawler


class TestCrawlerInit:
    """Тесты инициализации робота."""
    
    def test_load_config(self, temp_dir, test_config):
        config_file = temp_dir / 'config.yaml'
        with open(config_file, 'w') as f:
            yaml.dump(test_config, f)
        
        crawler = Crawler(str(config_file))
        
        assert crawler.db_config['database'] == 'test_search_engine'
        assert crawler.logic['delay_between_requests'] == 0
    
    def test_database_connection(self, temp_dir, test_config, clean_db):
        config_file = temp_dir / 'config.yaml'
        with open(config_file, 'w') as f:
            yaml.dump(test_config, f)
        
        crawler = Crawler(str(config_file))
        
        assert crawler.collection.name == 'test_documents'
        assert crawler.collection.count_documents({}) == 0


class TestCheckpoints:
    """Тесты системы чекпоинтов."""
    
    def test_save_checkpoint(self, temp_dir, test_config):
        config_file = temp_dir / 'config.yaml'
        with open(config_file, 'w') as f:
            yaml.dump(test_config, f)
        
        crawler = Crawler(str(config_file))
        crawler.checkpoint = {'wiki_processed': 100}
        crawler.save_checkpoint()
        
        checkpoint_file = Path(test_config['checkpoints']['state_file'])
        assert checkpoint_file.exists()
    
    def test_load_checkpoint(self, temp_dir, test_config):
        config_file = temp_dir / 'config.yaml'
        checkpoint_file = Path(test_config['checkpoints']['state_file'])
        checkpoint_file.parent.mkdir(parents=True, exist_ok=True)
        
        import json
        with open(checkpoint_file, 'w') as f:
            json.dump({'wiki_processed': 50}, f)
        
        with open(config_file, 'w') as f:
            yaml.dump(test_config, f)
        
        crawler = Crawler(str(config_file))
        
        assert crawler.checkpoint.get('wiki_processed') == 50


class TestDocumentInsertion:
    """Тесты вставки документов."""
    
    def test_insert_document(self, temp_dir, test_config, clean_db):
        config_file = temp_dir / 'config.yaml'
        with open(config_file, 'w') as f:
            yaml.dump(test_config, f)
        
        crawler = Crawler(str(config_file))
        
        doc = {
            'url': 'https://test.com/doc1',
            'raw_html': '<html>Test</html>',
            'source': 'test',
            'last_crawled_at': datetime.now().timestamp()
        }
        
        result = crawler.insert_document(doc)
        assert result is True
        assert clean_db['test_documents'].count_documents({}) == 1
    
    def test_duplicate_url(self, temp_dir, test_config, clean_db):
        config_file = temp_dir / 'config.yaml'
        with open(config_file, 'w') as f:
            yaml.dump(test_config, f)
        
        crawler = Crawler(str(config_file))
        
        doc = {
            'url': 'https://test.com/doc1',
            'raw_html': '<html>Test</html>',
            'source': 'test',
            'last_crawled_at': datetime.now().timestamp()
        }
        
        assert crawler.insert_document(doc) is True
        assert crawler.insert_document(doc) is False
        assert clean_db['test_documents'].count_documents({}) == 1


class TestModeDetection:
    """Тесты определения режима работы."""
    
    def test_empty_database_returns_seed(self, temp_dir, test_config, clean_db):
        config_file = temp_dir / 'config.yaml'
        with open(config_file, 'w') as f:
            yaml.dump(test_config, f)
        
        crawler = Crawler(str(config_file))
        mode = crawler.determine_mode()
        
        assert mode == 'seed'
    
    def test_old_documents_returns_recrawl(self, temp_dir, test_config, clean_db):
        config_file = temp_dir / 'config.yaml'
        with open(config_file, 'w') as f:
            yaml.dump(test_config, f)
        
        crawler = Crawler(str(config_file))
        
        old_timestamp = (datetime.now() - timedelta(days=5)).timestamp()
        
        clean_db['test_documents'].insert_one({
            'url': 'https://test.com/old',
            'raw_html': '<html>Old</html>',
            'source': 'test',
            'last_crawled_at': old_timestamp
        })
        
        mode = crawler.determine_mode()
        assert mode == 'recrawl'
    
    def test_fresh_documents_returns_none(self, temp_dir, test_config, clean_db):
        config_file = temp_dir / 'config.yaml'
        with open(config_file, 'w') as f:
            yaml.dump(test_config, f)
        
        crawler = Crawler(str(config_file))
        
        clean_db['test_documents'].insert_one({
            'url': 'https://test.com/fresh',
            'raw_html': '<html>Fresh</html>',
            'source': 'test',
            'last_crawled_at': datetime.now().timestamp()
        })
        
        mode = crawler.determine_mode()
        assert mode is None
