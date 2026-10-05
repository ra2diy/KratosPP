## A. 同址冲突（DEFINE_HOOK × DEFINE_HOOK）

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

## B. 同址冲突（含 JUMP / PATCH 等其他内存改写指令）

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

## C. 区间部分重叠（起始不同）

| Kratos 地址 | Kratos 区间 | Kratos hook | Kratos 位置 | Phobos 地址 | Phobos 区间 | Phobos hook | Phobos 位置 |
| --- | --- | --- | --- | --- | --- | --- | --- |
