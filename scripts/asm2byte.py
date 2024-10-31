
import subprocess
import argparse
import re
import os

def run_cmd(cmd: str):
    p = subprocess.Popen(cmd.split(' '), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = p.communicate()
    return out.decode('utf-8')

def parse_objdump(output: str):
    code = []
    for line in output.strip().split('\n'):
        match = re.search(r'\w+:?\s*([0-9a-f]{2})\s*([0-9a-f]{2})', line)
        if match:
            # 将找到的字节转换为整数并添加到列表中
            code.append(int(match.group(1), 16))
            code.append(int(match.group(2), 16))
    return code

def main():
    
    parser = argparse.ArgumentParser()
    # must
    parser.add_argument('input', type=str, help='input file')
    args = parser.parse_args()
    
    if not os.path.exists(args.input):
        print(f"Error: {args.input} does not exist.")
        exit(1)
    
    run_cmd(f"gcc -c {args.input} -o tmp.o")
    asm_output = run_cmd(f"objdump -d tmp.o")
    
    # a.o:     file format elf64-x86-64
    # Disassembly of section .text:

    # 0000000000000000 <.text>:
    # 0:   b0 48                   mov    $0x48,%al
    # 2:   e6 f1                   out    %al,$0xf1
    # 4:   b0 65                   mov    $0x65,%al
    # 6:   e6 f1                   out    %al,$0xf1
    # 8:   b0 6c                   mov    $0x6c,%al
    # a:   e6 f1                   out    %al,$0xf1
    # c:   b0 6c                   mov    $0x6c,%al
    # e:   e6 f1                   out    %al,$0xf1
    # 10:   b0 6f                   mov    $0x6f,%al
    # 12:   e6 f1                   out    %al,$0xf1
    # 14:   b0 0a                   mov    $0xa,%al
    # 16:   e6 f1                   out    %al,$0xf1
    # 18:   f4                      hlt
    asm_code = parse_objdump(asm_output)
    print(asm_code)
    
if __name__ == '__main__':
    main()