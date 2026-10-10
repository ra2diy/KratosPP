#include "StandEffect.h"

#include <HouseClass.h>

#include <DriveLocomotionClass.h>
#include <MechLocomotionClass.h>
#include <ShipLocomotionClass.h>
#include <WalkLocomotionClass.h>

#include <Ext/Common/PaintballSyncManager.h>

#include <Ext/Helper/FLH.h>
#include <Ext/Helper/Gift.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>

#include <Ext/SyncEventType/TechnoScriptCommandEvent.h>
#include <Ext/TechnoType/TechnoStatus.h>
#include <Ext/TechnoType/TurretAngle.h>

void StandEffect::CreateAndPutStand()
{
	CoordStruct location = pObject->GetCoords();
	TechnoTypeClass* pType = TechnoTypeClass::Find(Data->Type.c_str());
	if (pType)
	{
		pStand = abstract_cast<TechnoClass*, true>(pType->CreateObject(AE->pSourceHouse));
	}
	if (pStand)
	{
		// 初始化替身的状态设置
		SetupStandStatus();
		pStand->UpdatePlacement(PlacementType::Remove); // Mark(MarkType::Up)
		bool canGuard = AE->pSourceHouse->IsControlledByHuman();
		if (pStand->WhatAmI() == AbstractType::Building)
		{
			standIsBuilding = true;
			canGuard = true;
		}
		else
		{
			abstract_cast<FootClass*, true>(pStand)->Locomotor->Lock();
		}
		// only computer units can hunt
		Mission mission = canGuard ? Mission::Guard : Mission::Hunt;
		pStand->QueueMission(mission, false);
		// 放出地图
		if (!pObject->InLimbo)
		{
			if (!TryPutTechno(pStand, location, nullptr, true))
			{
				End(location);
				return;
			}
		}
		// 移动到指定的位置
		LocationMark locationMark = GetRelativeLocation(pObject, Data->Offset);
		if (!locationMark.IsEmpty())
		{
			SetLocation(locationMark.Location);
			ForceFacing(locationMark.Dir);
		}
		// 攻击来源
		TechnoClass* pSource = AE->pSource;
		if (Data->AttackSource && !IsDeadOrInvisible(pSource) && CanAttack(pStand, pSource, true))
		{
			pStand->SetTarget(pSource);
		}
	}
}

TechnoStatus* StandEffect::SetupStandStatus()
{
	TechnoStatus* status = GetStatus<TechnoExt, TechnoStatus>(pStand);
	// 初始化设置
	if (status)
	{
		status->VirtualUnit = Data->VirtualUnit;
		status->MyStandData = *Data;
		TechnoClass* pMaster = nullptr;
		// 设置替身的所有者
		if (pTechno)
		{
			pMaster = pTechno;
			// 同阵营同步状态机，比如染色
			TechnoStatus* masterStatus = nullptr;
			if (pTechno->Owner == AE->pSourceHouse && TryGetStatus<TechnoExt>(pTechno, masterStatus))
			{
				// 每个TechnoStatus都有一个独立的Paintball，只能值同步，不可以指针同步
				// status->_Paintball = masterStatus->Paintball;

				// 使用Master的Paintball的thisName作为同步ID
				std::string syncId = masterStatus->Paintball->thisName;
				// 注册到同步管理器
				PaintballSyncManager::Register(syncId, masterStatus->Paintball);
				PaintballSyncManager::Register(syncId, status->Paintball);

				// 立即同步一次
				PaintballSyncManager::Sync(syncId, masterStatus->Paintball);
			}
			if (IsAircraft())
			{
				masterIsRocket = pTechno->GetTechnoType()->MissileSpawn;
				masterIsSpawned = masterIsRocket || pTechno->GetTechnoType()->Spawned;
			}
		}
		else if (pBullet)
		{
			pMaster = pBullet->Owner;
		}
		status->SetupStand(*Data, pMaster);
		status->MyMasterIsSpawned = masterIsSpawned;
		// 额外附加AE
		AttachEffect* standAEM = nullptr;
		if (Data->Attach && TryGetAEManager<TechnoExt>(pStand, standAEM))
		{
			if (!Data->AttachEffects.empty())
			{
				standAEM->Attach(Data->AttachEffects, {}, false, pStand, pStand->Owner);
			}
			if (!Data->AttachEffectsFromMaster.empty() && pMaster)
			{
				standAEM->Attach(Data->AttachEffectsFromMaster, {}, false, pMaster, pMaster->Owner);
			}
			if (!Data->AttachEffectsFromSource.empty() && AE->pSource)
			{
				standAEM->Attach(Data->AttachEffectsFromSource, {}, false, AE->pSource, AE->pSourceHouse);
			}
			if (!Data->AttachEffectsFromSpawnOwner.empty() && masterIsSpawned && pMaster)
			{
				standAEM->Attach(Data->AttachEffectsFromSpawnOwner, {}, false, pMaster, pMaster->Owner);
			}
		}
	}
	return status;
}

