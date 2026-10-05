# (Un)DeploysInto 反编译流程标注与问题分析

> 数据来源：`D:\Workspace\ra2mod\platform\gamemd\gamemd.idb`（IDA 8.3 + Hex-Rays）
> 原始反编译文本：`ida_work/vanilla_decompile.txt`
> 反汇编窗口：`ida_work/hook_disasm.txt`
> 2026-09-06 更新：代码已按“统一 DiscardOnTransform 开关”调整，GiftBox 与 DeploysInto 的继承筛选现在只认
> `DiscardOnTransform`（`Inheritable` 降级为兼容旧 INI 的别名）。文中第 5.1/5.2 节关于“筛选 Inheritable、
> 清场 DiscardOnTransform”相互打架的历史分析已被此修改解决，仅作背景参考。

本文把 IDA 里命名错乱/语义含糊的伪代码修正成可读版本，并标出两个 Kratos Hook
（`0x739971`、`0x44A04C`）相对原版流程的确切位置，最后给出 `TechnoStatus::DeploysInto`
“管理器交换成功但 AE 没有继承”最可能漏掉的原因。

---

## 0. 先修正两个函数名（IDA 原名是错的）

| 地址 | IDA 显示的名字 | 修正后的名字 | 实际语义 |
| --- | --- | --- | --- |
| `0x7393C0` | `W?Try_To_Deploy$:UnitClass$n()i` | `UnitClass::Try_To_Deploy` | 载具部署成建筑（DeploysInto，如 MCV → 基地/矿场） |
| `0x449C30` | `W?Mission_Deconstruction$:BuildingClass$n()i` | `BuildingClass::Mission_Deconstruction` | 建筑出售/拆除任务；其中 `MissionStatus==1` 且带 `PoweredUnit` 的分支就是“建筑收回成载具”（UndeploysInto，如卖基地得到 MCV） |

因此 Kratos 里 `BuildingClass_Selling_TransferAE` 这个名字是误导的：
`0x44A04C` 并不在“卖成钱”的路径上，而是在 `Mission_Deconstruction` 内
“造回载具”的分支里，也就是 `UndeploysInto` 真正的原版入口。

---

## 1. UnitClass::Try_To_Deploy（部署：载具 → 建筑）

### 1.1 修正后的关键流程

```cpp
int UnitClass::Try_To_Deploy(UnitClass* this)   // 0x7393C0
{
    // 1. 各种前置检查：能否部署、是否在移动、是否已有目的地……
    // 2. 把载具标记为 Underground、清空占用格

    // 3. 创建目标建筑（此时会触发 Kratos TechnoClass_CTOR，
    //    建筑自己的 GameObject/TechnoStatus/AttachEffect 已建好）
    BuildingClass* pBuilding = new BuildingClass(UnitType->DeploysInto, Owner);
    pBuilding->QueueMission(Mission::Construction, 0);
    if (!pBuilding->Put(...))          // -> Kratos Put hook，先给“新建筑管理器”喂了一次 OnPut
        ...失败回滚...

    // 4. 重新指定 vec_Techno 里指向本载具的目标到新建筑
    // 5. 复制 Group / Veterancy / 血量 / 朝向 / Target / 所属 / SlaveManager

    // >>> Kratos Hook 0x739971（EBP = 源载具 this，EBX = 新建筑）<<<
    //     此时：新建筑已创建并 Put；源载具仍然活着（最后才 UnInit）。
    //     这里调用 TechnoStatus::DeploysInto(pBuilding, /*isDeploying=*/true)

    // 6. 复制 StrX_6、Tag/Trigger、播放部署音效
    // 7. this->UnInit(源载具)   —— 源对象在 Hook 之后才被拆除
}
```

### 1.2 原版伪代码收尾段（已修正注释）

