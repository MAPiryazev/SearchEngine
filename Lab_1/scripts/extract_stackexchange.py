#!/usr/bin/env python3
"""
Быстрый потоковый парсер дампа Stack Exchange
Извлекает вопросы и ответы из Posts.xml
"""

import json
import re
from pathlib import Path
from lxml import etree
import sys

sys.path.insert(0, str(Path(__file__).parent))
from utils.config import Config

class StackExchangeParser:
    """Оптимизированный парсер дампов Stack Exchange"""
    
    def __init__(self, posts_xml_path, output_path, max_docs=None):
        self.posts_xml_path = Path(posts_xml_path)
        self.output_path = Path(output_path)
        self.max_docs = max_docs
        self.doc_count = 0
        
    def clean_html(self, text):
        """Очистка HTML от тегов"""
        if not text:
            return ""
        
        # Убираем HTML теги
        text = re.sub(r'<code>.*?</code>', ' [код] ', text, flags=re.DOTALL)
        text = re.sub(r'<pre>.*?</pre>', ' [блок кода] ', text, flags=re.DOTALL)
        text = re.sub(r'<[^>]+>', '', text)
        
        # Декодируем HTML entities
        text = text.replace('&lt;', '<')
        text = text.replace('&gt;', '>')
        text = text.replace('&amp;', '&')
        text = text.replace('&quot;', '"')
        text = text.replace('&#39;', "'")
        
        # Нормализуем пробелы
        text = re.sub(r'\s+', ' ', text).strip()
        
        return text
    
    def fast_iter(self, context):
        """Быстрая итеративная обработка XML"""
        with open(self.output_path, 'w', encoding='utf-8') as outfile:
            for event, elem in context:
                if elem.tag == 'row':
                    doc = self.process_row(elem)
                    if doc:
                        outfile.write(json.dumps(doc, ensure_ascii=False) + '\n')
                        self.doc_count += 1
                        
                        if self.doc_count % 1000 == 0:
                            print(f"Обработано документов: {self.doc_count}", end='\r')
                        
                        if self.max_docs and self.doc_count >= self.max_docs:
                            break
                    
                    # Освобождаем память
                    elem.clear()
                    while elem.getprevious() is not None:
                        del elem.getparent()[0]
            
            del context
    
    def process_row(self, row_elem):
        """Извлекает данные из элемента <row>"""
        # PostTypeId: 1 = Question, 2 = Answer
        post_type = row_elem.get('PostTypeId')
        
        # Берём только вопросы (они содержательнее)
        if post_type != '1':
            return None
        
        post_id = row_elem.get('Id')
        title = row_elem.get('Title')
        body = row_elem.get('Body')
        
        if not body:
            return None
        
        clean_text = self.clean_html(body)
        
        # Фильтруем короткие посты
        if len(clean_text) < 100:
            return None
        
        # Собираем полный текст: заголовок + тело
        if title:
            full_text = f"{title}. {clean_text}"
        else:
            full_text = clean_text
        
        # Мета-информация
        tags = row_elem.get('Tags', '')
        score = row_elem.get('Score', '0')
        creation_date = row_elem.get('CreationDate')
        view_count = row_elem.get('ViewCount', '0')
        
        return {
            'id': self.doc_count,
            'external_id': post_id,
            'source': 'stackexchange',
            'title': title or f"Вопрос {post_id}",
            'text': full_text,
            'meta': {
                'url': f'https://ru.stackoverflow.com/questions/{post_id}',
                'timestamp': creation_date,
                'tags': tags,
                'score': int(score),
                'views': int(view_count),
                'raw_length_bytes': len(body.encode('utf-8')) if body else 0,
                'language': 'ru'
            }
        }
    
    def parse(self):
        """Запускает парсинг дампа"""
        print(f"Начинаем парсинг: {self.posts_xml_path}")
        print(f"Результат будет записан в: {self.output_path}")
        
        if not self.posts_xml_path.exists():
            print(f" Файл не найден: {self.posts_xml_path}")
            return
        
        try:
            # Создаём потоковый парсер
            context = etree.iterparse(
                str(self.posts_xml_path), 
                events=('end',), 
                tag='row'
            )
            
            self.fast_iter(context)
            
            print(f"\n✓ Готово! Обработано документов: {self.doc_count}")
            
        except Exception as e:
            print(f"\n Ошибка парсинга: {e}")

def main():
    """CLI интерфейс"""
    Config.create_dirs()
    
    # Пути
    posts_xml = Config.CORPUS_RAW_DIR / 'stackexchange' / 'Posts.xml'
    output_path = Config.CORPUS_EXTRACTED_DIR / 'stackexchange' / 'docs.jsonl'
    
    if not posts_xml.exists():
        print(f" Файл Posts.xml не найден: {posts_xml}")
        print(f"\nСкачай и распакуй дамп:")
        print(f"  python scripts/download_stackexchange.py")
        print(f"  cd corpus/raw/stackexchange")
        print(f"  7z x ru-stackoverflow-com.7z")
        return
    
    # Запускаем парсер
    parser = StackExchangeParser(
        posts_xml_path=posts_xml,
        output_path=output_path,
        max_docs=Config.STACKEXCHANGE_MIN_DOCS
    )
    parser.parse()

if __name__ == '__main__':
    main()
