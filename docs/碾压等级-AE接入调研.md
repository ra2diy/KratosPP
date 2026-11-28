# 碾压等级接入 Kratos AE —— 可行性调研

- 调研对象：Phobos PR #1616 `[Vanilla Enhancement] Crush level system`（作者 NetsuNegi）
- 对照代码：本仓库 `KratosAI`（分支 `springy`，HEAD `4e82c9a`）
- 结论日期：2026-11-26

---

## 0. 结论（先看这段）

**能做，地址层面干净，但工作量不在 AE 上，在引擎侧。**

1. **Kratos 现在一行碾压逻辑都没有**。全仓库 grep `Crush`，只命中 `YRpp` 里的结构体字段声明（`TechnoTypeClass::Crusher` / `ObjectTypeClass::Crushable` / `TechnoClass::Uncrushable` …），`src/` 下没有任何碾压相关 hook。
   所以这不是"给 AE 加个字段"，而是**先把 Phobos 那套碾压等级底座整套移植进 Kratos**，再在底座上加 AE 入口。
2. **PR 的 23 个 hook 地址与 Kratos 现有 hook 零重叠**（逐条核对过源码与 `docs/Hook地址冲突清单.md`），可以原样落位。
3. **AE 侧改动很小**：两个聚合字段 + 一处每帧缓存刷新 + 两处查询合并。因为 `TechnoStatus::RecalculateStatus()` 本来就每帧、每个单位调用一次 `CountAttachStatusMultiplier()`，加字段几乎零成本。
4. 真正的风险不在"能不能"，而在：
   - 与 Phobos 同开时**同功能两份实现**（建议二选一）；
   - 旧存档兼容（AE 数据块带长度校验）；
   - 联机同步（碾压直接决定移动与伤害）；
   - `ObjectClass::IsCrushable` 是**整函数替换**，需 IDA 核对原语义不丢分支。

**预估工作量：5～8 个工作日**（引擎移植 2～3 天，AE 接入 0.5～1 天，边界与联机测试 2～3 天，文档 0.5 天），不含 MOD 侧数值调参。

---

## 1. Phobos PR #1616 到底是什么

### 1.1 状态

| 项 | 值 |
|---|---|
| 状态 | `open`，**未合并**（`merged_at: null`） |
| 可合并性 | `mergeable: false`（与 develop 冲突，`mergeable_state: dirty`） |
| 规模 | 43 commits，13 files，+325 / −4 |
| 分支 | `NetsuNegi:feature/crush_level` |
| 标签 | New feature / Tested / T1 |
| 改动文件 | `Rules/Body.cpp|h`、`Techno/Body.cpp|h`、`TechnoType/Body.cpp|h`、`Unit/Hooks.Crushing.cpp`、文档与 `.po` |

> 因为与 develop 冲突，**不能直接 cherry-pick**，只能按内容重写。这一点对 Kratos 无影响（本来就要重写）。

### 1.2 机制

把原版二态的 `Crusher` / `Crushable` 布尔改成"等级"，判定统一为：

```
允许碾压  ⇔  碾压者 CrushLevel  >  被碾压者 CrushableLevel      （严格大于）
```

等级取值沿用原版的语义编码（这是这套设计最巧的地方，**默认值就能 1:1 复刻原版行为**）：

| 等级 | 含义 |
|---|---|
| `0` | 谁都能压（`Crushable=yes` / `DeployedCrushable=yes` 的类型默认落到 0） |
| `5` | `[General] CrusherLevel` / `CrushableLevel` 默认值：普通碾压者压不动"不可压"单位 |
| `10` | `OmniCrusherLevel` / `OmniCrushResistantLevel`：全压 / 全免疫 |
| 其它 | 由 `TechnoType` 的 INI 或（本方案）AE 给出 |

- **建筑**：`BuildingCrushableLevel` 默认取 `OmniCrushResistantLevel`（10）→ 默认谁都压不动，配置后可被压。
- **墙 / 沙袋**：`[WallModel] WallCrushableLevel=10`，且 `MovementZone=CrusherAll` 直接放行。
- **部署步兵**：走 `TechnoClass::Uncrushable` 分支，读 `DeployedCrushableLevel`。
- **前置条件保留**：非友军、未被铁幕、`pThis != pCrusher`。

### 1.3 INI 面