void StandEffect::ExplodesOrDisappear(bool peaceful)
{
	TechnoClass* pTemp = pStand;
	pStand = nullptr;
	if (pTemp)
	{
		bool explodes = !peaceful && (Data->Explodes || notBeHuman || (masterIsRocket && onRocketExplosion && Data->ExplodesWithRocket)) && !pTemp->BeingWarpedOut && !pTemp->WarpingOut;
		TechnoStatus* standStatus = nullptr;
		if (TryGetStatus<TechnoExt>(pTemp, standStatus))
		{
			standStatus->DestroySelf->DestroyNow(!explodes);
			// 如果替身处于Limbo状态，OnUpdate不会执行，需要手动触发
			if ((masterIsRocket || pTemp->InLimbo) && !Common::IsScenarioClear)
			{
				standStatus->OnUpdate();
			}
		}
		else
		{
			if (explodes)
			{
				pTemp->TakeDamage(pTemp->Health + 1, pTemp->GetTechnoType()->Crewed);
			}
			else
			{
				pTemp->Limbo();
				pTemp->UnInit(); // 替身攻击建筑时死亡会导致崩溃，莫名其妙的bug
			}
		}
	}
	Deactivate();
	AE->TimeToDie();
}

void StandEffect::UpdateStateBullet()
{
	// synch target
	RemoveStandIllegalTarget();
	AbstractClass* pTarget = pBullet->Target;
	if (pTarget && Data->SameTarget)
	{
		pStand->SetTarget(pTarget);
	}
	if (!pTarget && Data->SameLoseTarget)
	{
		pStand->SetTarget(pTarget);
		if (pStand->SpawnManager)
		{
			pStand->SpawnManager->Destination = pTarget;
		}
	}
	switch (Data->Targeting)
	{
	case StandTargeting::LAND:
		if (pStand->IsInAir())
		{
			ClearAllTarget(pStand);
		}
		break;
	case StandTargeting::AIR:
		if (!pStand->IsInAir())
		{
			ClearAllTarget(pStand);
		}
		break;
	}
}

