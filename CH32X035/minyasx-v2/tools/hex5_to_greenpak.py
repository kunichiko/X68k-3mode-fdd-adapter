# tools/hex5_to_greenpak.py
Import("env")
from pathlib import Path
import os

proj = Path(env["PROJECT_DIR"])
outdir = proj / "src" / "greenpak"
outdir.mkdir(parents=True, exist_ok=True)

def pick_input(idx):
    # 1) ini の custom_gp_hexN 2) 環境変数 GP_HEXN 3) 既定パス firmware/greenpakN.hex
    key = f"custom_gp_hex{idx}"
    try:
        p = env.GetProjectOption(key)
    except Exception:
        p = None
    if not p:
        p = os.environ.get(f"GP_HEX{idx}", f"firmware/greenpak{idx}.hex")
    p = Path(p)
    return p if p.is_absolute() else (proj / p)

def parse_ihex(lines):
    mem = {}
    upper = 0
    for line in lines:
        line=line.strip()
        if not line or line[0] != ':': continue
        raw = bytes.fromhex(line[1:])
        cnt = raw[0]
        addr = (raw[1]<<8)|raw[2]
        rtyp = raw[3]
        data = raw[4:4+cnt]
        csum = raw[4+cnt]
        if ((sum(raw[:-1])+csum)&0xFF)!=0:
            raise ValueError("Intel HEX checksum error")
        if rtyp==0x00:  # data
            base=(upper<<16)|addr
            for i,b in enumerate(data): mem[base+i]=b
        elif rtyp==0x01: break
        elif rtyp==0x04: upper=(data[0]<<8)|data[1]
        else: pass
    if not mem: return 0, bytearray()
    mn, mx = min(mem), max(mem)
    buf = bytearray([0x00]*(mx-mn+1))   # 未記載は0x00（必要なら0xFFに）
    for a,v in mem.items(): buf[a-mn]=v
    return mn, buf

def emit(idx, base, image):
    h = outdir / f"greenpak{idx}.h"
    c = outdir / f"greenpak{idx}.c"
    with h.open("w") as fh:
        fh.write(f"""#pragma once
#include <stdint.h>
#define GREENPAK{idx}_BASE 0x{base:04X}
#define GREENPAK{idx}_SIZE {len(image)}
extern const uint8_t GREENPAK{idx}_IMAGE[GREENPAK{idx}_SIZE];
""")
    def rows(b):
        out=[]
        for i in range(0,len(b),16):
            out.append("  "+", ".join(f"0x{v:02X}" for v in b[i:i+16]))
        return ",\n".join(out)
    with c.open("w") as fc:
        fc.write(f"""#include "greenpak/greenpak{idx}.h"
const uint8_t GREENPAK{idx}_IMAGE[GREENPAK{idx}_SIZE] = {{
{rows(image)}
}};
""")
    print(f"[hex5] greenpak{idx}: base=0x{base:04X} size={len(image)} -> {h.name}, {c.name}")

for i in range(1,6):
    ip = pick_input(i)
    if ip.exists():
        base, img = parse_ihex(ip.read_text().splitlines())
    else:
        print(f"[hex5] WARN: not found: {ip} -> emit empty")
        base, img = 0, bytearray()
    emit(i, base, img)
