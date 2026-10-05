# P2：以 Phobos 为准，修 Kratos 三个单端回调（只读分析）

分析证据：`Phobos/src/**`、`KratosPP/src/**`，以及本机 `gamemd.idb` 的 IDA 复核
（脚本 `ida_work/dump_event_execute*.py` → `ida_work/event_execute_dump*.txt`，只读不改工程）。
未修改任何源码。

## 第 1 步：Phobos 的做法与"为什么两端一致"

**挂点**：`Phobos/src/Ext/Techno/Hooks.Misc.cpp:1035` `DEFINE_HOOK(0x4C7512, EventClass_Execute_StopCommand, 0x6)`。
IDA 实证：

- `0x4C7512` 位于 `Networking_RespondToEvent`(0x4C6CB0) 内 `switch (EventClass::Type - 1)` 的 **case IDLE**；
  case 入口 `0x4C74CB` 的 IDA 符号就是 **`HandleEvent_ev_IDLE`**（`jpt_4C6CD7[5] → 0x4C74CB`，`EventType::IDLE / NetworkEvents::Idle = 6`）。
  该 case 覆盖 `0x4C74CB..0x4C76BB`，主体做的事就是"停"：`TargetClass::As_Techno` → 清 planning token →
  清 target/destination（`[+480h]`/`[+3C8h]`/`Assign_Destination_Cell(nullptr)`），收割机在 Harvest(10)/Return(12) 时 `QueueMission(Guard)`。
- `Networking_RespondToEvent` 是**所有客户端都会跑的事件执行体**：全二进制只有两处调用它，都在 DoList 处理循环里
  （`0x64C911`、`0x64CC0A`；前者遍历 `EventClass::DoList`、按 `Event.HouseIndex == PlayerPtr->ArrayIndex` 记 ResponseTime 后调用）。
- 事件**在单端产生**：`StopCommandClass_Execute`(0x730EA0) 在 `0x730EEB` 执行 `push 6; call dword ptr [edx+374h]`；
  vtable `+0x374` 的实现是 `0x6FFE00`，函数体只有：`TargetClass(this)` → `sub_4C65E0`（Target 型事件构造）→
  `EventClass::OutList` 上限(0x80)判定 + 写 `NetworkPackets0` + `timeGetTime` 时间戳。**体内没有任何本地停止动作**。
  ⇒ 按 S 后单位确实会停，只能是因为**本机自己的事件后来也过了 `0x4C74CB`**。
  这条不是推测，而是"把副作用放到这里 ⇒ 必两端一致"的正面证据。

**它做了什么**（`Hooks.Misc.cpp:1035-1069`，条件全部是同步量）：

```cpp
GET(TechnoClass* const, pThis, ESI);   // 0x4C74D3 起 ESI = 事件 TargetClass 解析出的 techno
if (auto const pUnit = abstract_cast<UnitClass*, true>(pThis)) {
    if (pUnit->CurrentMission == Mission::Unload && pType->DeployFire && !pType->IsSimpleDeployer) {
        pUnit->SetTarget(nullptr); pThis->QueueMission(Mission::Guard, true); }
    auto const pExt = UnitExt::Fetch(pUnit);
    pExt->SubterraneanHarvStatus = 0; pExt->SubterraneanHarvRallyPoint = nullptr; }
else if (auto const pBuilding = abstract_cast<BuildingClass*, true>(pThis)) { ... ForceMission(Mission::Guard); }
return 0;
```

对象只从事件里的 `TargetClass` 反解析；判据只有 `Mission` / `TechnoType` 标志；`return 0` ⇒ Syringe 重放被偷的 6 字节后再继续，**不截断链**。

**护栏（第 1.2 问）**：这两个 hook 里 Phobos **完全没用** `SessionClass::IsMultiplayer()` / `IsCurrentPlayer()` / `IsControlledByHuman()`。它在事件接收路径附近用到的只有三处：

- `Hooks.Misc.cpp:1115-1138` `EventClass_RespondToEvent_CheckControllability`（挂在 `0x4C74CB`/`0x4C71CA` 等 case 入口，8 字节）→ `TechnoExt::CanReceiveEvent`(`Ext/Techno/Body.cpp:1106`)：
  `Berzerk`/`Spawned`/`SlaveOwner` 是同步标志，可参考；但其中的 `pHouse->IsCurrentPlayer() && pOwner->IsControlledByCurrentPlayer()` 是**本机玩家**判定，
  同一事件在不同客户端可能得出不同结论 ⇒ **不要照抄这一条**。
