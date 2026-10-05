# -*- coding: utf-8 -*-
"""
重新生成《Hook 地址冲突清单》（KratosPP × Phobos）。

从两个仓库的源码树里提取「所有会改写 gamemd 内存的指令」，按字节区间比对：
  - DEFINE_HOOK / DEFINE_HOOK_AGAIN            (Syringe trampoline)
  - DEFINE_JUMP / DEFINE_FUNCTION_JUMP / DEFINE_DYNAMIC_JUMP  (静态/动态跳转改写)
  - DEFINE_PATCH / DEFINE_DYNAMIC_PATCH(_TYPED)               (静态/动态字节改写)
  - DEFINE_NAKED_HOOK                          (= DEFINE_FUNCTION_JUMP(LJMP,...))

不参与比对：DEFINE_REFERENCE / DEFINE_EXPORT（只做符号引用，不写内存）。

输出三张表：
  A. 同址：DEFINE_HOOK × DEFINE_HOOK（与旧版清单可比）
  B. 同址：其余内存改写指令（含 JUMP/PATCH）中，未被 A 覆盖的
  C. 区间重叠但起始不同（真正的「相邻打架」）

实现要点：
  * 先把全文的注释（// 与 /* */）按原长度替换成空格，再在「纯代码」上按平衡括号抽宏调用。
    这样既能正确剔除被注释掉的 hook，又能处理参数里夹注释的情况（如 `/* Offset */ 0x8259B`），
    且行号与偏移量保持不变。
  * 字符串/字符字面量内容保留不动，避免 DEFINE_PATCH 的字符串数据被误吞。

用法：
  python tools/gen_hook_conflict.py \
      --kratos-src <...>/KratosPP/src --phobos-src <...>/Phobos/src \
      --out-md <...>/hook_conflict_new.md --out-tsv <...>/hook_conflict_new.tsv \
      --old-md <...>/docs/Hook地址冲突清单.md
"""
import argparse
import bisect
import os
import re
from collections import defaultdict

MACROS = [
    "DEFINE_HOOK_AGAIN",
    "DEFINE_HOOK",
    "DEFINE_FUNCTION_JUMP",
    "DEFINE_DYNAMIC_JUMP",
    "DEFINE_NAKED_HOOK",
    "DEFINE_DYNAMIC_PATCH_TYPED",
    "DEFINE_DYNAMIC_PATCH",
    "DEFINE_JUMP",
    "DEFINE_PATCH",
]

# #pragma pack(1) 下 _<jumpType> 结构体的大小
JUMP_SIZE = {
    "LJMP": 5, "CALL": 5, "CALL6": 6, "VTABLE": 4,
    "SHORT": 2, "SHORTJMP": 2, "JMP": 5,
}
DEFAULT_JUMP_SIZE = 5


def blank_comments(text):
    """把所有注释内容替换成空格（保留换行，保证偏移/行号不变）。
    字符串/字符字面量内容保留。"""
    out = list(text)
    i, n = 0, len(text)
    state = None  # None | 'line' | 'block' | 'str' | 'chr'
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if state is None:
            if c == "/" and nxt == "/":
                out[i] = out[i + 1] = " "
                i += 2
                state = "line"
            elif c == "/" and nxt == "*":
                out[i] = out[i + 1] = " "
                i += 2
                state = "block"
            elif c == '"':
                state = "str"
                i += 1
            elif c == "'":
                state = "chr"
                i += 1
            else:
                i += 1
        elif state == "line":
            if c == "\n":
                state = None
            else:
                out[i] = " "
            i += 1
        elif state == "block":
            if c == "*" and nxt == "/":
                out[i] = out[i + 1] = " "
                i += 2
                state = None
            else:
                if c != "\n":
                    out[i] = " "
                i += 1
        elif state == "str":
            if c == "\\":
                i += 2
            elif c == '"':
                state = None
                i += 1
            else:
                i += 1
        else:  # 'chr'
            if c == "\\":
                i += 2
            elif c == "'":
                state = None
                i += 1
            else:
                i += 1
    return "".join(out)


def split_args(s):
    parts, depth, cur = [], 0, ""
    for c in s:
        if c in "([":
            depth += 1
            cur += c
        elif c in ")]":
            depth -= 1
            cur += c
        elif c == "," and depth == 0:
            parts.append(cur.strip())
            cur = ""
        else:
            cur += c
    if cur.strip():
        parts.append(cur.strip())
    return parts


