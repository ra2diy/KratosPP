# KratosPP × Phobos Hook 地址冲突清单

> **本版**：2026-10-04 重新生成（上一版 2026-08-23）
> **数据源**：KratosPP `152f148`（2026-09-11 · 版本号 0.2.3p3）· Phobos `e91cdc20`（2026-10-04 · `v0.5-alpha1-128`）
> **统计口径**：源码中**所有会改写 gamemd 内存的指令** —— `DEFINE_HOOK`、`DEFINE_HOOK_AGAIN`、`DEFINE_JUMP`、`DEFINE_FUNCTION_JUMP`、`DEFINE_DYNAMIC_JUMP`、`DEFINE_NAKED_HOOK`、`DEFINE_PATCH`、`DEFINE_DYNAMIC_PATCH(_TYPED)`。
> **不计入**：`DEFINE_REFERENCE` / `DEFINE_EXPORT`（只做符号引用，不写内存）；被注释停用的 hook（例如 Kratos `Hooks/AnimExtHook.cpp:49` 那批主动停用的）。
> **生成器**：`tools/gen_hook_conflict.py`（§1–§4 的 A/B/C/STATS 表）· `tools/analyze_hook_chain.py`（§10 的 D 表），重新生成方法见 §9。
> **加载 / 执行顺序（实测）**：**Ares → Kratos → Phobos**。同一地址上各 hook 串成一条链依次执行：
> `return 0` 续链，**`return <非 0>` 立即跳转并掐断整条链**（后面的 hook 与原始指令重放全被跳过）。
> 因此「同址」≠「共存」——详见 §10，它决定了本清单里哪些冲突真的会咬人。
>
> **★ 增量更新（2026-10-05，P1 处置）**：Kratos **删除 1 条 Hook**（`0x4DA87A`，见 §7 表 / 报告 §16.1）、
> **改写 1 组实现**（`0x75C7E0`/`0x6A3E50`/`0x5B19D0`/`0x517100`/`0x4B4820` 五处 `In_Which_Layer`
> 改为纯函数，见报告 §16.2）。**同址冲突面因此收窄**：Kratos 现有 Hook 集合里已不含 `0x4DA87A`。
> 本文档 §1–§4 的 A/B/C/STATS 表仍是 `152f148`（Kratos 2026-09-11）的快照，**尚未按新集合重跑**
> `tools/gen_hook_conflict.py`；本次只有"减 1 条"这一种变化，方向上是冲突面**变小**，不影响既有结论。

---

## 0. 本次更新要点

1. **冲突面收窄**：同址 hook × hook 从旧的 **192 处 → 136 个地址 / 140 对**。
   主因是 Phobos 0.5 的扩展系统重构（提交 `2cb961e6 Rework the extension system into a mirror class hierarchy (#2291)`）删掉了绝大部分 `*_SaveLoad_Prefix` / `*_Load_Suffix` / `*_Save_Suffix` hook ——
   §8.2 里消失的 63 对中有 **56 对**属于这一族。**对 Kratos 是好消息**：那批存档 hook 不再与 Phobos 打架。

2. **新增了一个旧版完全没有的维度**：`DEFINE_JUMP` / `DEFINE_PATCH` 这类**静态改写**与 `DEFINE_HOOK` 的冲突。
   旧版只比 hook，漏掉了 8 处（§3 的 B 表）。其中 `0x6D481D` 是**真·覆盖**而非「链式共存」——
   Phobos 在那里写 `LJMP`，Kratos 在同一地址挂 `DEFINE_HOOK(..., 0x7)`，两者都直接写同一段字节。

3. **新出现 6 对同址 hook**，其中三类必须人工处理（详见 §5）：
   - `0x6F9039` —— 「治疗武器决定索敌半径」的**同一功能、两套实现**；
   - `0x6FC749` —— `CanFire` 的「目标在哪一层、这个武器能不能打」判定；
   - `0x4C24C3` / `0x4C25D0` / `0x4C26D5` —— Kratos 源码注释里明写 `// copy from Phobos` 的三处 EBolt 颜色 hook。

