# -*- coding: utf-8 -*-
"""
分析「同一地址多重 Hook」在链式执行下的相互影响（Kratos 是否掐断 Phobos）。

前提（Syringe 语义 + 实测加载顺序）：
  * 加载/执行顺序：Ares  ->  Kratos  ->  Phobos
  * 同一地址上，各 hook 依注册顺序串成一条链依次执行
  * Hook 体 `return 0`          => 继续跑链上的下一个 hook（跑完全部后重放被替换的原始指令）
  * Hook 体 `return <非 0 地址>`  => 立即跳转，**链上后面的 hook 与原始指令重放全部被跳过**

判定规则（三档）：
  * ALLPATHS —— Kratos 的 hook 体内**没有任何 `return 0`**，所有路径都跳走
                 => Phobos 在该地址的 hook **必定**永不执行
  * BRANCH   —— 体内既有 `return <非0>` 也有 `return 0`
                 => **条件性**掐断，取决于运行时分支 / 门控开关；默认门控为 true 的要特别标出
  * ZERO     —— 全部 `return 0`（或无显式 return）=> 链正常，Phobos 可执行

另外：
  * 名字或函数体注释里出现 "Phobos" 的 => 标注为 **故意接管**（Kratos 作者明知并主动复刻/接管）
  * 静态改写（DEFINE_JUMP / DEFINE_PATCH）不走 Syringe 链，是裸字节写入：
    后加载者覆盖前者 => **Phobos（最后加载）覆盖 Kratos 的写入**

输出：ida_work/hook_chain.tsv + 一段 markdown（供 --splice-doc 的 AUTO:D 标记回填）。
"""
import argparse
import importlib.util
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
GEN_PY = os.path.join(HERE, "gen_hook_conflict.py")


def load_gen():
    spec = importlib.util.spec_from_file_location("gen_hook_conflict", GEN_PY)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def find_hooks(code, text):
    """在已置空注释的 code 上抽出 DEFINE_HOOK / DEFINE_HOOK_AGAIN 及其函数体。
    返回 [(macro, addr, name, size, loc, body_code, body_orig)]，body 为 None 表示只有声明。"""
    out = []
    for macro in ("DEFINE_HOOK_AGAIN", "DEFINE_HOOK"):
        needle = macro + "("
        start = 0
        while True:
            i = code.find(needle, start)
            if i < 0:
                break
            start = i + 1
            if i > 0 and (code[i - 1].isalnum() or code[i - 1] == "_"):
                continue
            j = i + len(needle)
            depth = 1
            while j < len(code) and depth > 0:
                if code[j] == "(":
                    depth += 1
                elif code[j] == ")":
                    depth -= 1
                j += 1
            if depth != 0:
                continue
            args = code[i + len(needle): j - 1]
            parts = [p.strip() for p in args.split(",")]
            if len(parts) < 3:
                continue
            try:
                addr = int(parts[0], 16)
            except ValueError:
                continue
            name = parts[1]
            try:
                size = int(parts[2], 16)
            except ValueError:
                size = 5
            k = j
            while k < len(code) and code[k] in " \t\r\n":
                k += 1
            body_code = body_orig = None
            if k < len(code) and code[k] == "{":
                d = 1
                m = k + 1
                while m < len(code) and d > 0:
                    if code[m] == "{":
                        d += 1
                    elif code[m] == "}":
                        d -= 1
                    m += 1
                body_code = code[k + 1: m - 1]
                body_orig = text[k + 1: m - 1]  # blank_comments 保长，坐标一致
            loc = text.count("\n", 0, i) + 1
            out.append((macro, addr, name, size, loc, body_code, body_orig))
    return out


ZERO_TOKENS = ("0", "0x0", "0X0", "NULL", "nullptr", "false", "FALSE")


