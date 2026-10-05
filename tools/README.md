# 崩溃 / 反汇编分析工具

配合 `debug\snapshot-*\extcrashdump.dmp`（Ares 生成的 minidump）与 `output\Debug\Kratos.pdb` 使用，
用于把「游戏里的一次崩溃」还原到「Kratos 的源码行」。

脚本本身不依赖构建环境，只需要一个 Python 3 解释器；仓库里可直接借用 IDA 自带的解释器：

```
set PY=D:\Workspace\ra2mod\platform\IDA_Pro_v8.3_Portable\python311\python.exe
```

## 1. `dump_scan.py` — 解析 minidump

```
%PY% tools\dump_scan.py "<游戏目录>\debug\snapshot-YYYYMMDD-HHMMSS\extcrashdump.dmp"
```

输出：流目录、**模块基址表**（用来把 EIP 换算成模块内偏移）、异常记录、崩溃线程寄存器 / 上下文、
以 EIP 为中心的代码字节、以及 ESP/EBP 处的栈内容（栈上的地址会标注落在哪个模块 + 偏移）。

## 2. `sym_lookup.ps1` — 把运行地址换算成函数与源码行

用 dbghelp 加载 `Kratos.dll` + `Kratos.pdb`，按 dump 里的**运行时基址**查符号（无需 VS/调试器）：

```
powershell -NoProfile -ExecutionPolicy Bypass -File tools\sym_lookup.ps1 `
  -ModulePath "D:\Games\Yuri's Revenge\Kratos.dll" `
  -SearchPath "D:\Games\Yuri's Revenge" `
  -Base 0x50400000 -Size 0x3F5000 0x506B1F6B 0x506884F0
```

- `-Base` 用 `dump_scan.py` 打出的模块基址（Kratos.dll 通常 0x50400000，以 dump 为准）。
- `-SearchPath` 指向 `.pdb` 所在目录；也可以直接指向 `output\Debug`。
- 可一次传多个地址（崩溃 EIP + 栈上的返回地址）。

## 3. `mem_tool.py` — 读 dump 里的内存

```
%PY% tools\mem_tool.py <dump> dump 0x20FB0E60 0x100      # 看某个对象的内存
%PY% tools\mem_tool.py <dump> scan 0x20FB0E60 0x400 0x1CBA55F8   # 在对象里找某个指针
%PY% tools\mem_tool.py <dump> chain 0x20FB0E60 0x1CBA55F8 0x400  # 指针链：对象 -> ExtData -> OwnerObject
```

适合用来确认「崩溃时这个对象到底挂在谁身上」「缓存的字段是什么值」这类问题。

## 4. `encoding_tool.py` — 源码编码/行尾检查

`.editorconfig` 要求 `utf-8-bom` + `crlf`，但仓库里存在无 BOM / 混用行尾的文件。
编辑过源文件后可用它检查或归一化（注意：`fix` 会重写文件，先确认 git 工作区干净）：

```
%PY% tools\encoding_tool.py report src\**\*.h
%PY% tools\encoding_tool.py fix    src\Ext\ObjectType\AttachEffect.cpp
```

## 典型案例：2026-09-11 奴隶矿场收起成载具崩溃

1. `dump_scan.py` → 崩溃在 `Kratos.dll`，基址 `0x50400000`，EIP `0x506B1F6B`（偏移 `0x2B1F6B`），
   异常参数 `[0, 0x660]`（读空指针 + 0x660）。
2. `sym_lookup.ps1` → `AttachEffect::OnUpdate + 0x10B`，`AttachEffect.cpp:1524`
   （`abstract_cast<BuildingClass*, true>(pTechno)->HasPower`）。
3. 反汇编 + 寄存器 → `IsBuilding()` 返回 true（用的是缓存的 `_absType`），
   但 `pTechno->WhatAmI()` 已是 `AbstractType::Unit(1)`，cast 得到 nullptr 后直接解引用。
4. 结论：`InheritAE` 把建筑的 AE 管理器搬到新载具上时只换了 `_extData`，
   `_absType/_ownerType` 仍是旧宿主的缓存 → 已在 `ObjectScript::ExtChanged()` 中统一重置。

## 5. `gen_hook_conflict.py` — 生成 KratosPP × Phobos Hook 地址冲突清单

从两个仓库的源码里提取**所有会改写 gamemd 内存的指令**（`DEFINE_HOOK`、`DEFINE_HOOK_AGAIN`、
`DEFINE_JUMP`、`DEFINE_FUNCTION_JUMP`、`DEFINE_DYNAMIC_JUMP`、`DEFINE_NAKED_HOOK`、
`DEFINE_PATCH`、`DEFINE_DYNAMIC_PATCH(_TYPED)`），按字节区间比对，输出三张表：

- **A**：同址（`DEFINE_HOOK` × `DEFINE_HOOK`）
- **B**：同址（含 `JUMP` / `PATCH` 等**静态改写**）—— 这一类比 A 危险，因为静态写入不与 Syringe 协商，会直接覆盖
- **C**：区间部分重叠（起始不同，即「相邻打架」）

```
set PY=C:\Users\chris\.workbuddy\binaries\python\versions\3.13.12\python.exe

