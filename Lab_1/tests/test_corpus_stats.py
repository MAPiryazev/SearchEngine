#!/usr/bin/env python3
"""
Тесты статистики корпуса документов
"""

import json
from pathlib import Path
import pytest


class TestCorpusStatistics:
    """Проверка статистики корпуса"""
    
    @pytest.fixture
    def corpus_paths(self):
        """Пути к файлам корпуса"""
        base_dir = Path(__file__).parent.parent
        return {
            'wiki': base_dir / 'corpus' / 'extracted' / 'wiki' / 'docs.jsonl',
            'stackoverflow': base_dir / 'corpus' / 'extracted' / 'stackexchange' / 'docs.jsonl'
        }
    
    def test_wiki_corpus_exists(self, corpus_paths):
        """Проверка существования корпуса Википедии"""
        assert corpus_paths['wiki'].exists(), "Файл корпуса Википедии не найден"
    
    def test_stackoverflow_corpus_exists(self, corpus_paths):
        """Проверка существования корпуса StackOverflow"""
        assert corpus_paths['stackoverflow'].exists(), "Файл корпуса StackOverflow не найден"
    
    def test_wiki_minimum_documents(self, corpus_paths):
        """Проверка минимального количества документов Wiki"""
        with open(corpus_paths['wiki'], 'r', encoding='utf-8') as f:
            count = sum(1 for _ in f)
        
        assert count >= 30000, f"Недостаточно документов Wiki: {count} < 30000"
    
    def test_stackoverflow_minimum_documents(self, corpus_paths):
        """Проверка минимального количества документов SO"""
        with open(corpus_paths['stackoverflow'], 'r', encoding='utf-8') as f:
            count = sum(1 for _ in f)
        
        assert count >= 30000, f"Недостаточно документов SO: {count} < 30000"
    
    def test_wiki_document_structure(self, corpus_paths):
        """Проверка структуры документов Wiki"""
        with open(corpus_paths['wiki'], 'r', encoding='utf-8') as f:
            first_line = f.readline()
            doc = json.loads(first_line)
        
        required_fields = ['id', 'external_id', 'source', 'title', 'text', 'meta']
        for field in required_fields:
            assert field in doc, f"Отсутствует поле: {field}"
        
        assert doc['source'] == 'wiki'
        assert len(doc['text']) > 100, "Текст слишком короткий"
    
    def test_stackoverflow_document_structure(self, corpus_paths):
        """Проверка структуры документов StackOverflow"""
        with open(corpus_paths['stackoverflow'], 'r', encoding='utf-8') as f:
            first_line = f.readline()
            doc = json.loads(first_line)
        
        required_fields = ['id', 'external_id', 'source', 'title', 'text', 'meta']
        for field in required_fields:
            assert field in doc, f"Отсутствует поле: {field}"
        
        assert doc['source'] == 'stackoverflow'
        assert len(doc['text']) > 50, "Текст слишком короткий"
    
    def test_wiki_average_document_size(self, corpus_paths):
        """Проверка среднего размера документа Wiki"""
        total_size = 0
        count = 0
        
        with open(corpus_paths['wiki'], 'r', encoding='utf-8') as f:
            for line in f:
                doc = json.loads(line)
                total_size += len(doc['text'])
                count += 1
                if count >= 1000:  # Проверяем первую 1000
                    break
        
        avg_size = total_size / count
        assert avg_size > 1000, f"Средний размер слишком мал: {avg_size} байт"
    
    def test_no_duplicate_ids(self, corpus_paths):
        """Проверка отсутствия дубликатов ID"""
        for name, path in corpus_paths.items():
            ids = set()
            duplicates = []
            
            with open(path, 'r', encoding='utf-8') as f:
                for line in f:
                    doc = json.loads(line)
                    doc_id = doc['id']
                    if doc_id in ids:
                        duplicates.append(doc_id)
                    ids.add(doc_id)
            
            assert len(duplicates) == 0, f"Найдены дубликаты ID в {name}: {duplicates[:10]}"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