def top_level_returns(body):
    """只取 hook 函数体「顶层」的 return，排除 lambda 内部的 return
    （lambda 内的 return 是给 lambda 自己用的，不是给 Syringe 的跳转目标）。"""
    res = []
    i, n = 0, len(body)
    stack = []  # True = 该花括号是 lambda 体
    while i < n:
        c = body[i]
        if c == "{":
            is_lambda = False
            j = i - 1
            while j >= 0 and body[j] in " \t\r\n":
                j -= 1
            if j >= 0 and body[j] == "]":
                is_lambda = True
            elif j >= 0 and body[j] == ")":
                d, k = 1, j - 1
                while k >= 0 and d > 0:
                    if body[k] == ")":
                        d += 1
                    elif body[k] == "(":
                        d -= 1
                    k -= 1
                m = k
                while m >= 0 and body[m] in " \t\r\n":
                    m -= 1
                if m >= 0 and body[m] == "]":
                    is_lambda = True
            stack.append(is_lambda)
            i += 1
            continue
        if c == "}":
            if stack:
                stack.pop()
            i += 1
            continue
        if (c == "r" and body.startswith("return", i)
                and (i == 0 or not (body[i - 1].isalnum() or body[i - 1] == "_"))
                and not any(stack)):
            j, d = i + 6, 0
            while j < n:
                ch = body[j]
                if ch in "([{":
                    d += 1
                elif ch in ")]}":
                    if d > 0:
                        d -= 1
                    else:
                        break  # 不配对的收括号 => 边界
                elif ch == ";" and d == 0:
                    break
                j += 1
            res.append(body[i + 6:j].strip())
            i = j
            continue
        i += 1
    return res


def classify_returns(body):
    """返回 (档位, 非0返回值列表)。
    档位: ALLPATHS(全路径跳转) / BRANCH(分支跳转) / ZERO(全 0) / NONE(无 return)"""
    if not body:
        return "NONE", []
    raw = top_level_returns(body)
    if not raw:
        return "NONE", []
    nonzero = [r for r in raw if r not in ZERO_TOKENS]
    if not nonzero:
        return "ZERO", []
    has_zero = len(nonzero) != len(raw)
    return ("BRANCH" if has_zero else "ALLPATHS"), nonzero


