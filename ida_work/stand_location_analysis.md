# 替身 Location 渲染期写入 —— 结论摘要（只读静态分析，未运行 IDA、未改任何 mod 源码）

## Q1 两个调用点都在「渲染期」（无逻辑期调用点）
- 分发口：`AttachEffect::OnGScreenRender` 注册于 `EventSystems::Render`（AttachEffect.h:257/262/313）← `DEFINE_HOOK(0x4F4497, GScreenClass_Render)`（Hooks/GScreenHook.cpp:21）＝ `GScreenClass::DrawOnTop` **最开头、任何实际绘制之前**。
- `:1484`（BeginRender 分支 `args==nullptr`；条件 `ae->IsAlive()` + `aeData.Stand.Enable` + 非火车）—— AttachEffect.cpp:1481-1484：
  `OffsetData offsetData = aeData.Stand.Offset; CoordStruct standOffset = this->StackOffset(...); LocationMark locationMark = GetRelativeLocation(pObject, offsetData, standOffset); ae->UpdateStandLocation(locationMark);`
- `:1309` 在 `UpdateTrainStandLocation`（:1285）内，唯一调用者 `:1478`（**同一** BeginRender 分支）⇒ 同样渲染期；条件 `Stand.IsTrain` + `_locationMarks` 非空，写的是逻辑期 `MarkLocation()`（:1516，← `OnUpdate`）记录的历史点。
- 全库 grep：`UpdateStandLocation` 只出现在 `AttachEffectScript.cpp:188` 与上述两处 ⇒ **逻辑期（`OnUpdate` / `OnUpdateEnd` / `LogicUpdateEvent`）没有任何替身定位调用**。置信度：高。

## Q2 写什么 / 依赖什么
- 链路 `AttachEffectScript.cpp:188-193` → `StandEffect::UpdateLocation` StandEffect.cpp:556-560：
  `_isMoving = _lastLocationMark.Location != locationMark.Location; _lastLocationMark = locationMark; SetLocation(locationMark.Location); SetFacing(locationMark.Dir, false);`
- `SetLocation`（:564-586）→ `pStand->SetLocation(location)` ＝ `ObjectClass::SetPosition 0x5F6940`（只写 `[obj+0x9C/+0xA0/+0xA4]`）+ `SetHeight(0)`（只写 Z）。
- 值来源：FLH.cpp:211-246 → Techno 分支 `GetFLHAbsoluteCoords(pTechno, offset, data.IsOnTurret)`（:234），起点 `pTechno->GetCoords()`（:109）＝ `GetRenderCoords 0x5F65A0`，再乘 `GetMatrix3D()`＝`Locomotor->Draw_Matrix`（:46）+ 炮塔偏移/角（:133-142），叠加 `Stand.Offset/Direction/IsOnTurret/IsOnWorld` 与 `StackOffset`（AttachEffect.cpp:1168，仅依赖同 owner 的 AE 遍历顺序与配置）。
- ⇒ **不读**屏幕坐标、相机、GScreen 状态、渲染次数；是「主身已同步字段（坐标/朝向/倾斜/loco 变换）+ 配置」的纯函数。唯一保留意见：基准是**渲染口径**的 `GetRenderCoords`+`Draw_Matrix`（本项目自认结构性隐患：docs/替身倾斜与抖动-2026-10-05.md:258-268）。置信度：高（无渲染输入）/ 中（Draw_Matrix 是否只读同步字段）。

## Q3 渲染期这次写入在补偿什么
- 逻辑期**没有**任何每帧写替身 Location 的地方：`pStand->SetLocation` 的全部调用点＝`StandEffect.cpp:57`（`OnStart` 初值）与 `:559`（← 渲染回调）；本项目自己的结论一致：ida_work/render_vs_logic_report.txt:317-319。
- 帧序（同报告 :45-57）：③`DrawOnTop`(hook 0x4F4497) → ④Ground `LayerClass::Sort 0x55DBC8` → ⑤`LogicClass_Update 0x55DC9E` → ⑥hash `0x55DE40`。**渲染在本帧逻辑之前**，且 hook 在 `TacticalClass_Draw 0x4F44DF` 之前 ⇒ 写入当场作用于本次绘制。
- 主身移动**晚于** mod 的两个逻辑回调：`OnUpdate`＝hook `0x6F9E50`（TechnoExtHook.cpp:136，`TechnoClass::Update` 入口）、`OnUpdateEnd`＝`0x6FAF7A/0x6FAFFD`（:150-151），两者都在 `TechnoClass::Update`(0x6F9E50-0x6FB005) 内；而它由 `FootClass::AI` 在 `0x4DA539` 调用（ida_work/logic_or_render.txt:60/68），主身 X/Y 移动发生在同一个 `FootClass::AI` 更后面：`0x4DA877 call [ecx+40h]`＝`ILocomotion::Process`(Drive=0x4B0500) → `0x4B08A4 call [reg+18Ch]`(`ObjectClass::UpdatePosition`)（ida_work/cv_verify.txt:156、p16_placement2_probe.txt:127）。
- ⇒ 渲染写＝「绘制前一刻把替身贴到主身最新逻辑坐标」，保证**画面**永不脱节；它**不是**在补一个逻辑期写（逻辑期根本没有）；也正因为它落在渲染期，**某端 0 次渲染 ⇒ 该端不更新 ⇒ 当帧 hash 立即分叉**。置信度：高。

