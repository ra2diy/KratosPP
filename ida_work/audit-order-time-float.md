# KratosPP 同步审计：顺序 / 时间 / 浮点（只读审计，未修改任何文件）

范围：`D:\Workspace\ra2mod\platform\KratosPP\src`（451 个文件）。方法：grep `unordered_map/set`、指针 key 容器、`sort/stable_sort`、`timeGetTime/GetTickCount/chrono`、`sqrt/cos/sin/atan2`、`Random::`，再读上下文判断"结果是否进入模拟"。未运行 IDA。

## 1. 发现表（按严重度排序）

| # | 文件:行 | 模式 | 为什么两端会不同（一句话） | 影响模拟 | 建议改法 | 置信度 |
|---|---|---|---|---|---|---|
| 1 | `src/Ext/EffectType/Effect/AttackBeaconEffect.cpp:77,87,106-137` | `std::map<std::pair<double,int>, TechnoClass*>`：key 含 `double` 距离，再按该序"从前往后招满数量上限" | 招募谁的顺序由 double 距离决定（`GetCoords().DistanceFrom`），任何 1ULP 差异（不同 CPU/UCRT/FMA）都会翻转 map 序 ⇒ 两端招募不同单位 | 是（`SetTarget`＋`AttackBeaconRecruited`） | 排序键改成整数（`(int)dist` 或 `DistanceFromSquared` 取整）＋ `UniqueID` 兜底；只在类型内排序会更好：先按类型分桶再按 (distInt, UniqueID) 排 | 中高（机制确定，需浮点差异触发） |
| 2 | `src/Ext/Helper/Physics.cpp:477-536`（关键 `527`、`529`） | `double s = angleScore*100 + distToTarget*10 + preOccPenalty;` + `if (s < bestScore)` 选落点 | 落点由 double 评分比较决定（`sqrt`/除法/方向归一），近等值时比较结果可能不同 ⇒ 落地/空降/传送落点不同 | 是（落点进位置模拟） | 用整数评分（角度用整数点积/叉积近似、距离用整数平方），或对评分做量化（`round` 到 1e-3）后再比，并显式 `(score, CellStruct)` 二级序 | 中 |
| 3 | `src/Ext/Helper/Finder.h:21-31`（`DereferenceLess`）＋调用点 `Finder.cpp:150,187,245,270,321,381,400`、`Utilities/Helpers.Alex.h:183` | `std::set<T*, DereferenceLess>`：解引用后按 `AbstractClass::operator<`（`YRpp/AbstractClass.h:143` = **UniqueID**）排序 | 不是裸指针序（这点做对了），但整个选择序＝UniqueID 序；`FindObject/FindTechnoOnMark` 取**第一个匹配**，一旦两端 UniqueID 不一致（读档/重连/非模拟路径建对象）⇒ 目标/AE 附加对象分叉 | 是 | 保留 UniqueID 序（优于地址序），但必须**验证 UniqueID 跨端一致**（可复用已有 `Utilities/SyncLogging.cpp:29-33` 的 RNG/帧日志，增加 UniqueID 校验）；文档中把该假设写死；更稳可显式 `(WhatAmI, UniqueID)` 排序 | 中（需人工判断 UniqueID 一致性） |
| 4 | `src/Ext/TechnoType/AircraftGuard.cpp:169-179`（同类：`Finder.cpp:169`、`AircraftGuard.cpp:184-196`） | 遍历 `GetCellSpreadTechnos` 返回的 vector / `TechnoExt::StandArray`，命中即 `break` 取第一个 | 返回序＝UniqueID 序，代码里还留着 `// TODO 对检索到的单位按威胁值排序`；谁当目标取决于该序 | 是（`SetTarget`） | 显式排序键 `(威胁值, UniqueID)`，或至少把 UniqueID 序当契约写进注释/断言 | 中 |
| 5 | `src/Ext/ObjectType/AttachEffect.cpp:1443-1464, 1468-1504`（`1478`、`1484` `UpdateStandLocation`） | 用函数级 `static StackOffsetMap`（全对象共享、每次调用 clear）＋在**渲染回调**里推进替身定位 | 替身 Location 在 `GScreenClass::Render`（`Hooks/GScreenHook.cpp:21-31` 广播）里更新；若两端渲染回调次数/顺序不严格等于逻辑帧 1:1（最小化、卡帧、hook 未触发）⇒ 替身位置分叉 | 是（需人工判断） | 把 `UpdateStandLocation` 移回 `OnUpdate`（逻辑帧），渲染只做显示偏移；`static` 累加器改为局部/按对象存放 | 中低 |
| 6 | `src/Utilities/Container.h:151, 328-341` | `ContainerMapBase::Items` = `std::unordered_map<const void*, void*>`（**指针哈希**），`InvalidateExtDataPointer/DetachExtDataPointer` 全量遍历 | 遍历序＝指针哈希序，必然两端不同；当前各 Ext 未覆写 `InvalidatePointer/Detach`（近似空实现）故暂无后果，但属于"埋雷" | 低（潜在） | 改为按创建序的 `std::vector<Extension*>`；或遍历前按稳定键（UniqueID）排序 | 中低 |
| 7 | `src/Ext/Common/PaintballSyncManager.h:57` ＋ `.cpp:56-71,114-117` | `std::unordered_set<PaintballState*>` 遍历并 `Activate()/Deactivate()` | 遍历序＝指针哈希序；当前每个元素写入完全相同的数据＋布尔，**与序无关**，所以现在不分叉；一旦 `Activate` 链上出现 RNG/计时器/"后写覆盖"就分叉 | 低（潜在） | 换 `std::vector<PaintballState*>`＋显式按注册序（或名字）排序，消除隐患 | 中低 |
| 8 | `src/Ext/EffectType/Effect/VectorEffect.cpp:676-678, 2190-2196, 2267-2271, 2915-2929, 2689-2690` → 消费点 `Ext/ObjectType/AttachEffect.cpp:252-255` | `std::cos/sin/atan2/sqrt` 的 double 累积 → `static_cast<int>` 写入 `MoveDisp`（位移）/`CurrentPitch` | 位移进模拟；同一二进制通常位一致，但 `cos/sin/atan2` 来自 UCRT，**跨不同 Windows/UCRT 版本**可能 1ULP 不同，截断成 int 时可能跨整数边界 | 是 | 轨迹改成由 `SyncEventManager` 广播的结果驱动（只一端算），或整数/定点算法；`Kratos.vcxproj:518/563` 只有 `Optimization=MaxSpeed`、未设 `FloatingPointModel`（x86 默认 `/fp:precise`，**没有** `/fp:fast`，是好事）——建议显式写死 `Precise` | 低（需人工判断） |
| 9 | `src/Hooks/MapExtHook.cpp:366-446` | `std::set<CellClass*>`（默认指针序）遍历绘制格子 | 顺序＝地址序，两端不同，但只影响覆盖绘制次序（`TacticalClass_Draw_Placement_Recheck` 内） | 否（纯表现） | 无需改；若要确定性可用 `CellStruct` 排序 | 高（安全） |
| 10 | `src/Ext/TechnoType/DamageText.h:77-78` ＋ `.cpp:49-77` | `std::map<DamageTextEntity*, …>`（指针序）每帧遍历 | 顺序＝地址序，仅决定跳伤害数字的先后 | 否（纯表现） | 无需改 | 高（安全） |
| 11 | `src/Hooks/MapExtHook.cpp:491-509` | 遍历 `TechnoExt::BaseUnitArray/BaseStandArray`（UniqueID 序）命中即 break | 只决定本地"能否在此放置"的 UI 判定；放置本身走事件同步 | 否（本地 UI） | 无需改 | 中高（安全） |
| 12 | `src/Common/Components/Component.cpp:96`；`src/Hooks/AircraftExtHook.cpp:439`；`src/Ext/Common/PreOccupancyManager.h:55` | `std::set<Component*>`（仅 DEBUG）、`unordered_set<AircraftClass*>`、`std::map<void*, …>` | 只做 find/insert/erase 成员判定，**从不遍历** | 否 | 无需改 | 高（安全） |
| 13 | `src/Ext/Helper/MathEx.h:66-107`（`MakeTargetPad`/`Hit`） | `std::map<Point2D,int>`＋`Random::RandomRanged` | 键是整数 (X,Y)，遍历序确定；RNG 用共享 `ScenarioClass::Random`，两端同帧同序消耗 | 是（但确定） | 保留 | 高（安全） |
| 14 | `src/Ext/TrailType/Trail.cpp:158,219`（调用点 `TechnoTrail.cpp:48-81`、`BulletTrail.cpp:25-46`） | 在名字像渲染的函数里消耗**共享** RNG | 调用链在 `OnUpdateEnd`（每逻辑帧、两端必跑、无本地可见性门控）⇒ 一致；一旦被挪进 `OnGScreenRender` 立刻造成 RNG 流分叉 | 是（当前确定） | 保持"渲染路径只用 `VisualRandomRanged`"的约定（正确范例：`Ext/TechnoType/Status/TargetLaser.cpp:129-131`） | 中高（安全） |
| 15 | `src/Ext/ObjectType/AttachEffect.cpp:239-272`（`263-266`） | `ForeachChild`（`_children` vector＝附加序）累加 `MoveDisp` | 整数加法和序无关；但 `Freeze/FrozenPos` 是"最后写入者胜"，同一对象有多个带冻结的 Vector AE 时取决于附加序（锁步下确定） | 是（当前确定） | 若要绝对稳，显式取"AEA 名/ID 最小者"而非最后写入者 | 低 |

