# ida_work —— 本地 IDA 分析工具集（**不是项目的一部分**）

> **定位**：本目录**不属于 Kratos 项目**，只是开发者在本地做逆向取证时攒下的 IDA 脚本与文档。
> 放在仓库里纯粹是为了**方便其他开发者快速定位与复用**。
> **目录里的脚本一律假设「本机已经有一份 `gamemd.idb`」**——仓库里不含、也不会含 IDA 数据库
> （正本 219 MB，另有备份）。

---

## 1. 前置条件（按本机真实路径写）

| 项 | 本机路径 | 说明 |
| --- | --- | --- |
| IDA 8.3 便携版 | `D:\Workspace\ra2mod\platform\IDA_Pro_v8.3_Portable\` | 用 **`idat.exe`**（文本模式，无界面，适合批处理）；`ida.exe` 是带界面的版本 |
| 规范 IDA 数据库 | `D:\Workspace\ra2mod\platform\gamemd\gamemd.idb` | 219 MB，正本；备份 `gamemd.idb.backup_20260906` |
| 反编译器 | 便携版自带 Hex-Rays | 29 个脚本 `import ida_hexrays`，缺失会直接失败 |
| Python | 便携版自带 | 版本不对时用 `IDA_Pro_v8.3_Portable\idapyswitch.exe` 切换 |

以上都是**本机绝对路径**。脚本内部同样硬编码了输出路径，例如：

```python
OUT = r"D:\Workspace\ra2mod\platform\KratosPP\ida_work\p21_setheight.txt"
```

换机器时先全局替换 `D:\Workspace\ra2mod\platform\...`。
**不要**把 `gamemd.idb` 复制进本目录（`.gitignore` 会挡，也没有必要）。

---

## 2. 怎么跑

```bat
"D:\Workspace\ra2mod\platform\IDA_Pro_v8.3_Portable\idat.exe" ^
  -A ^
  -L"D:\Workspace\ra2mod\platform\KratosPP\ida_work\my_run.log" ^
  -S"D:\Workspace\ra2mod\platform\KratosPP\ida_work\p21_setheight.py" ^
  "D:\Workspace\ra2mod\platform\gamemd\gamemd.idb"