void StandEffect::UpdateStateTechno(bool masterIsDead)
{
	// 主身已 UnInit：pTechno 悬空，任何读取都是未定义行为（防御性检查，
	// 正常情况下主身销毁后本效果的派发源也已随之销毁，不会再进入这里）。
	if (_masterDeleted || !pTechno)
	{
		return;
	}
	if (pTechno->IsSinking && Data->RemoveAtSinking)
	{
		ExplodesOrDisappear(true);
		return;
	}
	if (masterIsDead && AE->AEManager->InSelling())
	{
		ExplodesOrDisappear(true);
		return;
	}
	// reset state
	pStand->UpdatePlacement(PlacementType::Remove);

	if (Data->SameHouse && (AEData.ReceiverOwn || Data->IsVirtualTurret))
	{
		pStand->SetOwningHouse(pTechno->Owner);
	}
	// synch state
	// ★ 替身不得进入引擎的溺水状态（2026-10-05 沉船 C0000005 根因修复）：
	// 同步 IsSinking/unknown_3CA 会让引擎把替身当成正在沉没的单位，
	// 走自己的溺水终结逻辑（0x70B570 倾斜积分器每帧 ARF±0.01 与 SameTilter 竞态、
	// 0x707140 沉没计深，到达删除深度即 UnInit 替身）。
	// 替身随 FLH 锚点沉得比主身更深，会先于主身到达删除深度被引擎提前删除，
	// 随后渲染/逻辑帧对 pStand 空指针解引用 —— 这就是沉船崩溃的直接原因。
	// 替身的姿态由 SameTilter 单向驱动、位置由 FLH 锚点驱动，状态保持"正常存活"，
	// 从创建到主身销毁全程由本效果管理（主身 UnInit 时的收尾见 OnTechnoDelete）。
	pStand->IsSinking = false;
	pStand->unknown_3CA = 0;
	pStand->InLimbo = pTechno->InLimbo;
	pStand->OnBridge = pTechno->OnBridge;
	// ★ 主身沉没/坠落期间，替身应处于"死亡"状态（2026-10-05）：
	// 不再同步 IsSinking 给替身（否则引擎会把它当溺水单位提前 UnInit，见上文），
	// 改由这里显式进入"行为死亡"：清空目标、不同步任务、强制 Sleep，
	// 只保留位置/姿态跟随。替身不在任何格子内（上方 UpdatePlacement(Remove) 保持
	// IsOnMap=0），引擎的点选、自动索敌、溅射伤害均走格子枚举 ——
	// 因此替身天然不可被选中、不可被索敌、不可被溅射伤害，加上此处的
	// Sleep + 清目标即等价于旧 IsSinking 同步所提供的"死亡"语义。
	if (MasterGoingDown())
	{
		ClearAllTarget(pStand);
		onStopCommand = false;
		pStand->QueueMission(Mission::Sleep, true);
		return;
	}
	if (Data->SameTilter && !Data->IsTrain)
	{
		// Rocking*PerFrame 是「每帧步进量（角速度）」，不是角度（详见 OnGScreenRender 的说明）。
		// 引擎的倾斜积分器 TechnoClass vt+0x41C(0x70B570) 每个逻辑帧都会跑一次
		// （TechnoClass::Update 0x6FA236，IsVoxel 为真时）。
		// 这里在逻辑帧也清零一次，保证即使某个渲染帧没有派发事件，
		// 替身的倾斜也不会因引擎积分而漂移 —— 替身的角度只由主身单向驱动。
		// 注意：这两个字段虽在 TechnoClass::GetCRC 里，但不在每帧锁步校验值
		// sub_64DAB0 的哈希输入中（那里只有 Location.X/Y 与朝向），写常量 0 两端必然一致。
		pStand->RockingForwardsPerFrame = 0.0f;
		pStand->RockingSidewaysPerFrame = 0.0f;
	}
	if (pStand->Owner == pTechno->Owner)
	{
		// 同阵营限定
		pStand->Cloakable = pTechno->Cloakable;
		pStand->CloakState = pTechno->CloakState;
		pStand->WarpingOut = pTechno->WarpingOut; // 超时空传送冻结
		// 替身不是强制攻击jojo的超时空兵替身
		if (!Data->ForceAttackMaster || !pStand->TemporalImUsing)
		{
			pStand->BeingWarpedOut = pTechno->BeingWarpedOut; // 被超时空兵冻结
		}
		pStand->Deactivated = pTechno->Deactivated; // 遥控坦克

		pStand->IronCurtainTimer = pTechno->IronCurtainTimer;
		pStand->IronTintTimer = pTechno->IronTintTimer;
		// pStand->CloakDelayTimer = pTechno->CloakDelayTimer; // 反复进入隐形
		pStand->IdleActionTimer = pTechno->IdleActionTimer;
		pStand->Berzerk = pTechno->Berzerk;
		pStand->EMPLockRemaining = pTechno->EMPLockRemaining;
		pStand->ShouldLoseTargetNow = pTechno->ShouldLoseTargetNow;

		// synch status
		if (Data->IsVirtualTurret)
		{
			pStand->FirepowerMultiplier = pTechno->FirepowerMultiplier;
			pStand->ArmorMultiplier = pTechno->ArmorMultiplier;
		}

		// synch ammo
		if (Data->SameAmmo)
		{
			pStand->Ammo = pTechno->Ammo;
		}

		// synch Passengers
		if (Data->SamePassengers)
		{
			// Pointer<FootClass> pPassenger = pTechno->Passengers.FirstPassenger;
			// if (!pPassenger.IsNull)
			// {
			//     Pointer<TechnoTypeClass> pType = pPassenger->Type;
			//     Pointer<TechnoClass> pNew = pType->CreateObject(AE.pSourceHouse).Convert<TechnoClass>();
			//     pNew->Put(default, DirType.N);
			//     Logger.Log($"{Game.CurrentFrame} 把jojo的乘客塞进替身里");
			//     pStand->Passengers.AddPassenger(pNew.Convert<FootClass>());
			// }
		}

		// synch Promote
		if (pStand->GetTechnoType()->Trainable)
		{
			if (Data->PromoteFromSpawnOwner && masterIsSpawned && !IsDead(pTechno->SpawnOwner))
			{
				pStand->Veterancy = pTechno->SpawnOwner->Veterancy;
			}
			else if (Data->PromoteFromMaster)
			{
				pStand->Veterancy = pTechno->Veterancy;
			}
		}

		// synch PrimaryFactory
		pStand->IsPrimaryFactory = pTechno->IsPrimaryFactory;
	}

	if (pStand->InLimbo)
	{
		ClearAllTarget(pStand);
		return;
	}

	// Get mission
	Mission mission = pTechno->CurrentMission;

	// check power off and moving
	bool masterIsMoving = mission == Mission::Move || mission == Mission::AttackMove;
	if (IsBuilding())
	{
		if (standIsBuilding && pTechno->Owner == pStand->Owner)
		{
			pStand->Focus = pTechno->Focus;
		}
	}
	else if (!masterIsMoving)
	{
		FootClass* pFoot = abstract_cast<FootClass*, true>(pTechno);
		masterIsMoving = pFoot->Locomotor->Is_Moving() && pFoot->GetCurrentSpeed() > 0;
	}

	// check fire
	bool powerOff = Data->Powered && AE->AEManager->PowerOff;
	bool canFire = !powerOff && (Data->MobileFire || !masterIsMoving);

	// ★ 异阵营（寄生类）替身**自主行动**：只有"同阵营"的替身才跟随主身的任务/目标。
	//   参照取 `pTechno->Owner`（**主身 Owner**），理由与证据：
	//   ① 本函数两端每帧都会跑 ⇒ 参照必须是**同步量**。`HouseClass::CurrentPlayer` 是"本机玩家"
	//      （YRpp/HouseClass.h:173 "House of player at this computer"），各客户端不同 ⇒ 用它必失同步；
	//      `pTechno->Owner` 是对象表里的同步量。
	//   ② 语义就是"同属一个阵营的替身才跟随它的主身"：本组件描述的正是 (主身 pTechno, 替身 pStand)
	//      这一对，判据即 `pStand->Owner == pTechno->Owner`；嵌套替身各有自己的组件
	//      ⇒ 天然"逐层独立判定"。
	//   ③ 证据：替身的 Owner 来自创建它的 AE 来源方（StandEffect.cpp:24
	//      `CreateObject(AE->pSourceHouse)`），"同阵营与否"正是 StandEffect.cpp:84-86 判断
	//      "同阵营同步状态机"用的同一判据；配置 `SameHouse` 时上面 :226-229 已先把替身 Owner
	//      改成主身 Owner ⇒ 仍然跟随。
	bool followMaster = pStand->Owner && pStand->Owner == pTechno->Owner;

	if (canFire)
	{
		// synch mission
		if (followMaster)
		{
			switch (mission)
			{
			case Mission::Guard:
			case Mission::Area_Guard:
				Mission standMission = pStand->CurrentMission;
				if (standMission != Mission::Attack)
				{
					pStand->QueueMission(mission, true);
				}
				break;
			}
		}
	}
	else
	{
		ClearAllTarget(pStand);
		onStopCommand = false;
		pStand->QueueMission(Mission::Sleep, true);
	}

	// synch target
	if (Data->ForceAttackMaster)
	{
		if (!powerOff && !masterIsDead)
		{
			// 替身是超时空兵，被冻住时不能开火，需要特殊处理
			if (pStand->BeingWarpedOut && pStand->TemporalImUsing)
			{
				pStand->BeingWarpedOut = false;
				if (CanAttack(pStand, pTechno, true))
				{
					// 检查ROF
					if (pStand->ROFTimer.Expired())
					{
						int weaponIdx = pStand->SelectWeapon(pTechno);
						pStand->Fire_IgnoreType(pTechno, weaponIdx);
						int rof = 0;
						WeaponStruct* pWeapon = pStand->GetWeapon(weaponIdx);
						if (pWeapon && pWeapon->WeaponType)
						{
							rof = pWeapon->WeaponType->ROF;
						}
						if (rof > 0)
						{
							pStand->ROFTimer.Start(rof);
						}
					}
				}
			}
			else
			{
				if (CanAttack(pStand, pTechno, true))
				{
					pStand->SetTarget(pTechno);
				}
			}
		}
	}
	else if (followMaster)
	{
		if (!onStopCommand)
		{
			// synch Target
			RemoveStandIllegalTarget();
			AbstractClass* pTarget = pTechno->Target;
			if (pTarget)
			{
				if (Data->SameTarget && canFire && CanAttack(pStand, pTarget, true))
				{
					pStand->SetTarget(pTarget);
				}
			}
			else
			{
				if (Data->SameLoseTarget || !canFire)
				{
					ClearAllTarget(pStand);
				}
			}
		}
		else
		{
			onStopCommand = false;
		}
	}
	switch (Data->Targeting)
	{
	case StandTargeting::LAND:
		if (pStand->IsInAir())
		{
			ClearAllTarget(pStand);
		}
		break;
	case StandTargeting::AIR:
		if (!pStand->IsInAir())
		{
			ClearAllTarget(pStand);
		}
		break;
	}

	// synch Moving anim
	if (Data->IsTrain || Data->SameMoving)
	{
		FootClass* pFoot = abstract_cast<FootClass*, true>(pStand);
		ILocomotion* loco = pFoot->Locomotor.get();
		GUID locoId = pStand->GetTechnoType()->Locomotor;
		if (locoId == LocomotionClass::CLSIDs::Drive
			|| locoId == LocomotionClass::CLSIDs::Walk
			|| locoId == LocomotionClass::CLSIDs::Mech
			)
		{
			if (masterIsMoving)
			{
				if (_isMoving)
				{
					if (!Data->IsTrain)
					{
						// 移动前，设置替身的朝向与JOJO相同
						pStand->PrimaryFacing.SetCurrent(pTechno->PrimaryFacing.Current());
					}
					// 往前移动，播放移动动画
					if (_walkRateTimer.Expired())
					{
						// VXL只需要帧动起来，就会播放动画
						// 但SHP动画，还需要检查Loco.Is_Moving()为true时，才可以播放动画 0x73C69D
						pFoot->WalkedFramesSoFar++;
						_walkRateTimer.Start(pFoot->GetTechnoType()->WalkRate);
					}
					// 为SHP素材设置一个总的运动标记
					TechnoStatus* status = nullptr;
					if (TryGetStatus<TechnoExt>(pStand, status))
					{
						status->StandIsMoving = true;
					}
					// DriveLoco.Is_Moving()并不会判断IsDriving
					// ShipLoco.Is_Moving()并不会判断IsDriving
					// HoverLoco.Is_Moving()与前面两个一样，只用位置判断是否在运动
					// 以上几个是通过判断位置来确定是否在运动
					// WalkLoco和MechLoco则只返回IsMoving来判断是否在运动
					if (locoId == LocomotionClass::CLSIDs::Walk)
					{
						WalkLocomotionClass* pLoco = dynamic_cast<WalkLocomotionClass*>(loco);
						pLoco->IsReallyMoving = true;
					}
					else if (locoId == LocomotionClass::CLSIDs::Mech)
					{
						MechLocomotionClass* pLoco = dynamic_cast<MechLocomotionClass*>(loco);
						pLoco->IsMoving = true;
					}
				}
			}
			else
			{
				if (_isMoving)
				{
					// 停止移动
					// 为SHP素材设置一个总的运动标记
					TechnoStatus* status = nullptr;
					if (TryGetStatus<TechnoExt>(pStand, status))
					{
						status->StandIsMoving = false;
					}
					if (locoId == LocomotionClass::CLSIDs::Walk)
					{
						WalkLocomotionClass* pLoco = dynamic_cast<WalkLocomotionClass*>(loco);
						pLoco->IsReallyMoving = false;
					}
					else if (locoId == LocomotionClass::CLSIDs::Mech)
					{
						MechLocomotionClass* pLoco = dynamic_cast<MechLocomotionClass*>(loco);
						pLoco->IsMoving = false;
					}
				}
				_isMoving = false;
			}
		}
	}
}