```ini
[General]
CrusherLevel=5                 ; 普通碾压者等级
CrushableLevel=5               ; "不可压"单位的默认等级
OmniCrusherLevel=10            ; 全压者等级
OmniCrushResistantLevel=10     ; 全免疫等级
BuildingCrushableLevel=        ; 建筑被压等级，默认 OmniCrushResistantLevel

[WallModel]
WallCrushableLevel=10          ; 墙被压等级

[SOMEVEHICLE]                  ; 作为碾压者
CrushLevel=
CrushLevel.Veteran=
CrushLevel.Elite=

[SOMETECHNO]                   ; 作为被碾压者
CrushableLevel=
CrushableLevel.Veteran=
CrushableLevel.Elite=

[SOMEINFANTRY]                 ; 部署后
DeployedCrushableLevel=
DeployedCrushableLevel.Veteran=
DeployedCrushableLevel.Elite=
```

`CrushLevel` 在类型上**优先于** `Crusher` 标记：即使 `Crusher=no`，写了 `CrushLevel=5` 也能压。
→ 这正是"碾压能力可以外挂"的口子，也是 AE 接入的立足点。

### 1.4 代码改动（23 个地址）

| 组 | 地址 | 位置 | 作用 |
|---|---|---|---|
| A. 碾压者判定（11 处） | `0x4B3504` `0x4B414F` | `DriveLocomotionClass::PassableCheck` ×2 | 内联 `Crusher` 判断 → `GetCrushLevel>0` |
| | `0x5B098E` `0x5B1034` `0x5B1418` | `MechLocomotionClass::ProcessMoving` ×3 | 同上（机甲） |
| | `0x6A1044` | `ShipLocomotionClass::ProcessMoving` | 同上 |
| | `0x6A2B53` `0x6A377B` | `ShipLocomotionClass::PassableCheck` ×2 | 同上 |
| | `0x73AFEB` | `UnitClass::PerCellProcess` | 同上 |
| | `0x73FC6C` | `UnitClass::IsCellOccupied` | 同上 |
| | `0x741733` | `UnitClass::CrushCell` | 同上 |
| B. 拆前置判断（4 处） | `0x73FB2A→0x73FB47`、`0x73FE5F→0x73FE7C`、`0x741529→0x741546`、`0x7438F7→0x743918` | 四处 `DEFINE_JUMP(LJMP…)` | 跳过"调用 `IsCrushable` 前的 `Crusher` 检查"，把决策全部交给 `IsCrushable` |
| C. 语义扩展（6 处） | `0x741603` | `UnitClass::ApproachTarget` | OmniCrusher 判定 → `>= OmniCrusherLevel` |
| | `0x73B013` | `PerCellProcess`（墙/沙袋） | `> WallCrushableLevel` 才可压 |
| | `0x73F42E` | `IsCellOccupied`（墙） | 同上 |
| | `0x4B19B8` | `DriveLocomotionClass::ProcessMoving`（墙） | 同上 |
| | `0x4B1A1B` | 同上（CrusherAll） | `CrusherAll` 或 `> OmniCrusherLevel` |
| | `0x4B3569` + `0x6A2BB8` | `PassableCheck`（Drive/Ship，`DEFINE_HOOK_AGAIN`） | `CrusherAll` 或 `> WallCrushableLevel` |
| D. 收口（1 处） | `0x5F6CD0` | `ObjectClass::IsCrushable` | **整函数替换**：等级比较取代原来的布尔判断 |

B 组的 4 个 `DEFINE_JUMP` 是"无条件跳过"——相当于把引擎里"你是不是 Crusher"的门票撕掉，改由 D 组按等级裁决。
所以 **A 组 11 处 + D 组必须一起上**，只上其中一半会出现"能压但不能走进去"或反之。

### 1.5 两个实现细节（移植时要保留）

- `GetCrushLevel` / `GetCrushableLevel` 都用"三段 fallback"：`Elite → Veteran → Rookie → 类型兜底`，任一档 `>=0` 即返回。
- `GetCrushableLevel` 对"部署步兵"和"普通单位"是两条分支（`pInfantry->Uncrushable`），不能合并。

---

## 2. Kratos 现状核查

### 2.1 有没有碾压逻辑

没有。`src/` 全量 grep `Crush` 无命中，只有 `YRpp` 的结构体字段：

- `TechnoTypeClass::Crusher / OmniCrusher / OmniCrushResistant / TiltsWhenCrushes`
- `ObjectTypeClass::Crushable`（✓ 可复用，`OverlayTypeClass` 继承它，所以 PR 里 `pOverlay->Crushable` 在 Kratos 的 YRpp 里也能编译）
- `InfantryTypeClass::DeployedCrushable`
- `TechnoClass::Uncrushable`