def collect(src_root):
    """返回 (by_name, recs)；by_name: name -> (cls, nonzero, deliberate)"""
    g = load_gen()
    by_name = {}
    recs = []
    for dirpath, _dn, filenames in os.walk(src_root):
        for fn in filenames:
            if not fn.lower().endswith((".cpp", ".h", ".hpp")):
                continue
            full = os.path.join(dirpath, fn)
            try:
                with open(full, encoding="utf-8-sig", errors="replace") as fp:
                    text = fp.read()
            except OSError:
                continue
            code = g.blank_comments(text)
            rel = os.path.relpath(full, src_root).replace("\\", "/")
            for macro, addr, name, size, ln, body_code, body_orig in find_hooks(code, text):
                loc = "%s:%d" % (rel, ln)
                cls, nonzero = classify_returns(body_code)
                blob = (name + "\n" + (body_orig or "")).lower()
                deliberate = "phobos" in blob
                if body_code is not None:
                    by_name[name] = (cls, nonzero, deliberate)
                recs.append((addr, name, loc, macro, size, cls, nonzero, deliberate))
    filled = []
    for addr, name, loc, macro, size, cls, nonzero, deliberate in recs:
        if cls == "NONE" and name in by_name:
            cls, nonzero, deliberate = by_name[name]
        filled.append((addr, name, loc, macro, size, cls, nonzero, deliberate))
    return by_name, filled


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--kratos-src", required=True)
    ap.add_argument("--phobos-src", required=True)
    ap.add_argument("--out-tsv", required=True)
    ap.add_argument("--splice-doc", default="")
    args = ap.parse_args()

    g = load_gen()
    K = g.parse_repo(args.kratos_src)
    P = g.parse_repo(args.phobos_src)

    _by_name, krecs = collect(args.kratos_src)
    kret = {}
    for addr, name, loc, macro, size, cls, nonzero, deliberate in krecs:
        kret[(addr, name)] = (cls, nonzero, deliberate, loc)

    kh, ph = set(), set()
    for r in K:
        if r[4] == "HOOK":
            kh.add(r[0])
    for r in P:
        if r[4] == "HOOK":
            ph.add(r[0])
    same = sorted(kh & ph)

    rows = []
    for r in K:
        addr, size, name, loc, kind, macro = r
        if addr not in same:
            continue
        cls, nonzero, deliberate, _l = kret.get((addr, name), ("NONE", [], False, loc))
        nzs = ",".join(re.sub(r"\s+", " ", r) for r in sorted(set(nonzero))[:3])
        nzs = nzs.replace("|", "\\|")
        rows.append(("HOOK", addr, name, loc, cls, nzs, deliberate))

    kall, pall = {}, {}
    for r in K:
        kall.setdefault(r[0], []).append(r)
    for r in P:
        pall.setdefault(r[0], []).append(r)
    for addr in sorted(set(kall) & set(pall)):
        if addr in same:
            continue
        for kr in kall[addr]:
            rows.append(("STATIC", addr, kr[2], kr[3], kr[4] + ":" + kr[5], "", False))

    rows.sort(key=lambda x: (x[1], x[0] != "HOOK", x[2]))

    always = [x for x in rows if x[0] == "HOOK" and x[4] == "ALLPATHS"]
    branch = [x for x in rows if x[0] == "HOOK" and x[4] == "BRANCH"]
    zero = [x for x in rows if x[0] == "HOOK" and x[4] == "ZERO"]
    none = [x for x in rows if x[0] == "HOOK" and x[4] == "NONE"]
    static = [x for x in rows if x[0] == "STATIC"]
    deliberate_all = [x for x in (always + branch) if x[6]]

    print("=== 链式影响分析（序：Ares -> Kratos -> Phobos）===")
    print("  必定掐断（全路径跳转，Phobos 永不执行）: %d" % len(always))
    print("  条件掐断（分支跳转，取决于门控/运行时）: %d" % len(branch))
    print("  其中「故意接管」（名字/注释提到 Phobos）: %d" % len(deliberate_all))
    print("  安全（全 return 0，Phobos 正常执行）: %d" % len(zero))
    print("  无显式 return（等价续链）: %d" % len(none))
    print("  静态改写同址（Phobos 后写 => 覆盖 Kratos）: %d" % len(static))

    with open(args.out_tsv, "w", encoding="utf-8") as fp:
        fp.write("KIND\tADDR\tKRATOS_NAME\tKRATOS_LOC\tKRATOS_RET\tNONZERO\tDELIBERATE\n")
        for kind, addr, name, loc, cls, nzs, delib in rows:
            fp.write("%s\t%08X\t%s\t%s\t%s\t%s\t%s\n" % (kind, addr, name, loc, cls, nzs, "Y" if delib else ""))

    if args.splice_doc:
        def tbl(items, with_ret=True):
            if with_ret:
                out = ["| 地址 | Kratos hook | Kratos 返回值 | 位置 |", "| --- | --- | --- | --- |"]
                for _k, addr, name, loc, _c, nzs, _d in items:
                    out.append("| 0x%08X | `%s` | `%s` | %s |" % (addr, name, nzs or "(分支)", loc))
            else:
                out = ["| 地址 | Kratos hook | 位置 |", "| --- | --- | --- |"]
                for _k, addr, name, loc, _c, _n, _d in items:
                    out.append("| 0x%08X | `%s` | %s |" % (addr, name, loc))
            return out

        L = []
        L.append("> **判定前提**：链序 **Ares → Kratos → Phobos**；同一地址上各 hook 串成一条链依次执行，")
        L.append("> `return 0` 续链，`return <非 0>` 立即跳转并**掐断整条链**（后面的 hook 与原始指令重放全部被跳过）。")
        L.append("> 因此凡是「Kratos 会返回非 0」的地址，**Phobos 的 hook 就会被跳过**。")
        L.append("")
        L.append("### D.1 必定掐断（%d 处）：Kratos 的 hook 体内**没有** `return 0`，所有路径都跳走 ⇒ Phobos 永不执行" % len(always))
        L.append("")
        L += tbl(always)
        L.append("")
        L.append("### D.2 条件掐断（%d 处）：体内同时存在 `return <非0>` 与 `return 0`，是否掐断取决于运行分支 / 门控开关" % len(branch))
        L.append("")
        L += tbl(branch)
        L.append("")
        L.append("### D.3 安全（%d 处）：Kratos 全部 `return 0`，Phobos 可正常执行" % len(zero))
        L.append("")
        L += tbl(zero, False)
        L.append("")
        if none:
            L.append("### D.4 无显式 `return`（走函数末尾，等价于返回 0 续链）（%d 处）" % len(none))
            L.append("")
            L += tbl(none, False)
            L.append("")
        L.append("### D.5 静态改写同址（%d 处）：**后加载的 Phobos 覆盖 Kratos 的写入**（方向与 hook 相反）" % len(static))
        L.append("")
        L.append("| 地址 | Kratos 指令 | 位置 |")
        L.append("| --- | --- | --- |")
        for _k, addr, name, loc, cls, _n, _d in static:
            L.append("| 0x%08X | `%s` (%s) | %s |" % (addr, name, cls, loc))
        L.append("")
        done = g.splice_doc(args.splice_doc, {"D": "\n".join(L)})
        print("回填 %s: %s" % (args.splice_doc, ", ".join(done) or "(无可用标记)"))
    print("写出: %s" % args.out_tsv)


if __name__ == "__main__":
    main()