- `Ext/Event/Body.cpp:111-132`：模拟状态按事件 `HouseIndex` 决定、**无条件**两端都改；只有侧边栏重绘才用 `HouseClass::CurrentPlayer == pHouse`
  ⇒ 这是正确样板："**同步量无条件执行，UI 才用 CurrentPlayer 门控**"。
- `Ext/Techno/Body.cpp:777-789` `ClickedApproachObject`：发端只把 `Unsorted::MoveFeedback`（纯本机 UI）用于播语音，
  然后 `EventExt{Type, HouseIndex, Frame, TargetClass}` 入队（发射端信息随事件走）。
  `Phobos` 的 `IsMultiplayer()` 只出现在 `Phobos.cpp/Save.cpp/Timers` 等非事件接收处；`IsControlledByHuman()` 用在与 AI 合法区分行为的地方（如 `Hooks.Misc.cpp:1166` 的 GuardArea 模式），它是 **House 级、全端一致的同步量**，可用。

## 第 2 步：三个副作用逐条判定

| 副作用 | 需要的输入 | 两端都能从同步状态推导？ | 判定 |
| --- | --- | --- | --- |
| `AircraftGuard::OnStopCommand`（`AircraftGuard.cpp:477-484`）：`_onStopCommand=true; CancelAreaGuard(); State=STOP; ForceMission(Enter)` | ①被下令的 techno（事件 `TargetClass`）②组件字段 `State/_onStopCommand/_destCenter/_destList/_destIndex`（`CancelAreaGuard` 见 `:132-137`），只在 `OnUpdate` 与 `StartAreaGuard` 里写过，而后者由 `AircraftClass_Mission_GuardArea_NoTarget_Enter`(0x41A96C，**双端**任务逻辑) 调用 ③常量 `Mission::Enter` | 全部可得，**没有任何"只有发起端才知道"的参数** | **可以**：把现在 `TechnoExtHook.cpp:512` 那段整体搬到 `0x4C7512`（见第 3 步写法） |
| `JumpjetCarryall::CancelMission`（`:37-44`，由 `:389-397` 的 `OnGuardCommand`/`OnStopCommand` 调用）：清 `_pTarget/_pTargetCell/_toPayload`，`_status = reset ? READY : STOP`，`GetTechnoStatus()->CarryallLanding = false` | ①被下令的 carryall（事件可得）②组件自身字段（`JumpjetCarryall.h:119-125`，已进存档 `:78-91`）③`reset`：`CancelMission(bool reset = false)`（`h:111`），两个调用点都用默认实参 ⇒ 常量 `false`（`_status=STOP`） | 可得 | **可以**（Stop 走 `0x4C7512`；Guard 见第 3 步）。⚠同区域 `ActionClick`(`UnitClass_ClickedMission` 内 0x7388FD) → `StartMission()` 是**单端点击路径**，直接写 `_pTarget/_pTargetCell/_status` + `SetDestination` + `QueueMission(Move)`：只修 `CancelMission` 会留下"进任务一套、出任务另一套"的不对称 |
| `StandEffect::ClearAllTarget`（`:931-947`）+ `onStopCommand=true` | ①master（事件可得）②它自己的 `pStand`：两端各自的 `StandEffect` 组件都有该字段，`TechnoExt::StandArray`(`Extension/TechnoExt.h:59`) 也在两端由 `TechnoStatus::OnPut_Stand`（`Stand.cpp:24-47`）对称维护 ③`onStopCommand` 标志（`OnUpdate:421-444` 消费、已进存档） | 可得 | **可以**：`ClearAllTarget` + `onStopCommand` 是**无条件**部分，直接搬进 `0x4C7512` 收端。**`if (!pStand->IsSelected)` 这一段不可以进收端**：`IsSelected` 是**本机选中集**，两端不同 |

关于第三条的补充：`!pStand->IsSelected` 那一段**可以留在发端**，因为它唯一的效果是
`pStand->ClickedEvent(NetworkEvents::Idle)`（`StandEffect.cpp:941`）——那会产生一条**真·广播事件**，
"有没有这条事件"由发起端决定、"两端都执行同一条"由引擎保证 ⇒ 本来就对称。
所以最小修法：收端只做 `ClearAllTarget(pStand); onStopCommand = true;`，
发端保留 `if (!pStand->IsSelected) pStand->ClickedEvent(NetworkEvents::Idle);`，
并**删掉发端手工的 `cc->OnStopCommand()` 转发**（那条 IDLE 事件到了收端，由下面的通用 hook 替它跑）。