## Q4 直接删掉渲染期写入
- **会严重退化：替身从此永久冻结在创建位置**（`SetFacing` 的方向跟随同时停）。它是唯一写入者；替身 loco 被 `Lock()`（StandEffect.cpp:39）、`SetDestination` 被 hook `0x4D94B0` 拦掉（StandExtHook.cpp:58），引擎不会自行移动它。置信度：高。

## Q5 替代方案（首选：用**已存在**的逻辑末期事件，零新增 hook）
- 落点：`EventSystems::Logic`＋`Events::LogicUpdateEvent`＋`EventArgsLate` ＝ `DEFINE_HOOK(0x55B719, LogicClass_Update_Late)`（Hooks/GeneralHook.cpp:114），即 `LogicClass::Update`(0x55AFB0-0x55B71D) 的**函数尾声** `add esp,28h; retn`；唯一调用者 `Main_Loop 0x55DC9E`（ida_work/sync_fns_dump.txt:997-1000）⇒ **每逻辑帧恰好一次、在所有对象 AI/移动之后、hash(0x55DE40) 之前**（docs/设计文档.md:97 已定义为"逻辑帧晚阶段"）。
- 改动点：① AttachEffect.h:257 `Awake()` 增 `EventSystems::Logic.AddHandler(Events::LogicUpdateEvent, this, &AttachEffect::OnLogicUpdate);`，:262 `Destroy()` 与 :313 `Load()` 同步增删；② 新增 `AttachEffect::OnLogicUpdate`（Ext/ObjectType/AttachEffect.cpp），`if (args != EventArgsLate) return;` 后执行现 :1466-1486 的定位块（含火车分支）；③ 从 `OnGScreenRender` 的 BeginRender 分支删除 :1474-1486（倾斜/动画 offset 等纯视觉写入保留）。hook 已存在 ⇒ 不违反"src/Ext 不放 DEFINE_HOOK"。
- 必须注意：`GetRelativeLocation` 的 Bullet 分支 `sourcePos += Velocity`（FLH.cpp:239/264，注释"取下一帧"）搬到逻辑末后需去掉预测（加参数）或对该类替身保留渲染路径；`ae->IsAlive()`(:1471) 有副作用（EnableEffects/PauseEffects/Deactivate，AttachEffectScript.cpp:234-309），只能"搬"不能"两处都调"。
- 0 次渲染：照旧更新（失焦不再分叉）；多次渲染：已不参与，天然帧幂等。附带收益：hash 里的替身 X/Y 变成"主身本帧坐标"，比现状（滞后一帧）更紧。
- 备选（不推荐）：纯显示偏移。引擎没有替身专用绘制偏移字段，绘制位置来自对象自身的 `GetCoords`→`GetRenderCoords 0x5F65A0` 与 `Locomotor->Draw_Matrix`，而 `GetCoords` 同时被逻辑消费（替身倾斜与抖动-2026-10-05.md:258-262），只能 hook 这些访问器，改动面更大且逻辑/显示口径分叉；现有显示层先例仅 `ObjectClass_ReturnRealYSort 0x5F6BF7`（StandExtHook.cpp:457）与 `GetZAdjustment 0x4DB091`（:405）。
- 置信度：高（落点与时序）；中（Bullet 分支、`IsAlive()` 副作用各需实测一处）。

## Q6 StandEffect.cpp:715 注释的"跳过路径"
- 同症状、不同机理：那是 `ae->IsAlive()`（AttachEffect.cpp:1471）这个状态门把定位整块跳过，而非渲染/逻辑时序；判据（`IsDead(pStand)`、`OwnerIsDead()`）都取自同步状态 ⇒ 两端一起跳，**本身不失同步**，只是功能/画面停摆；搬到逻辑末期若沿用同一门控，症状会原样保留，需单独处理。置信度：中高。

## 附带同类风险
- 同一渲染分支的 `ae->UpdateAnimOffset`（AttachEffect.cpp:1492 → `AnimStatus::Offset`，AnimationEffect.cpp:13-19）在渲染期写共享状态，而它在**逻辑期**被消费成 `pAnim->SetLocation`（AnimStatus.cpp:209/218）⇒ 动画 Location 进层数组、进 hash，属同一类失同步隐患，建议一并审。
