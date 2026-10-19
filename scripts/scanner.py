import os

# Caminho onde a ISO foi extraída pelo wit
extracted_dir = os.path.expanduser("~/strikers-ai-core/extracted/files")

def scan_game_files():
    print("Varredura de arquivos do Strikers 2013 iniciada...")
    file_count = 0
    extensions = {}

    for root, dirs, files in os.walk(extracted_dir):
        for file in files:
            file_count += 1
            ext = os.path.splitext(file)[1].lower()
            extensions[ext] = extensions.get(ext, 0) + 1

    print(f"\nTotal de arquivos encontrados: {file_count}")
    print("Distribuição por extensão:")
    for ext, count in sorted(extensions.items(), key=lambda x: x[1], reverse=True):
        print(f"  {ext if ext else '[sem extensão]'}: {count} arquivos")

if __name__ == "__main__":
    scan_game_files()
