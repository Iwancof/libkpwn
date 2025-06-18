#!/usr/bin/env python3
import sys
import os
from elftools.elf.elffile import ELFFile

def main():
    if len(sys.argv) != 2:
        print("Error: Invalid number of arguments", file=sys.stderr)
        print("Usage: gen-offsets-helper.py <elf_path>", file=sys.stderr)
        sys.exit(1)
    
    elf_path = sys.argv[1]
    
    if not os.path.exists(elf_path):
        print(f"Error: ELF file not found: {elf_path}", file=sys.stderr)
        sys.exit(1)
    
    try:
        with open(elf_path, 'rb') as f:
            elf = ELFFile(f)
            text = elf.get_section_by_name('.text')
            
            if not text:
                print("Error: .text section not found in ELF file", file=sys.stderr)
                sys.exit(1)
            
            kbase = text['sh_addr']
            symtab = elf.get_section_by_name('.symtab') or elf.get_section_by_name('.dynsym')
            
            if not symtab:
                print("Error: No symbol table found in ELF file", file=sys.stderr)
                sys.exit(1)
            
            # Track seen symbols to prevent duplicates
            seen_symbols = set()
            symbol_count = 0
            
            # Print header information
            print("/* Auto-generated kernel symbol offsets */")
            print(f"/* Generated from: {os.path.basename(elf_path)} */")
            print(f"/* Kernel base (.text): 0x{kbase:x} */")
            print()
            
            for sym in symtab.iter_symbols():
                addr = sym['st_value']
                if addr >= kbase and sym.name:
                    offset = addr - kbase
                    
                    # Replace dots with underscores and convert to uppercase
                    clean_name = sym.name.replace('.', '_').upper()
                    symbol_define = f"KSYM_{clean_name}"
                    
                    # Check for duplicates
                    if symbol_define not in seen_symbols:
                        seen_symbols.add(symbol_define)
                        print(f"#define {symbol_define} 0x{offset:x}")
                        symbol_count += 1
            
            print(f"/* Generated {symbol_count} symbol definitions */", file=sys.stderr)
    
    except Exception as e:
        print(f"Error processing ELF file: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()