## 第 3 步：收端挂点 + 一条消息事件（Guard 路径）

**Stop 路径照抄 Phobos**（不新增线格式）：

```cpp
// TechnoExtHook.cpp: 现有 0x730DEB/0x730E56/0x730EEB 的"直接改状态"全部撤掉（纯 UI 的留下）
DEFINE_HOOK(0x4C7512, EventClass_RespondToEvent_Idle_TechnoScript, 0x6)  // 与 Phobos 同址同长
{
    GET(TechnoClass*, pThis, ESI);      // ESI=techno；⚠EDI 已在 0x4C74DE 被 xor 清零，不可用；事件指针已不在寄存器里
    if (auto pExt = TechnoExt::ExtMap.Find(pThis))
        pExt->_GameObject->Foreach([](Component* c)
            { if (auto cc = dynamic_cast<ITechnoScript*>(c)) cc->OnStopCommand(); });
    return 0;                            // 原 6 字节 mov eax,[esi+0ACh] 由 Syringe 重放
}
```

**Guard 路径用 Kratos 自家隧道**（原版 Guard 的载体不是 IDLE，而是
`Player_Assign_Mission(Mission::Area_Guard = 11, cell, 0, 0)`：`0x730DEB/0x730E56` 的 `push 0Bh; call [edi+378h]`，
`+0x378` 实现 `0x6FFBE0`（`retn 10h` = 4 参数）内部 `Networking_FillEvent` = `0x4C6860` = MegaMission 构造器；
收端 case 是 `0x4C71CA`，而 **Phobos 已经在该 case 的入口(0x4C71CA,8B) 与内部(0x4C7462,5B) 各挂一个 hook**，再挤进去需要同址同长并复刻其 `SkipGameCode=0x4C6D42`，不划算）：

```cpp
// SyncEventManager.h: 追加 MsgId（只追加、勿改号）
MsgId::TechnoScriptCommand = 2,
// Ext/SyncEventType/TechnoCommandEvent.h
#pragma pack(push, 1)
struct EventData {
    TargetClass Whom;    // 5B = int m_ID + uint8 m_RTTI (YRpp/TargetClass.h:145-146)：被下令的 techno
    TargetClass Stand;   // 5B：发端解析出的替身；无替身 = 空 TargetClass{} 全 0
    uint8_t     Command; // 1B：1 = Stop → OnStopCommand()，2 = Guard → OnGuardCommand()
    uint8_t     Flags;   // 1B：bit0 = includeStand（发端 !pStand->IsSelected 的判定结果，本机信息随事件走）
};
#pragma pack(pop)
static_assert(sizeof(EventData) == 12);   // 12 > SmallPayloadMax(6) ⇒ SyncEventManager::Raise 自动走大隧道 0x51
```

- **发送点**（保持单端，就用现有 hook 的位置）：`0x730EEB`(Stop) 与 `0x730DEB`/`0x730E56`(Guard)，
  `SyncEventManager::Raise(MsgId::TechnoScriptCommand, pTechno->Owner, data)`（house 用单位所属、别用 CurrentPlayer），
  `return 0` 让原版事件照旧发出。
- **接收点**：**不需要新引擎 hook**，用既有的 `Hooks/SyncEventHook.cpp:8`（`0x4C6CC8` → `SyncEventManager::Dispatch`）。
  收端：`GetEventHouse()` 取 house → `Whom.As_Techno()` 重解析（null/已死/InLimbo 直接 return）→ 对该 techno 的所有 `ITechnoScript` 派发 `Command`；
  `Flags.bit0` 时对 `Stand.As_Techno()` 再派发一次，且必须校验 `AmIStand(pStand)` 且 `TechnoStatus::pMyMaster == Whom`。
- **必须放收端**：一切模拟状态（`State/_onStopCommand/CancelAreaGuard/ForceMission/CancelMission/ClearAllTarget/onStopCommand`、转发给替身组件的回调）。
- **仍可留发端**：语音/音效/光标/侧边栏等纯本机 UI；`Unsorted::MoveFeedback` 类判定；
  "`!pStand->IsSelected` → `pStand->ClickedEvent(Idle)`"（产生广播事件，只能发端做）。
