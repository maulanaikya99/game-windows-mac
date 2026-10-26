import sys, pefile, capstone
pe = pefile.PE(sys.argv[1], fast_load=True); base = pe.OPTIONAL_HEADER.ImageBase
t = pe.sections[0]; data = t.get_data(); va0 = base + t.VirtualAddress
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
for a in sys.argv[2:]:
    s, e = [int(x, 16) for x in a.split('-')]
    print('====', a)
    for i in md.disasm(data[s - va0:e - va0], s):
        print(hex(i.address), i.mnemonic, i.op_str)
