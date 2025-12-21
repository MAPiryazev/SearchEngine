import os
from pathlib import Path
from dotenv import load_dotenv

PROJECT_ROOT = Path(__file__).parent.parent.parent

# Загружаем .env из корня
load_dotenv(PROJECT_ROOT / '.env')

class Config:
    """Конфигурация Python-части из .env"""
    
    CORPUS_RAW_DIR = PROJECT_ROOT / os.getenv('CORPUS_RAW_DIR', 'corpus/raw')
    CORPUS_EXTRACTED_DIR = PROJECT_ROOT / os.getenv('CORPUS_EXTRACTED_DIR', 'corpus/extracted')
    CORPUS_ANALYSIS_DIR = PROJECT_ROOT / os.getenv('CORPUS_ANALYSIS_DIR', 'corpus/analysis')
    
    WIKI_DUMP_URL = os.getenv('WIKI_DUMP_URL')
    WIKI_MIN_DOCS = int(os.getenv('WIKI_MIN_DOCS', 50000))
    WIKI_LANGUAGE = os.getenv('WIKI_LANGUAGE', 'ru')
    
    STACKEXCHANGE_SITE = os.getenv('STACKEXCHANGE_SITE', 'ru.stackoverflow.com')
    STACKEXCHANGE_MIN_DOCS = int(os.getenv('STACKEXCHANGE_MIN_DOCS', 50000))
    
    TOTAL_TARGET_DOCS = int(os.getenv('TOTAL_TARGET_DOCS', 100000))
    ENCODING = os.getenv('ENCODING', 'utf-8')
    BUFFER_SIZE = int(os.getenv('BUFFER_SIZE', 65536))
    
    @classmethod
    def create_dirs(cls):
        """Создаёт директории для корпуса"""
        for dir_path in [cls.CORPUS_RAW_DIR, cls.CORPUS_EXTRACTED_DIR, cls.CORPUS_ANALYSIS_DIR]:
            dir_path.mkdir(parents=True, exist_ok=True)
            (dir_path / 'wiki').mkdir(exist_ok=True)
            (dir_path / 'stackexchange').mkdir(exist_ok=True)
