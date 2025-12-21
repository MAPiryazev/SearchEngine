import sys
import yaml
import json
import time
import logging
import bz2
import re
import requests
import hashlib
from pathlib import Path
from datetime import datetime, timedelta
from urllib.parse import quote
from typing import Optional, Dict, Any
from lxml import etree
import xml.etree.ElementTree as ET
from pymongo import MongoClient, ASCENDING
from pymongo.errors import DuplicateKeyError


class Crawler:
    """Робот для обкачки документов из дампов и обновления по URL."""
    
    def __init__(self, config_path: str):
        self.config_path = Path(config_path)
        self.load_config()
        self.setup_logging()
        self.setup_database()
        self.load_checkpoint()
        
        self.logger.info(f"Инициализация: {self.config_path}")
    
    def load_config(self) -> None:
        with open(self.config_path, 'r', encoding='utf-8') as f:
            self.config = yaml.safe_load(f)
        
        self.db_config = self.config['db']
        self.logic = self.config['logic']
        self.sources = self.config['sources']
        self.checkpoint_config = self.config['checkpoints']
    
    def setup_logging(self) -> None:
        log_config = self.config['logging']
        log_file = Path(log_config['file'])
        log_file.parent.mkdir(parents=True, exist_ok=True)
        
        formatter = logging.Formatter(
            '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
        )
        
        self.logger = logging.getLogger('Crawler')
        self.logger.setLevel(log_config['level'])
        
        file_handler = logging.FileHandler(log_file, encoding='utf-8')
        file_handler.setFormatter(formatter)
        self.logger.addHandler(file_handler)
        
        if log_config.get('console', True):
            console_handler = logging.StreamHandler()
            console_handler.setFormatter(formatter)
            self.logger.addHandler(console_handler)
    
    def setup_database(self) -> None:
        self.logger.info("Подключение к MongoDB")
        
        self.client = MongoClient(self.db_config['uri'])
        self.db = self.client[self.db_config['database']]
        self.collection = self.db[self.db_config['collection']]
        
        for idx_config in self.db_config.get('indexes', []):
            fields = [(f, ASCENDING) for f in idx_config['fields']]
            unique = idx_config.get('unique', False)
            self.collection.create_index(fields, unique=unique)
        
        doc_count = self.collection.count_documents({})
        self.logger.info(f"Документов в базе: {doc_count}")
    
    def load_checkpoint(self) -> None:
        if not self.checkpoint_config['enabled']:
            self.checkpoint = {}
            return
        
        checkpoint_file = Path(self.checkpoint_config['state_file'])
        
        if checkpoint_file.exists():
            with open(checkpoint_file, 'r', encoding='utf-8') as f:
                self.checkpoint = json.load(f)
            self.logger.info(f"Загружен чекпоинт: {self.checkpoint}")
        else:
            self.checkpoint = {}
    
    def save_checkpoint(self) -> None:
        if not self.checkpoint_config['enabled']:
            return
        
        checkpoint_file = Path(self.checkpoint_config['state_file'])
        checkpoint_file.parent.mkdir(parents=True, exist_ok=True)
        
        with open(checkpoint_file, 'w', encoding='utf-8') as f:
            json.dump(self.checkpoint, f, ensure_ascii=False, indent=2)
    
    def run(self, mode: str = 'auto') -> None:
        self.logger.info(f"Запуск в режиме: {mode}")
        
        if mode == 'auto':
            mode = self.determine_mode()
        
        if mode == 'seed':
            self.run_seed()
        elif mode == 'recrawl':
            self.run_recrawl()
    
    def determine_mode(self) -> Optional[str]:
        doc_count = self.collection.count_documents({})
        
        if doc_count == 0:
            self.logger.info("База пустая, режим SEED")
            return 'seed'
        
        threshold = datetime.now() - timedelta(days=self.logic['recrawl_after_days'])
        old_docs = self.collection.count_documents({
            'last_crawled_at': {'$lt': threshold.timestamp()}
        })
        
        if old_docs > 0:
            self.logger.info(f"Найдено {old_docs} старых документов, режим RECRAWL")
            return 'recrawl'
        
        self.logger.info("Все документы актуальны")
        return None
    
    def run_seed(self) -> None:
        self.logger.info("РЕЖИМ: SEED")
        
        total_inserted = 0
        
        if self.sources['wikipedia']['enabled']:
            total_inserted += self.seed_wikipedia()
        
        if self.sources['stackoverflow']['enabled']:
            total_inserted += self.seed_stackoverflow()
        
        self.logger.info(f"SEED завершен. Документов: {total_inserted}")
    
    def seed_wikipedia(self) -> int:
        self.logger.info("Обработка Wikipedia")
        
        source_config = self.sources['wikipedia']
        seed_config = source_config['seed']
        
        dump_path = Path(seed_config['path'])
        if not dump_path.exists():
            self.logger.warning(f"Дамп не найден: {dump_path}")
            return 0
        
        max_docs = seed_config.get('max_docs')
        checkpoint_key = 'wiki_processed'
        start_from = self.checkpoint.get(checkpoint_key, 0)
        
        processed = 0
        inserted = 0
        
        try:
            bz2_file = bz2.open(dump_path, 'rb')
            context = ET.iterparse(bz2_file, events=('start', 'end'))
            
            event, root = next(context)
            match = re.match(r'\{.*\}', root.tag)
            ns = match.group(0) if match else ''
            
            for event, elem in context:
                if event == 'end' and elem.tag == f'{ns}page':
                    processed += 1
                    
                    if processed <= start_from:
                        elem.clear()
                        root.clear()
                        continue
                    
                    doc = self.process_wiki_page(elem, ns)
                    
                    if doc and self.insert_document(doc):
                        inserted += 1
                        
                        if inserted % self.checkpoint_config['save_every'] == 0:
                            self.checkpoint[checkpoint_key] = processed
                            self.save_checkpoint()
                            self.logger.info(f"Чекпоинт: обработано {processed}, вставлено {inserted}")
                    
                    elem.clear()
                    root.clear()
                    
                    if max_docs and inserted >= max_docs:
                        break
            
            bz2_file.close()
            
        except KeyboardInterrupt:
            self.checkpoint[checkpoint_key] = processed
            self.save_checkpoint()
        
        self.logger.info(f"Wikipedia: обработано {processed}, вставлено {inserted}")
        return inserted
    
    def process_wiki_page(self, page_elem, ns: str) -> Optional[Dict[str, Any]]:
        try:
            title_elem = page_elem.find(f'{ns}title')
            if title_elem is None or not title_elem.text:
                return None
            
            title = title_elem.text
            
            if ':' in title:
                prefix = title.split(':', 1)[0]
                if prefix in ['Википедия', 'Wikipedia', 'Обсуждение', 'Участник',
                              'Шаблон', 'Template', 'Файл', 'File', 'Категория', 'Category',
                              'Портал', 'Проект', 'Справка', 'MediaWiki', 'Модуль']:
                    return None
            
            if page_elem.find(f'{ns}redirect') is not None:
                return None
            
            page_id_elem = page_elem.find(f'{ns}id')
            page_id = page_id_elem.text if page_id_elem is not None else None
            
            revision = page_elem.find(f'{ns}revision')
            if revision is None:
                return None
            
            text_elem = revision.find(f'{ns}text')
            if text_elem is None or not text_elem.text or len(text_elem.text) < 100:
                return None
            
            timestamp_elem = revision.find(f'{ns}timestamp')
            timestamp_str = timestamp_elem.text if timestamp_elem is not None else None
            
            url = f"https://ru.wikipedia.org/wiki/{quote(title.replace(' ', '_'))}"
            
            return {
                'url': url,
                'raw_html': text_elem.text,
                'source': 'wiki',
                'last_crawled_at': datetime.now().timestamp(),
                'external_id': page_id,
                'title': title,
                'timestamp': timestamp_str
            }
        except:
            return None
    
    def seed_stackoverflow(self) -> int:
        self.logger.info("Обработка Stack Overflow")
        
        source_config = self.sources['stackoverflow']
        seed_config = source_config['seed']
        
        posts_xml = Path(seed_config['path'])
        if not posts_xml.exists():
            self.logger.warning(f"Файл не найден: {posts_xml}")
            return 0
        
        max_docs = seed_config.get('max_docs')
        checkpoint_key = 'so_processed'
        start_from = self.checkpoint.get(checkpoint_key, 0)
        
        processed = 0
        inserted = 0
        
        try:
            context = etree.iterparse(str(posts_xml), events=('end',), tag='row')
            
            for event, elem in context:
                processed += 1
                
                if processed <= start_from:
                    elem.clear()
                    continue
                
                doc = self.process_so_post(elem)
                
                if doc and self.insert_document(doc):
                    inserted += 1
                    
                    if inserted % self.checkpoint_config['save_every'] == 0:
                        self.checkpoint[checkpoint_key] = processed
                        self.save_checkpoint()
                        self.logger.info(f"Чекпоинт: обработано {processed}, вставлено {inserted}")
                
                elem.clear()
                while elem.getprevious() is not None:
                    del elem.getparent()[0]
                
                if max_docs and inserted >= max_docs:
                    break
            
        except KeyboardInterrupt:
            self.checkpoint[checkpoint_key] = processed
            self.save_checkpoint()
        
        self.logger.info(f"Stack Overflow: обработано {processed}, вставлено {inserted}")
        return inserted
    
    def process_so_post(self, row_elem) -> Optional[Dict[str, Any]]:
        try:
            if row_elem.get('PostTypeId') != '1':
                return None
            
            post_id = row_elem.get('Id')
            body = row_elem.get('Body')
            
            if not body or len(body) < 50:
                return None
            
            return {
                'url': f"https://ru.stackoverflow.com/questions/{post_id}",
                'raw_html': body,
                'source': 'stackoverflow',
                'last_crawled_at': datetime.now().timestamp(),
                'external_id': post_id,
                'title': row_elem.get('Title') or f"Вопрос {post_id}",
                'creation_date': row_elem.get('CreationDate'),
                'tags': row_elem.get('Tags', ''),
                'score': int(row_elem.get('Score', '0'))
            }
        except:
            return None
    
    def insert_document(self, doc: Dict[str, Any]) -> bool:
        try:
            self.collection.insert_one(doc)
            return True
        except DuplicateKeyError:
            return False
    
    def run_recrawl(self) -> None:
        self.logger.info("РЕЖИМ: RECRAWL")
        
        threshold = datetime.now() - timedelta(days=self.logic['recrawl_after_days'])
        
        query = {
            '$or': [
                {'last_crawled_at': {'$exists': False}},
                {'last_crawled_at': {'$lt': threshold.timestamp()}}
            ]
        }
        
        total = self.collection.count_documents(query)
        self.logger.info(f"Документов для обновления: {total}")
        
        if total == 0:
            return
        
        max_docs = self.logic.get('max_docs_per_run')
        cursor = self.collection.find(query).limit(max_docs or total)
        
        updated = 0
        
        for doc in cursor:
            new_html = self.fetch_url(doc['url'])
            
            if new_html:
                old_hash = hashlib.md5(doc['raw_html'].encode()).hexdigest()
                new_hash = hashlib.md5(new_html.encode()).hexdigest()
                
                if old_hash != new_hash:
                    self.collection.update_one(
                        {'_id': doc['_id']},
                        {'$set': {'raw_html': new_html, 'last_crawled_at': datetime.now().timestamp()}}
                    )
                    updated += 1
            
            time.sleep(self.logic['delay_between_requests'])
        
        self.logger.info(f"RECRAWL завершен. Обновлено: {updated}")
    
    def fetch_url(self, url: str) -> Optional[str]:
        headers = {'User-Agent': self.logic['user_agent']}
        
        for attempt in range(self.logic['max_retries']):
            try:
                response = requests.get(url, headers=headers, timeout=self.logic['request_timeout'])
                if response.status_code == 200:
                    return response.text
            except:
                time.sleep(1)
        
        return None


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='Поисковый робот')
    parser.add_argument('config', help='Путь к YAML конфигу')
    parser.add_argument('--mode', choices=['auto', 'seed', 'recrawl'], default='auto')
    
    args = parser.parse_args()
    
    crawler = Crawler(args.config)
    crawler.run(mode=args.mode)


if __name__ == '__main__':
    main()