void StandEffect::RemoveStandIllegalTarget()
{
	AbstractClass* pTarget = pStand->Target;
	if (pTarget && !CanAttack(pStand, pTarget, true))
	{
		ClearAllTarget(pStand);
	}
}

void StandEffect::UpdateLocation(LocationMark locationMark)
{
	if (pStand)
	{
		if (!locationMark.Location.IsEmpty() && !_isMoving)
		{
			_isMoving = _lastLocationMark.Location != locationMark.Location;
		}
		_lastLocationMark = locationMark;
		SetLocation(locationMark.Location);
		SetFacing(locationMark.Dir, false);
	}
}

void StandEffect::SetLocation(CoordStruct location)
{
	if (!pStand || !pTechno)
	{
		return;
	}
	pStand->SetLocation(location);
	if (!Data->IsTrain && Data->SameMoving && Data->StickOnFloor
		&& !pStand->GetTechnoType()->JumpJet
		&& pTechno->GetHeight() <= 0
		// 主身正在下沉 / 坠毁时不要强制贴地：此时主身的 Z 由沉没/坠落逻辑支配
		// （引擎在 IsSinking 时每帧让 AngleRotatedForwards ±0.01 并持续下沉），
		// 强行把替身压到所在格子的地面高度会让它与主身分离
		// —— 典型现象：战列舰下沉时炮塔掉到水面、脱离船身。
		// 该判据是「主身已同步状态」的纯函数，两端一致；
		// 且 sub_64DAB0 的锁步校验只用 Location.X/Y（不用 Z），改 Z 不会造成失同步。
		&& !pTechno->IsSinking && !pTechno->IsCrashing
		)
	{
		pStand->SetHeight(0);
	}
	if (!standIsBuilding)
	{
		pStand->SetFocus(nullptr);
	}
}

