#!/usr/bin/env python3
"""
Экономный парсер дампа Русской Википедии - минимальное потребление памяти
"""

import json
import bz2
import re
import gc
import xml.etree.ElementTree as ET
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).parent))
from utils.config import Config

class WikiParser:
    """Экономный парсер с минимальным использованием памяти"""
    
    def __init__(self, dump_path, output_path, max_docs=None):
        self.dump_path = Path(dump_path)
        self.output_path = Path(output_path)
        self.max_docs = max_docs
        self.doc_count = 0
        self.NS = None  # будет определён автоматически
        
    def clean_wikitext(self, text):
        """Очистка викитекста от разметки"""
        if not text:
            return ""
        
        text = re.sub(r'\{\{[^}]+\}\}', '', text)
        text = re.sub(r'\[\[(File|Файл|Image|Изображение):[^\]]+\]\]', '', text, flags=re.IGNORECASE)
        text = re.sub(r'\[\[(?:[^|\]]*\|)?([^\]]+)\]\]', r'\1', text)
        text = re.sub(r'\[https?://[^\]]+\]', '', text)
        text = re.sub(r'<[^>]+>', '', text)
        text = re.sub(r"'{2,}", '', text)
        text = re.sub(r'\s+', ' ', text).strip()
        
        return text
    
    def parse(self):
        """Парсинг с минимальным использованием памяти"""
        print(f"Начинаем парсинг: {self.dump_path}")
        print(f"Результат: {self.output_path}")
        print("⚠️  Это займёт 30-60 минут для 50k документов\n")
        
        bz2_file = bz2.open(self.dump_path, 'rb')
        output_file = open(self.output_path, 'w', encoding='utf-8', buffering=8192)
        
        try:
            # Используем iterparse для потоковой обработки
            context = ET.iterparse(bz2_file, events=('start', 'end'))
            
            # Получаем корневой элемент и извлекаем namespace
            event, root = next(context)
            
            # Автоматически определяем namespace из тега
            match = re.match(r'\{.*\}', root.tag)
            self.NS = match.group(0) if match else ''
            
            print(f"✓ Обнаружен namespace: {self.NS}")
            
            for event, elem in context:
                # Обрабатываем только закрывающие теги <page>
                if event == 'end' and elem.tag == f'{self.NS}page':
                    doc = self._process_page(elem)
                    
                    if doc:
                        output_file.write(json.dumps(doc, ensure_ascii=False) + '\n')
                        self.doc_count += 1
                        
                        if self.doc_count % 500 == 0:
                            print(f"✓ Обработано: {self.doc_count} документов", end='\r')
                            output_file.flush()
                        
                        if self.max_docs and self.doc_count >= self.max_docs:
                            print(f"\n✓ Достигнут лимит: {self.max_docs} документов")
                            break
                    
                    # КРИТИЧНО: очищаем корень, чтобы освободить память
                    elem.clear()
                    root.clear()
                    
        except KeyboardInterrupt:
            print(f"\n⚠️  Прервано пользователем. Сохранено {self.doc_count} документов")
        
        finally:
            bz2_file.close()
            output_file.close()
            print(f"\n✓ Готово! Обработано документов: {self.doc_count}")
            print(f"Файл: {self.output_path}")
    
    def _process_page(self, page_elem):
        """Извлечение данных из элемента page"""
        try:
            # Title
            title_elem = page_elem.find(f'{self.NS}title')
            if title_elem is None or not title_elem.text:
                return None
            
            title = title_elem.text
            
            # Пропускаем служебные страницы
            if ':' in title:
                prefix = title.split(':', 1)[0]
                if prefix in ['Википедия', 'Wikipedia', 'Обсуждение', 'Участник', 
                              'Шаблон', 'Template', 'Файл', 'File', 'Категория', 'Category',
                              'Портал', 'Проект', 'Справка', 'MediaWiki', 'Модуль']:
                    return None
            
            # Пропускаем редиректы
            redirect = page_elem.find(f'{self.NS}redirect')
            if redirect is not None:
                return None
            
            # Page ID
            page_id_elem = page_elem.find(f'{self.NS}id')
            page_id = page_id_elem.text if page_id_elem is not None else str(self.doc_count)
            
            # Revision (последняя версия)
            revision = page_elem.find(f'{self.NS}revision')
            if revision is None:
                return None
            
            # Text
            text_elem = revision.find(f'{self.NS}text')
            if text_elem is None or not text_elem.text:
                return None
            
            raw_text = text_elem.text
            clean_text = self.clean_wikitext(raw_text)
            
            # Фильтр коротких
            if len(clean_text) < 100:
                return None
            
            # Timestamp
            timestamp_elem = revision.find(f'{self.NS}timestamp')
            timestamp = timestamp_elem.text if timestamp_elem is not None else None
            
            return {
                'id': self.doc_count,
                'external_id': page_id,
                'source': 'wiki',
                'title': title,
                'text': clean_text,
                'meta': {
                    'url': f'https://ru.wikipedia.org/wiki/{title.replace(" ", "_")}',
                    'timestamp': timestamp,
                    'raw_length_bytes': len(raw_text.encode('utf-8')),
                    'language': 'ru'
                }
            }
        
        except Exception as e:
            return None


def main():
    """CLI интерфейс"""
    Config.create_dirs()
    
    dump_path = Config.CORPUS_RAW_DIR / 'wiki' / 'ruwiki-latest-pages-articles.xml.bz2'
    output_path = Config.CORPUS_EXTRACTED_DIR / 'wiki' / 'docs.jsonl'
    
    if not dump_path.exists():
        print(f"❌ Файл дампа не найден: {dump_path}")
        return
    
    parser = WikiParser(
        dump_path=dump_path,
        output_path=output_path,
        max_docs=Config.WIKI_MIN_DOCS
    )
    parser.parse()

if __name__ == '__main__':
    main()