%PY% tools\gen_hook_conflict.py ^
  --kratos-src src ^
  --phobos-src D:\Workspace\ra2mod\platform\Phobos\src ^
  --out-md  ida_work\hook_conflict_new.md ^
  --out-tsv ida_work\hook_conflict_new.tsv ^
  --old-md  docs\Hook地址冲突清单.md

rem 再把表格回填进文档（替换 <!-- AUTO:key --> 标记之间的内容）
%PY% tools\gen_hook_conflict.py ... --splice-doc docs\Hook地址冲突清单.md
```

`docs\Hook地址冲突清单.md` 里的表格区都由 `<!-- AUTO:A/B/C/STATS -->` 标记包着，
所以只需要维护叙述部分，表格每次运行自动同步（重复运行是幂等的）。

**两个实现要点**（都是踩过的坑）：

1. 先把全文注释（`//` 与 `/* */`）按**原长度**替换成空格，再在「纯代码」上按**平衡括号**抽宏调用。
   这样既能剔除被 `/* */` 整体停用的 hook（如 `Hooks/AnimExtHook.cpp` 里那批），
   又能处理参数里夹注释的情况（`DEFINE_PATCH(/* Offset */ 0x825F9B, ...)`），且行号不变。
   字符串 / 字符字面量内容要**保留**，否则 `DEFINE_PATCH` 的字符串数据会被误吞。
2. 不要用「逐行正则」：它会把注释里的停用 hook 算进来，且漏掉跨行声明。

## 6. `analyze_hook_chain.py` — 同址 Hook 的「链式执行」相互影响分析

`gen_hook_conflict.py` 只回答「谁和谁撞了地址」；本脚本回答**「撞了之后谁真正生效」**。

前提（Syringe 链式语义 + 实测加载顺序）：

- 加载 / 执行顺序：**Ares → Kratos → Phobos**；
- 同一地址上各 hook 依注册顺序**串成一条链**依次执行；
- Hook 体 `return 0` ⇒ 续链（跑完全部后重放被替换的原始指令）；
- Hook 体 `return <非 0 地址>` ⇒ **立即跳转，链上后面的 hook 与原始指令重放全部被跳过**。

于是「Kratos 返回非 0」的地址上，**Phobos 的 hook 会被静默跳过**。判定分三档：

- `ALLPATHS` —— Kratos 的 hook 体内**没有任何 `return 0`**，所有路径都跳走 ⇒ **Phobos 永不执行**；
- `BRANCH` —— 体内既有 `return <非0>` 又有 `return 0` ⇒ **条件掐断**，取决于运行分支 / 门控开关；
- `ZERO` —— 全部 `return 0` ⇒ 链正常，Phobos 可执行。

另外：名字或**函数体注释**里出现 `Phobos` 的会标注为**故意接管**（Kratos 作者明知并主动复刻）；
静态改写（`DEFINE_JUMP` / `DEFINE_PATCH`）不走链，是裸字节写入 —— **后加载的 Phobos 覆盖 Kratos**（方向与 hook 相反）。

```
%PY% tools\analyze_hook_chain.py ^
  --kratos-src src ^
  --phobos-src D:\Workspace\ra2mod\platform\Phobos\src ^
  --out-tsv ida_work\hook_chain.tsv ^
  --splice-doc docs\Hook地址冲突清单.md
```

`docs\Hook地址冲突清单.md` §10 的 D 表由 `<!-- AUTO:D -->` 标记回填。

**三个实现坑**（都踩过）：

1. `return` 语句要**只取 hook 函数体的顶层**，排除 **lambda 内部**的 `return`（`[&](int x){ return x > 0; }` 是给 lambda 自己用的）；
   否则会把 `return (bool表达式)` 误判成跳转。
2. 解析 `return` 表达式到 `;` 为止；**不要在 `d == 0` 时遇 `)` 就截断**，
   否则 `return (cond) ? A : B;` 会被截成 `(cond`，若三元分支里有 `0` 就会误判档位。
3. 写 TSV 前要把返回值里的**换行 / 制表符压成空格**，否则会把一行切成多行 / 多列。

## 相关目录

- `ida_work\`：gamemd 的 IDA 反编译产物（脚本 + dump 文本）。**IDB 不入库** ——
  正本是 `D:\Workspace\ra2mod\platform\gamemd\gamemd.idb`（约 219MB）。
  ⚠️ **别拷副本**：`ida.exe -A -S<script>.py "…\platform\gamemd\gamemd.idb"` **直接在正本上跑**，
  改名/注释要留在库里供人打开查看（见 `.workbuddy\memory\MEMORY.md §五`）。
  正常收尾留下的 `gamemd.id0/.id1/.nam/.til` 是库的工作形态，不是副本，不必清理。
- `docs\IDA反编译-DeploysInto流程分析.md`：DeploysInto / UndeploysInto 两个 hook 的时序分析。