`ObjectClass::IsCrushable` 在 YRpp 里没有声明，但这不影响——PR 是直接 hook 地址。

### 2.2 地址冲突核查

逐条核对 `src/` 全部地址常量 + `docs/Hook地址冲突清单.md`：

- **Kratos 侧：23 个地址全部未占用。**
- **Phobos 侧（0.5-alpha1-128 快照）**：`Hooks.Crushing.cpp` 里已有一批"碾压减速 / 碾压前倾"的 hook，地址与 PR **相邻但不重叠**：

| Phobos 已占 | 用途 | 与 PR 的关系 |
|---|---|---|
| `0x4B1150`、`0x6A0813` | 碾压减速系数 | 无关地址 |
| `0x4B19F7`(0xD)、`0x4B1A4B`(0xD) | 碾压前倾 | PR 的 `0x4B19B8`、`0x4B1A1B` 在其前后，字节区间不交叠 |
| `0x4B1146`、`0x6A0809` | 跳过碾压减速 | 无关地址 |
| `0x73B05B` | 压 overlay 时的倾斜 | PR 的 `0x73B013` 跳转目标是 `0x73B036`/`0x73B074`，会经过 `0x73B05B`，不冲突 |
| `0x741941` | 压单位时的倾斜 | 与 PR 的 `0x741733` 不同函数段 |

→ **结论：地址层面干净，可以落位。** 唯一没法百分百确认的是 Phobos 全仓库是否另有 `0x5F6CD0` 的 hook（GitHub 代码搜索需登录，我只核对了 `Hooks.Crushing.cpp` 与冲突清单）。若你的 Phobos 版本也整函数替换了 `IsCrushable`，两份实现只能生效一份（见 §5.1）。

### 2.3 Kratos AE 架构（接入点）

三条链，缺一不可，但都很规整：

1. **数据定义**：`AttachEffectData`（`src/Ext/EffectType/AttachEffectData.h`）聚合所有效果数据，五个宏维护"include / 定义 / Read / GetScriptNames / Process"五张表。
2. **组件实例化**：`DECLARE_COMPONENT` 静态注册到 `ComponentFactory`，`AttachEffectScript::EnableEffects()` 按名字 `AddComponent` → 自动实例化，无需改工厂。
3. **数值聚合**：`CrateBuffData`（`Status.*` 前缀）由 `AttachEffect::CountAttachStatusMultiplier()` 汇总，`TechnoStatus::RecalculateStatus()` 每帧写进引擎字段（`FirepowerMultiplier` / `ArmorMultiplier` / `SpeedMultiplier` / 隐身）。

关键事实：

- `TechnoStatus` 是**所有 Techno 的全局脚本**（`TechnoExt::AddGlobalScripts` 里 `push_back(TechnoStatus::ScriptName)`），步兵/载具/建筑/飞机都有。
- `RecalculateStatus()` 在 `TechnoStatus::OnUpdate()` 里**每帧每单位**调用一次 → 加两个 int 字段的边际成本≈0。
- 现有 `Status.RangeCell` 就是"加法聚合"的现成范例（`multiplier.RangeCell += ...`），碾压等级照抄即可。

现有 AE 体量：26 个效果器 + 18 个状态机（`AttachEffectData` 里的 `EFFECT_VAR_DEFINE` 清单），新增一个数据块属于常规操作。

---

## 3. AE 侧怎么接

### 3.1 数据流（接入后）

```
AE 附加（弹头 / 类型自带 / 热键 / 部署 / 礼盒 / 乘客…）
   ↓  AttachEffectScript（Enable、IsPaused、血量触发、堆叠都已就绪）
AttachEffect::CountAttachStatusMultiplier()        ← 每帧一次（已有调用）
   ↓  CrushLevel / CrushableLevel 两个 int 做加法聚合
TechnoStatus::RecalculateStatus()                  ← 写缓存字段，O(1) 读
   ↓
TechnoExt::GetCrushLevel() / GetCrushableLevel()   ← 23 个 hook 里调用
   ↓
引擎碾压判定（> 比较）
```

**为什么必须缓存**：`IsCrushable` / `PassableCheck` 在单位移动时**逐格逐单位**触发，现场遍历 AE 组件树（`dynamic_cast` + 递归）会明显拖慢大规模行军。缓存到 `TechnoStatus` 的 int 上，hook 里只是一次 map 查找 + 一次整数加法。