```cpp
// 7398E1 起：把源载具的属性补到新建筑上
pBld->Group     = pUnit->Group;
pBld->Veterancy = pUnit->Veterancy;
if (!HouseClass::Is_Player(pBld->Owner)) { pBld->BeingProduced = true; ... }
if (pBld->Type->...转向类旗标...) { ...设置 BarrelFacing... }
pBld->SetTarget(pUnit->Target);
pBld->Techno_198(pUnit->Owner);      // 注册到所属
pBld->field_6DD = 1;                  // “放置完成/可被卖出”标记
...AI 基地处理（ConYard 特有）...

// 血量按比例继承
ratio = ObjectClass::Health_Ratio(pUnit);
pBld->Health = pBld->Type->Strength * ratio;
pBld->BackupHealth = pBld->Health;

if (pUnit->SlaveManager)
    SlaveManagerClass_ReplaceWhichBelongsToUnit(pUnit->SlaveManager, pBld);

// >>> 0x739971 就停在这里（lea eax,[ebp+StrX_6] 的前一条）<<<

// 之后原版还要做：
qmemcpy(&pBld->StrX_6, &pUnit->StrX_6, 0x1C);   // 复制“科技树/语音组”等字符串段
StrX_4060F0(&pUnit->StrX_6, 0, 0);               // 源清空
...
if (Selected) pBld->Select();
(pUnit->Techno_3A0)(ratio);                       // Hex-Rays 无法命名的虚调用
if (pUnit->Tag) { pBld->Attach_Trigger(pUnit->Tag); ... }
...
result = pUnit->UnInit();                         // 源载具最后才 UnInit
```

**时序结论（方向一）**：交换管理器时，源载具和目标建筑“都活着、都在场上”，
Hook 之后源才 `UnInit`。指针层面没有“对象已经没了”的问题。

---

## 2. BuildingClass::Mission_Deconstruction（收起：建筑 → 载具）

### 2.1 修正后的关键流程（只有 MissionStatus==1 + field_6DD + PoweredUnit 的分支）

```cpp
int BuildingClass::Mission_Deconstruction(BuildingClass* this)  // 0x449C30
{
    if (MissionStatus == 1)
    {
        if (field_6DD)                       // 已经放好、可以被“拆回单位”
        {
            if (BuildingType->UndeploysInto 存在 && 各种 MCV 条件满足)
            {
                // 1. 先算 Refund，再创建目标载具
                UnitClass* pUnit = new UnitClass(BuildingType->PoweredUnit, Owner);

                // 2. 收集场上正攻击本建筑的载具（一会儿改指到新载具）
                ...

                // 3. 本建筑先从地图移除 ！！在 Hook 之前！！
                this->Remove();              // 0x449FE7
                //    -> Kratos 0x6F6AC4 Remove hook 会先给建筑组件派发 OnRemove

                // 4. 新载具 Put（在 Hook 之前）
                if (pUnit->Put(...))         // 0x44A002
                {                            // -> Kratos Put hook 先喂新载具 OnPut
                    pUnit->Health = Strength * ratio; ...

                    if (this->SlaveManager)
                        SlaveManagerClass_ReplaceWhichBelongsToUnit(this->SlaveManager, pUnit);

                    // >>> Kratos Hook 0x44A04C（EBP = 建筑 this，EBX = 新载具）<<<
                    //     此时：新载具已 Put；源建筑已被 Remove、还没 UnInit。
                    //     这里调用 TechnoStatus::DeploysInto(pUnit, /*isDeploying=*/false)

                    // 之后原版继续：复制 Tag/Trigger、StrX_6、Group 等
                    // 最后 Object_DC(...)、StrX_DTOR_3(...)、this->UnInit() 拆掉建筑
                }
            }
        }
    }
}
```

**时序结论（方向二）**：Hook 时新旧对象也都在（建筑已 Remove 但未销毁、载具已 Put），
交换指针本身在时序上可行。

但要注意两个方向二特有的“前置减员”：

1. `this->Remove()` 在 Hook 前执行 → `AttachEffect::OnRemove()` 已经跑过，
   **`DiscardOnEntry = true` 的 AE 此刻已经 End**，交换时根本拿不到。
2. 建筑正处在 `Mission_Deconstruction`（出售/拆除任务）中。若 AE 配置了
   `DiscardOnSelling`/`DiscardOnUndeploying`，它们会在 AE 管理器的
   `CheckDurationAndDisable()` 里被清掉（取决于它跑在 Hook 前还是后）。

---

## 3. 两个 Hook 的确切位置（来自反汇编窗口）

### 0x739971（UnitClass::Try_To_Deploy 内）

```asm
73995E  jz      loc_739971        ; 没有 SlaveManager 就跳过
739965  mov     ecx, [ebp+2D8h]   ; this->SlaveManager
73996B  push    ebx               ; newOwner = 新建筑 (EBX)
73996C  call    SlaveManagerClass_ReplaceWhichBelongsToUnit
739971  ; <<<<<<<< Kratos Hook：EBP = 源载具, EBX = 新建筑
739971  lea     eax, [ebp+StrX_6] ; 之后开始把源载具的 StrX_6 拷给建筑
...
739A..  call    this->UnInit      ; 源载具最后拆除
```