```

要注意的点：

- **`-S` 后面必须是脚本的绝对路径。** 写相对路径时 IDA 会按 **idb 所在目录**去解析，脚本根本不会执行，
  而且**静默失败**——只在 `-L` 指定的日志里留一行，容易以为"跑过了"。这是本目录最容易踩的坑。
- `-A` = 无人值守（autonomous），跑完自动退出。
- `-L<file>` 把 IDA 自己的运行日志写出来，出问题先看它。
- 脚本**必须自己退出**，否则批处理会一直挂着。惯例写法：

  ```python
  import ida_pro
  try:
      ...            # 全部分析 + 写输出文件
  finally:
      f.close()
      ida_pro.qexit(0)
  ```

  本目录 82 个脚本已带 `ida_pro.qexit(0)`；另有 39 个用 `ida_auto.auto_wait()` / `batch()` 等待自动分析结束，
  这些脚本跑起来会明显更慢（分钟级）。
- **副作用**：`-A` 跑完 IDA 会**重新打包 idb**；中途崩溃或被强杀，会在 idb 同目录留下 `.id0` / `.id1` / `.id2` /
  `.nam` / `.til` 等未打包残留。这些都已加入 `.gitignore`，确认没用可以直接删。
- 想完全不碰正本：先复制一份再跑，例如 `copy "D:\Workspace\ra2mod\platform\gamemd\gamemd.idb" %TEMP%\gamemd_work.idb`，
  然后把 `-S`/idb 参数换成副本。历史上有过 `gamemd_p1.idb` 之类的副本，已在 2026-10-05 清理。
- 每个脚本通常只写**一个**输出文件（脚本里的 `OUT = r"..."`），跑完直接看那个文件。

---

## 3. 命令启动器 `run_*.cmd`

`run_p21.cmd`、`run_crate_select.cmd`、`run_fieldwriters.cmd` … 每个对应一个探针，作用是"一条命令跑完"。

> ⚠️ 这些 `.cmd` 里的 **IDA 路径、脚本路径、idb 文件名都是按当时本机环境硬编码的**
> （早期用的是 `gamemd_p1.idb` 之类临时副本，那些副本已在 2026-10-05 清理）。
> **换机器 / 换 idb 名 / 换目录后必须先把 `.cmd` 改对**，否则会像 `-S` 相对路径那样静默失败。
> 拿不准时照第 2 节手工拼一条命令更稳。

---

## 4. 入库 / 不入库边界

- **入库**：分析脚本 `*.py`、命令启动器 `*.cmd`、文档 `*.md`（含本 README）。
- **不入库，但保留在本地磁盘**：
  - 探针输出 `*_probe.txt`、反汇编转储 `*_disasm.txt` / `*_dump.txt`、报告 `*_report.txt`、证据表 `*.tsv`；
  - 运行日志 `*.log`（含 `ida_*.log`、`chain_run.log`、`build_*.log`）；
  - 一次性测试脚手架与夹具：`port_unit_test/`、`e2e_logs/`、`e2e_https/`、`_logs/`、`_tables_tmp/`、`vtprobe/`；
  - IDA 产物：`*.idb` / `*.i64` / `*.id0` / `*.id1` / `*.id2` / `*.nam` / `*.til`。

实现方式是 `.gitignore` **白名单**：`/ida_work/**` 全忽略，再放行 `**/*.py`、`**/*.md`、`**/*.cmd`
（脚手架目录另加例外，即使里面是 `.py` 也不入库）。

确有必要把某份证据一起提交时，用 `git add -f <path>`，并在文档里写明为什么要入库。

---

## 5. 证据怎么重现

`docs/*.md` 里引用了大量 `ida_work/*.txt` / `*.tsv` 作为证据，**这些文件都不在仓库里**（见第 4 节）。
重现方法：在文档里找到脚本名 → 按第 2 节跑一遍 → 看脚本 `OUT` 指向的文件。

常用对照（脚本 → 输出）：

| 脚本 | 输出 | 用途 |
| --- | --- | --- |
| `dump_replaced_bytes.py` | `replaced_bytes.tsv` | 每条 Hook 被替换指令的精确反汇编 + 写目标寄存器 + 下一条指令 |
| `audit_hooks.py` | `audit_hooks.txt` | Hook 全量审计 |
| `dump_gate.py` / `dump_key_checks.py` | `gate_check.txt` / `key_checks.txt` | 门控条件、`GetWeapon` 语义、随机数扫描 |
| `dump_inwhichlayer.py` | `inwhichlayer.txt` | locomotion `+0x74` 交叉对比与 `InWhichLayer` 惯例 |
| `dump_loco_layer.py` / `dump_loco_callsites{,2,3}.py` | `loco_layer.txt` / `loco_callsites*.txt` | loco Hook 的 vtable 归属与字节级槽位定位 |
| `hotspots.py` | `hotspots.txt` | 430 条 Hook 按宿主函数分组，找"每帧热点" |
| `probe_zadjust_{ida,hosts,callsites}.py` | `zadjust_*_report.txt` | ZAdjustment 字段归属与调用点 |
| `render_path_probe{,2,3}.py` | `render_path_probe*.txt` | 绘制路径（层号升序 / 下标升序） |
| `stand_draw_probe{,2}.py` | `stand_draw_probe*.txt` | 替身绘制、`CompareYSortValues`、`vt[0xB8]` 归属 |
| `stand_tilt_probe{,2..8}.py` | `stand_tilt_probe*.txt` | 倾斜/抖动字段的读写者与 `GetCRC` 覆盖 |
| `resolve_logic_or_render.py` | `logic_or_render.txt` | 判定某个 Hook 落在逻辑帧还是渲染帧 |
| `dump_inrange.py` / `dump_stack_layout.py` | `inrange_disasm.txt` / `stack_layout.txt` | 射程索敌、栈布局 |
| `check_db_state.py` | `db_state_report.txt` | 检查 idb 里注释/重命名是否还在 |
| `p21_setheight.py` … `p37_isvoxel.py` | `p*_*.txt` | 按调查顺序编号的逐题探针 |

想查某个脚本到底写哪个文件：

```powershell
Select-String -Path D:\Workspace\ra2mod\platform\KratosPP\ida_work\*.py -Pattern '^OUT\s*='
```

---

## 6. 命名约定

| 前缀 | 含义 |
| --- | --- |
| `p<编号>_<主题>.py` | 按调查顺序编号的探针（`p21_setheight.py`、`p37_isvoxel.py`） |
| `dump_*.py` | 把某个函数 / 结构 / 调用点整体 dump 出来 |
| `probe_*.py`、`*_probe.py` | 围绕一个具体假设做的取证 |
| `resolve_*.py` | 解析地址归属、调用链 |
| `verify_*.py` | 复核既有结论 |
| `annotate_*.py` | 往 idb 里写注释 / 重命名（会改 idb，注意先备份） |
| `_*.py` | 临时/一次性小工具（`_cmp.py`、`_vtcount.py`） |

不依赖 IDA 的分析器在仓库根的 `tools/`，见 `tools/README.md`。
