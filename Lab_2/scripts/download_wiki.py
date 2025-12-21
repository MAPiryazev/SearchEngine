
import urllib.request
from pathlib import Path


WIKI_DUMP_URL = "https://dumps.wikimedia.org/ruwiki/latest/ruwiki-latest-pages-articles.xml.bz2"
OUTPUT_DIR = Path("corpus/raw/wiki")


def download_file(url: str, output_path: Path, chunk_size: int = 8192) -> bool:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    print(f"Скачивание: {url}")
    print(f"Сохранение в: {output_path}")
    
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
                        print(f"\rПрогресс: {percent:.1f}% ({mb_downloaded:.1f} / {mb_total:.1f} MB)", end='')
                    else:
                        mb_downloaded = downloaded / (1024 * 1024)
                        print(f"\rСкачано: {mb_downloaded:.1f} MB", end='')
        
        print("\nЗавершено")
        return True
        
    except Exception as e:
        print(f"\nОшибка: {e}")
        if output_path.exists():
            output_path.unlink()
        return False


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output_file = OUTPUT_DIR / "ruwiki-latest-pages-articles.xml.bz2"
    
    if output_file.exists():
        size_mb = output_file.stat().st_size / (1024 * 1024)
        print(f"Файл существует: {output_file} ({size_mb:.1f} MB)")
        
        answer = input("Скачать заново? (y/N): ").strip().lower()
        if answer != 'y':
            return
        
        output_file.unlink()
    
    print(f"\nСкачивание дампа Википедии (~3-4 GB, 10-30 минут)\n")
    
    if download_file(WIKI_DUMP_URL, output_file):
        size_mb = output_file.stat().st_size / (1024 * 1024)
        print(f"\nДамп скачан: {output_file} ({size_mb:.1f} MB)")


if __name__ == '__main__':
    main()
