import sys
import struct

OPCODES = {
    'LOAD': 0x01,
    'ADD': 0x02,
    'SUB': 0x03,
    'LOADM': 0x04,
    'STORE': 0x05,
    'JMP': 0x06,
    'JZ': 0x07,
    'PRINT': 0x08,
    'PUSH': 0x09,
    'POP': 0x0A,
    'CALL': 0x0B,
    'RET': 0x0C,
    'IN': 0x0D,
    'OUT': 0x0E,
    'CMP': 0x0F,
    'LOADR': 0x10,
    'STORER': 0x11,
    'HALT': 0xFF
}

REGISTERS = {f'R{i}': i for i in range(8)}

def parse_line(line, labels=None, current_addr=0):
    parts = line.strip().split()
    if not parts or parts[0].startswith(';'):
        return None, 0
    
    if parts[0].endswith(':'):
        if len(parts) > 1:
            parts = parts[1:] 
        else:
            return None, 0 
            
    if not parts: return None, 0

    mnemonic = parts[0].upper()
    if mnemonic not in OPCODES:
        raise ValueError(f"Unknown mnemonic: {mnemonic}")
    
    op = OPCODES[mnemonic]
    args = parts[1:]
    args = [a.replace(',', '') for a in args]
    
    bytes_out = bytearray([op])
    size = 1 
    
    if mnemonic == 'LOAD': 
        size += 3 
        if labels is not None:
            reg = REGISTERS[args[0]]
            val = int(args[1])
            bytes_out.append(reg)
            bytes_out.extend(struct.pack('<H', val)) 
            
    elif mnemonic in ['ADD', 'SUB', 'CMP', 'LOADR', 'STORER']:
        size += 2 
        if labels is not None:
            r1 = REGISTERS[args[0]]
            r2 = REGISTERS[args[1]]
            bytes_out.append(r1)
            bytes_out.append(r2)
        
    elif mnemonic == 'LOADM': 
        size += 3
        if labels is not None:
            reg = REGISTERS[args[0]]
            val = labels.get(args[1], int(args[1])) if args[1] in labels else int(args[1])
            bytes_out.append(reg)
            bytes_out.extend(struct.pack('<H', val))

    elif mnemonic == 'STORE': 
        size += 3
        if labels is not None:
            reg = REGISTERS[args[0]]
            val = labels.get(args[1], int(args[1])) if args[1] in labels else int(args[1])
            bytes_out.append(reg)
            bytes_out.extend(struct.pack('<H', val))
        
    elif mnemonic in ['JMP', 'JZ', 'CALL']: 
        size += 2
        if labels is not None:
            target = args[0]
            if target in labels:
                addr = labels[target]
            else:
                addr = int(target)
            bytes_out.extend(struct.pack('<H', addr))
        
    elif mnemonic in ['PRINT', 'PUSH', 'POP', 'IN', 'OUT']: 
        size += 1
        if labels is not None:
            reg = REGISTERS[args[0]]
            bytes_out.append(reg)
        
    elif mnemonic in ['HALT', 'RET']:
        pass
        
    return bytes_out, size

def assemble(src_file, dst_file):
    with open(src_file, 'r') as f:
        lines = f.readlines()
        
    labels = {}
    addr = 0
    
    for line in lines:
        parts = line.strip().split()
        if not parts or parts[0].startswith(';'): continue
        
        if parts[0].endswith(':'):
            label_name = parts[0][:-1]
            labels[label_name] = addr
            if len(parts) > 1:
                rest_of_line = " ".join(parts[1:])
                _, size = parse_line(rest_of_line)
                addr += size
        else:
             _, size = parse_line(line)
             addr += size
             
    print(f"Labels found: {labels}")

    program = bytearray()
    for i, line in enumerate(lines):
        try:
            clean_line = line.strip()
            parts = clean_line.split()
            if parts and parts[0].endswith(':'):
                if len(parts) > 1:
                    clean_line = " ".join(parts[1:])
                else:
                    continue 
            
            b, _ = parse_line(clean_line, labels=labels)
            if b:
                program.extend(b)
        except Exception as e:
            print(f"Error on line {i+1}: {line.strip()} -> {e}")
            sys.exit(1)
            
    with open(dst_file, 'wb') as f:
        f.write(program)
    print(f"Assembled {src_file} to {dst_file} ({len(program)} bytes)")

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python3 asm.py source.asm output.bin")
        sys.exit(1)
    assemble(sys.argv[1], sys.argv[2])