### 3.2 两个方案

| | 方案 A：挂 `Status.*` 块 | 方案 B：独立 `CrushLevel` 效果器 |
|---|---|---|
| INI 写法 | `Status.CrushLevel=3` | `CrushLevel=3` / `CrushLevel.Elite=5` |
| 改动 | `CrateBuffData` +2 字段、聚合函数 +2 行 | 新增 EffectData/EffectScript 各一对，改 5 张表 + 序列化顺序 |
| 精英变体 | 不支持 | 支持（`EliteData` 模式已有先例） |
| 绝对覆盖语义 | 不好表达 | 可加 `CrushLevel.Set=yes` |
| 文档 | 说明书 `Status.*` 段加两行 | 需要新开一段 |
| 风险 | 极低（复用现有聚合与存档） | 中（新增类型，存档顺序必须追加在末尾） |

**建议：先做 A**（半天），把玩法跑通；确有"精英档不同 / 绝对覆盖"需求再升级到 B。两者可以共存（A 做增量，B 做覆盖）。

方案 A 的语义建议：

```ini
[CrushBoost]
Duration=300
Status.CrushLevel=3          ; 碾压等级 +3（可负）
Status.CrushableLevel=-2     ; 被碾压等级 -2（更容易被压，可负）
```

### 3.3 语义细节（必须提前定）

1. **加法还是覆盖**：默认加法。若出现"AE 要求绝对等级"，再加 `Max` / `Set` 语义，不要混在加法里。
2. **钳制**：`CrushableLevel` 聚合后建议**下限钳到 0**。否则负等级会让"等级 0 的非碾压者"也能压（B 组 4 个 JUMP 已经把门票撕了，只靠 `>` 比较兜底）。
3. **精英档**：`GetCrushLevel` 的 `Elite/Veteran/Rookie` 三段仍在类型侧读；AE 增量不区分军衔（方案 A）。
4. **部署步兵**：AE 增量应加到 `DeployedCrushableLevel` 分支上，而不是普通分支。
5. **替身（Stand）**：`CountAttachStatusMultiplier` 对"虚拟炮塔替身"会反向取使者的加成，碾压建议沿用同一规则；普通替身不应把碾压等级带给使者。
6. **暂停 / 失效**：`CountAttachStatusMultiplier` 已经过滤 `IsPaused()`（血量触发、初始延迟），AE 暂停期间等级自动回落——这是想要的行为，写进文档即可。

---

## 4. 改造清单（文件级）

| # | 文件 | 改什么 | 量级 |
|---|---|---|---|
| 1 | `src/Ext/Common/CommonStatus.h` | `GeneralData` 加 5 个全局值；`WallCrushableLevel` 用 `INI::GetSection(INI::Rules,"WallModel")` 懒读（可省掉 Phobos 的 `0x66D242` hook） | ~25 行 |
| 2 | `src/Extension/TechnoTypeExt.h` | `TypeData` 加 `CrushLevel/CrushableLevel/DeployedCrushableLevel` 各三档（默认 −1） | ~35 行 |
| 3 | **新增** `src/Hooks/CrushHook.cpp` | 23 处 hook + `GetCrushLevel/GetCrushableLevel` 调用 | ~260 行 |
| 3b | `Kratos.vcxproj` | 新 `.cpp` 加进 `ClCompile`（工程是显式文件清单，共 209 条） | 1 行 |
| 4 | `src/Extension/TechnoExt.h/.cpp` | 两个静态查询函数（合并"类型基准 + AE 增量"） | ~40 行 |
| 5 | `src/Ext/TechnoType/TechnoStatus.h` | 缓存字段 `CrushLevelBonus` / `CrushableLevelBonus` | ~4 行 |
| 6 | `src/Ext/TechnoType/Status/CrateBuff.cpp` | `RecalculateStatus()` 里刷新缓存 | ~6 行 |
| 7 | `src/Ext/EffectType/Effect/CrateBuffData.h` | 两个字段 + `Enable` 判定 + `Serialize` 追加到末尾 | ~12 行 |
| 8 | `src/Ext/ObjectType/AttachEffect.cpp` | `CountAttachStatusMultiplier()` 聚合两行 | ~2 行 |
| 9 | `附件包/Kratos食用说明书.ini`、`docs/使用文档.md` | AE 标签与类型标签说明 | ~20 行 |

