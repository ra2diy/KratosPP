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

## 相关目录

- `ida_work\`：gamemd 的 IDA 反编译产物（脚本 + dump 文本）。其中 `gamemd.idb`（约 219MB）
  不入库，需要时把 `D:\Workspace\ra2mod\platform\gamemd\gamemd.idb` 拷过来即可。
- `docs\IDA反编译-DeploysInto流程分析.md`：DeploysInto / UndeploysInto 两个 hook 的时序分析。
