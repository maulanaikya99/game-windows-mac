import sys, struct, pefile
pe = pefile.PE(sys.argv[1], fast_load=True)
pe.parse_data_directories([pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_RESOURCE']])
icons, groups = {}, []
for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
    for e in t.directory.entries:
        for l in e.directory.entries:
            d = pe.get_data(l.data.struct.OffsetToData, l.data.struct.Size)
            if t.id == 3: icons[e.id] = d
            elif t.id == 14: groups.append(d)
g = groups[0]; n = struct.unpack('<HHH', g[:6])[2]
best = max((struct.unpack('<BBBBHHIH', g[6+i*14:20+i*14]) for i in range(n)), key=lambda x: (x[0] or 256, x[6]))
data = icons[best[7]]
open(sys.argv[2], 'wb').write(data if data[:8] == b'\x89PNG\r\n\x1a\n' else struct.pack('<HHH',0,1,1)+struct.pack('<BBBBHHII',*best[:7],22)+data)
print(best[0] or 256, 'png' if data[:4]==b'\x89PNG' else 'ico')
