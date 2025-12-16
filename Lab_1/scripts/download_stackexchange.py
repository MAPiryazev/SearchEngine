import sys
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from utils.config import Config


def download_file(url, output_path, chunk_size=8192):
    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    print(f"Downloading: {url}")
    print(f"Destination: {output_path}")
    
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
                        print(f"\rProgress: {percent:.1f}% ({mb_downloaded:.1f} MB / {mb_total:.1f} MB)", end='')
                    else:
                        mb_downloaded = downloaded / (1024 * 1024)
                        print(f"\rDownloaded: {mb_downloaded:.1f} MB", end='')
        
        print("\nDownload completed successfully.")
        return True
        
    except Exception as e:
        print(f"\nDownload failed: {e}")
        if output_path.exists():
            output_path.unlink()
        return False


def main():
    Config.create_dirs()
    
    site = "ru.stackoverflow.com"
    base_url = "https://archive.org/download/stackexchange"
    
    output_dir = Config.CORPUS_RAW_DIR / 'stackexchange'
    output_dir.mkdir(parents=True, exist_ok=True)
    
    posts_url = "https://archive.org/download/stackexchange/ru.stackoverflow.com.7z"
    posts_output = output_dir / f"{site}.7z"
    
    if posts_output.exists():
        size_mb = posts_output.stat().st_size / (1024 * 1024)
        print(f"File already exists: {posts_output}")
        print(f"Size: {size_mb:.1f} MB")
        
        answer = input("Download again? (y/N): ").strip().lower()
        if answer != 'y':
            print("Cancelled.")
            return
        
        posts_output.unlink()
    
    print(f"\nDownloading {site} dump...")
    print(f"Expected size: ~400-600 MB\n")
    
    success = download_file(posts_url, posts_output)



if __name__ == '__main__':
    main()