补充（不计入 15 条）：全仓 `std::sort` 只有 2 处——`Ext/EffectType/Effect/HostEffect.cpp:33`、`Ext/StateType/State/GiftBoxState.cpp:50`，都是 `std::sort(types.begin(), types.end())` 对**字符串**去重（`std::unique`），是"显式稳定序"的正确用法；`Utilities/Helpers.Alex.h:440-463` 的 `selectionsort`（默认 `std::less<>`，对指针会是地址序）在 src 内**无调用点**。`Ext/Common/FireSuperManager.cpp:22-43` 的全局 FIFO 队列、`Ext/Helper/Finder.cpp:100-143` 的 `FindRandomTechno`（遍历引擎 `TechnoClass::Array` + 共享 RNG）都是从确定性模拟侧入队的，安全。

## 2. 指针为 key / 参与排序清单（本主题最致命的一类）

**A. 会遍历的裸指针序（默认 `std::less<T*>`＝地址序）**
- `src/Hooks/MapExtHook.cpp:366` `std::set<CellClass*> cells` — 遍历，纯绘制（安全）。
- `src/Ext/TechnoType/DamageText.h:77-78` `std::map<DamageTextEntity*, …> _damageCache/_repairCache` — 遍历，纯显示（安全）。
- `src/Common/Components/Component.cpp:96` `std::set<Component*> destroyingSet` — 仅 DEBUG 递归保护，不遍历（安全）。
- `src/Utilities/Container.h:151` `std::unordered_map<const void*, void*>` — **会遍历**（第 328/339 行回调），当前回调空实现；唯一"指针哈希遍历"的核心容器（见发现 6）。
- `src/Ext/Common/PaintballSyncManager.h:57` `std::unordered_set<PaintballState*>` — **会遍历**（`.cpp:56`），当前与序无关（见发现 7）。
- 仅做查找、不遍历（安全）：`src/Ext/Common/PreOccupancyManager.h:55` `std::map<void*, …> m_unitToCell`、`src/Hooks/AircraftExtHook.cpp:439` `std::unordered_set<AircraftClass*>`。

