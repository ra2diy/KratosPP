import struct, sys

path = sys.argv[1]
data = open(path, 'rb').read()


def u32(off):
    return struct.unpack_from('<I', data, off)[0]


def u64(off):
    return struct.unpack_from('<Q', data, off)[0]


sig, ver, nstreams, dirrva = struct.unpack_from('<4sIII', data, 0)
streams = {}
for i in range(nstreams):
    t, size, rva = struct.unpack_from('<III', data, dirrva + i * 12)
    streams.setdefault(t, (size, rva))

mems = []
size, rva = streams[9]
count = u64(rva)
pos = u64(rva + 8)
off = rva + 16
for i in range(count):
    start = u64(off)
    sz = u64(off + 8)
    mems.append((start, sz, pos))
    pos += sz
    off += 16


def read_mem(addr, length):
    for start, sz, p in mems:
        if start <= addr and addr + length <= start + sz:
            o = p + (addr - start)
            return data[o:o + length]
    return None


def dump(addr, length, indent='  '):
    b = read_mem(addr, length)
    if b is None:
        print('%s%08X <unmapped>' % (indent, addr))
        return None
    for i in range(0, length, 16):
        words = ' '.join('%08X' % struct.unpack_from('<I', b, i + j)[0]
                         for j in range(0, min(16, length - i), 4))
        asc = ''.join(chr(c) if 32 <= c < 127 else '.' for c in b[i:i + 16])
        print('%s%08X: %-52s %s' % (indent, addr + i, words, asc))
    return b


def find_value(addr, length, value, maxscan):
    b = read_mem(addr, length)
    if b is None:
        return []
    return [j for j in range(0, length - 4, 4) if struct.unpack_from('<I', b, j)[0] == value]


mode = sys.argv[2]
if mode == 'dump':
    dump(int(sys.argv[3], 0), int(sys.argv[4], 0))
elif mode == 'chain':
    # print pointer targets and search for value
    base = int(sys.argv[3], 0)
    target = int(sys.argv[4], 0)
    scan = int(sys.argv[5], 0)
    blob = read_mem(base, scan)
    print('searching %d bytes at %08X for pointer targets containing %08X' % (scan, base, target))
    for i in range(0, scan, 4):
        v = struct.unpack_from('<I', blob, i)[0]
        if 0x00010000 <= v < 0x80000000 and read_mem(v, 4):
            hits = find_value(v, min(0x8000, 0x8000), target, 0x8000)
            if hits:
                print('  %08X+0x%X -> %08X contains target at %s' % (
                    base, i, v, ','.join('+0x%X' % h for h in hits)))
elif mode == 'scan':
    base = int(sys.argv[3], 0)
    size = int(sys.argv[4], 0)
    target = int(sys.argv[5], 0)
    hits = find_value(base, size, target, size)
    print('hits at ' + ','.join('0x%X' % h for h in hits) if hits else 'no hits')
