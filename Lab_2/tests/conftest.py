import pytest
import tempfile
import shutil
from pathlib import Path
from pymongo import MongoClient


@pytest.fixture
def temp_dir():
    """Временная директория для тестов."""
    tmp = tempfile.mkdtemp()
    yield Path(tmp)
    shutil.rmtree(tmp)


@pytest.fixture
def test_config(temp_dir):
    """Тестовая конфигурация."""
    return {
        'db': {
            'uri': 'mongodb://localhost:27017/',
            'database': 'test_search_engine',
            'collection': 'test_documents',
            'indexes': [
                {'fields': ['url'], 'unique': True}
            ]
        },
        'logic': {
            'delay_between_requests': 0,
            'recrawl_after_days': 1,
            'max_retries': 2,
            'request_timeout': 5,
            'user_agent': 'TestBot/1.0'
        },
        'sources': {
            'wikipedia': {
                'enabled': True,
                'seed': {'max_docs': 10}
            },
            'stackoverflow': {
                'enabled': True,
                'seed': {'max_docs': 10}
            }
        },
        'checkpoints': {
            'enabled': True,
            'save_every': 5,
            'state_file': str(temp_dir / 'checkpoint.json')
        },
        'logging': {
            'level': 'ERROR',
            'file': str(temp_dir / 'test.log'),
            'console': False
        }
    }


@pytest.fixture
def mongo_client():
    """MongoDB клиент для тестов."""
    client = MongoClient('mongodb://localhost:27017/')
    yield client
    client.drop_database('test_search_engine')
    client.close()


@pytest.fixture
def clean_db(mongo_client):
    """Очистка тестовой базы."""
    db = mongo_client['test_search_engine']
    db['test_documents'].delete_many({})
    yield db