**B. "看着是指针 key、其实是按对象值排序"（做对了，但引入了 UniqueID 依赖）**
- `src/Ext/Helper/Finder.h:21-31` `DereferenceLess` → `std::less<>()(*lhs,*rhs)` → `YRpp/AbstractClass.h:143` `operator<` = `UniqueID < UniqueID`；别名 `DistinctCollector`，用于 `Finder.cpp:150,245,321,381,391`、`Utilities/Helpers.Alex.h:183`。
- `src/Extension/TechnoExt.h:12-18` `TechnoClassLess` = `a->UniqueID < b->UniqueID`，用于 `StandArray/ImmuneStandArray/BaseUnitArray/BaseStandArray`（`TechnoExt.h:51-60`）——遍历序稳定，是正确做法；建议在文档中固定"UniqueID 是唯一允许的排序键"这一契约。
- 参考 Phobos：`Phobos\src\Ext\Bullet\Trajectories\StraightTrajectory.cpp:923-936` 同样是"距离排序 + `UniqueID` 兜底"（它也是 double 距离比较，风险同类，可抄它的**显式 sort**结构，但建议把距离换成整数）。

**C. 指针参与 `sort/比较器`**
- 全仓无 `std::sort/std::stable_sort` 用指针比较；`Helpers.Alex.h:453` 的 `selectionsort` 默认 `std::less<>`（对指针会退化成地址序），但**无调用点**——若将来启用，必须显式传比较器。