- **绝对禁止进收端**：`ObjectClass::CurrentObjects`（选中集）、`ObjectClass::IsSelected`、`HouseClass::CurrentPlayer/IsCurrentPlayer`、
  任何 `ClickedEvent(...)`/`Raise(...)`（收端再生成事件 = 回环/事件风暴）。

## 第 4 步：风险与回归点

1. **单机**：按 S 只入队不动作（`0x6FFE00` 体内无本地动作）⇒ 单机也必须经过 `0x4C74CB`，理论上照常工作；但**必须实机确认**（含暂停/规划模式、存档读入瞬间）。若实测发现单机不触发，最小兜底 `if (!SessionClass::IsMultiplayer()) { 本地也跑一遍 }`（排除联机 ⇒ 无失同步风险）。
2. **Phobos 同址链**：Phobos 已占 `0x4C7512`(6B)、`0x4C74CB`(8B)、`0x4C71CA`(8B)、`0x4C7462`(5B)。同址共存的前提是**同长度 + `return 0`**（让被偷字节由 Syringe 重放、链继续）；长度不一致会互踩被偷字节 ⇒ 崩。所以照抄 `0x4C7512` 就用 `size=0x6`；两 DLL 装载顺序不同也要成立 ⇒ 两侧都需实机验证。Guard 路径不碰 `0x4C71CA` 正是为了避开这一点。
3. **读档/重连**：收端必须容忍 `As_Techno()` 为空（对象已删/ID 被复用）、`InLimbo`、组件已 `Disable()`。Kratos 的组件字段都在存档里（`AircraftGuard.h:67-78`、`JumpjetCarryall.h:78-91`、`StandEffect.h:80-96`）⇒ 读档后语义一致；但队列里残留的旧事件会按新规则执行，读档/重连要实测。
4. **幂等与顺序**：同帧内 master 与 stand 的事件顺序不保证；`ClearAllTarget`(`Ext/Helper/Status.cpp:497-518`) 会 `QueueMission(Mission::Stop)`、`ForceMission(Mission::Enter)`，需保证重复执行无害。Kratos 的 `0x4C7512` 早于 Phobos 的清理与原版 IDLE 主体（后者仅在 Harvester+Harvest/Return 时 `QueueMission(Guard)`）⇒ **重点回归：收割机、运输机、替身**。
5. **收端护栏比发端更严**（`0x4C7512` 前已有三个早退 `[esi+0x90]/[esi+0x81]/[esi+0x418]`，其后还有 `CurrentMission==Construction(18)/Selling(19)` 早退）⇒ 行为差异需实测；若要不带这些门控，可改挂 case 入口 `0x4C74CB`，但那里是 Phobos 的 8 字节 controllability hook。
6. **等价性陷阱**：`pStand->IsSelected`、`StandArray` 的遍历序号/裸指针都不能进载荷（`std::map<TechnoClass*>` 的序依赖指针值，两端不同）——替身一律用 `TargetClass`（RTTI+ID）。

## 置信度与需实机/IDA 确认的点

- **高**：`0x4C74CB = HandleEvent_ev_IDLE`（跳转表 idx5→Type 6 + IDA 符号名 + Phobos hook 命名 + `push 6`，四方一致）；`0x4C7512` 处 `ESI`=techno、`EDI` 已被清零；`Networking_RespondToEvent` 全二进制只有 DoList 循环那两处调用。
- **高**：Guard 热键 = `Player_Assign_Mission(Mission::Area_Guard(11), cell, 0, 0)` → MegaMission 事件（`push 0Bh` + `[+0x378]`实现 `0x6FFBE0`/`retn 10h` + 内部 `Networking_FillEvent`=`0x4C6860`，三路交叉验证）。
- **需实机**：①单机是否同样经 `0x4C74CB`；②收端早退门控对 Kratos 语义的实际影响；③`ActionClick`/`StartMission`（`0x7388FD`）单端写入在联机的真实后果（我判断是**独立的失同步源**，建议单独立项）。
- **需确认（非本 IDB 可证）**：`0x50/0x51` 是否与同进程其它 DLL 撞号（本机 IDB 只能证明引擎自身 0x00-0x2F 号段）。
- **未证实**：Phobos `CanReceiveEvent` 里 `IsCurrentPlayer()` 分支在"事件 house ≠ 单位 owner"时是否真的全端一致（我判断不一致）。
