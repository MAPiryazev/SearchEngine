#!/usr/bin/env python3
"""
Скачивание дампа Stack Exchange (ru.stackoverflow.com)
"""

import sys
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from utils.config import Config

def download_file(url, output_path, chunk_size=8192):
    """Скачивает файл с прогресс-баром"""
    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    print(f"Скачиваю: {url}")
    print(f"Сохраняю в: {output_path}")
    
    try:
        with urllib.request.urlopen(url) as response:
            total_size = int(response.headers.get('Content-Length', 0))
            
            downloaded = 0
            with open(output_path, 'wb') as f:
                while True:
                    chunk = response.read(chunk_size)
                    if not chunk:
                        break
                    
                    f.write(chunk)
                    downloaded += len(chunk)
                    
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
        print(f"\n❌ Ошибка при скачивании: {e}")
        if output_path.exists():
            output_path.unlink()
        return False

def main():
    """Скачивает дамп Stack Exchange"""
    Config.create_dirs()
    
    # Русский Stack Overflow
    site = "ru.stackoverflow.com"
    base_url = "https://archive.org/download/stackexchange"
    
    output_dir = Config.CORPUS_RAW_DIR / 'stackexchange'
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Скачиваем Posts.xml (основной файл с вопросами и ответами)
    posts_url = "https://archive.org/download/stackexchange/ru.stackoverflow.com.7z"
    posts_output = output_dir / f"{site}.7z"
    
    if posts_output.exists():
        size_mb = posts_output.stat().st_size / (1024 * 1024)
        print(f"⚠ Файл уже существует: {posts_output}")
        print(f"Размер: {size_mb:.1f} MB")
        
        answer = input("Скачать заново? (y/N): ").strip().lower()
        if answer != 'y':
            print("Отменено.")
            return
        
        posts_output.unlink()
    
    print(f"\n📥 Начинаю скачивание дампа {site}...")
    print(f"⚠ Размер файла: ~400-600 MB\n")
    
    success = download_file(posts_url, posts_output)
    
    if success:
        size_mb = posts_output.stat().st_size / (1024 * 1024)
        print(f"\n✓ Дамп скачан: {posts_output}")
        print(f"Размер: {size_mb:.1f} MB")
        print(f"\n⚠ Нужно распаковать .7z архив:")
        print(f"  sudo apt install p7zip-full")
        print(f"  7z x {posts_output} -o{output_dir}")
        print(f"\nПотом запусти парсер:")
        print(f"  python scripts/extract_stackexchange.py")

if __name__ == '__main__':
    main()
