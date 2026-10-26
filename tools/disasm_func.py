import sys, pefile, capstone, struct
pe = pefile.PE(sys.argv[1], fast_load=True); base = pe.OPTIONAL_HEADER.ImageBase
pd = [s for s in pe.sections if s.Name.startswith(b'.pdata')][0].get_data()
funcs = [struct.unpack_from('<III', pd, i) for i in range(0, len(pd) - 11, 12)]
t = pe.sections[0]; data = t.get_data(); va0 = base + t.VirtualAddress
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
imp = {}
pe.parse_data_directories([pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_IMPORT']])
for e in pe.DIRECTORY_ENTRY_IMPORT:
    for i in e.imports: imp[i.address] = (e.dll + b'!' + (i.name or b'')).decode()
for a in sys.argv[2:]:
    a = int(a, 16) - base
    f = [x for x in funcs if x[0] <= a < x[1]]
    if not f: print('no func', hex(a)); continue
    s, e, _ = f[0]
    print('==== func', hex(base + s), hex(base + e))
    for i in md.disasm(data[s - t.VirtualAddress:e - t.VirtualAddress], base + s):
        extra = ''
        if i.mnemonic in ('call', 'jmp') and i.op_str.startswith('0x'):
            tgt = int(i.op_str, 16)
            # thunk?
            off = tgt - va0
            if 0 <= off < len(data) - 6 and data[off:off+2] == b'\xff\x25':
                disp = struct.unpack_from('<i', data, off + 2)[0]
                extra = '  ; ' + imp.get(tgt + 6 + disp, '?')
        if 'rip' in i.op_str:
            pass
        print(hex(i.address), i.mnemonic, i.op_str, extra)