void StandEffect::SetFacing(DirStruct dir, bool forceSetTurret)
{
	if (!Data->FreeDirection)
	{
		if (pStand->HasTurret() || Data->LockDirection)
		{
			// 替身有炮塔直接转身体
			pStand->PrimaryFacing.SetCurrent(dir);
		}

		// 检查是否需要同步转炮塔
		if ((!pStand->Target || Data->LockDirection) && !pStand->GetTechnoType()->TurretSpins)
		{
			// Logger.Log("设置替身{0}炮塔的朝向", Type.Type);
			if (forceSetTurret)
			{
				ForceFacing(dir);
			}
			else
			{
				if (pStand->HasTurret())
				{
					// 炮塔的旋转交给炮塔旋转自己控制
					TurretAngle* status = nullptr;
					if (!TryGetStatus<TechnoExt>(pStand, status))
					{
						pStand->SecondaryFacing.SetDesired(dir);
					}
				}
				else
				{
					pStand->PrimaryFacing.SetDesired(dir);
				}
			}
		}
	}
}

void StandEffect::ForceFacing(DirStruct dir)
{
	pStand->PrimaryFacing.SetCurrent(dir);
	if (pStand->HasTurret())
	{
		// 炮塔限界
		TurretAngle* status = nullptr;
		if (TryGetScript<TechnoExt>(pTechno, status) && status->DefaultAngleIsChange(dir))
		{
			pStand->SecondaryFacing.SetCurrent(status->LockTurretDir);
		}
		else
		{
			pStand->SecondaryFacing.SetCurrent(dir);
		}
	}
}

