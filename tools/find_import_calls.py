"""Cari pemanggil fungsi impor di exe PE x64 (lewat thunk jmp [rip] atau call [rip]).
Pakai: uv run -q --with pefile --with numpy python -I find_import_calls.py <exe> <substring nama impor>...
Contoh: ... NSUNSC.exe XInputGetState DirectInput8Create"""
import sys, struct, pefile
import numpy as np
pe = pefile.PE(sys.argv[1], fast_load=True)
pe.parse_data_directories([pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_IMPORT']])
base = pe.OPTIONAL_HEADER.ImageBase
want = [w.encode() for w in sys.argv[2:]]
targets = {}
for e in pe.DIRECTORY_ENTRY_IMPORT:
    for i in e.imports:
        if i.name and any(w in i.name for w in want):
            targets[i.address - base] = (e.dll + b'!' + i.name).decode()
t = [s for s in pe.sections if s.Characteristics & 0x20000000][0]
data = np.frombuffer(t.get_data(), dtype=np.uint8); rva0 = t.VirtualAddress
n = len(data) - 4
d = (data[:n].astype(np.int64) | data[1:n+1].astype(np.int64) << 8 | data[2:n+2].astype(np.int64) << 16 | data[3:n+3].astype(np.int64) << 24)
d = np.where(d >= 2**31, d - 2**32, d)
pos = np.arange(n)
dst = rva0 + pos + 4 + d
for tgt, name in targets.items():
    for h in np.nonzero(dst == tgt)[0]:
        op = bytes(data[h-2:h])
        print(name, 'ref di', hex(base + rva0 + h - 2), op.hex())
        if op == b'\xff\x25':  # thunk: cari call rel32 ke thunk
            thunk = rva0 + h - 2
            for c in np.nonzero((dst == thunk))[0]:
                if data[c-1] == 0xE8: print('   dipanggil dari', hex(base + rva0 + c - 1))
