"""检查/修正源码文件编码：统一 CRLF 行尾、UTF-8 BOM（与 .editorconfig 保持一致）。

用法：
    encoding_tool.py report <file...>   # 只报告
    encoding_tool.py fix    <file...>   # 归一化为 CRLF + UTF-8 BOM
"""
import sys

BOM = b'\xef\xbb\xbf'


def info(b):
    try:
        b.decode('utf-8')
        utf8 = True
    except UnicodeDecodeError:
        utf8 = False
    crlf = b.count(b'\r\n')
    lf = b.count(b'\n') - crlf
    return utf8, crlf, lf


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    mode, files = sys.argv[1], sys.argv[2:]
    if mode not in ('report', 'fix'):
        print(__doc__)
        return 1
    for p in files:
        b = open(p, 'rb').read()
        utf8, crlf, lf = info(b)
        bom = b.startswith(BOM)
        if mode == 'report':
            print('%-40s bom=%-5s utf8=%-5s crlf=%-5d lf=%-4d %s' % (
                p, bom, utf8, crlf, lf, 'OK' if (bom and utf8 and lf == 0) else 'NEEDS-FIX'))
            continue
        body = b[len(BOM):] if bom else b
        body = body.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
        out = BOM + body
        open(p, 'wb').write(out)
        print('%-40s %s -> bom=True crlf=%d lf=0 (%d bytes)' % (
            p, 'fixed' if out != b else 'unchanged', out.count(b'\r\n'), len(out)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