### 0x44A04C（BuildingClass::Mission_Deconstruction 内）

```asm
449FE7  call    this->Remove      ; 建筑先移出地图
44A002  call    pUnit->Put        ; 新载具放上地图
...
44A046  push    ebx               ; newOwner = 新载具 (EBX)
44A047  call    SlaveManagerClass_ReplaceWhichBelongsToUnit
44A04C  ; <<<<<<<< Kratos Hook：EBP = 源建筑, EBX = 新载具
44A04C  mov     eax, [ebp+214h]   ; 之后继续拷字段/Tag/StrX
...
44AB..  call    this->Object_DC / StrX_DTOR_3 / UnInit
```

---

## 4. 与旧实现（HEAD 的 TransferAttachedEffects）对比

提交 `0e9de0b`（“允许 DiscardOnTransform=false 的 AE 在 (Un)DeploysInto 时继承”）
在这两个 Hook 上用的是**复制式**转移：

| 项目 | HEAD `TransferAttachedEffects` | 当前工作区 `InheritAE + 整体交换` |
| --- | --- | --- |
| 筛选条件 | `!DiscardOnTransform` 且 `CanAffectType(目标)` | `Inheritable` 且不是 GiftBox/Transform 型 AE |
| 转移到目标 | 在目标管理器上**新建** AttachEffectScript | 把源管理器整棵子树**搬**到目标 GO |
| 剩余时间 | 读取 timeLeft 后写入新脚本 | 脚本原样保留（理论上更精确） |
| 交换后动作 | 只 `RecalculateStatus` + `MarkForRedraw` | 对两个管理器调用 `ExtChanged()`，并把目标 GO 标记 `ExtChanged=true` |

`AttachEffect::ExtChanged()` 的实现（`src/Ext/ObjectType/AttachEffect.h`）：

```cpp
virtual void ExtChanged() override
{
    _typeData = nullptr;
    _groupData = nullptr;
    _attachOnceFlag = false;
    DetachWhenTransform();   // <-- 问题点
}
```

`DetachWhenTransform()`（`src/Ext/ObjectType/AttachEffect.cpp:1000`）：

```cpp
void AttachEffect::DetachWhenTransform()
{
    ForeachChild([](Component* c) {
        auto ae = dynamic_cast<AttachEffectScript*>(c);
        if (ae->AEData.DiscardOnTransform && !ae->AEData.Transform.Enable)
        {
            ae->TimeToDie();
        }
    });
}
```

而 `AttachEffectData` 里：

```cpp
bool DiscardOnTransform = true;   // 默认发生类型改变时失效
```

---

## 5. 最可疑的遗漏点

### 5.1 交换后调用 `ExtChanged()` = 亲手把要继承的 AE 标死

`InheritAE()` 的执行顺序是：

1. 先把 `!Inheritable` / GiftBox / Transform 型 AE `TimeToDie`；
2. `CheckDurationAndDisable(true)` + `ClearDisableComponent()` 清掉它们；
3. 交换两个管理器并换 `_extData`；
4. **紧接着对“搬过去的管理器”调用 `ExtChanged()`**；
5. 又把目标 GO 标成 `ExtChanged=true`（下一帧 `OnForeachEnd` 会再广播一次）。

第 4/5 步的 `ExtChanged()` 会执行 `DetachWhenTransform()`。因为
`DiscardOnTransform` 默认是 `true`，只要 AE 不是“Transform 型 AE”，
它就会被 `TimeToDie()`：

```cpp
void AttachEffectScript::TimeToDie()
{
    _hold = false;
    _immortal = false;
    _lifeTimer.Stop();     // 计时器归零
}
```

下一帧管理器 `OnUpdate()` 里的 `CheckDurationAndDisable()` 会发现它
`!IsAlive()`，于是 `End()` + `Disable()`，然后被 `ClearDisableComponent()` 回收。

**表现正是“管理器确实交换了、子节点列表里好像也有东西，但 AE 没继承/很快消失”。**

旧实现为什么没有这个问题？它**没有**调用目标管理器的 `ExtChanged()`，
只是把允许继承的 AE 重新 `Attach` 到目标管理器上；`ExtChanged/DetachWhenTransform`
是给“同一个对象原地改变类型”（`ChangeTechnoTypeTo`）用的，不适合“换了个新对象”。

> 如果你的测试 AE 明确写了 `DiscardOnTransform=false`，那么 5.1 不会杀掉它；
> 此时请看 5.2/5.3，并用第 6 节的日志确认它到底死在哪个环节。

