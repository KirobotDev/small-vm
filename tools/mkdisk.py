import struct
import sys
import os


MAGIC = b'\xde\xad\xbe\xef'
MAX_FILES = 64 
ENTRY_SIZE = 24 

def create_disk(output_file, input_files):
    entries = []
    current_offset = 6 + (MAX_FILES * ENTRY_SIZE) 
    data_blob = bytearray()
    
    for fname in input_files:
        if not os.path.exists(fname):
            print(f"File not found: {fname}")
            continue
            
        with open(fname, 'rb') as f:
            content = f.read()
            
        name = os.path.basename(fname).encode('ascii')[:15] 
        size = len(content)
        
        entries.append({
            'name': name,
            'offset': current_offset,
            'size': size
        })
        
        data_blob.extend(content)
        current_offset += size
        
    with open(output_file, 'wb') as f:
        f.write(MAGIC)
        f.write(struct.pack('<H', len(entries)))
        
        for e in entries:
            f.write(e['name'].ljust(16, b'\0'))
            f.write(struct.pack('<II', e['offset'], e['size']))
            
        for _ in range(MAX_FILES - len(entries)):
             f.write(b'\0' * ENTRY_SIZE)
             
        f.write(data_blob)
        
    print(f"Created {output_file} with {len(entries)} files.")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python3 mkdisk.py disk.img [files...]")
        sys.exit(1)
        
    create_disk(sys.argv[1], sys.argv[2:])