def find_calls(code, names):
    """在「已置空注释」的代码上按平衡括号抽取宏调用。
    返回 [(name, argstr, offset)]。"""
    res = []
    for name in names:
        needle = name + "("
        start = 0
        while True:
            i = code.find(needle, start)
            if i < 0:
                break
            start = i + 1
            if i > 0 and (code[i - 1].isalnum() or code[i - 1] == "_"):
                continue  # 排除 FINE_HOOK 之类
            j = i + len(needle)
            depth = 1
            while j < len(code) and depth > 0:
                c = code[j]
                if c == "(":
                    depth += 1
                elif c == ")":
                    depth -= 1
                j += 1
            if depth != 0:
                continue
            res.append((name, code[i + len(needle): j - 1], i))
    return res


def literal_size(arg):
    """DEFINE_PATCH 单个参数占几个字节（字符串按转义后的长度估算）。"""
    a = arg.strip()
    if len(a) >= 2 and a[0] == '"' and a[-1] == '"':
        body = a[1:-1]
        cnt, i = 0, 0
        while i < len(body):
            if body[i] == "\\":
                # \xNN 占 1 字节；其余转义占 1 字节
                if i + 1 < len(body) and body[i + 1] == "x":
                    i += 4
                else:
                    i += 2
            else:
                i += 1
            cnt += 1
        return max(cnt, 1)
    if len(a) >= 2 and a[0] == "'" and a[-1] == "'":
        return 1
    return 1


def parse_repo(src_root):
    """返回 [(addr, size, name, loc, kind, macro)]，按 (addr, size) 排序。"""
    recs = []
    for dirpath, _dn, filenames in os.walk(src_root):
        for fn in filenames:
            if not fn.lower().endswith((".cpp", ".h", ".hpp")):
                continue
            full = os.path.join(dirpath, fn)
            try:
                with open(full, "r", encoding="utf-8-sig", errors="replace") as fp:
                    text = fp.read()
            except OSError:
                continue
            code = blank_comments(text)
            rel = os.path.relpath(full, src_root).replace("\\", "/")
            for macro, argstr, off in find_calls(code, MACROS):
                ln = text.count("\n", 0, off) + 1
                loc = "%s:%d" % (rel, ln)
                args = split_args(argstr)
                if not args:
                    continue
                try:
                    if macro in ("DEFINE_HOOK", "DEFINE_HOOK_AGAIN"):
                        if len(args) < 3:
                            continue
                        addr = int(args[0], 16)
                        name = args[1]
                        try:
                            size = int(args[2], 16)
                        except ValueError:
                            size = 5
                        kind = "HOOK"
                    elif macro in ("DEFINE_JUMP", "DEFINE_FUNCTION_JUMP"):
                        if len(args) < 3:
                            continue
                        jt = args[0].strip()
                        addr = int(args[1], 16)
                        name = args[2]
                        size = JUMP_SIZE.get(jt, DEFAULT_JUMP_SIZE)
                        kind = "JUMP"
                    elif macro == "DEFINE_DYNAMIC_JUMP":
                        if len(args) < 4:
                            continue
                        jt = args[0].strip()
                        name = args[1]
                        addr = int(args[2], 16)
                        size = JUMP_SIZE.get(jt, DEFAULT_JUMP_SIZE)
                        kind = "JUMP"
                    elif macro == "DEFINE_NAKED_HOOK":
                        if len(args) < 2:
                            continue
                        addr = int(args[0], 16)
                        name = args[1]
                        size = 5
                        kind = "JUMP"
                    elif macro == "DEFINE_PATCH":
                        if len(args) < 2:
                            continue
                        addr = int(args[0], 16)
                        name = "DEFINE_PATCH"
                        size = sum(literal_size(a) for a in args[1:])
                        kind = "PATCH"
                    elif macro in ("DEFINE_DYNAMIC_PATCH", "DEFINE_DYNAMIC_PATCH_TYPED"):
                        ai = 2 if macro == "DEFINE_DYNAMIC_PATCH" else 3
                        if len(args) < ai + 1:
                            continue
                        addr = int(args[ai - 1], 16)
                        name = args[0] if macro == "DEFINE_DYNAMIC_PATCH" else args[1]
                        size = sum(literal_size(a) for a in args[ai:])
                        kind = "PATCH"
                    else:
                        continue
                except ValueError:
                    continue
                if size <= 0:
                    size = 1
                recs.append((addr, size, name, loc, kind, macro))
    seen, uniq = set(), []
    for r in recs:
        key = (r[0], r[1], r[2], r[3], r[4], r[5])
        if key in seen:
            continue
        seen.add(key)
        uniq.append(r)
    return sorted(uniq, key=lambda r: (r[0], r[1], r[3]))


