#!/usr/bin/env python3
"""
Скачивание дампа Русской Википедии
"""

import sys
import urllib.request
from pathlib import Path

# Добавляем scripts/ в путь
sys.path.insert(0, str(Path(__file__).parent))

from utils.config import Config

def download_file(url, output_path, chunk_size=8192):
    """Скачивает файл с прогресс-баром"""
    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    print(f"Скачиваю: {url}")
    print(f"Сохраняю в: {output_path}")
    
    try:
        # Открываем соединение
        with urllib.request.urlopen(url) as response:
            total_size = int(response.headers.get('Content-Length', 0))
            
            # Скачиваем частями
            downloaded = 0
            with open(output_path, 'wb') as f:
                while True:
                    chunk = response.read(chunk_size)
                    if not chunk:
                        break
                    
                    f.write(chunk)
                    downloaded += len(chunk)
                    
                    # Показываем прогресс
                    if total_size > 0:
                        percent = (downloaded / total_size) * 100
                        mb_downloaded = downloaded / (1024 * 1024)
                        mb_total = total_size / (1024 * 1024)
                        print(f"\rПрогресс: {percent:.1f}% ({mb_downloaded:.1f} MB / {mb_total:.1f} MB)", end='')
                    else:
                        mb_downloaded = downloaded / (1024 * 1024)
                        print(f"\rСкачано: {mb_downloaded:.1f} MB", end='')
        
        print("\n✓ Скачивание завершено!")
        return True
        
    except Exception as e:
        print(f"\n Ошибка при скачивании: {e}")
        if output_path.exists():
            output_path.unlink()  # Удаляем неполный файл
        return False

def main():
    """Скачивает дамп Wikipedia"""
    Config.create_dirs()
    
    # URL и путь
    url = Config.WIKI_DUMP_URL
    output_path = Config.CORPUS_RAW_DIR / 'wiki' / 'ruwiki-latest-pages-articles.xml.bz2'
    
    # Проверяем наличие файла
    if output_path.exists():
        size_mb = output_path.stat().st_size / (1024 * 1024)
        print(f"⚠ Файл уже существует: {output_path}")
        print(f"Размер: {size_mb:.1f} MB")
        
        answer = input("Скачать заново? (y/N): ").strip().lower()
        if answer != 'y':
            print("Отменено.")
            return
        
        output_path.unlink()
    
    # Скачиваем
    print(f"\n Начинаю скачивание дампа Русской Википедии...")
    print(f"⚠ Это большой файл (~3-4 GB), может занять 10-30 минут\n")
    
    success = download_file(url, output_path)
    
    if success:
        size_mb = output_path.stat().st_size / (1024 * 1024)
        print(f"\n✓ Дамп скачан: {output_path}")
        print(f"Размер: {size_mb:.1f} MB")
        print(f"\nТеперь запусти парсер:")
        print(f"  python scripts/extract_wiki.py")
    else:
        print("\n Не удалось скачать дамп")

if __name__ == '__main__':
    main()