void StandEffect::OnTechnoDelete(EventSystem* sender, Event e, void* args)
{
	if (args == pStand)
	{
		pStand = nullptr;
	}
	else if (pTechno && args == pTechno)
	{
		// ★ 主身被引擎 UnInit（沉没/坠毁动画结束、被摧毁、变卖等）：
		// 锚点消失，替身必须随之收尾。旧行为里替身靠被同步的 IsSinking
		// 由引擎自行沉没删除；现在替身全程由本效果管理（不再进入引擎
		// 溺水状态），若不在此显式移除，替身会以 Mark(Up)+Sleep 状态
		// 残留在渲染层里永远漂浮在原地。
		// peaceful=true：走替身自身的 DestroySelf 机制（与 ExplodesOrDisappear
		// 其余调用点一致），不在主身 UnInit 的重入窗口里直接 UnInit。
		_masterDeleted = true;
		if (pStand)
		{
			ExplodesOrDisappear(true);
		}
	}
}

void StandEffect::ExtChanged()
{
	// 宿主可能已经被 InheritAE 换成另一个对象，先重新识别宿主类型
	ObjectScript::ExtChanged();
	SetupStandStatus();
}

void StandEffect::OnStart()
{
	EventSystems::General.AddHandler(Events::ObjectUnInitEvent, this, &StandEffect::OnTechnoDelete);
	CreateAndPutStand();
}

void StandEffect::End(CoordStruct location)
{
	EventSystems::General.RemoveHandler(Events::ObjectUnInitEvent, this, &StandEffect::OnTechnoDelete);
	if (pStand)
	{
		ExplodesOrDisappear(false);
	}
}

void StandEffect::OnPause()
{
	End(CoordStruct::Empty);
}

void StandEffect::OnRecover()
{
	OnStart();
}

bool StandEffect::MasterGoingDown()
{
	// ★ 两个防护缺一不可（2026-10-05 沉船 C0000005 修复）：
	// 1) pStand 为空 —— 替身已被删除（无论何种原因）时必须返回 false，
	//    让 IsAlive/OnGScreenRender/OnUpdate/OnWarpUpdate 的门控正常早退；
	//    否则放行后的代码会对 pStand 空指针解引用（EAX=0 崩溃的直接成因）。
	// 2) !_masterDeleted —— 主身已 UnInit，pTechno 是悬空指针，不得再读。
	return pStand && !_masterDeleted && pTechno
		&& !Data->RemoveAtSinking && (pTechno->IsSinking || pTechno->IsCrashing);
}

bool StandEffect::IsAlive()
{
	// 主身沉没/坠落期间替身被同步了 IsSinking/IsCrashing（UpdateStateTechno 第 246 行），
	// IsDead()（Status.cpp:157 把 IsSinking/IsCrashing 视同死亡）会把替身误判为死亡。
	// 若在此返回 false，AttachEffect 的门控（AttachEffectScript::AliveCached，
	// 即 OnLogicUpdate / OnGScreenRender 里的替身定位与渲染派发）会把
	// UpdateStandLocation + OnGScreenRender 全部跳过 —— 沉没动画期间替身定位完全停摆。
	if (IsDead(pStand) && !MasterGoingDown())
	{
		return _pause;
	}
	return true;
}