## 3. 挂钟时间清单（逻辑用途 vs 表现用途）

- **逻辑用途：0 处。** 对 `src/**` 全文（大小写不敏感）grep `timeGetTime|GetTickCount|GetSystemTime|GetLocalTime|QueryPerformanceCounter|std::chrono|std::time|_time64|ftime|Imports::` → **零命中**。所有延时/冷却/周期都用帧计时 `CDTimerClass`：
  - `Ext/EffectType/Effect/AttackBeaconEffect.cpp:68-72` `_delayTimer.Start(_delay)`、`Ext/EffectType/Effect/CopyEffect.cpp:21,35` `_cycleTimer.Start(GetDelayFrame())`、`Ext/StateType/State/PaintballState.cpp:17,36` `_rgbTimer.Start(15)`、`Ext/TechnoType/Status/TargetLaser.cpp:132` `ShakeTimer.Start(delay)`、帧号统一取 `Unsorted::CurrentFrame`（`Ext/TechnoType/DamageText.cpp:47`）。
  - `Ext/Common/SyncEventManager.cpp:109` 提到引擎 `QueueClass::Add` 内的 Timings 记录 —— 引擎自身机制，按要求不计。
- **表现用途（仅此一处与时间概念相关，且安全）**：`Ext/TechnoType/Status/TargetLaser.cpp:129-131` 的激光抖动时延用 `VisualRandomRanged`（本地 `mt19937`，`Ext/Helper/MathEx.h:35-46`）——**正确范例**，说明作者已区分"共享 RNG vs 本地随机"。
- **风险提示（非挂钟，但同类）**：`Ext/Helper/DrawEx.cpp:43,56-58,72,215` 与 `Ext/TrailType/Trail.cpp:158,219` 在命名像渲染的函数里消耗**共享** RNG（`ScenarioClass::Random`）；当前调用链在开火流程（`Ext/Helper/Weapon.cpp:377-438`）与 `*Trail::OnUpdateEnd`（`TechnoTrail.cpp:48-81`、`BulletTrail.cpp:25-46`），两端都跑，故一致。**一旦这些函数被挪进 `OnGScreenRender`/GScreen 系列回调，会立刻造成两端 RNG 流分叉** ⇒ 该约定应写进注释并加 code review 检查项。

结论摘要：挂钟时间类别 **PASS**；`unordered_*` 遍历与指针序问题基本已被作者规避（改用了 `UniqueID` 序 / `VisualRandomRanged`），真正的剩余风险集中在 **(a) 浮点参与"排序/挑一个"的模拟决策**（发现 1、2、8）与 **(b) 对 `UniqueID` 跨端一致的隐式依赖**（发现 3、4）。
