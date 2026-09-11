import struct, sys, collections

path = sys.argv[1]
data = open(path, 'rb').read()
print('filesize', len(data))


def u32(off):
    return struct.unpack_from('<I', data, off)[0]


def u64(off):
    return struct.unpack_from('<Q', data, off)[0]


sig, ver, nstreams, dirrva = struct.unpack_from('<4sIII', data, 0)
print('sig', sig, 'ver', ver, 'streams', nstreams, 'dirrva', hex(dirrva))

streams = collections.OrderedDict()
for i in range(nstreams):
    t, size, rva = struct.unpack_from('<III', data, dirrva + i * 12)
    streams.setdefault(t, []).append((size, rva))
    print('stream type %d size %d rva 0x%x' % (t, size, rva))

STREAM_NAMES = {
    3: 'ThreadList', 4: 'ModuleList', 5: 'MemoryList', 6: 'Exception',
    7: 'SystemInfo', 8: 'ThreadExList', 9: 'Memory64List', 10: 'CommentA',
    11: 'CommentW', 12: 'HandleData', 13: 'FunctionTable', 14: 'UnloadedModuleList',
    15: 'MiscInfo', 16: 'MemoryInfoList', 17: 'ThreadInfoList', 18: 'HandleOperationList',
    19: 'Token', 20: 'JavaScriptData', 21: 'SystemMemoryInfo', 22: 'ProcessVmCounters',
}
for t in streams:
    print('  stream', t, STREAM_NAMES.get(t, '?'))


def read_mdstring(rva):
    n = u32(rva)
    return data[rva + 4:rva + 4 + n].decode('utf-16-le', 'replace')


modules = []
if 4 in streams:
    size, rva = streams[4][0]
    n = u32(rva)
    off = rva + 4
    print('\nmodules:', n)
    for i in range(n):
        base = u64(off)
        imgsize = u32(off + 8)
        tds = u32(off + 16)
        name_rva = u32(off + 20)
        name = read_mdstring(name_rva)
        modules.append((base, imgsize, name))
        print('  %-46s base=0x%08X size=0x%08X ts=0x%08X' % (name, base, imgsize, tds))
        off += 108

if 7 in streams:
    size, rva = streams[7][0]
    arch, level, prod, nproc, ptype, ver_major, ver_minor, build, plat = struct.unpack_from('<HHHBBIIII', data, rva)
    csd_rva = u32(rva + 24)
    csd = read_mdstring(csd_rva) if csd_rva else ''
    print('\nsysteminfo arch=%d level=%d prod=%d ver=%d.%d.%d plat=%d csd=%s' % (
        arch, level, prod, ver_major, ver_minor, build, plat, csd))


def find_module(addr):
    for base, size, name in modules:
        if base <= addr < base + size:
            return name, base, addr - base
    return None, None, None


mems = {}
if 9 in streams:
    size, rva = streams[9][0]
    count = u64(rva)
    base_rva = u64(rva + 8)
    off = rva + 16
    print('\nmemory64 ranges:', count)
    pos = base_rva
    for i in range(count):
        start = u64(off)
        sz = u64(off + 8)
        mems[start] = (pos, sz)
        pos += sz
        off += 16
elif 5 in streams:
    size, rva = streams[5][0]
    count = u32(rva)
    off = rva + 4
    print('\nmemory ranges:', count)
    for i in range(count):
        start = u64(off)
        dsize = u32(off + 8)
        drva = u32(off + 12)
        mems[start] = (drva, dsize)
        off += 16


def read_mem(addr, length):
    for start, (pos, sz) in mems.items():
        if start <= addr and addr + length <= start + sz:
            o = pos + (addr - start)
            return data[o:o + length]
    return None


def fmt_word(addr, v):
    mod, mbase, moff = find_module(v)
    if mod:
        return '  <%s+0x%X>' % (mod.split('\\')[-1], moff)
    return ''


if 6 in streams:
    size, rva = streams[6][0]
    tid = u32(rva)
    code = u32(rva + 8)
    flags = u32(rva + 12)
    addr = u64(rva + 24)
    nparam = u32(rva + 32)
    params = [u64(rva + 40 + 8 * i) for i in range(min(nparam, 15))]
    print('\nexception tid=0x%X code=0x%08X flags=0x%X addr=0x%08X nparam=%d' % (tid, code, flags, addr, nparam))
    print('  params', [hex(p) for p in params])
    name, base, off = find_module(addr)
    print('  fault module: %s base=0x%s offset=0x%s' % (name, hex(base) if base else '?', hex(off) if off is not None else '?'))
    csize, crva = struct.unpack_from('<II', data, rva + 160)
    print('  context size=%d rva=0x%x' % (csize, crva))
    m = read_mem(addr - 48, 96)
    if m:
        print('  bytes at EIP (dump memory):', m.hex(' '))
    if csize and crva:
        eip = u32(crva + 0xB8)
        esp = u32(crva + 0xC4)
        ebp = u32(crva + 0xB4)
        print('  ctx EIP=0x%08X ESP=0x%08X EBP=0x%08X' % (eip, esp, ebp))
        for r, o in [('EAX', 0xB0), ('EBX', 0xA4), ('ECX', 0xAC), ('EDX', 0xA8),
                     ('ESI', 0xA0), ('EDI', 0x9C)]:
            v = u32(crva + o)
            print('   %s=0x%08X%s' % (r, v, fmt_word(0, v)))
        segs = struct.unpack_from('<IIIIII', data, crva + 0x8C)
        print('   Gs=0x%X Fs=0x%X Es=0x%X Ds=0x%X CS=0x%X SS=0x%X EFla=0x%08X' % (
            segs[0], segs[1], segs[2], segs[3], segs[4], segs[5], u32(crva + 0xC0)))
        mod, mbase, moff = find_module(eip)
        print('  EIP module: %s offset 0x%X' % (mod, moff or 0))
        for regname, val in [('ESI', u32(crva + 0xA0)), ('EDI', u32(crva + 0x9C)),
                             ('EBX', u32(crva + 0xA4)), ('EDX', u32(crva + 0xA8)),
                             ('ECX', u32(crva + 0xAC)), ('EAX', u32(crva + 0xB0))]:
            m = read_mem(val, 32)
            print('  *%s=0x%08X -> %s' % (regname, val, m.hex(' ') if m else '<unmapped>'))
        for label, sp in [('ESP', esp), ('EBP', ebp)]:
            m = read_mem(sp, 160)
            if not m:
                print('  %s=0x%08X <unmapped>' % (label, sp))
                continue
            print('  %s=0x%08X:' % (label, sp))
            for i in range(0, len(m), 4):
                v = struct.unpack_from('<I', m, i)[0]
                print('    %08X: %08X%s' % (sp + i, v, fmt_word(sp + i, v)))