def load_old_list(md_path):
    """旧清单单表：| 地址 | Kratos hook | Kratos 位置 | Phobos hook | Phobos 位置 |
    只认「两边都是合法标识符」的行，避免把 B 表（跳转描述）与 C 表误算进来。"""
    rows = set()
    if not md_path or not os.path.exists(md_path):
        return rows
    addr_re = re.compile(r"^\|\s*(0x[0-9A-Fa-f]+)\s*\|([^|]*)\|([^|]*)\|([^|]*)\|")
    ident_re = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
    for line in open(md_path, encoding="utf-8-sig", errors="replace"):
        m = addr_re.match(line.strip())
        if not m:
            continue
        kn, pn = m.group(2).strip(), m.group(4).strip()
        if not ident_re.match(kn) or not ident_re.match(pn):
            continue
        rows.add((int(m.group(1), 16), kn, pn))
    return rows


def desc(r):
    return "%s -> %s" % (r[5], r[2])


def splice_doc(doc_path, sections):
    """把 sections 回填进 doc_path 里的 <!-- AUTO:key --> ... <!-- /AUTO:key --> 之间。
    同一 key 出现多次时全部替换。返回实际替换过的 key 列表。"""
    if not os.path.exists(doc_path):
        return []
    with open(doc_path, encoding="utf-8") as fp:
        txt = fp.read()
    done = []
    for key, body in sections.items():
        b = "<!-- AUTO:%s -->" % key
        e = "<!-- /AUTO:%s -->" % key
        pieces, pos, hit = [], 0, False
        while True:
            i = txt.find(b, pos)
            if i < 0:
                break
            j = txt.find(e, i + len(b))
            if j < 0:
                break
            pieces.append(txt[pos: i + len(b)])
            pieces.append("\n" + body.strip() + "\n")
            pieces.append(txt[j: j + len(e)])
            pos = j + len(e)
            hit = True
        if hit:
            pieces.append(txt[pos:])
            txt = "".join(pieces)
            done.append(key)
    if done:
        with open(doc_path, "w", encoding="utf-8") as fp:
            fp.write(txt)
    return done


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--kratos-src", required=True)
    ap.add_argument("--phobos-src", required=True)
    ap.add_argument("--out-md", required=True)
    ap.add_argument("--out-tsv", required=True)
    ap.add_argument("--old-md", default="")
    ap.add_argument("--splice-doc", default="",
                    help="把 A/B/C/STATS 四张表回填进该文档的 <!-- AUTO:key --> 标记里")
    args = ap.parse_args()

    K = parse_repo(args.kratos_src)
    P = parse_repo(args.phobos_src)

    def cnt(recs, kind):
        return sum(1 for r in recs if r[4] == kind)

    print("Kratos 内存改写指令: %d  (HOOK %d / JUMP %d / PATCH %d)" % (
        len(K), cnt(K, "HOOK"), cnt(K, "JUMP"), cnt(K, "PATCH")))
    print("Phobos 内存改写指令: %d  (HOOK %d / JUMP %d / PATCH %d)" % (
        len(P), cnt(P, "HOOK"), cnt(P, "JUMP"), cnt(P, "PATCH")))

    kh, ph = defaultdict(list), defaultdict(list)
    for r in K:
        if r[4] == "HOOK":
            kh[r[0]].append(r)
    for r in P:
        if r[4] == "HOOK":
            ph[r[0]].append(r)
    same_hook = sorted(set(kh) & set(ph))
    pairs_a = []
    for a in same_hook:
        for kr in sorted(kh[a]):
            for pr in sorted(ph[a]):
                pairs_a.append((a, kr, pr))

    kall, pall = defaultdict(list), defaultdict(list)
    for r in K:
        kall[r[0]].append(r)
    for r in P:
        pall[r[0]].append(r)
    extra_same_addr = []
    for a in sorted(set(kall) & set(pall)):
        if a in kh and a in ph:
            continue
        for kr in sorted(kall[a]):
            for pr in sorted(pall[a]):
                extra_same_addr.append((a, kr, pr))

    P_by_start = sorted(P, key=lambda r: r[0])
    p_starts = [r[0] for r in P_by_start]
    overlaps = []
    for kr in K:
        ka, ksz = kr[0], kr[1]
        kb = ka + ksz
        hi = bisect.bisect_left(p_starts, kb)
        lo = bisect.bisect_left(p_starts, ka - 0x100)
        for i in range(max(0, lo), hi):
            pr = P_by_start[i]
            pa, psz = pr[0], pr[1]
            if pa == ka:
                continue
            if pa < kb and ka < pa + psz:
                overlaps.append((kr, pr))

    print("同址 hook×hook: %d 地址 / %d 对" % (len(same_hook), len(pairs_a)))
    print("同址 其他指令: %d 处" % len(extra_same_addr))
    print("区间重叠(起始不同): %d 对" % len(overlaps))

    secA = ["| 地址 | Kratos hook | Kratos 位置 | Phobos hook | Phobos 位置 |",
            "| --- | --- | --- | --- | --- |"]
    for a, kr, pr in pairs_a:
        secA.append("| 0x%08X | %s | %s | %s | %s |" % (a, kr[2], kr[3], pr[2], pr[3]))

    secB = ["| 地址 | Kratos 指令 | Kratos 位置 | Phobos 指令 | Phobos 位置 |",
            "| --- | --- | --- | --- | --- |"]
    for a, kr, pr in extra_same_addr:
        secB.append("| 0x%08X | %s | %s | %s | %s |" % (a, desc(kr), kr[3], desc(pr), pr[3]))

    secC = ["| Kratos 地址 | Kratos 区间 | Kratos hook | Kratos 位置 | Phobos 地址 | Phobos 区间 | Phobos hook | Phobos 位置 |",
            "| --- | --- | --- | --- | --- | --- | --- | --- |"]
    for kr, pr in overlaps:
        secC.append("| 0x%08X | [%X,%X) | %s | %s | 0x%08X | [%X,%X) | %s | %s |" % (
            kr[0], kr[0], kr[0] + kr[1], kr[2], kr[3],
            pr[0], pr[0], pr[0] + pr[1], pr[2], pr[3]))

    stats = "\n".join([
        "| 指标 | KratosPP | Phobos |",
        "| --- | --- | --- |",
        "| 内存改写指令合计 | **%d** | **%d** |" % (len(K), len(P)),
        "| `DEFINE_HOOK` + `DEFINE_HOOK_AGAIN` | %d | %d |" % (cnt(K, "HOOK"), cnt(P, "HOOK")),
        "| `DEFINE_JUMP` / `_FUNCTION_JUMP` / `_DYNAMIC_JUMP` / `_NAKED_HOOK` | %d | %d |" % (cnt(K, "JUMP"), cnt(P, "JUMP")),
        "| `DEFINE_PATCH` / `_DYNAMIC_PATCH(_TYPED)` | %d | %d |" % (cnt(K, "PATCH"), cnt(P, "PATCH")),
        "",
        "- 同址 **hook × hook**：**%d** 个地址 / **%d** 对" % (len(same_hook), len(pairs_a)),
        "- 同址（含 JUMP / PATCH 等其他改写）：另有 **%d** 处" % len(extra_same_addr),
        "- 区间部分重叠（起始不同，真正的「相邻打架」）：**%d** 对" % len(overlaps),
    ])

    L = ["## A. 同址冲突（DEFINE_HOOK × DEFINE_HOOK）", ""]
    L += secA
    L += ["", "## B. 同址冲突（含 JUMP / PATCH 等其他内存改写指令）", ""]
    L += secB
    L += ["", "## C. 区间部分重叠（起始不同）", ""]
    L += secC
    L.append("")

    with open(args.out_md, "w", encoding="utf-8") as fp:
        fp.write("\n".join(L))
    with open(args.out_tsv, "w", encoding="utf-8") as fp:
        fp.write("ADDR\tKRATOS_KIND\tKRATOS_NAME\tKRATOS_LOC\tPHOBOS_KIND\tPHOBOS_NAME\tPHOBOS_LOC\n")
        for a, kr, pr in pairs_a:
            fp.write("%08X\tHOOK\t%s\t%s\tHOOK\t%s\t%s\n" % (a, kr[2], kr[3], pr[2], pr[3]))
        for a, kr, pr in extra_same_addr:
            fp.write("%08X\t%s\t%s\t%s\t%s\t%s\t%s\n" % (a, kr[4], kr[2], kr[3], pr[4], pr[2], pr[3]))

    if args.old_md:
        old = load_old_list(args.old_md)
        new = set((a, kr[2], pr[2]) for a, kr, pr in pairs_a)
        added, removed = sorted(new - old), sorted(old - new)
        print("\n=== 相对旧清单（2026-08-23）的差异 ===")
        print("新增 同址 hook×hook 对: %d" % len(added))
        for a, kn, pn in added:
            print("  + 0x%08X  %s  <->  %s" % (a, kn, pn))
        print("消失 同址 hook×hook 对: %d" % len(removed))
        for a, kn, pn in removed:
            print("  - 0x%08X  %s  <->  %s" % (a, kn, pn))

    print("\n写出: %s" % args.out_md)
    print("写出: %s" % args.out_tsv)

    if args.splice_doc:
        done = splice_doc(args.splice_doc, {
            "A": "\n".join(secA),
            "B": "\n".join(secB),
            "C": "\n".join(secC),
            "STATS": stats,
        })
        print("回填 %s: %s" % (args.splice_doc, ", ".join(done) or "(无可用标记)"))


if __name__ == "__main__":
    main()