void StandEffect::OnGScreenRender(CoordStruct location)
{
	// ★ 主身沉没/坠落期间不得早退：OwnerIsDead() 在主身血量归零（开始沉没）那一刻
	//   即被 IsDead(pTechno)（把 IsSinking 视同死亡）置真并永久缓存；
	//   替身自身也因被同步 IsSinking 而被 IsDead 误判。
	//   若此处返回，替身的 Location 冻结在水面，之后替身靠引擎自身的
	//   溺水逻辑各自下沉（与主身脱钩）—— 正是"三个对象都在水面各自下沉"的根源。
	if ((IsDead(pStand) || AE->OwnerIsDead()) && !MasterGoingDown())
	{
		return;
	}
	if (standIsBuilding || !IsFoot() || Data->IsTrain || !Data->SameTilter)
	{
		return;
	}

	// ============================================================
	// 同步倾斜（Tilt）
	//
	// 引擎模型（IDA 实证，gamemd 0x70B570 = TechnoClass 虚表槽 +0x41C）：
	//   1) 每个逻辑帧，TechnoClass::Update(0x6F9E50) 会在 0x6FA228 判 IsVoxel()，
	//      为真时在 0x6FA236 调用一次 vt+0x41C。⇒ 每个 voxel 单位每逻辑帧跑一次倾斜更新，
	//      替身（自身也是 TechnoClass）当然也会跑。
	//   2) 该函数在多个分支里都执行同一套「积分」（不变量）：
	//          AngleRotatedSideways += RockingSidewaysPerFrame;   // 0x70B649 / 0x70B659
	//          AngleRotatedForwards += RockingForwardsPerFrame;   // 0x70B65F / 0x70B66B
	//      ⇒ Rocking*PerFrame 的语义是「每帧步进量（角速度）」，绝不是角度。
	//   3) 这两个「角」随后被 locomotor 的 Draw_Matrix 消费（DriveLocomotionClass 的
	//      vt+0x24 = 0x4AFF60；ShipLocomotionClass 的 0x69F670），所以倾斜是经由
	//      Draw_Matrix 进入绘制矩阵的 —— 而 GetMatrix3D() 正是用它来解算替身 Offset，
	//      也就是说 Offset 的位置本来就已跟随主身倾斜，不需要额外补偿。
	//   4) 分支选择：IsSinking(0x3CD) → 每帧 AngleRotatedForwards ±= 0.01（按船头八分圆定符号）；
	//      else IsCrashing(0x425) → 纯积分（非 BalloonHover 单位无回正，会无界累积）；
	//      else 行走摇摆逻辑（RockingSidewaysPerFrame == 0 时会把 AngleRotated* 归零）。
	//
	// 因此这里唯一正确的事情是：
	//   a) 把「角度」写进替身的 AngleRotated*；
	//   b) 把替身自己的 Rocking*PerFrame 清零（★ 关键）。
	// 绝不能把角度写进 Rocking*PerFrame —— 那等于命令替身「每帧再转这么多弧度」，
	// 替身会在自己的积分器里把角度无界累积，典型症状：
	//   · 主身移动/摇摆时替身画面抖动；
	//   · 主身下沉（战列舰）时替身脱离船身，并「自己往一边倾斜」。
	// ============================================================
	float forwards = pTechno->AngleRotatedForwards;
	float sideways = pTechno->AngleRotatedSideways;

	// 把主身（体坐标系）下的倾斜角旋转到替身（体坐标系）。
	// 用两端的真实朝向差做一次二维旋转（rotZ），因此对
	// Stand.Offset.Direction / Stand.Dir / Offset.IsOnTurret（炮塔联动）/ FreeDirection
	// 全部成立，且自动覆盖全部 16 个方向（原实现是手写 8 分支，0/2/6/14 为空实现，
	// 且 8 号方向漏了对 forwards 的取反）。
	// 标定基准：旧代码 case 4/10/12 的行为与 theta = Direction*22.5° 完全等价。
	double theta = pStand->PrimaryFacing.Current().GetRadian()
		- pTechno->PrimaryFacing.Current().GetRadian();
	while (theta > Math::Pi)
	{
		theta -= Math::TwoPi;
	}
	while (theta < -Math::Pi)
	{
		theta += Math::TwoPi;
	}
	if (theta != 0.0)
	{
		const double c = std::cos(theta);
		const double s = std::sin(theta);
		const float f = forwards;
		const float sd = sideways;
		forwards = static_cast<float>(f * c - sd * s);
		sideways = static_cast<float>(f * s + sd * c);
	}

	pStand->AngleRotatedForwards = forwards;
	pStand->AngleRotatedSideways = sideways;
	// ★ 步进量必须为 0：由主身的角度单向驱动替身，禁止替身自己积分
	pStand->RockingForwardsPerFrame = 0.0f;
	pStand->RockingSidewaysPerFrame = 0.0f;

	// 同步 替身 与 JOJO 的地形角度
	ILocomotion* masterLoco = abstract_cast<FootClass*, true>(pTechno)->Locomotor.get();
	ILocomotion* standLoco = abstract_cast<FootClass*, true>(pStand)->Locomotor.get();

	DWORD previousRamp = 0;
	DWORD currentRamp = 0;

	if (DriveLocomotionClass* pMasterDriveLoco = dynamic_cast<DriveLocomotionClass*>(masterLoco))
	{
		previousRamp = pMasterDriveLoco->PreviousRamp;
		currentRamp = pMasterDriveLoco->CurrentRamp;
	}
	else if (ShipLocomotionClass* pMasterShipLoco = dynamic_cast<ShipLocomotionClass*>(masterLoco))
	{
		previousRamp = pMasterShipLoco->PreviousRamp;
		currentRamp = pMasterShipLoco->CurrentRamp;
	}

	if (DriveLocomotionClass* pStandDriveLoco = dynamic_cast<DriveLocomotionClass*>(standLoco))
	{
		pStandDriveLoco->PreviousRamp = previousRamp;
		pStandDriveLoco->CurrentRamp = currentRamp;
	}
	else if (ShipLocomotionClass* pStandShipLoco = dynamic_cast<ShipLocomotionClass*>(standLoco))
	{
		pStandShipLoco->PreviousRamp = previousRamp;
		pStandShipLoco->CurrentRamp = currentRamp;
	}
}

void StandEffect::OnPut(CoordStruct* pCoord, DirType dirType)
{
	if (IsDead(pStand))
	{
		return;
	}
	if (pStand->InLimbo)
	{
		CoordStruct location = *pCoord;
		if (!TryPutTechno(pStand, location, nullptr, true))
		{
			End(location);
		}
	}
}

