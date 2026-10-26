import sys, pefile
pe = pefile.PE(sys.argv[1], fast_load=True); base = pe.OPTIONAL_HEADER.ImageBase
for a in sys.argv[2:]:
    b = pe.get_data(int(a, 16) - base, 64)
    print(a, b)
