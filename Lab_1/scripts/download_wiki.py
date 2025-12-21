
import sys
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from utils.config import Config


def download_file(url, output_path, chunk_size=8192):
ъ    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    print(f"Скачивается: {url}")
    print(f"Сохраняется: {output_path}")
    
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
        
        print("\nСкачивание завершено")
        return True
        
    except Exception as e:
        print(f"\nОшибка при скачивании: {e}")
        if output_path.exists():
            output_path.unlink()
        return False


def main():
    Config.create_dirs()
    
    url = Config.WIKI_DUMP_URL
    output_path = Config.CORPUS_RAW_DIR / 'wiki' / 'ruwiki-latest-pages-articles.xml.bz2'
    
    if output_path.exists():
        size_mb = output_path.stat().st_size / (1024 * 1024)
        print(f"Файл уже существует: {output_path}")
        print(f"Размер: {size_mb:.1f} MB")
        
        answer = input("Скачать заново? (y/N): ").strip().lower()
        if answer != 'y':
            print("Отменено.")
            return
        
        output_path.unlink()
    
    print(f"\nскачивание дампа Русской Википедии")
    
    success = download_file(url, output_path)
    
    if success:
        size_mb = output_path.stat().st_size / (1024 * 1024)
        print(f"\nДамп скачан: {output_path}")
    else:
        print("\nНе удалось скачать дамп")


if __name__ == '__main__':
    main()