void StandEffect::OnRemove()
{
	if (IsDead(pStand))
	{
		return;
	}
	if (!AE->OwnerIsDead())
	{
		pStand->Limbo();
	}
}

void StandEffect::OnUpdate()
{
	if (IsDead(pStand) && !MasterGoingDown())
	{
		return;
	}
	if (pTechno)
	{
		UpdateStateTechno(AE->OwnerIsDead());
	}
	else
	{
		UpdateStateBullet();
	}
}

void StandEffect::OnWarpUpdate()
{
	if (IsDead(pStand) && !MasterGoingDown())
	{
		return;
	}
	if (pTechno)
	{
		UpdateStateTechno(AE->OwnerIsDead());
	}
	else
	{
		UpdateStateBullet();
	}
}

void StandEffect::OnTemporalEliminate(TemporalClass* pTemporal)
{
	End(pObject->GetCoords());
}

void StandEffect::OnReceiveDamageDestroy()
{
	onReceiveDamageDestroy = true;
	// 我不做人了JOJO
	notBeHuman = Data->ExplodesWithMaster;
	if (pTechno)
	{
		if (pStand)
		{
			// 沉没，坠机，不销毁替身
			pStand->QueueMission(Mission::Sleep, true);
		}
	}
	else if (pBullet)
	{
		// 抛射体上的宿主直接炸
		End(pBullet->GetCoords());
	}
}

// 收端处理器：由隧道 `TechnoScriptCommandEvent::Respond` 在**两端**各派发一次（目标就是该 techno
// 自己）。本层只做**下潜**：把这条命令转给"自己这一层的替身"（`pStand`），于是嵌套替身
// （替身的替身）逐层收到命令。
//
// ★ 门控与"由谁发送"：
//   · 发送由 `Raise*Command` 把关，三个条件：`IsDead(pStand)`、`pStand->IsSelected`
//     （**本机**选中集，只用来决定"这一端要不要投这条消息"，与主身穿在热键执行体里的写法同构）、
//     `pStand->Owner == pTechno->Owner`（替身 Owner 来自创建它的 AE 来源方 ⇒ **同步量**）。
//   · "原版命令恰好一条"由收端 `On*Command_Stand` 的
//     `!IsEngineAlreadySent() && IsCurrentInitiator()` 保证（论证见 TechnoScriptCommandEvent.h）。
//   · 本函数自身不读任何本机状态、不发事件（发送全部交给 `Raise*Command`）。
//
// ★ 逐层下潜是**消息链**（第 N 层收到消息 → 投第 N+1 层的消息 → 由引擎事件循环再派发），
//   **不是 C++ 递归** ⇒ 不占栈、不会爆栈；替身构成以 `pMyMaster` 为父的有向树（每个替身
//   由唯一的 StandEffect 组件创建）⇒ 无环、有限 ⇒ **无需深度保护**。
//   ⚠ 已知代价（不构成失同步）：第 2 层起，`Respond` 在**两端**都会走到本函数 ⇒ 第 d 层实际
//   投出 2^(d-1) 条消息（每层 ×2）。重复派发在两端是**对称**的（收端补发仍有
//   `IsCurrentInitiator()` 收敛），但会放大 `EventClass::OutList`（上限 128）的压力。
//   若要收紧，可在 `Raise*Command` 里加 `IsCurrentInitiator()` 门控，只让发起端投递。
void StandEffect::OnGuardCommand()
{
	RaiseGuardCommand();
}

// 收端处理器：同上（隧道 Respond 两端各派发一次）。Stop 的"原版命令"是那条 IDLE 事件。
void StandEffect::OnStopCommand()
{
	RaiseStopCommand();
}

// 发端，Master收到Guard命令后，替替身派发相同的命令
void StandEffect::RaiseGuardCommand()
{
	if (IsDead(pStand) || pStand->IsSelected)
	{
		return;
	}
	if (pStand->Owner && pStand->Owner == pTechno->Owner)
	{
		// engineAlreadySent = false：替身没被选中 ⇒ 引擎不会给它发原版命令，
		// 收端 OnGuardCommand_Stand 需在发起端补发（恰好一条）。
		TechnoScriptCommandEvent::Raise(pStand, TechnoScriptCommandEvent::Command::Guard, false);
	}
}

// 发端，Master收到Stop命令后，替替身派发相同的命令
void StandEffect::RaiseStopCommand()
{
	if (IsDead(pStand) || pStand->IsSelected)
	{
		return;
	}
	if (pStand->Owner && pStand->Owner == pTechno->Owner)
	{
		// engineAlreadySent = false：同上，收端补发原版 IDLE（恰好一条）。
		TechnoScriptCommandEvent::Raise(pStand, TechnoScriptCommandEvent::Command::Stop, false);
	}
}

void StandEffect::OnRocketExplosion()
{
	onRocketExplosion = true;
	if (!onReceiveDamageDestroy)
	{
		ExplodesOrDisappear(Data->ExplodesWithMaster);
	}

}