> 第 4 步的查询函数建议长这样（伪代码）：
>
> ```cpp
> int TechnoExt::GetCrushLevel(FootClass* pThis)
> {
>     int level = /* 类型三档 → Crusher/OmniCrusher/全局兜底，同 PR */;
>     TechnoStatus* status = nullptr;
>     if (TryGetStatus<TechnoExt>(pThis, status))
>         level += status->CrushLevelBonus;
>     return level;
> }
> ```

---

## 5. 风险与待验证项

### 5.1 与 Phobos 双开 = 同功能两份实现

- 你现在的环境是 `Ares → Kratos → Phobos` 链式加载（见冲突清单文档）。同址 hook 会串成一条链，**`return 非 0` 会立即跳转并截断整条链**。
- `ObjectClass::IsCrushable` 是整函数替换，谁先返回谁说了算。两份实现同时在，**后加载的那份永远不执行**，但会给人"两个都在工作"的错觉。
- **建议：只在一侧实现**。若 Phobos 侧将来合并该 PR，Kratos 就不要再搬 D 组；或者反过来，Kratos 实现后不要再装带该功能的 Phobos。

### 5.2 旧存档

- `CrateBuffData` 加字段会让 AE 数据块变长。Kratos 的读档有块长度校验（`ExStreamReader::ExpectEndOfBlock`），旧档大概率读不过 → 表现为该单位 AE 丢失或读档失败。
- 处理选项：① 接受不兼容，更新说明里写"跨版本不兼容存档"；② 给 AE 数据块加版本号（改动大，不建议为这一个功能做）。
- 结论：**上线前必须实测旧档**，别让玩家踩。

### 5.3 联机同步

- 碾压判定直接改变移动（能不能穿格）和伤害（压死），任何一端算错 = 失同步。
- 等级输入只有两个来源：INI 与 AE 状态，都是确定性的 → 理论安全。
- 需要额外确认：`Cumulative` / `RandomDelay` / `InitialRandomDelay` 这类带随机的 AE 是否已走 Kratos 的同步随机通道（既有的 `docs/联机失同步排查报告.md` 里有现成验证方法）。
- 建议：把"碾压等级"加进帧校验日志（`Utilities/SyncLogging`），出问题能直接定位。

### 5.4 `ObjectClass::IsCrushable` 整函数替换

- PR 完全重写了这个函数，**必须先在 IDA 里核对原函数的所有分支**：铁幕、友军、`Uncrushable`、非 Techno 对象（地形/残骸）分别怎么走。
- 仓库里已有 `ida_work/` 全套探针脚本，核对成本不高（半天）。这是唯一"可能踩坑但看不见"的地方，不要跳过。

### 5.5 边界对象

| 对象 | 风险 | 验证方式 |
|---|---|---|
| 部署步兵 | `Uncrushable` 分支 + `DeployedCrushableLevel` | 部署/收起反复压 |
| 建筑 | AE 给建筑加可压等级后，引擎是否真的走 `IsCrushable` 路径未知 | 单独测"可压建筑" |
| 墙 / 沙袋 | `WallCrushableLevel` + `CrusherAll` 双通道 | 压墙、压沙袋 |
| 机甲 / 船 | PR 单独 hook 了 Mech / Ship 的 loco | 有对应单位就测 |
| 替身（Stand） | Stand 也是 TechnoClass，会被新 `IsCrushable` 判定；"替身被压"是全新行为 | 带替身单位互压 |
| 运输中单位 | 在车里时 `IsCrushable` 是否被调用 | 压运输车 |

### 5.6 性能

- 11 处 `IsCrusher` 在 `PassableCheck` 里，逐格调用。**必须缓存**，不能现场遍历 AE 树。
- 缓存每帧刷新一次，最多滞后 1 帧（AE 附加当帧不生效）——可接受，但要写进文档。

### 5.7 行为突变

- AE 到期瞬间等级回落：可能出现"压到一半停下"或"站在已占用格子上"。引擎会把单位挤开/弹开，需要实测是否卡住。
- AE 让单位获得碾压能力后，寻路会主动穿敌军单位格；等级消失后可能被卡在敌群里。

---

## 6. 分阶段与工作量