### 5.2 筛选语义换掉了：Inheritable ≠ DiscardOnTransform

HEAD 版本对 (Un)DeploysInto 的语义是“**DiscardOnTransform=false 才继承**”
（原版“变形”的规则）。新 `InheritAE` 是从 GiftBox 搬来的
“**Inheritable 才继承**”语义，而且**从不检查 `CanAffectType(目标)`**：

- 以前能用 `DiscardOnTransform=false` 控制的 AE，现在完全不看这个字段；
- 以前会被 `CanAffectType` 拦下的、对目标类型无效的 AE，现在也会被整包搬过去，
  之后由运行时各 Effect 自己决定是否生效。

如果测试 AE 两个开关都没写对（例如只写了 `Inheritable=true`，仍保留默认
`DiscardOnTransform=true`），那么 5.1 就会把它清掉；即使你按旧文档只写了
`DiscardOnTransform=false`，新代码里又没有对应的继承入口语义，行为也不一致。

### 5.3 整体交换丢掉了“目标自己的一次性初始化”

交换之前，新目标已经完成过一次 `Put`，它自己管理器的
`_attachStateEffectFlag / _attachOnceFlag / _location / _ownerIsDead`
都是“作为新对象”产生的状态；交换后这些状态跟着旧管理器走：

- `_attachStateEffectFlag` 在源单位 Put 时已经置 true，
  所以搬到建筑后**不会重新**执行 `AttachStateEffect()`（建筑自身的 State 型 AE 会缺）；
- `_ownerIsDead`/`InSelling` 如果已在源（出售中的建筑）上被置 true，
  搬给新载具后，管理器 `OnUpdate` 里 `if (!_ownerIsDead)` 整段会被跳过，
  新载具的 TypeData 段 AE 也不会再补；
- 方向二里建筑在 Hook 前已 `Remove()`，`DiscardOnEntry=true` 的 AE 已 End，
  这一步发生在你交换之前，属于“拿不到”，不是交换失败。

### 5.4 附带回归：GiftBox 现在只有第一份礼物继承

当前工作区把 GiftBox 的“给每份礼物复制 AE”改成了“只对第一份礼物执行一次
`InheritAE` 整包交换”，后续礼物 `inheritAE=false` 后不再继承
（GiftBox.cpp 中原来的 `inheritableAEs` 列表和“所有礼物都需要继承”的修复被删了）。
即使 DeploysInto 修好，这个回归也要一并处理。

---

## 6. 建议的验证日志点位

在 `InheritAE`（`src/Ext/Helper/Gift.cpp:424`）和 `DeploysInto`
（`src/Ext/TechnoType/Status/DeployTo.cpp:177`）里加日志，区分“没搬成”和“搬了被清”：

1. 筛选前：`boxAEM->Count()`，并打印每个 AE 的
   `Name / DiscardOnTransform / DiscardOnEntry / DiscardOnSelling / Inheritable / Transform.Enable / IsAlive`。
2. `CheckDurationAndDisable` 之后、交换之前：`boxAEM->Count()`。
3. 交换完成、`ExtChanged()` 调用之后：再数一次（这一步应能看到 TimeToDie 的 AE）。
4. 下一帧管理器 `OnUpdate`（可在 `AttachEffect::OnUpdate` 加一行只对指定 AE 打印）
   确认是被 `CheckDurationAndDisable` 清掉的，还是根本没活过。

如果日志显示“交换后 Count 不变、下一帧才变 0”，基本就是 5.1；
如果“筛选前就是 0/少”，则是方向二的 `DiscardOnEntry/OnRemove` 或配置问题。

## 7. 修复方向（供参考，未改动源码）

1. 若保留整体交换方案：
   - 交换后**不要**对搬过去的管理器调用会触发 `DetachWhenTransform` 的
     `ExtChanged()`；只重置 `_typeData/_groupData/_attachOnceFlag` 等缓存；
   - 在交换前用原语义（`DiscardOnTransform=false` + `CanAffectType(目标)`）筛选；
   - 对新 owner 补一次等价 `OnPut/AttachStateEffect` 语义，并考虑重置
     `_ownerIsDead/_attachStateEffectFlag/_location`；
   - GiftBox 分支改回“每个礼物各自继承”。
2. 最稳的回退：`DeploysInto` 继续使用 HEAD 的复制式
   `TransferAttachedEffects`（`DiscardOnTransform=false` 才转移、复制剩余时间、
   转移后 `TimeToDie/End` 源脚本），这套实现当初就是为这两个 Hook 写的。