4. **Kratos 的 Stand 区（层序 + 排序键 + ZAdjust）在 Phobos 侧已不再有对应实现**（§7）。
   ⚠️ **注意用词**：这不是「Phobos 没有这个功能」——
   **Phobos 的 `Attach Unit` 就是源自 Kratos 的 `Stand`**（经原作者亲自授权；
   且 `KratosPP` 是 `DPKratos`(C#) 的移植，**其 git 日期不可用于判断先后**）。
   准确说法是：**同一功能在 Phobos 侧被保留，但其中的层序 / 排序实现已被删除**。
   与 `docs/联机失同步排查报告.md` §11.7（更正）/ §12.1（源流）结论一致。

5. **SyncLog 埋点是 1:1 复制的**（15 处同名同址）。Kratos 的 `Utilities/SyncLogging.cpp` 整体衍生自 Phobos 的 `Misc/SyncLogging.cpp`，
   只有 `0x6F9B7E` 一处「撞车」：Kratos 拿它做 SyncLog，Phobos 拿它做 AI 空中目标修正（§5.6）。

6. **发现一处「默认开启的接管」**：`0x702299`（残骸数量计算）Kratos 与 Phobos 都实现了同一功能，
   但 Kratos 的开关 `AllowMakeVoxelDebrisByKratos` **默认就是 `true`** —— 即「什么都没配置」时它也在生效，
   会静默顶掉 Phobos 的 `DebrisMaximums` / `DebrisMinimums` 修正。详见 §5.8。

7. **新增「链式执行顺序」维度（§10）**：实测加载 / 执行顺序为 **Ares → Kratos → Phobos**，同址 hook 是**链式**的 ——
   `return 0` 续链，`return <非 0>` 立即跳转并**掐断整条链**。据此逐条判定（工具：`tools/analyze_hook_chain.py`）：
   - **30 处「必定掐断」**：Kratos 的 hook 体内**没有任何 `return 0`**，所有路径都跳走 ⇒ **Phobos 在该地址的 hook 永不执行**；
   - **14 处「条件掐断」**：体内同时存在 `return <非0>` 与 `return 0`，是否掐断取决于运行分支 / 门控开关；
   - **92 处安全**：Kratos 全部 `return 0`，Phobos 可正常执行；
   - **8 处静态改写方向相反**：`DEFINE_JUMP` / `DEFINE_PATCH` 是裸字节写入、不走链，**后加载的 Phobos 覆盖 Kratos**；
   - **Ares 在最前**：它若返回非 0，会**同时掐掉 Kratos 与 Phobos**（无 Ares 源码，无法静态枚举）。

---

## 1. 统计

<!-- AUTO:STATS -->
| 指标 | KratosPP | Phobos |
| --- | --- | --- |
| 内存改写指令合计 | **437** | **1903** |
| `DEFINE_HOOK` + `DEFINE_HOOK_AGAIN` | 430 | 1654 |
| `DEFINE_JUMP` / `_FUNCTION_JUMP` / `_DYNAMIC_JUMP` / `_NAKED_HOOK` | 5 | 203 |
| `DEFINE_PATCH` / `_DYNAMIC_PATCH(_TYPED)` | 2 | 46 |

- 同址 **hook × hook**：**136** 个地址 / **140** 对
- 同址（含 JUMP / PATCH 等其他改写）：另有 **8** 处
- 区间部分重叠（起始不同，真正的「相邻打架」）：**0** 对
<!-- /AUTO:STATS -->

> 与旧版的数字不能直接相减：旧版用「逐行正则」统计，会把被 `/* */` 整体停用的 hook 也算进去，本版口径更严。
> `DEFINE_PATCH` 的长度按参数个数估算（字符串字面量按转义后的长度算），用于区间重叠判断已经足够。

---

## 2. 同址冲突 A 表：`DEFINE_HOOK` × `DEFINE_HOOK`

两边在**同一字节**上各挂一个 Syringe hook。Syringe 允许同址多 hook 链式执行，
但**谁先执行、谁 `return` 后就跳走**是不确定的，因此同址 = 行为不确定。

<!-- AUTO:A -->
| 地址 | Kratos hook | Kratos 位置 | Phobos hook | Phobos 位置 |
| --- | --- | --- | --- | --- |
| 0x004147F9 | AircraftClass_Draw_Shadow_SkipPhobos | Hooks/AircraftExtHook.cpp:62 | AircraftClass_Draw_Shadow | Ext/TechnoType/Hooks.MatrixOp.cpp:927 |
| 0x00418506 | AircraftClass_Mission_Attack_FireDone | Hooks/AircraftExtHook.cpp:451 | AircraftClass_Mission_Attack_Delay1B | Ext/Aircraft/Hooks.cpp:244 |
| 0x004186B6 | AircraftClass_Mission_Attack5_HoverFireTwice | Hooks/AircraftExtHook.cpp:476 | AircraftClass_Mission_Attack_FireAtTarget2_BurstFix | Ext/Aircraft/Hooks.cpp:132 |
| 0x0041A96C | AircraftClass_Mission_GuardArea_NoTarget_Enter | Hooks/AircraftExtHook.cpp:285 | AircraftClass_Mission_AreaGuard | Ext/Aircraft/Hooks.cpp:821 |
| 0x0041AA80 | AircraftClass_AssignDestination_SyncLog | Utilities/SyncLogging.cpp:345 | AircraftClass_AssignDestination_SyncLog | Misc/SyncLogging.cpp:466 |
| 0x0041BB30 | AircraftClass_OverrideMission_SyncLog | Utilities/SyncLogging.cpp:391 | AircraftClass_OverrideMission_SyncLog | Misc/SyncLogging.cpp:512 |
| 0x0041D604 | AirstrikeClass_PointerGotInvalid_ResetForTarget | Hooks/AirstrikeExtHook.cpp:213 | AirstrikeClass_PointerGotInvalid_ResetForTarget | Ext/Techno/Hooks.Airstrike.cpp:126 |
| 0x0041D97B | AirstrikeClass_Setup_SkipBuildingCheck | Hooks/AirstrikeExtHook.cpp:71 | AirstrikeClass_Fire_SetAirstrike | Ext/Techno/Hooks.Airstrike.cpp:34 |
| 0x0041DA52 | AirstrikeClass_ResetTarget_OriginalTarget | Hooks/AirstrikeExtHook.cpp:88 | AirstrikeClass_ResetTarget_OriginalTarget | Ext/Techno/Hooks.Airstrike.cpp:47 |
| 0x0041DA80 | AirstrikeClass_ResetTarget_NewTarget | Hooks/AirstrikeExtHook.cpp:98 | AirstrikeClass_ResetTarget_NewTarget | Ext/Techno/Hooks.Airstrike.cpp:56 |
| 0x0041DAA4 | AirstrikeClass_ResetTarget_ResetForOldTarget | Hooks/AirstrikeExtHook.cpp:108 | AirstrikeClass_ResetTarget_ResetForOldTarget | Ext/Techno/Hooks.Airstrike.cpp:65 |
| 0x0041DAD4 | AirstrikeClass_Reset | Hooks/AirstrikeExtHook.cpp:166 | AirstrikeClass_ResetTarget_ResetForNewTarget | Ext/Techno/Hooks.Airstrike.cpp:76 |
| 0x0041DBD4 | AirstrikeClass_ClearTarget | Hooks/AirstrikeExtHook.cpp:180 | AirstrikeClass_Stop_ResetForTarget | Ext/Techno/Hooks.Airstrike.cpp:88 |
| 0x00422126 | AnimClass_CTOR | Hooks/AnimExtHook.cpp:27 | AnimClass_CTOR_NullType | Ext/Anim/Body.cpp:550 |
| 0x00422967 | AnimClass_DTOR | Hooks/AnimExtHook.cpp:40 | AnimClass_DTOR | Ext/Anim/Body.cpp:570 |
| 0x004242E1 | AnimClass_Trailer_Remap | Hooks/AnimExtHook.cpp:231 | AnimClass_AI_TrailerAnim | Ext/Anim/Hooks.cpp:184 |
| 0x00424807 | AnimClass_Next | Hooks/AnimExtHook.cpp:152 | AnimClass_AI_Next | Ext/Anim/Hooks.cpp:246 |
| 0x0042784B | AnimTypeClass_CTOR | Hooks/AnimTypeExtHook.cpp:10 | AnimTypeClass_CTOR | Ext/AnimType/Body.cpp:257 |
| 0x004287DC | AnimTypeClass_LoadFromINI | Hooks/AnimTypeExtHook.cpp:54 | AnimTypeClass_LoadFromINI | Ext/AnimType/Body.cpp:274 |
| 0x00428EA8 | AnimTypeClass_SDDTOR | Hooks/AnimTypeExtHook.cpp:21 | AnimTypeClass_SDDTOR | Ext/AnimType/Body.cpp:265 |
| 0x0043F9E0 | BuildingClass_Mark_Airstrike | Hooks/AirstrikeExtHook.cpp:434 | BuildingClass_Mark_Airstrike | Ext/Techno/Hooks.Airstrike.cpp:211 |
| 0x00443B90 | BuildingClass_AssignTarget_SyncLog | Utilities/SyncLogging.cpp:318 | BuildingClass_AssignTarget_SyncLog | Misc/SyncLogging.cpp:439 |
| 0x00448DF1 | BuildingClass_SetOwningHouse_Airstrike | Hooks/AirstrikeExtHook.cpp:447 | BuildingClass_SetOwningHouse_Airstrike | Ext/Techno/Hooks.Airstrike.cpp:220 |
| 0x00451ABC | BuildingClass_PlayAnim_Airstrike | Hooks/AirstrikeExtHook.cpp:460 | BuildingClass_PlayAnim_Airstrike | Ext/Techno/Hooks.Airstrike.cpp:229 |
| 0x00452041 | BuildingClass_452000_Airstrike | Hooks/AirstrikeExtHook.cpp:473 | BuildingClass_452000_Airstrike | Ext/Techno/Hooks.Airstrike.cpp:238 |
| 0x00455D50 | BuildingClass_AssignDestination_SyncLog | Utilities/SyncLogging.cpp:356 | BuildingClass_AssignDestination_SyncLog | Misc/SyncLogging.cpp:477 |
| 0x00456E5A | BuildingClass_Flash_Airstrike | Hooks/AirstrikeExtHook.cpp:486 | BuildingClass_Flash_Airstrike | Ext/Techno/Hooks.Airstrike.cpp:247 |
| 0x004664BA | BulletClass_CTOR | Hooks/BulletExtHook.cpp:27 | BulletClass_CTOR | Ext/Bullet/Body.cpp:547 |
| 0x00466556 | BulletClass_Init | Hooks/BulletExtHook.cpp:84 | BulletClass_Init | Ext/Bullet/Hooks.cpp:8 |
| 0x004665E9 | BulletClass_DTOR | Hooks/BulletExtHook.cpp:40 | BulletClass_DTOR | Ext/Bullet/Body.cpp:556 |
| 0x004666F7 | BulletClass_Update | Hooks/BulletExtHook.cpp:112 | BulletClass_AI | Ext/Bullet/Hooks.cpp:53 |
| 0x004666F7 | BulletClass_Update | Hooks/BulletExtHook.cpp:112 | BulletClass_AI_Trajectories | Ext/Bullet/Trajectories/PhobosTrajectory.cpp:436 |
| 0x0046745C | BulletClass_Update_ChangeVelocity | Hooks/BulletExtHook.cpp:294 | BulletClass_AI_Position_Trajectories | Ext/Bullet/Trajectories/PhobosTrajectory.cpp:502 |
| 0x00467E53 | BulletClass_AI_PreDetonation_Vector | Hooks/BulletExtHook.cpp:361 | BulletClass_AI_PreDetonation_Trajectories | Ext/Bullet/Trajectories/PhobosTrajectory.cpp:490 |
| 0x004690C1 | BulletClass_Detonate | Hooks/BulletExtHook.cpp:139 | BulletClass_Logics_DetonateOnAllMapObjects | Ext/Bullet/Hooks.DetonateLogics.cpp:80 |
| 0x00469A75 | BulletClass_Detonate_GetHouse | Hooks/BulletExtHook.cpp:165 | BulletClass_Logics_DamageHouse | Ext/Bullet/Hooks.DetonateLogics.cpp:67 |
| 0x00469C46 | BulletClass_Detonate_WHAnim_Remap | Hooks/BulletExtHook.cpp:184 | BulletClass_Logics_DamageAnimSelected | Ext/Bullet/Hooks.DetonateLogics.cpp:314 |
| 0x0046BDD9 | BulletTypeClass_CTOR | Hooks/BulletTypeExtHook.cpp:10 | BulletTypeClass_CTOR | Ext/BulletType/Body.cpp:224 |
| 0x0046C41C | BulletTypeClass_LoadFromINI | Hooks/BulletTypeExtHook.cpp:56 | BulletTypeClass_LoadFromINI | Ext/BulletType/Body.cpp:241 |
| 0x0046C8B6 | BulletTypeClass_SDDTOR | Hooks/BulletTypeExtHook.cpp:21 | BulletTypeClass_SDDTOR | Ext/BulletType/Body.cpp:233 |
| 0x0048A551 | WarheadTypeClass_AnimList_SplashList | Hooks/WarheadTypeExtHook.cpp:71 | WarheadTypeClass_AnimList_SplashList | Ext/WarheadType/Hooks.cpp:187 |
| 0x004AE95E | DisplayClass_sub_4AE750_DisallowBuildingNonAttackPlanning | Hooks/BuildingExtHook.cpp:90 | DisplayClass_sub_4AE750_DisallowBuildingNonAttackPlanning | Ext/Building/Hooks.cpp:888 |
| 0x004C1E42 | EBolt_CTOR | Hooks/EBoltExtHook.cpp:23 | EBolt_CTOR | Ext/EBolt/Body.cpp:79 |
| 0x004C20BC | EBolt_Draw_Arcs | Hooks/EBoltExtHook.cpp:110 | EBolt_DrawArcs | Ext/EBolt/Hooks.cpp:57 |
| 0x004C24C3 | EBolt_DrawFirst_Color | Hooks/EBoltExtHook.cpp:120 | EBolt_DrawFirst_Color | Ext/EBolt/Hooks.cpp:68 |
| 0x004C25D0 | EBolt_DrawSecond_Color | Hooks/EBoltExtHook.cpp:130 | EBolt_DrawSecond_Color | Ext/EBolt/Hooks.cpp:78 |
| 0x004C26D5 | EBolt_DrawThird_Color | Hooks/EBoltExtHook.cpp:140 | EBolt_DrawThird_Color | Ext/EBolt/Hooks.cpp:88 |
| 0x004C2951 | EBolt_DTOR | Hooks/EBoltExtHook.cpp:31 | EBolt_DTOR | Ext/EBolt/Body.cpp:88 |
| 0x004C9300 | FacingClass_Set_SyncLog | Utilities/SyncLogging.cpp:295 | FacingClass_Set_SyncLog | Misc/SyncLogging.cpp:416 |
| 0x004D8F40 | FootClass_OverrideMission_SyncLog | Utilities/SyncLogging.cpp:402 | FootClass_OverrideMission_SyncLog | Misc/SyncLogging.cpp:523 |
| 0x004DDD66 | FootClass_IsLandZoneClear_ReplaceHardcode | Hooks/AircraftExtHook.cpp:577 | FootClass_IsLandZoneClear_ReplaceHardcode | Ext/Aircraft/Hooks.cpp:532 |
| 0x004F4583 | GScreenClass_Render_Late | Hooks/GScreenHook.cpp:27 | GScreenClass_DrawText | Phobos.cpp:329 |
| 0x004F6532 | HouseClass_CTOR | Hooks/HouseExtHook.cpp:16 | HouseClass_CTOR | Ext/House/Body.cpp:1139 |
| 0x004F7371 | HouseClass_DTOR | Hooks/HouseExtHook.cpp:28 | HouseClass_DTOR | Ext/House/Body.cpp:1149 |
| 0x0050114D | HouseClass_InitFromINI | Hooks/HouseExtHook.cpp:62 | HouseClass_InitFromINI | Ext/House/Body.cpp:1158 |
| 0x005194EF | InfantryClass_DrawIt_InAir_Shadow_Skip | Hooks/InfantryExtHook.cpp:17 | InfantryClass_DrawIt_DrawShadow | Misc/Hooks.BugFixes.cpp:2749 |
| 0x0051AA40 | InfantryClass_AssignDestination_SyncLog | Utilities/SyncLogging.cpp:367 | InfantryClass_AssignDestination_SyncLog | Misc/SyncLogging.cpp:488 |
| 0x0051B1F0 | InfantryClass_AssignTarget_SyncLog | Utilities/SyncLogging.cpp:307 | InfantryClass_AssignTarget_SyncLog | Misc/SyncLogging.cpp:428 |
| 0x0051EAE0 | InfantryClass_WhatAction_Cursor | Hooks/AirstrikeExtHook.cpp:33 | TechnoClass_WhatAction_AllowAirstrike | Ext/Techno/Hooks.Airstrike.cpp:160 |
| 0x0052F639 | YR_CmdLineParse | Hooks/GeneralHook.cpp:45 | _YR_CmdLineParse | Phobos.cpp:289 |
| 0x0054D600 | JumpjetLocomotionClass_MovingUpdate_DontTurnInCell | Hooks/TechnoExtHook.cpp:1137 | JumpjetLocomotionClass_MovementAI_JumpjetStraightAscend | Ext/Unit/Hooks.Jumpjet.cpp:402 |
| 0x00550D1F | LaserDrawClass_DrawInHouseColor_Context_Set | Hooks/LaserDrawHook.cpp:19 | LaserDrawClass_DrawInHouseColor_Context_Set | Misc/Hooks.LaserDraw.cpp:12 |
| 0x00550F47 | LaserDrawClass_DrawInHouseColor_BetterDrawing | Hooks/LaserDrawHook.cpp:26 | LaserDrawClass_DrawInHouseColor_BetterDrawing | Misc/Hooks.LaserDraw.cpp:19 |
| 0x0064736D | Queue_AI_WriteDesyncLog | Utilities/SyncLogging.cpp:245 | Queue_AI_WriteDesyncLog | Misc/SyncLogging.cpp:350 |
| 0x0064CD11 | ExecuteDoList_WriteDesyncLog | Utilities/SyncLogging.cpp:260 | ExecuteDoList_WriteDesyncLog | Misc/SyncLogging.cpp:369 |
| 0x0065C7D0 | Random2Class_Random_SyncLog | Utilities/SyncLogging.cpp:271 | Random2Class_Random_SyncLog | Misc/SyncLogging.cpp:392 |
| 0x0065C88A | Random2Class_RandomRanged_SyncLog | Utilities/SyncLogging.cpp:281 | Random2Class_RandomRanged_SyncLog | Misc/SyncLogging.cpp:402 |
| 0x0065E97F | HouseClass_CreateAirstrike_SetTargetForUnit | Hooks/AirstrikeExtHook.cpp:230 | HouseClass_CreateAirstrike_SetTargetForUnit | Ext/Techno/Hooks.Airstrike.cpp:138 |
| 0x0065E997 | Airstrike_Supported_Reinforcements_Put | Hooks/AirstrikeExtHook.cpp:263 | HouseClass_SendAirstrike_PlaceAircraft | Ext/House/Hooks.cpp:330 |
| 0x0067CEF0 | SaveGame_Start | Hooks/SaveGameHook.cpp:29 | ScenarioClass_SaveGame_AdjustMPSaveFileName | Phobos.Save.cpp:128 |
| 0x0067D04E | Game_Save_SavegameInformation | Extension.cpp:251 | GameSave_SavegameInformation | Phobos.Ext.cpp:406 |
| 0x0067D32C | SaveGame_Ext | Extension.cpp:235 | SaveGame_Phobos | Phobos.Ext.cpp:361 |
| 0x0067E826 | LoadGame_Ext | Extension.cpp:243 | LoadGame_Phobos | Phobos.Ext.cpp:371 |
| 0x0067FD9D | LoadOptionsClass_GetFileInfo | Extension.cpp:258 | LoadOptionsClass_GetFileInfo | Phobos.Ext.cpp:426 |
| 0x0067FDB1 | LoadOptionsClass_GetFileInfo | Extension.cpp:259 | LoadOptionsClass_GetFileInfo | Phobos.Ext.cpp:427 |
| 0x00685659 | Scenario_ClearClasses_End | Hooks/GeneralHook.cpp:88 | Scenario_ClearClasses | Phobos.Ext.cpp:351 |
| 0x006CE6F6 | SuperWeaponTypeClass_CTOR | Hooks/SuperWeaponTypeHook.cpp:10 | SuperWeaponTypeClass_CTOR | Ext/SWType/Body.cpp:359 |
| 0x006CEE43 | SuperWeaponTypeClass_LoadFromINI | Hooks/SuperWeaponTypeHook.cpp:53 | SuperWeaponTypeClass_LoadFromINI | Ext/SWType/Body.cpp:377 |
| 0x006CEFE0 | SuperWeaponTypeClass_SDDTOR | Hooks/SuperWeaponTypeHook.cpp:21 | SuperWeaponTypeClass_SDDTOR | Ext/SWType/Body.cpp:368 |
| 0x006F348F | TechnoClass_WhatWeaponShouldIUse_Airstrike | Hooks/AirstrikeExtHook.cpp:39 | TechnoClass_WhatWeaponShouldIUse_Airstrike | Ext/Techno/Hooks.Airstrike.cpp:8 |
| 0x006F36DB | TechnoClass_SelectWeapon | Hooks/TechnoExtHook.cpp:774 | TechnoClass_WhatWeaponShouldIUse | Ext/Techno/Hooks.Firing.cpp:121 |
| 0x006F4500 | TechnoClass_DTOR | Hooks/TechnoExtHook.cpp:49 | TechnoClass_DTOR | Ext/Techno/Body.cpp:1262 |
| 0x006F65D1 | TechnoClass_DrawHealthBar_Building | Hooks/TechnoExtHook.cpp:447 | TechnoClass_DrawHealthBar_Buildings | Ext/Techno/Hooks.Pips.cpp:74 |
| 0x006F683C | TechnoClass_DrawHealthBar_Other | Hooks/TechnoExtHook.cpp:462 | TechnoClass_DrawHealthBar_Units | Ext/Techno/Hooks.Pips.cpp:100 |
| 0x006F6AC4 | TechnoClass_Remove | Hooks/TechnoExtHook.cpp:123 | TechnoClass_Limbo | Ext/Techno/Hooks.cpp:384 |
| 0x006F9039 | TechnoClass_Greatest_Threat_HealWeaponRange | Hooks/TechnoExtHook.cpp:528 | TechnoClass_SelectAutoTarget_HealGuardRange | Ext/Techno/Hooks.Targeting.cpp:94 |
| 0x006F9B7E | TechnoClass_SelectAutoTarget_SyncLog | Utilities/SyncLogging.cpp:426 | TechnoClass_SelectAutoTarget_AIAirTargetingFix1 | Misc/Hooks.BugFixes.cpp:3074 |
| 0x006F9E50 | TechnoClass_Update | Hooks/TechnoExtHook.cpp:136 | TechnoClass_AI | Ext/Techno/Hooks.cpp:30 |
| 0x006FC339 | TechnoClass_CanFire | Hooks/TechnoExtHook.cpp:402 | TechnoClass_CanFire | Ext/Techno/Hooks.Firing.cpp:313 |
| 0x006FC749 | TechnoClass_CanFire_WhichLayer_Stand | Hooks/StandExtHook.cpp:93 | TechnoClass_CanFire_AntiUnderground | Ext/Techno/Hooks.Firing.cpp:540 |
| 0x006FCDB0 | TechnoClass_AssignTarget_SyncLog | Utilities/SyncLogging.cpp:329 | TechnoClass_AssignTarget_SyncLog | Misc/SyncLogging.cpp:450 |
| 0x006FD38D | TechnoClass_DrawSth_DrawToInvisoFlakScatterLocation | Hooks/WeaponExtHook.cpp:105 | TechnoClass_DrawSth_DrawToInvisoFlakScatterLocation | Ext/Techno/Hooks.WeaponEffects.cpp:222 |
| 0x006FD446 | TechnoClass_LaserZap_IsSingleColor | Hooks/WeaponExtHook.cpp:34 | TechnoClass_LaserZap_IsSingleColor | Ext/Techno/Hooks.WeaponEffects.cpp:290 |
| 0x006FD446 | TechnoClass_LaserZap_IsSingleColor | Hooks/WeaponExtHook.cpp:34 | TechnoClass_LaserZap_Tracking | Misc/Hooks.LaserDraw.cpp:291 |
| 0x006FD494 | TechnoClass_FireEBolt_SetWeaponData | Hooks/EBoltExtHook.cpp:44 | TechnoClass_FireEBolt_SetExtMap_AfterAres | Ext/EBolt/Hooks.cpp:17 |
| 0x006FD514 | TechnoClass_DrawSth_DrawToInvisoFlakScatterLocation | Hooks/WeaponExtHook.cpp:104 | TechnoClass_DrawSth_DrawToInvisoFlakScatterLocation | Ext/Techno/Hooks.WeaponEffects.cpp:221 |
| 0x006FD70D | TechnoClass_DrawSth_DrawToInvisoFlakScatterLocation | Hooks/WeaponExtHook.cpp:103 | TechnoClass_DrawSth_DrawToInvisoFlakScatterLocation | Ext/Techno/Hooks.WeaponEffects.cpp:220 |
| 0x006FF08B | TechnoClass_Fire_RecordBullet | Hooks/WeaponExtHook.cpp:66 | TechnoClass_Fire_RecordBullet | Ext/Techno/Hooks.WeaponEffects.cpp:20 |
| 0x006FF15F | TechnoClass_FireAt_ObstacleCellSet | Hooks/WeaponExtHook.cpp:86 | TechnoClass_FireAt_ObstacleCellSet | Ext/Techno/Hooks.WeaponEffects.cpp:28 |
| 0x006FF29E | TechnoClass_Fire_ROFMultiplier | Hooks/TechnoExtHook.cpp:995 | TechnoClass_FireAt_ChargeTurret2 | Ext/Techno/Hooks.Firing.cpp:984 |
| 0x006FF43F | TechnoClass_FireAt_TargetSet | Hooks/WeaponExtHook.cpp:134 | TechnoClass_FireAt_FeedbackWeapon | Ext/Techno/Hooks.Firing.cpp:818 |
| 0x006FF43F | TechnoClass_FireAt_TargetSet | Hooks/WeaponExtHook.cpp:134 | TechnoClass_FireAt_TargetSet | Ext/Techno/Hooks.WeaponEffects.cpp:252 |
| 0x006FF660 | TechnoClass_FireAt_ObstacleCellUnset | Hooks/WeaponExtHook.cpp:153 | TechnoClass_FireAt_LateLogic | Ext/Techno/Hooks.Firing.cpp:882 |
| 0x006FF660 | TechnoClass_FireAt_ObstacleCellUnset | Hooks/WeaponExtHook.cpp:153 | TechnoClass_FireAt_ObstacleCellUnset | Ext/Techno/Hooks.WeaponEffects.cpp:271 |
| 0x007013A0 | TechnoClass_OverrideMission_SyncLog | Utilities/SyncLogging.cpp:413 | TechnoClass_OverrideMission_SyncLog | Misc/SyncLogging.cpp:534 |
| 0x00701900 | TechnoClass_ReceiveDamage | Hooks/TechnoExtHook.cpp:217 | TechnoClass_ReceiveDamage_Shield | Ext/Techno/Hooks.ReceiveDamage.cpp:15 |
| 0x007019D8 | TechnoClass_ReceiveDamage_At_Least1 | Hooks/TechnoExtHook.cpp:242 | TechnoClass_ReceiveDamage_SkipLowDamageCheck | Ext/Techno/Hooks.ReceiveDamage.cpp:195 |
| 0x00701DFF | TechnoClass_ReceiveDamageEnd | Hooks/TechnoExtHook.cpp:350 | TechnoClass_ReceiveDamage_FlyingStrings | Ext/Techno/Hooks.ReceiveDamage.cpp:226 |
| 0x00702050 | TechnoClass_ReceiveDamage_Destroy | Hooks/TechnoExtHook.cpp:368 | TechnoClass_ReceiveDamage_AttachEffectExpireWeapon | Ext/Techno/Hooks.ReceiveDamage.cpp:348 |
| 0x00702299 | TechnoClass_Destroy_VxlDebris_Remap | Hooks/TechnoExtHook.cpp:692 | TechnoClass_ReceiveDamage_Debris | Misc/Hooks.BugFixes.cpp:137 |
| 0x00705860 | TechnoClass_DrawAirstrikeFlare_SetContext | Hooks/AirstrikeExtHook.cpp:370 | TechnoClass_DrawAirstrikeFlare_SetContext | Ext/Techno/Hooks.Airstrike.cpp:283 |
| 0x007058F6 | TechnoClass_DrawAirstrikeFlare_LineColor | Hooks/AirstrikeExtHook.cpp:380 | TechnoClass_DrawAirstrikeFlare_LineColor | Ext/Techno/Hooks.Airstrike.cpp:293 |
| 0x0070597A | TechnoClass_DrawAirstrikeFlare_DotColor | Hooks/AirstrikeExtHook.cpp:408 | TechnoClass_DrawAirstrikeFlare_DotColor | Ext/Techno/Hooks.Airstrike.cpp:317 |
| 0x0070E92F | TechnoClass_Update_Airstrike_Tint_Timer | Hooks/AirstrikeExtHook.cpp:421 | TechnoClass_UpdateAirstrikeTint | Ext/Techno/Hooks.Airstrike.cpp:187 |
| 0x00711835 | TechnoTypeClass_CTOR | Hooks/TechnoTypeExtHook.cpp:10 | TechnoTypeClass_CTOR | Ext/TechnoType/Body.cpp:1915 |
| 0x0071A88D | TemporalClass_Update | Hooks/TechnoExtHook.cpp:181 | TemporalClass_AI | Ext/Techno/Hooks.cpp:85 |
| 0x0071BB2C | TerrainClass_TakeDamage_NowDead_Add | Hooks/TerrainExtHook.cpp:14 | TerrainClass_TakeDamage_NowDead_Add | Ext/TerrainType/Hooks.cpp:286 |
| 0x0071DBC0 | TerrainTypeClass_CTOR | Hooks/TerrainTypeExtHook.cpp:10 | TerrainTypeClass_CTOR | Ext/TerrainType/Body.cpp:129 |
| 0x0071E0A6 | TerrainTypeClass_LoadFromINI | Hooks/TerrainTypeExtHook.cpp:59 | TerrainTypeClass_LoadFromINI | Ext/TerrainType/Body.cpp:150 |
| 0x0071E364 | TerrainTypeClass_SDDTOR | Hooks/TerrainTypeExtHook.cpp:25 | TerrainTypeClass_SDDTOR | Ext/TerrainType/Body.cpp:141 |
| 0x007258D0 | DetachThisFromAll | Hooks/PointerExpireHook.cpp:41 | AnnounceInvalidPointer | Phobos.Ext.cpp:334 |
| 0x0073C47A | UnitClass_DrawAsVXL_Shadow_SkipPhobos | Hooks/AircraftExtHook.cpp:42 | UnitClass_DrawAsVXL_Shadow | Ext/TechnoType/Hooks.MatrixOp.cpp:648 |
| 0x00741970 | UnitClass_AssignDestination_SyncLog | Utilities/SyncLogging.cpp:378 | UnitClass_AssignDestination_SyncLog | Misc/SyncLogging.cpp:499 |
| 0x0074942E | VoxelAnimClass_CTOR | Hooks/VoxelAnimExtHook.cpp:15 | VoxelAnimClass_CTOR | Ext/VoxelAnim/Body.cpp:67 |
| 0x007499F1 | VoxelAnimClass_DTOR | Hooks/VoxelAnimExtHook.cpp:24 | VoxelAnimClass_DTOR | Ext/VoxelAnim/Body.cpp:77 |
| 0x0074AEB0 | VoxelAnimTypeClass_CTOR | Hooks/VoxelAnimTypeExtHook.cpp:10 | VoxelAnimTypeClass_CTOR | Ext/VoxelAnimType/Body.cpp:70 |
| 0x0074B4F0 | VoxelAnimTypeClass_LoadFromINI | Hooks/VoxelAnimTypeExtHook.cpp:56 | VoxelAnimTypeClass_LoadFromINI | Ext/VoxelAnimType/Body.cpp:91 |
| 0x0074B51B | VoxelAnimTypeClass_LoadFromINI | Hooks/VoxelAnimTypeExtHook.cpp:55 | VoxelAnimTypeClass_LoadFromINI | Ext/VoxelAnimType/Body.cpp:90 |
| 0x0074B54A | VoxelAnimTypeClass_LoadFromINI | Hooks/VoxelAnimTypeExtHook.cpp:54 | VoxelAnimTypeClass_LoadFromINI | Ext/VoxelAnimType/Body.cpp:89 |
| 0x0074B561 | VoxelAnimTypeClass_LoadFromINI | Hooks/VoxelAnimTypeExtHook.cpp:53 | VoxelAnimTypeClass_LoadFromINI | Ext/VoxelAnimType/Body.cpp:88 |
| 0x0074B607 | VoxelAnimTypeClass_LoadFromINI | Hooks/VoxelAnimTypeExtHook.cpp:52 | VoxelAnimTypeClass_LoadFromINI | Ext/VoxelAnimType/Body.cpp:87 |
| 0x0074BA31 | VoxelAnimTypeClass_DTOR | Hooks/VoxelAnimTypeExtHook.cpp:21 | VoxelAnimTypeClass_DTOR | Ext/VoxelAnimType/Body.cpp:79 |
| 0x0075D1A9 | WarheadTypeClass_CTOR | Hooks/WarheadTypeExtHook.cpp:12 | WarheadTypeClass_CTOR | Ext/WarheadType/Body.cpp:876 |
| 0x0075DEA0 | WarheadTypeClass_LoadFromINI | Hooks/WarheadTypeExtHook.cpp:58 | WarheadTypeClass_LoadFromINI | Ext/WarheadType/Body.cpp:895 |
| 0x0075E5C8 | WarheadTypeClass_SDDTOR | Hooks/WarheadTypeExtHook.cpp:23 | WarheadTypeClass_SDDTOR | Ext/WarheadType/Body.cpp:885 |
| 0x00771EE9 | WeaponTypeClass_CTOR | Hooks/WeaponTypeExtHook.cpp:10 | WeaponTypeClass_CTOR | Ext/WeaponType/Body.cpp:536 |
| 0x007729B0 | WeaponTypeClass_LoadFromINI | Hooks/WeaponTypeExtHook.cpp:57 | WeaponTypeClass_LoadFromINI | Ext/WeaponType/Body.cpp:556 |
| 0x007729C7 | WeaponTypeClass_LoadFromINI | Hooks/WeaponTypeExtHook.cpp:55 | WeaponTypeClass_LoadFromINI | Ext/WeaponType/Body.cpp:555 |
| 0x0077311D | WeaponTypeClass_SDDTOR | Hooks/WeaponTypeExtHook.cpp:21 | WeaponTypeClass_SDDTOR | Ext/WeaponType/Body.cpp:545 |
| 0x007CD810 | ExeRun | Hooks/GeneralHook.cpp:57 | ExeRun | Phobos.cpp:265 |
<!-- /AUTO:A -->

---

## 3. 同址冲突 B 表：含 `JUMP` / `PATCH` 等其他内存改写

这一类**比 A 表危险**：`DEFINE_JUMP` / `DEFINE_PATCH` 是**静态字节写入**，
不会和 Syringe 的 trampoline 协商，直接覆盖同一段内存 —— 两边会**互相破坏**。

> 表中「位置」列是**各自仓库 `src/` 下的相对路径**。例如最后两行的 `Utilities/Debug.cpp` 分别指
> `KratosPP/src/Utilities/Debug.cpp` 与 `Phobos/src/Utilities/Debug.cpp` —— 目录同名纯属巧合，不是同一个文件。

<!-- AUTO:B -->
| 地址 | Kratos 指令 | Kratos 位置 | Phobos 指令 | Phobos 位置 |
| --- | --- | --- | --- | --- |
| 0x004C24BE | DEFINE_JUMP -> 0x4C24C3 | Hooks/EBoltExtHook.cpp:119 | DEFINE_JUMP -> 0x4C24C3 | Ext/EBolt/Hooks.cpp:67 |
| 0x004C25CB | DEFINE_JUMP -> 0x4C25D0 | Hooks/EBoltExtHook.cpp:129 | DEFINE_JUMP -> 0x4C25D0 | Ext/EBolt/Hooks.cpp:77 |
| 0x004C26CF | DEFINE_JUMP -> 0x4C26D5 | Hooks/EBoltExtHook.cpp:139 | DEFINE_JUMP -> 0x4C26D5 | Ext/EBolt/Hooks.cpp:87 |
| 0x006D481D | DEFINE_HOOK -> TacticalClass_Draw_AirstrikeLaser_SkipBuildingCheck | Hooks/AirstrikeExtHook.cpp:303 | DEFINE_JUMP -> 0x6D482D | Ext/Techno/Hooks.Airstrike.cpp:6 |
| 0x007CD8EA | DEFINE_JUMP -> GET_OFFSET(_ExeTerminate) | Hooks/GeneralHook.cpp:79 | DEFINE_NAKED_HOOK -> _ExeTerminate | Phobos.cpp:274 |
| 0x007E4610 | DEFINE_FUNCTION_JUMP -> BuildingTypeClass_CanUseWaypoint | Hooks/BuildingExtHook.cpp:88 | DEFINE_FUNCTION_JUMP -> BuildingTypeClass_CanUseWaypoint | Ext/Building/Hooks.cpp:886 |
| 0x00825F9B | DEFINE_PATCH -> DEFINE_PATCH | Utilities/Debug.cpp:97 | DEFINE_PATCH -> DEFINE_PATCH | Utilities/Debug.cpp:90 |
| 0x008332F4 | DEFINE_PATCH -> DEFINE_PATCH | Utilities/Debug.cpp:102 | DEFINE_PATCH -> DEFINE_PATCH | Utilities/Debug.cpp:95 |
<!-- /AUTO:B -->

---

## 4. 区间部分重叠 C 表（起始不同）

起始地址不同、但 `[addr, addr+size)` 相交 —— 即「相邻打架」。

<!-- AUTO:C -->
| Kratos 地址 | Kratos 区间 | Kratos hook | Kratos 位置 | Phobos 地址 | Phobos 区间 | Phobos hook | Phobos 位置 |
| --- | --- | --- | --- | --- | --- | --- | --- |
<!-- /AUTO:C -->

> 本次扫描结果为 **0 对**：两边都没有出现「A 的 hook 长度吃掉 B 的 hook 起点」的情况。
> 注意 `0x4C24BE → 0x4C24C3` 这种「5 字节 LJMP 正好指向下一条指令」的写法不计入重叠（它是**故意**的定点改写，见 §5.4）。

---

## 5. 必须人工处理的「同址但语义冲突」

### 5.1 `0x6F9039` —— 治疗武器 → 索敌半径（同功能、两套实现）

| | Kratos | Phobos |
| --- | --- | --- |
| hook 名 | `TechnoClass_Greatest_Threat_HealWeaponRange` | `TechnoClass_SelectAutoTarget_HealGuardRange` |
| 位置 | `src/Hooks/TechnoExtHook.cpp:528` | `src/Ext/Techno/Hooks.Targeting.cpp:94` |
| 尺寸 | `0x5` | `0x5` |
| 写入 | `R->EDI(...)` → `return 0x6F903E` | `R->EDI(...)` → `return 0x6F903E` |
| 取值 | `max(GuardRange, 炮塔武器 Range, 副武器 Range)` | 先找**真正的治疗武器**（`Damage + AmbientDamage < 0`），再 `max(512, 治疗武器 Range + LeptonsPerCell)` |

**冲突性质**：同址、同尺寸、同语义、**不同实现**，而且两边都写 `EDI`、都跳 `0x6F903E`。

**行为差异**：
- Phobos 的版本只在找到治疗武器时才扩大半径，扩大量 = 治疗武器射程 + 1 格；
- Kratos 的版本**不检查武器是不是治疗武器**，直接取「主武器/副武器的较大射程」。
  在这条被 `Mission::Guard` + 负伤害武器门控的路径上，Kratos 的取值与 Phobos 并不等价。

**建议**：Kratos 侧检测到 Phobos 已注册时让路（或不重复实现），否则应改用 Phobos 的判定逻辑。
**注意**：这条路径**不参与**《联机失同步排查报告》里那次「无标签 + 地面步兵 + `Ctrl+Shift`」的复现
—— 它要求**负伤害武器 + `Mission::Guard(5)`**，普通步兵的 `AttackMove(29)` 走不到。

---

### 5.2 `0x6D481D` —— Phobos 写 `LJMP`，Kratos 挂 `HOOK`（**真·覆盖**）

```cpp
// Phobos  src/Ext/Techno/Hooks.Airstrike.cpp:6      —— 静态写入 5 字节 jmp
DEFINE_JUMP(LJMP, 0x6D481D, 0x6D482D)   // Allow airstrike flare draw to foot

// Kratos  src/Hooks/AirstrikeExtHook.cpp:303        —— Syringe trampoline，占 7 字节
DEFINE_HOOK(0x6D481D, TacticalClass_Draw_AirstrikeLaser_SkipBuildingCheck, 0x7)
{ enum { draw = 0x6D482D, skip = 0x6D48FA }; ... }
```

**这是本清单里最危险的一条**：不是「链式共存」，而是**两个写入者抢同一段字节**。
谁最后写谁生效，另一个被静默抹掉 —— 而且因为两边都不报错，只会在运行期表现为「某个功能莫名失效」。
Kratos 源码里那句注释（`为了兼容Phobos，只能使用Phobos的方式控制线条和点的颜色`）说明作者已经意识到这里和 Phobos 有耦合。

**建议**：Kratos 不要在同址另挂 hook。若必须定制，应改为在 `0x6D482D` 之后的分支上做，或提供「检测到 Phobos 时放弃本 hook」的开关。

---

### 5.3 `0x6FC749` —— `CanFire` 的「目标在哪一层、能不能打」

| | Kratos | Phobos |
| --- | --- | --- |
| hook 名 | `TechnoClass_CanFire_WhichLayer_Stand` | `TechnoClass_CanFire_AntiUnderground` |
| 位置 | `src/Hooks/StandExtHook.cpp:93` | `src/Ext/Techno/Hooks.Firing.cpp:540` |
| 尺寸 | `0x5` | `0x5` |
| 输入 | `GET(Layer, layer, EAX)` | `GET(Layer, layer, EAX)` + `GET(WeaponTypeClass*, pWeapon, EDI)` |
| 分支 | `layer != Ground` 且目标是替身 → `inAir(0x6FC74E)` / `onGround(0x6FC762)` | Air/Top 要求 `Projectile->AA`、Underground 要求 `AU`，否则 `Illegal(0x6FC86A)`；否则 `GoOtherChecks(0x6FC762)` |

**冲突性质**：两边都在**同一条「目标层级 → 该武器能否开火」的判定**上做决策，且都用 `0x6FC762` 作为「继续走原逻辑」的跳点。
区别是 Phobos 多一条 `Illegal = 0x6FC86A`（直接判定不可开火），而 Kratos 会跳到 `0x6FC74E`（回到 AA 检查）。

**影响**：这是**逻辑路径**（不是渲染），一旦两者都在场，谁先执行将决定「对空单位能不能打地面替身」这类结果；
若还要与联机同步挂钩，风险更高。

**建议**：合并实现，或明确让 Phobos 的 `AntiUnderground` 优先、Kratos 只在其「未命中」时接管。

---

### 5.4 `0x4C24C3` / `0x4C25D0` / `0x4C26D5` —— Kratos 照抄 Phobos 的 EBolt 颜色

Kratos 源码原文（`src/Hooks/EBoltExtHook.cpp:119-139`）：

```cpp
DEFINE_JUMP(LJMP, 0x4C24BE, 0x4C24C3)// Disable Ares's hook EBolt_Draw_Color1
DEFINE_HOOK(0x4C24C3, EBolt_DrawFirst_Color, 0x9)// copy from Phobos
DEFINE_JUMP(LJMP, 0x4C25CB, 0x4C25D0)// Disable Ares's hook EBolt_Draw_Color2
DEFINE_HOOK(0x4C25D0, EBolt_DrawSecond_Color, 0x6)// copy from Phobos
DEFINE_JUMP(LJMP, 0x4C26CF, 0x4C26D5)// Disable Ares's hook EBolt_Draw_Color3
DEFINE_HOOK(0x4C26D5, EBolt_DrawThird_Color, 0x6)// copy from Phobos
```

Phobos 侧（`src/Ext/EBolt/Hooks.cpp:67-88`）是**逐字相同**的写法。

**冲突性质**：**有意重复**。连「用 `LJMP` 把 Ares 的 5 字节 hook 抹掉」的手法都照搬了。
两边的 `0x4C24BE` / `0x4C25CB` / `0x4C26CF` 写的是同样的 `LJMP`（幂等，无害）；
但 `0x4C24C3` / `0x4C25D0` / `0x4C26D5` 上会**各挂一个 hook**，彼此读同一个 `BoltTemp` 状态、跑两遍。

**建议**：这类「照抄」应加注释标明来源与版本，避免将来 Phobos 改动后两边行为分叉。

---

### 5.5 `0x7CD8EA` / `0x7E4610` —— 同目标跳转

| 地址 | Kratos | Phobos | 说明 |
| --- | --- | --- | --- |
| `0x7CD8EA` | `DEFINE_JUMP(LJMP, ..., GET_OFFSET(_ExeTerminate))`<br>`src/Hooks/GeneralHook.cpp:79` | `DEFINE_NAKED_HOOK(_ExeTerminate)`<br>`src/Phobos.cpp:274` | 都把退出流程接管到自己的函数。**两边都接管 = 只有一个能生效**。 |
| `0x7E4610` | `DEFINE_FUNCTION_JUMP(VTABLE, ..., BuildingTypeClass_CanUseWaypoint)`<br>`src/Hooks/BuildingExtHook.cpp:88` | 同名同址<br>`src/Ext/Building/Hooks.cpp:886` | 改同一个 vtable 槽。同名同址，属于照抄。 |

---

### 5.6 `0x6F9B7E` —— Kratos 的 SyncLog 撞上 Phobos 的 AI 修正

| | Kratos | Phobos |
| --- | --- | --- |
| hook 名 | `TechnoClass_SelectAutoTarget_SyncLog` | `TechnoClass_SelectAutoTarget_AIAirTargetingFix1` |
| 位置 | `src/Utilities/SyncLogging.cpp:426` | `src/Misc/Hooks.BugFixes.cpp:3074` |

Kratos 的这个 hook 只在 `-SYNCLOG` 诊断模式下启用，Phobos 的是**常驻的 AI 修正**。
两者同址：**开启 SyncLog 时会与 Phobos 的修正互相干扰**。
诊断时请优先用「只加载 Kratos + `-SYNCLOG`」，或在开启 SyncLog 前先确认 Phobos 版本。

---

### 5.7 SyncLog 埋点：15 处同名同址重复

Kratos `Utilities/SyncLogging.cpp` 与 Phobos `Misc/SyncLogging.cpp` 有 **15 处同名同址**的埋点：

```
0x41AA80 AircraftClass_AssignDestination   0x4D8F40 FootClass_OverrideMission
0x41BB30 AircraftClass_OverrideMission     0x51AA40 InfantryClass_AssignDestination
0x443B90 BuildingClass_AssignTarget        0x51B1F0 InfantryClass_AssignTarget
0x455D50 BuildingClass_AssignDestination   0x64736D Queue_AI_WriteDesyncLog
0x4C9300 FacingClass_Set                   0x64CD11 ExecuteDoList_WriteDesyncLog
0x65C7D0 Random2Class_Random               0x6FCDB0 TechnoClass_AssignTarget
0x65C88A Random2Class_RandomRanged         0x7013A0 TechnoClass_OverrideMission
                                           0x741970 UnitClass_AssignDestination
```

这些是**诊断埋点**（只在开日志时写入），正常游戏不影响逻辑；
但两边同时开启会**双份写日志**。除此之外唯一「语义不同」的是 §5.6 的 `0x6F9B7E`。

---

### 5.8 `0x702299` —— 残骸数量计算（Kratos **默认开启**的接管）

| | Kratos | Phobos |
| --- | --- | --- |
| hook 名 | `TechnoClass_Destroy_VxlDebris_Remap` | `TechnoClass_ReceiveDamage_Debris` |
| 位置 | `src/Hooks/TechnoExtHook.cpp:692` | `src/Misc/Hooks.BugFixes.cpp:137` |
| 尺寸 | `0xA` | `0xA` |
| 门控 | `AudioVisual::Data()->AllowMakeVoxelDebrisByKratos`，**默认 `true`**（`src/Ext/Common/CommonStatus.h:108`） | 无（常驻） |
| 随机数 | `Random::RandomRanged(MinDebris, MaxDebris)` → `ScenarioClass::Instance->Random.RandomRanged(...)`（`src/Ext/Helper/MathEx.h:18`） | `ScenarioClass::Instance->Random.RandomRanged(MinDebris, MaxDebris)` |
| 收尾 | `times > 0` → `R->EBX(times); return 0x7023E5;`；`times == 0` → `return 0`（回落原版） | `totalSpawnAmount > 0` → 自实现 `DebrisMaximums` / `DebrisMinimums` / `DebrisTypes_Limit` 逻辑 |

Kratos 源码里已留了提醒注释：

```cpp
// Phobos hook 这个地址，要自己算随机数
```

**为什么危险**：
- 两边门控**不对称** —— Kratos 的开关**默认就是开的**，所以「什么都没配置」时它也会接管 `0x702299`；
- 两边的返回都是「非零即跳走」，而 Syringe 链式执行里**第一个返回非零的 hook 就终结整条链**；
  于是谁先执行，另一个的残骸修正就被静默跳过（Phobos 的 `DebrisMinimums` / `DebrisTypes_Limit` 失效，或反之）；
- 两边**语义并不等价**：Phobos 那版是复刻并扩写过的「残骸类型 / 上限 / 下限」完整逻辑，Kratos 那版只按 `DebrisMaximums` 铺开。

**与联机同步的关系**：两边消耗的共享随机数都取自 `ScenarioClass::Instance->Random`，
且都以「一次 `RandomRanged(MinDebris, MaxDebris)`」起步 —— 因此**不会**造成「两端消耗次数不同」这一类失同步。
但它是**无标签基线下默认生效**的 hook，排查失同步时不要漏掉它（见 `docs/联机失同步排查报告.md` §10.5）。

**建议**：二选一。Kratos 侧至少在检测到 Phobos 时把该开关默认值改为 `false`，或直接复用 Phobos 的实现。

---

## 6. Phobos 0.5 带来的 63 对「消失」

| 类别 | 数量 | 原因 |
| --- | --- | --- |
| `*_SaveLoad_Prefix` / `*_Load_Suffix` / `*_Save_Suffix` | **56** | Phobos 提交 `2cb961e6` 重构扩展系统，删除了各类型自己的存档 hook；只在 `Cell` / `Rules` / `Scenario` / `Sidebar` 保留了少数几个 |
| 其余 7 对 | 7 | Phobos 侧把 hook 换了地址或换了宏，见下表 |

其余 7 对（Phobos 已不再改写该地址）：

| 地址 | Kratos hook | Phobos（旧） | 说明 |
| --- | --- | --- | --- |
| `0x41DAD4` | `AirstrikeClass_ResetTarget_ResetForNewTarget` | 同名 | Phobos 侧已移除 |
| `0x4228D2` | `AnimClass_CTOR` | `AnimClass_CTOR_Load` | 扩展系统重构后不再需要 |
| `0x426598` | `AnimClass_SDDTOR` | 同名 | 同上 |
| `0x6F3260` | `TechnoClass_CTOR` | 同名 | 同上 |
| `0x6FF08B` | `TechnoClass_Fire_RecordBullet` | 同名 | **Kratos 侧同时删掉了自己的 `TechnoClass_Fire_SyncLog`**，所以少了一对 |
| `0x711AE0` | `TechnoTypeClass_DTOR` | 同名 | 扩展系统重构后不再需要 |
| `0x716123` | `TechnoTypeClass_LoadFromINI` | 同名 | 同上 |

> 抽查确认：`0x425280`、`0x70BF50`、`0x711AE0`、`0x6F3260` 在 Phobos 当前源码中出现次数均为 **0**，且没有任何 JUMP/PATCH 残留在这些地址上。

---

## 7. Kratos 独立改写的地址（Phobos 当前源码中不出现）

> **⚠️ 标题更正说明**：上一版此节标题写作「Kratos 独有、Phobos **完全不碰**的区域」，
> 容易被读成「Phobos 没有这个功能」—— **这是错的**。
> **Phobos 的 `Attach Unit` 正是源自 Kratos 的 `Stand`**（经原作者亲自授权）；
> 准确说法是「**这些具体地址**在 Phobos 当前源码中不出现」——
> 因为 Phobos **保留了功能、但删掉了这里的实现**。
> 最典型的是「Air/Top 层排序」这一族：Kratos 曾用 `0x55DBC3`（每帧排整层），
> Phobos 用的是**另一段代码** `0x4A9750`，在 0.4 因失同步**退役**了它，
> 改成"宿主换层后逐个重提交挂接动画"（见 `docs/联机失同步排查报告.md` §11.1 对照表 / §12.1）。
> **本轮改动**：Kratos 也把 `0x55DBC3` 整条删掉了，改为 `0x4A9768` 的提交时刻定点校正（报告 §12.9）。
> 另外：`KratosPP` 是 `DPKratos`(C#) 的 C++ 移植，**其 git 日期不可用于判断先后**。

以下地址在 Phobos 当前源码中**出现次数为 0** —— 即 Phobos 既不在这些地址 hook、也不 patch：

| 地址 | Kratos hook | 位置 | 说明 |
| --- | --- | --- | --- |
| `0x4A9768` | `DisplayClass_Submit_KeepStandAboveMaster` | `Hooks/StandExtHook.cpp:334` | **提交时刻**按 `Stand.ZOffset` 正负把替身摆到其 Master 的正确一侧（**取代**原 `0x55DBC3` 每帧整层排序，见报告 §12.4.1/§12.9）。★ 2026-10-04 修正：落点原为 `0x4A975E`（size `0x2`），**违反 Syringe 落点契约（`max(size,5)` 覆盖 + `return 0` 跳 `addr+5`）会造成执行流损坏**，已搬到 `Submit` 公共出口 `pop edi/pop esi/retn 4`（恰 5 字节）——详见 `docs/开局崩溃归因-2026-10-04.md`。★ 同日二次修正：原分区方向写反（把替身排到 Master **之前**=下层）；并补上 `Stand.ZOffset` 正负语义（Air 层不被引擎排序，故 ZOffset 对空中替身只能由本 Hook 实现）——详见 `docs/替身渲染层级与遮挡-2026-10-04.md` |
| `0x4DA87A` | ~~`FootClass_Update_UpdateLayer`~~ | ~~`Hooks/StandExtHook.cpp`~~ | ✅ **2026-10-05 已整条删除**（P1）。三条理由独立成立：① 引擎 `ObjectClass::Update`(`0x5F400E`) 本来就做同一件事，且 `FootClass::AI`(`0x4DA539 call TechnoClass_Update`) 先跑 ⇒ 本 Hook 恒为空转；② 与 `InWhichLayer()` 非纯函数叠加会**同帧 Submit 两次**；③ 原版此处 `cmp [esi+90h], bl` 是 **`IsAlive` 守卫**（`+0x90` = `IsAlive`，`LastLayer` 在 `+0x94`），**并非层比较** —— 原注释把字段偏移读错了。落点实测 `38 9E 90 00 00 00 \| 0F 84 ...`。见报告 §13.3 / §13.5 / **§16.1**。**该地址已不在 Kratos Hook 集合内**（DLL 自检计数 = 0） |
| `0x5F6BF7` | `ObjectClass_GetYSort` | `Hooks/StandExtHook.cpp:550` | 层内排序键（`vt[0xB8]`），**校验函数的输入之一** |
| `0x4D94B0` | `FootClass_SetDestination_Stand` | `Hooks/StandExtHook.cpp:58` | 替身寻路 |
| `0x6FCDBE` | `TechnoClass_SetTarget_Stand` | `Hooks/StandExtHook.cpp:141` | 替身锁定目标 |
| `0x4D9947` | `FootClass_Greatest_Threat_GetTarget` | `Hooks/StandExtHook.cpp:160` | 威胁搜索 |
| `0x75C7E0` | `WalkLocomotionClass_In_Which_Layer` | `Hooks/StandExtHook.cpp:660` | ✅ **2026-10-05 已修复**（P1，实质 P0）：`StandLayer::GetLayer` **改为纯函数**（去掉单槽静态缓存）。旧实现里 `layer = layer;` 是**自赋值空操作**，导致缓存命中分支**跳过"替身跟随 Master 层"逻辑** ⇒ 同一替身同帧两次调用可能返回不同层 ⇒ `Submit` 翻转 ⇒ 层数组顺序分叉 ⇒ 校验值不同 ⇒ 失同步。步兵用的就是这个。见报告 §13.4 / **§16.2** |
| `0x6A3E50` | `ShipLocomotionClass_In_Which_Layer` | `:668` | 同上（原版同为 `mov eax,2; retn 4`） |
| `0x5B19D0` | `MechLocomotionClass_In_Which_Layer` | `:676` | 同上 |
| `0x517100` | `HoverLocomotionClass_In_Which_Layer` | `:684` | 同上 |
| `0x4B4820` | `DriveLocomotionClass_In_Which_Layer` | `:692` | 同上（原版同为 `mov eax,2; retn 4`）。原 `KRATOS_STAND_LAYER_STRICT` 二分开关已于 2026-10-05 移除（无条件走纯函数版） |
| `0x54B8E9` | `JumpjetLocomotionClass_In_Which_Layer_Deviation` | `Hooks/StandExtHook.cpp:701` | 喷气机层恒为 Air |
| `0x51BB17` | `InfantryClass_Update_SkipCreateChronoSparkleAnimOnStand` | `:752` | 替身不生成超时空特效 |
| `0x51BBDF` | `TechnoClass_WarpUpdate` | `Hooks/TechnoExtHook.cpp:166` | `DEFINE_HOOK_AGAIN`，Infantry 侧 |
| `0x4B0521` / `0x69FC31` | `LocomotionClass_Update_Ramp` | `Hooks/TechnoExtHook.cpp:1020/1021` | 坡道速度 |

**⚠️ 例外（在 Phobos 侧存在，因此仍是冲突）**：

| 地址 | Kratos | Phobos | 见 |
| --- | --- | --- | --- |
| `0x6FC749` | `TechnoClass_CanFire_WhichLayer_Stand` | `TechnoClass_CanFire_AntiUnderground` | §5.3 |
| `0x702299` | `TechnoClass_Destroy_VxlDebris_Remap`（`TechnoExtHook.cpp:692`，受 `AudioVisual::Data()->AllowMakeVoxelDebrisByKratos` 门控，**默认 true**） | `TechnoClass_ReceiveDamage_Debris`（`Misc/Hooks.BugFixes.cpp:137`） | §5.8 |

**对比参照**：这一带曾是本清单最重要的一条「**下游已退役、上游仍在用**」的不对称 ——
`0x4A9750`（`DisplayClass::Submit_LayerSort`）那种"让 Air/Top 层也走 Y 有序插入"的做法，
在 Phobos 于 0.4 因失同步被**退役**（保留功能、只删排序）；而 Kratos 当时用的是**另一段代码**
`0x55DBC3`（每帧对整层 Air 调 `Sort()`），属于**同一失败类别**。
详见 `docs/联机失同步排查报告.md` §11.1 / §12.1 / §12.4.1。

> **本轮已消除这条不对称**：Kratos 侧 `0x55DBC3` 已整条删除，改为在数组唯一插入点
> （`DisplayClass::Submit`，`0x4A9768`）做定点校正。
> **当前两个项目都不再对 Air 层做按 Y 的排序**；区别只在"如何保证替身画在宿主之上"：
> Phobos = 换层后重提交挂接动画（依赖 `0x54B18E` / `0x4CD4E7` 这些 vanilla 重提交点）；
> Kratos = 提交时按 `Stand.ZOffset` 正负把替身钉到宿主的上/下侧。
> 因此 §7 里已**不再**出现任何"每帧"级别的层序操作。

**相邻单点（不冲突，但同属"遮挡"话题，记录备查）**：

| 地址 | 归属 | 说明 |
| --- | --- | --- |
| `0x424CB0` | **Phobos 独占**（`Anim/Hooks.cpp:352` `AnimClass_InWhichLayer_AttachedObjectLayer`，覆盖 6 字节 → `0x424CB0..0x424CB5`） | vanilla 语义：`AnimClass::OwnerObject`(`+0xCC`) 非空 ⇒ **恒返回 Ground(2)**；否则取 `Type->Layer`。Phobos 用 `Layer_UseObjectLayer` 让它继承 `OwnerObject->InWhichLayer()` 来修。Kratos 的 AE 不设 `OwnerObject`（`SetAnimOwner` 只写 `pAnim->Owner`），因此走 `Type->Layer` 分支，也没有 `Layer_UseObjectLayer` 这个开关。详见 `docs/替身渲染层级与遮挡-2026-10-04.md` §5 / §5.5 |
| `0x424CCA` | **Kratos 独占**（`Hooks/AnimExtHook.cpp` `AnimClass_InWhichLayer_FollowAttachOwner`，覆盖 6 字节 → `0x424CCA..0x424CCF`） | 落点 = `mov eax,[eax+364h]`（Type->Layer，恰好一条 6 字节指令），下一条 `0x424CD0` 即 `C3 retn`。**与 Phobos 的 `0x424CB0` 相邻但不重叠** —— 这是刻意的：原实现挂在 `0x424CB0`，会因 SyringeEx 的链式语义（返回非 0 即跳过链上后续 hook）静默掐掉 Phobos 那一版。迁址依据见 `docs/替身渲染层级与遮挡-2026-10-04.md` §5.5 |
| `0x551A30` | vanilla（`LayerClass::Sort`） | 唯一调用点 `0x55DBC8`，参数 `ecx = offset 0x8A0390`（**Ground 层**）。⇒ 引擎每帧只排 Ground 层，**Air 层永不排序** —— 这是"`Stand.ZOffset` 对空中替身失效"的根因 |

---

## 8. 与 2026-08-23 版的差异明细

### 8.1 新增（6 对）

| 地址 | Kratos hook | Phobos hook | 性质 |
| --- | --- | --- | --- |
| `0x00467E53` | `BulletClass_AI_PreDetonation_Vector` | `BulletClass_AI_PreDetonation_Trajectories` | 两边都新加 |
| `0x004C24C3` | `EBolt_DrawFirst_Color` | `EBolt_DrawFirst_Color` | Kratos 照抄 Phobos（§5.4） |
| `0x004C25D0` | `EBolt_DrawSecond_Color` | `EBolt_DrawSecond_Color` | 同上 |
| `0x004C26D5` | `EBolt_DrawThird_Color` | `EBolt_DrawThird_Color` | 同上 |
| `0x006F9039` | `TechnoClass_Greatest_Threat_HealWeaponRange` | `TechnoClass_SelectAutoTarget_HealGuardRange` | **语义冲突，§5.1** |
| `0x006FD446` | `TechnoClass_LaserZap_IsSingleColor` | `TechnoClass_LaserZap_Tracking` | Phobos 侧同址已有 **2 个** hook（另一个是 `TechnoClass_LaserZap_IsSingleColor`），**三向同址** |

### 8.2 消失（63 对）

构成见 §6。完整逐条列表见 `ida_work/hook_conflict_diff.txt`。

### 8.3 未变（沿用旧版的判定）

`0x6FF08B`、`0x701900`、`0x6FC749`、`0x702299`、`0x6F9B7E` 等绝大多数条目位置未变，仅行号随源码改动而移动。

---

## 9. 复现方法

```bash
# 1) 生成三张表（A/B/C）与 tsv
python tools/gen_hook_conflict.py \
  --kratos-src D:/Workspace/ra2mod/platform/KratosPP/src \
  --phobos-src D:/Workspace/ra2mod/platform/Phobos/src \
  --out-md    ida_work/hook_conflict_new.md \
  --out-tsv   ida_work/hook_conflict_new.tsv \
  --old-md    docs/Hook地址冲突清单.md

# 2) 把表格直接回填进本文档（替换 <!-- AUTO:key --> 标记之间的内容）
python tools/gen_hook_conflict.py \
  --kratos-src D:/Workspace/ra2mod/platform/KratosPP/src \
  --phobos-src D:/Workspace/ra2mod/platform/Phobos/src \
  --out-md    ida_work/hook_conflict_new.md \
  --out-tsv   ida_work/hook_conflict_new.tsv \
  --splice-doc docs/Hook地址冲突清单.md
```

**实现要点**（避免复现时的坑）：

- 先把全文的注释（`//` 与 `/* */`）按**原长度**替换成空格，再在「纯代码」上按**平衡括号**抽宏调用。
  这样既能正确剔除被注释停用的 hook，又能处理参数里夹注释的情况（如 `DEFINE_PATCH(/* Offset */ 0x825F9B, ...)`），
  且行号与偏移不变。
- 字符串 / 字符字面量**内容保留不动**，否则 `DEFINE_PATCH` 的字符串数据会被误吞。
- 旧的「逐行正则」做法有两个已知错误：会把 `/* */` 里的停用 hook 算进来；会漏掉跨行声明。
- `DEFINE_HOOK` 参数里若出现 `/* */` 注释、或声明跨行，正则法都会失效。

---

## 10. 同一地址多重 Hook 的链式执行（顺序 Ares → Kratos → Phobos）

### 10.0 前提：Syringe 的链式语义

`DEFINE_HOOK(addr, name, size)` 并**不独占**该地址。当多个模块（Ares / Kratos / Phobos）在**同一地址**声明 hook 时，
Syringe 把它们**串成一条链**，按**注册顺序（= DLL 加载顺序）**依次调用：

| 写法 | 行为 |
| --- | --- |
| `return 0` | 继续跑链上的**下一个** hook；全部返回 0 后，重放被替换的原始指令，再回到 `addr+size` |
| `return <非 0 地址>` | **立即跳转**到该地址 —— 链上**后面的 hook 与原始指令重放全被跳过** |

**实测顺序：Ares → Kratos → Phobos。** 于是：

- **Ares 最先** ⇒ 它返回非 0 时，Kratos 与 Phobos 在该地址的 hook **一起失效**；
- **Kratos 居中** ⇒ 它返回非 0 时，**Phobos 失效**（Kratos 自己不受影响）；
- **Phobos 最后** ⇒ 没有 hook 接在它后面，它的 `return <非0>` 只是「跳过原指令重放」，属正常行为。

反过来还有一条容易忽略的副作用：**Kratos 的 `return 0` 分支依赖「链最终会重放原指令」**，
但排在它后面的 Phobos 有权把控制流改道 —— 这时 Kratos 期待的「原逻辑继续」就不成立了。
所以判定一个同址冲突是否安全，不能只看 Kratos 自己。

### 10.1 影响方向总览

| 情形 | 谁压谁 | 处数 | 后果 |
| --- | --- | --- | --- |
| **必定掐断**：Kratos 体内没有任何 `return 0` | Kratos ⇒ Phobos | **30** | Phobos 的 hook **永不执行**（功能静默失效） |
| **条件掐断**：Kratos 体内既有 `return <非0>` 也有 `return 0` | Kratos ⇒ Phobos（仅部分分支） | **14** | Phobos 只在「Kratos 走 0 分支」时执行；**门控默认 true 的等价于必定掐断** |
| **安全** | —— | **92** | Kratos 全部 `return 0`，Phobos 正常执行 |
| **静态改写同址**（`DEFINE_JUMP` / `DEFINE_PATCH`） | **Phobos ⇒ Kratos**（方向相反） | **8** | 不走链、裸写字节，**后加载者赢** ⇒ Phobos 覆盖 Kratos 的 trampoline / 写入 |
| **Ares 返回非 0** | Ares ⇒ Kratos + Phobos | 未知 | 本仓库无 Ares 源码，无法静态枚举，只能运行时验证 |

### 10.2 逐条判定

<!-- AUTO:D -->
> **判定前提**：链序 **Ares → Kratos → Phobos**；同一地址上各 hook 串成一条链依次执行，
> `return 0` 续链，`return <非 0>` 立即跳转并**掐断整条链**（后面的 hook 与原始指令重放全部被跳过）。
> 因此凡是「Kratos 会返回非 0」的地址，**Phobos 的 hook 就会被跳过**。

### D.1 必定掐断（30 处）：Kratos 的 hook 体内**没有** `return 0`，所有路径都跳走 ⇒ Phobos 永不执行

| 地址 | Kratos hook | Kratos 返回值 | 位置 |
| --- | --- | --- | --- |
| 0x0041D604 | `AirstrikeClass_PointerGotInvalid_ResetForTarget` | `SkipGameCode` | Hooks/AirstrikeExtHook.cpp:213 |
| 0x0041D97B | `AirstrikeClass_Setup_SkipBuildingCheck` | `0x41D98B` | Hooks/AirstrikeExtHook.cpp:71 |
| 0x0041DA52 | `AirstrikeClass_ResetTarget_OriginalTarget` | `SkipGameCode` | Hooks/AirstrikeExtHook.cpp:88 |
| 0x0041DA80 | `AirstrikeClass_ResetTarget_NewTarget` | `SkipGameCode` | Hooks/AirstrikeExtHook.cpp:98 |
| 0x0041DAA4 | `AirstrikeClass_ResetTarget_ResetForOldTarget` | `SkipGameCode` | Hooks/AirstrikeExtHook.cpp:108 |
| 0x0041DBD4 | `AirstrikeClass_ClearTarget` | `clearTarget,resetTarget` | Hooks/AirstrikeExtHook.cpp:180 |
| 0x0043F9E0 | `BuildingClass_Mark_Airstrike` | `ContinueTintIntensity,NonAirstrike` | Hooks/AirstrikeExtHook.cpp:434 |
| 0x00448DF1 | `BuildingClass_SetOwningHouse_Airstrike` | `ContinueTintIntensity,NonAirstrike` | Hooks/AirstrikeExtHook.cpp:447 |
| 0x00451ABC | `BuildingClass_PlayAnim_Airstrike` | `ContinueTintIntensity,NonAirstrike` | Hooks/AirstrikeExtHook.cpp:460 |
| 0x00452041 | `BuildingClass_452000_Airstrike` | `ContinueTintIntensity,NonAirstrike` | Hooks/AirstrikeExtHook.cpp:473 |
| 0x00456E5A | `BuildingClass_Flash_Airstrike` | `ContinueTintIntensity,NonAirstrike` | Hooks/AirstrikeExtHook.cpp:486 |
| 0x00469C46 | `BulletClass_Detonate_WHAnim_Remap` | `0x469C98` | Hooks/BulletExtHook.cpp:184 |
| 0x004AE95E | `DisplayClass_sub_4AE750_DisallowBuildingNonAttackPlanning` | `SkipGameCode` | Hooks/BuildingExtHook.cpp:90 |
| 0x004C20BC | `EBolt_Draw_Arcs` | `plotIndex < arcCount ? DoLoop : Break` | Hooks/EBoltExtHook.cpp:110 |
| 0x004C24C3 | `EBolt_DrawFirst_Color` | `0x4C24E4,0x4C2515` | Hooks/EBoltExtHook.cpp:120 |
| 0x004C25D0 | `EBolt_DrawSecond_Color` | `0x4C25FD,0x4C262A` | Hooks/EBoltExtHook.cpp:130 |
| 0x004C26D5 | `EBolt_DrawThird_Color` | `0x4C26EE,0x4C2710` | Hooks/EBoltExtHook.cpp:140 |
| 0x004DDD66 | `FootClass_IsLandZoneClear_ReplaceHardcode` | `SkipGameCode` | Hooks/AircraftExtHook.cpp:577 |
| 0x0051EAE0 | `InfantryClass_WhatAction_Cursor` | `0x51EB06` | Hooks/AirstrikeExtHook.cpp:33 |
| 0x00550F47 | `LaserDrawClass_DrawInHouseColor_BetterDrawing` | `0x550F9D` | Hooks/LaserDrawHook.cpp:26 |
| 0x0064736D | `Queue_AI_WriteDesyncLog` | `0x647372` | Utilities/SyncLogging.cpp:245 |
| 0x006F348F | `TechnoClass_WhatWeaponShouldIUse_Airstrike` | `(pTargetTypeData->AllowAirstrike && (!pTargetType->ResourceDestination \|\| !pTargetType->ResourceGatherer)) ? Secondary : Primary,Primary,pTargetTypeData->AllowAirstrike ? Secondary : Primary` | Hooks/AirstrikeExtHook.cpp:39 |
| 0x006F36DB | `TechnoClass_SelectWeapon` | `0x6F36E3,Primary,Secondary` | Hooks/TechnoExtHook.cpp:774 |
| 0x006F9039 | `TechnoClass_Greatest_Threat_HealWeaponRange` | `0x6F903E` | Hooks/TechnoExtHook.cpp:528 |
| 0x006FC749 | `TechnoClass_CanFire_WhichLayer_Stand` | `inAir,onGround` | Hooks/StandExtHook.cpp:93 |
| 0x007019D8 | `TechnoClass_ReceiveDamage_At_Least1` | `0x7019E3` | Hooks/TechnoExtHook.cpp:242 |
| 0x007058F6 | `TechnoClass_DrawAirstrikeFlare_LineColor` | `SkipGameCode` | Hooks/AirstrikeExtHook.cpp:380 |
| 0x0070597A | `TechnoClass_DrawAirstrikeFlare_DotColor` | `SkipGameCode` | Hooks/AirstrikeExtHook.cpp:408 |
| 0x0070E92F | `TechnoClass_Update_Airstrike_Tint_Timer` | `ContinueTintIntensity,NonAirstrike` | Hooks/AirstrikeExtHook.cpp:421 |
| 0x0071A88D | `TemporalClass_Update` | `0x71A895,0x71AB08` | Hooks/TechnoExtHook.cpp:181 |

### D.2 条件掐断（14 处）：体内同时存在 `return <非0>` 与 `return 0`，是否掐断取决于运行分支 / 门控开关

| 地址 | Kratos hook | Kratos 返回值 | 位置 |
| --- | --- | --- | --- |
| 0x004147F9 | `AircraftClass_Draw_Shadow_SkipPhobos` | `0x4147FF,0x4148A5` | Hooks/AircraftExtHook.cpp:62 |
| 0x004186B6 | `AircraftClass_Mission_Attack5_HoverFireTwice` | `0x4186D7` | Hooks/AircraftExtHook.cpp:476 |
| 0x0041A96C | `AircraftClass_Mission_GuardArea_NoTarget_Enter` | `0x41A97A` | Hooks/AircraftExtHook.cpp:285 |
| 0x0041DAD4 | `AirstrikeClass_Reset` | `0x41DADA` | Hooks/AirstrikeExtHook.cpp:166 |
| 0x004242E1 | `AnimClass_Trailer_Remap` | `0x424322` | Hooks/AnimExtHook.cpp:231 |
| 0x00467E53 | `BulletClass_AI_PreDetonation_Vector` | `0x467FBA` | Hooks/BulletExtHook.cpp:361 |
| 0x004690C1 | `BulletClass_Detonate` | `0x46A2FB` | Hooks/BulletExtHook.cpp:139 |
| 0x0048A551 | `WarheadTypeClass_AnimList_SplashList` | `0x48A5AD` | Hooks/WarheadTypeExtHook.cpp:71 |
| 0x005194EF | `InfantryClass_DrawIt_InAir_Shadow_Skip` | `0x51958A` | Hooks/InfantryExtHook.cpp:17 |
| 0x0065E97F | `HouseClass_CreateAirstrike_SetTargetForUnit` | `SkipGameCode` | Hooks/AirstrikeExtHook.cpp:230 |
| 0x0065E997 | `Airstrike_Supported_Reinforcements_Put` | `result ? SkipGameCode : SkipGameCodeNoSuccess` | Hooks/AirstrikeExtHook.cpp:263 |
| 0x006FC339 | `TechnoClass_CanFire` | `0x6FCB7E,dw->Data.DisableWithTarget ? 0x6FC0DF : 0x6FCB7E` | Hooks/TechnoExtHook.cpp:402 |
| 0x00702299 | `TechnoClass_Destroy_VxlDebris_Remap` | `0x7023E5` | Hooks/TechnoExtHook.cpp:692 |
| 0x0073C47A | `UnitClass_DrawAsVXL_Shadow_SkipPhobos` | `0x73C485,0x73C5C9` | Hooks/AircraftExtHook.cpp:42 |

### D.3 安全（92 处）：Kratos 全部 `return 0`，Phobos 可正常执行

| 地址 | Kratos hook | 位置 |
| --- | --- | --- |
| 0x00418506 | `AircraftClass_Mission_Attack_FireDone` | Hooks/AircraftExtHook.cpp:451 |
| 0x0041AA80 | `AircraftClass_AssignDestination_SyncLog` | Utilities/SyncLogging.cpp:345 |
| 0x0041BB30 | `AircraftClass_OverrideMission_SyncLog` | Utilities/SyncLogging.cpp:391 |
| 0x00422126 | `AnimClass_CTOR` | Hooks/AnimExtHook.cpp:27 |
| 0x00422967 | `AnimClass_DTOR` | Hooks/AnimExtHook.cpp:40 |
| 0x00424807 | `AnimClass_Next` | Hooks/AnimExtHook.cpp:152 |
| 0x0042784B | `AnimTypeClass_CTOR` | Hooks/AnimTypeExtHook.cpp:10 |
| 0x004287DC | `AnimTypeClass_LoadFromINI` | Hooks/AnimTypeExtHook.cpp:54 |
| 0x00428EA8 | `AnimTypeClass_SDDTOR` | Hooks/AnimTypeExtHook.cpp:21 |
| 0x00443B90 | `BuildingClass_AssignTarget_SyncLog` | Utilities/SyncLogging.cpp:318 |
| 0x00455D50 | `BuildingClass_AssignDestination_SyncLog` | Utilities/SyncLogging.cpp:356 |
| 0x004664BA | `BulletClass_CTOR` | Hooks/BulletExtHook.cpp:27 |
| 0x00466556 | `BulletClass_Init` | Hooks/BulletExtHook.cpp:84 |
| 0x004665E9 | `BulletClass_DTOR` | Hooks/BulletExtHook.cpp:40 |
| 0x004666F7 | `BulletClass_Update` | Hooks/BulletExtHook.cpp:112 |
| 0x0046745C | `BulletClass_Update_ChangeVelocity` | Hooks/BulletExtHook.cpp:294 |
| 0x00469A75 | `BulletClass_Detonate_GetHouse` | Hooks/BulletExtHook.cpp:165 |
| 0x0046BDD9 | `BulletTypeClass_CTOR` | Hooks/BulletTypeExtHook.cpp:10 |
| 0x0046C41C | `BulletTypeClass_LoadFromINI` | Hooks/BulletTypeExtHook.cpp:56 |
| 0x0046C8B6 | `BulletTypeClass_SDDTOR` | Hooks/BulletTypeExtHook.cpp:21 |
| 0x004C1E42 | `EBolt_CTOR` | Hooks/EBoltExtHook.cpp:23 |
| 0x004C2951 | `EBolt_DTOR` | Hooks/EBoltExtHook.cpp:31 |
| 0x004C9300 | `FacingClass_Set_SyncLog` | Utilities/SyncLogging.cpp:295 |
| 0x004D8F40 | `FootClass_OverrideMission_SyncLog` | Utilities/SyncLogging.cpp:402 |
| 0x004F4583 | `GScreenClass_Render_Late` | Hooks/GScreenHook.cpp:27 |
| 0x004F6532 | `HouseClass_CTOR` | Hooks/HouseExtHook.cpp:16 |
| 0x004F7371 | `HouseClass_DTOR` | Hooks/HouseExtHook.cpp:28 |
| 0x0050114D | `HouseClass_InitFromINI` | Hooks/HouseExtHook.cpp:62 |
| 0x0051AA40 | `InfantryClass_AssignDestination_SyncLog` | Utilities/SyncLogging.cpp:367 |
| 0x0051B1F0 | `InfantryClass_AssignTarget_SyncLog` | Utilities/SyncLogging.cpp:307 |
| 0x0052F639 | `YR_CmdLineParse` | Hooks/GeneralHook.cpp:45 |
| 0x0054D600 | `JumpjetLocomotionClass_MovingUpdate_DontTurnInCell` | Hooks/TechnoExtHook.cpp:1137 |
| 0x00550D1F | `LaserDrawClass_DrawInHouseColor_Context_Set` | Hooks/LaserDrawHook.cpp:19 |
| 0x0064CD11 | `ExecuteDoList_WriteDesyncLog` | Utilities/SyncLogging.cpp:260 |
| 0x0065C7D0 | `Random2Class_Random_SyncLog` | Utilities/SyncLogging.cpp:271 |
| 0x0065C88A | `Random2Class_RandomRanged_SyncLog` | Utilities/SyncLogging.cpp:281 |
| 0x0067CEF0 | `SaveGame_Start` | Hooks/SaveGameHook.cpp:29 |
| 0x0067D04E | `Game_Save_SavegameInformation` | Extension.cpp:251 |
| 0x0067D32C | `SaveGame_Ext` | Extension.cpp:235 |
| 0x0067E826 | `LoadGame_Ext` | Extension.cpp:243 |
| 0x0067FD9D | `LoadOptionsClass_GetFileInfo` | Extension.cpp:258 |
| 0x0067FDB1 | `LoadOptionsClass_GetFileInfo` | Extension.cpp:259 |
| 0x00685659 | `Scenario_ClearClasses_End` | Hooks/GeneralHook.cpp:88 |
| 0x006CE6F6 | `SuperWeaponTypeClass_CTOR` | Hooks/SuperWeaponTypeHook.cpp:10 |
| 0x006CEE43 | `SuperWeaponTypeClass_LoadFromINI` | Hooks/SuperWeaponTypeHook.cpp:53 |
| 0x006CEFE0 | `SuperWeaponTypeClass_SDDTOR` | Hooks/SuperWeaponTypeHook.cpp:21 |
| 0x006F4500 | `TechnoClass_DTOR` | Hooks/TechnoExtHook.cpp:49 |
| 0x006F65D1 | `TechnoClass_DrawHealthBar_Building` | Hooks/TechnoExtHook.cpp:447 |
| 0x006F683C | `TechnoClass_DrawHealthBar_Other` | Hooks/TechnoExtHook.cpp:462 |
| 0x006F6AC4 | `TechnoClass_Remove` | Hooks/TechnoExtHook.cpp:123 |
| 0x006F9B7E | `TechnoClass_SelectAutoTarget_SyncLog` | Utilities/SyncLogging.cpp:426 |
| 0x006F9E50 | `TechnoClass_Update` | Hooks/TechnoExtHook.cpp:136 |
| 0x006FCDB0 | `TechnoClass_AssignTarget_SyncLog` | Utilities/SyncLogging.cpp:329 |
| 0x006FD38D | `TechnoClass_DrawSth_DrawToInvisoFlakScatterLocation` | Hooks/WeaponExtHook.cpp:105 |
| 0x006FD446 | `TechnoClass_LaserZap_IsSingleColor` | Hooks/WeaponExtHook.cpp:34 |
| 0x006FD494 | `TechnoClass_FireEBolt_SetWeaponData` | Hooks/EBoltExtHook.cpp:44 |
| 0x006FD514 | `TechnoClass_DrawSth_DrawToInvisoFlakScatterLocation` | Hooks/WeaponExtHook.cpp:104 |
| 0x006FD70D | `TechnoClass_DrawSth_DrawToInvisoFlakScatterLocation` | Hooks/WeaponExtHook.cpp:103 |
| 0x006FF08B | `TechnoClass_Fire_RecordBullet` | Hooks/WeaponExtHook.cpp:66 |
| 0x006FF15F | `TechnoClass_FireAt_ObstacleCellSet` | Hooks/WeaponExtHook.cpp:86 |
| 0x006FF29E | `TechnoClass_Fire_ROFMultiplier` | Hooks/TechnoExtHook.cpp:995 |
| 0x006FF43F | `TechnoClass_FireAt_TargetSet` | Hooks/WeaponExtHook.cpp:134 |
| 0x006FF660 | `TechnoClass_FireAt_ObstacleCellUnset` | Hooks/WeaponExtHook.cpp:153 |
| 0x007013A0 | `TechnoClass_OverrideMission_SyncLog` | Utilities/SyncLogging.cpp:413 |
| 0x00701900 | `TechnoClass_ReceiveDamage` | Hooks/TechnoExtHook.cpp:217 |
| 0x00701DFF | `TechnoClass_ReceiveDamageEnd` | Hooks/TechnoExtHook.cpp:350 |
| 0x00702050 | `TechnoClass_ReceiveDamage_Destroy` | Hooks/TechnoExtHook.cpp:368 |
| 0x00705860 | `TechnoClass_DrawAirstrikeFlare_SetContext` | Hooks/AirstrikeExtHook.cpp:370 |
| 0x00711835 | `TechnoTypeClass_CTOR` | Hooks/TechnoTypeExtHook.cpp:10 |
| 0x0071BB2C | `TerrainClass_TakeDamage_NowDead_Add` | Hooks/TerrainExtHook.cpp:14 |
| 0x0071DBC0 | `TerrainTypeClass_CTOR` | Hooks/TerrainTypeExtHook.cpp:10 |
| 0x0071E0A6 | `TerrainTypeClass_LoadFromINI` | Hooks/TerrainTypeExtHook.cpp:59 |
| 0x0071E364 | `TerrainTypeClass_SDDTOR` | Hooks/TerrainTypeExtHook.cpp:25 |
| 0x007258D0 | `DetachThisFromAll` | Hooks/PointerExpireHook.cpp:41 |
| 0x00741970 | `UnitClass_AssignDestination_SyncLog` | Utilities/SyncLogging.cpp:378 |
| 0x0074942E | `VoxelAnimClass_CTOR` | Hooks/VoxelAnimExtHook.cpp:15 |
| 0x007499F1 | `VoxelAnimClass_DTOR` | Hooks/VoxelAnimExtHook.cpp:24 |
| 0x0074AEB0 | `VoxelAnimTypeClass_CTOR` | Hooks/VoxelAnimTypeExtHook.cpp:10 |
| 0x0074B4F0 | `VoxelAnimTypeClass_LoadFromINI` | Hooks/VoxelAnimTypeExtHook.cpp:56 |
| 0x0074B51B | `VoxelAnimTypeClass_LoadFromINI` | Hooks/VoxelAnimTypeExtHook.cpp:55 |
| 0x0074B54A | `VoxelAnimTypeClass_LoadFromINI` | Hooks/VoxelAnimTypeExtHook.cpp:54 |
| 0x0074B561 | `VoxelAnimTypeClass_LoadFromINI` | Hooks/VoxelAnimTypeExtHook.cpp:53 |
| 0x0074B607 | `VoxelAnimTypeClass_LoadFromINI` | Hooks/VoxelAnimTypeExtHook.cpp:52 |
| 0x0074BA31 | `VoxelAnimTypeClass_DTOR` | Hooks/VoxelAnimTypeExtHook.cpp:21 |
| 0x0075D1A9 | `WarheadTypeClass_CTOR` | Hooks/WarheadTypeExtHook.cpp:12 |
| 0x0075DEA0 | `WarheadTypeClass_LoadFromINI` | Hooks/WarheadTypeExtHook.cpp:58 |
| 0x0075E5C8 | `WarheadTypeClass_SDDTOR` | Hooks/WarheadTypeExtHook.cpp:23 |
| 0x00771EE9 | `WeaponTypeClass_CTOR` | Hooks/WeaponTypeExtHook.cpp:10 |
| 0x007729B0 | `WeaponTypeClass_LoadFromINI` | Hooks/WeaponTypeExtHook.cpp:57 |
| 0x007729C7 | `WeaponTypeClass_LoadFromINI` | Hooks/WeaponTypeExtHook.cpp:55 |
| 0x0077311D | `WeaponTypeClass_SDDTOR` | Hooks/WeaponTypeExtHook.cpp:21 |
| 0x007CD810 | `ExeRun` | Hooks/GeneralHook.cpp:57 |

### D.5 静态改写同址（8 处）：**后加载的 Phobos 覆盖 Kratos 的写入**（方向与 hook 相反）

| 地址 | Kratos 指令 | 位置 |
| --- | --- | --- |
| 0x004C24BE | `0x4C24C3` (JUMP:DEFINE_JUMP) | Hooks/EBoltExtHook.cpp:119 |
| 0x004C25CB | `0x4C25D0` (JUMP:DEFINE_JUMP) | Hooks/EBoltExtHook.cpp:129 |
| 0x004C26CF | `0x4C26D5` (JUMP:DEFINE_JUMP) | Hooks/EBoltExtHook.cpp:139 |
| 0x006D481D | `TacticalClass_Draw_AirstrikeLaser_SkipBuildingCheck` (HOOK:DEFINE_HOOK) | Hooks/AirstrikeExtHook.cpp:303 |
| 0x007CD8EA | `GET_OFFSET(_ExeTerminate)` (JUMP:DEFINE_JUMP) | Hooks/GeneralHook.cpp:79 |
| 0x007E4610 | `BuildingTypeClass_CanUseWaypoint` (JUMP:DEFINE_FUNCTION_JUMP) | Hooks/BuildingExtHook.cpp:88 |
| 0x00825F9B | `DEFINE_PATCH` (PATCH:DEFINE_PATCH) | Utilities/Debug.cpp:97 |
| 0x008332F4 | `DEFINE_PATCH` (PATCH:DEFINE_PATCH) | Utilities/Debug.cpp:102 |
<!-- /AUTO:D -->

### 10.3 需要优先确认的几处

1. **明确「故意接管」的 4 处**（Kratos 源码里点名了 Phobos，属**知情设计**，不是 bug）：

   | 地址 | Kratos hook | 为何是「故意」 |
   | --- | --- | --- |
   | `0x4147F9` / `0x73C47A` | `AircraftClass_Draw_Shadow_SkipPhobos` / `UnitClass_DrawAsVXL_Shadow_SkipPhobos` | 名字里就写着 `SkipPhobos`；开关 `AudioVisual::Data()->AllowTakeoverPhobosShadowMaker` **默认 `true`** |
   | `0x6F36DB` | `TechnoClass_SelectWeapon` | 注释原文：「*Phobos 在此处对目标进行强制武器筛选，要兼容就得复刻一套一样的逻辑*」 |
   | `0x702299` | `TechnoClass_Destroy_VxlDebris_Remap` | 注释原文：「*Phobos hook 这个地址，要自己算随机数*」；开关 `AllowMakeVoxelDebrisByKratos` **默认 `true`** |

   > 这类「**复刻型接管**」是长期风险点：Kratos 复刻的是**当时**的 Phobos 逻辑，Phobos 一改，两边就不一致了。
   > 上面 4 处里，`0x4147F9` / `0x73C47A` 属**渲染路径**（不影响同步）；`0x6F36DB` / `0x702299` 落在**模拟路径**上。

2. **落在模拟路径上的**（优先级最高 —— 它们改变的是「两端必须一致」的行为）：
   `0x71A88D`（`TemporalClass_Update`）、`0x4690C1`（`BulletClass_Detonate`）、
   `0x48A551`（`WarheadTypeClass_AnimList_SplashList`）、`0x467E53`（`BulletClass_AI_PreDetonation`）、
   `0x6FC339`（`TechnoClass_CanFire`，条件型）、`0x6F348F`（`TechnoClass_WhatWeaponShouldIUse`）。

3. **`0x64736D Queue_AI_WriteDesyncLog`** —— 两边**同名同址**，Kratos 先跑且返回 `0x647372`，
   于是 **Phobos 的 SyncLog 写入永不执行**。实际影响：写出来的是 Kratos 的 `SYNC%d.TXT`，
   不会有 Phobos 在 `EnableMPSyncDebug` 下的 `SYNC%d_%03d.TXT`。**开日志排查失同步时要意识到这点**
   （见 `docs/联机失同步排查报告.md`）。

4. **好消息**：Phobos 那条 `DeploysIntoDesyncFix`（`0x73FEC1` / `0x47C640` / `0x7396D2`）
   **没有**与 Kratos 撞址 —— 它不会被掐断。

### 10.4 Ares 的位置

- Ares 是编译好的二进制，本仓库**没有源码**，无法像 Kratos × Phobos 那样静态枚举同址。
- 但方向已经确定：**Ares 最先**，凡是它 `return <非0>` 的地址，**Kratos 与 Phobos 都失效**。
  这可以解释一类现象 ——「Kratos 的某个 hook 明明写了却像没生效」。
- 已知的一处 Kratos 主动处理的 Ares 交互：`src/Hooks/EBoltExtHook.cpp` 用
  `DEFINE_JUMP(LJMP, 0x4C24BE, 0x4C24C3)` **主动禁用 Ares 在 `0x4C24C3` 的 hook**（源码注明 `// copy from Phobos`）。
- 若要确认 Ares 侧的影响，只能做**运行时验证**：在 Kratos 的 hook 里打点计数，看是否真被调用。

### 10.5 与失同步排查的关系

- 本节判定**不直接**说明「会不会失同步」—— 两端机器跑的是同一套 DLL、同一顺序，行为是确定的。
- 但它解释了一类**很难查的现象**：某个修复「明明开了却没用」。
  最典型的是：**Phobos 的失同步修复若与 Kratos 同址且被掐断，修复实际不生效**，
  于是「Phobos 已经修过的问题」在 Kratos + Phobos 组合下会**复现**。
- 因此排查失同步时**不能假设 Phobos 的修复已经生效**，要按 §10.2 的 D 表核对相关地址。

---

## 附：文档维护

- §1 / §2 / §3 / §4 的表格由 `tools/gen_hook_conflict.py --splice-doc` 回填；
  §10 的 D 表由 `tools/analyze_hook_chain.py --splice-doc` 回填。只改本文件的叙述部分，表格会在下次运行时自动保持同步。
- 原始数据：`ida_work/hook_conflict_new.tsv`（同址对）、`ida_work/hook_conflict_diff.txt`（与上一版的逐条差异）、
  `ida_work/hook_chain.tsv`（链式影响逐条判定）。
- 引用：`docs/联机失同步排查报告.md`（§10 无标签基线 / §11 与 Phobos 的交叉验证）。