| 阶段 | 内容 | 产出 | 估时 |
|---|---|---|---|
| P0 | 本次调研 | 本文档 | 已完成 |
| P1 | 引擎底座（不含 AE）：全局 + 类型 INI + 23 hook + IDA 核对 | 单机能压/不能压与原版一致 | 2～3 天 |
| P2 | AE 接入：`Status.*` 两字段 + 聚合 + 缓存 + 读档 | 弹头/类型赋予碾压能力 | 0.5～1 天 |
| P3 | 边界与联机：部署步兵 / 建筑 / 墙 / 替身 / 机甲 / 双机 | 测试报告 | 2～3 天 |
| P4 | 文档与说明书 | 标签说明 | 0.5 天 |

**建议先在 P1 结束时做一次"零 AE 回归"**：确认不打 AE 时，原版所有碾压行为逐条一致（普通坦克压兵、压围墙、压树、压不了建筑、压不了采矿车…）。这是后面所有工作的地基。

---

## 7. 需要拍板的三件事

1. **运行环境**：是否 `Kratos + Phobos` 同开？你用的 Phobos 是否已经带这个 PR（比如拿 nightly 自己合的）？
   → 决定 D 组要不要搬，避免两份实现互相截断。
2. **AE 语义**：`+加法` 就够，还是需要 `绝对覆盖` / `按精英档不同`？
   → 决定走方案 A 还是 B。
3. **类型级 INI 要不要保留**：保留 `CrushLevel=`（推荐，AE 只做增量），还是"纯 AE、类型上不写"？

另：是否也需要 **"被碾压等级"** 能被 AE 改（给敌人挂 debuff 让他变脆）？方案 A 天然支持（`Status.CrushableLevel` 可负），确认一下是否符合你的玩法预期。

---

## 附：本次核对的依据

- PR diff：`https://patch-diff.githubusercontent.com/raw/Phobos-developers/Phobos/pull/1616.diff`
- PR 元数据：`https://api.github.com/repos/Phobos-developers/Phobos/pulls/1616`（open / dirty / 43 commits）
- Phobos 现有碾压 hook：`https://raw.githubusercontent.com/Phobos-developers/Phobos/develop/src/Ext/Unit/Hooks.Crushing.cpp`
- 本仓库：`src/` 全量地址扫描、`docs/Hook地址冲突清单.md`、`src/Ext/EffectType/*`、`src/Ext/TechnoType/TechnoStatus.*`、`src/Ext/ObjectType/AttachEffect.cpp`

---

## 附二：实现记录（2026-11-26）

按方案 A 落地，分支 `feature/crush-level-ae`；Debug / Release 双构建通过。

| 文件 | 内容 |
|---|---|
| `src/Ext/Common/CommonStatus.h` | `[General]` 五个等级 + `[WallModel] WallCrushableLevel`（懒读该段，不额外挂钩子） |
| `src/Extension/TechnoTypeExt.h` | 9 个类型级标签：`CrushLevel` / `CrushableLevel` / `DeployedCrushableLevel` 各三档 |
| `src/Ext/Helper/Crush.h/.cpp` | `Crush::CalcCrushLevel/CalcCrushableLevel/GetCrushLevel/GetCrushableLevel`：类型基准 + 军衔三级回退 + 建筑/部署分支 |
| `src/Hooks/CrushHook.cpp` | 18 个 hook 块 + 4 个 JUMP，覆盖 23 个地址 |
| `src/Ext/EffectType/Effect/CrateBuffData.h` | `Status.CrushLevel` / `Status.CrushableLevel`，加法聚合，`Enable` 参与判定 |
| `src/Ext/ObjectType/AttachEffect.cpp` | `CountAttachStatusMultiplier` 聚合两个新值 |
| `src/Ext/TechnoType/TechnoStatus.h` | `CrushLevelCache` / `CrushableLevelCache` / `CrushLevelReady` |
| `src/Ext/TechnoType/Status/CrateBuff.cpp` | 每帧刷新缓存；被碾压等级合计后下限钳到 0 |

与调研阶段的差异：

- 被碾压等级**必须**钳到 0：B 组 4 个 JUMP 已经把引擎的 Crusher 前置判断拆掉，负值会让等级 0 的单位也能压。
- 类型级标签保留（方案 A + 类型基准），AE 只做增量。
- `WallCrushableLevel` 改为懒读 `[WallModel]`，省掉 Phobos 的 `0x66D242` hook。

待办：进游戏按 §5.5 的边界对象清单逐项验证（部署步兵、建筑、墙、机甲、船、替身、运输中单位、AE 中途失效、双机同步）。
