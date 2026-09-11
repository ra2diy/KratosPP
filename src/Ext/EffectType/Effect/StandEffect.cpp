#include "StandEffect.h"

#include <DriveLocomotionClass.h>
#include <MechLocomotionClass.h>
#include <ShipLocomotionClass.h>
#include <WalkLocomotionClass.h>

#include <Ext/Common/PaintballSyncManager.h>

#include <Ext/Helper/FLH.h>
#include <Ext/Helper/Gift.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>

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
	pStand->IsSinking = pTechno->IsSinking;
	pStand->unknown_3CA = pTechno->unknown_3CA;
	pStand->InLimbo = pTechno->InLimbo;
	pStand->OnBridge = pTechno->OnBridge;
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
	if (canFire)
	{
		// synch mission
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
	else
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
	pStand->SetLocation(location);
	if (!Data->IsTrain && Data->SameMoving && Data->StickOnFloor
		&& !pStand->GetTechnoType()->JumpJet
		&& pTechno->GetHeight() <= 0
		)
	{
		pStand->SetHeight(0);
	}
	pStand->SetFocus(nullptr);
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

bool StandEffect::IsAlive()
{
	if (IsDead(pStand))
	{
		return _pause;
	}
	return true;
}

void StandEffect::OnGScreenRender(CoordStruct location)
{
	if (IsDead(pStand) || AE->OwnerIsDead())
	{
		return;
	}
	if (!standIsBuilding && IsFoot())
	{
		// synch tilt
		if (!Data->IsTrain)
		{
			if (Data->SameTilter)
			{
				// Stand same tilter
				// rocker Squid capture ship
				// pStand->AngleRotatedForwards = pMaster->AngleRotatedForwards;
				// pStand->AngleRotatedSideways = pMaster->AngleRotatedSideways;

				if (Data->SameTilter)
				{
					float forwards = pTechno->AngleRotatedForwards;
					float sideways = pTechno->AngleRotatedSideways;
					float t = 0.0;
					// 计算方向
					switch (Data->Offset.Direction)
					{
					case 0: // 正前 N
						break;
					case 2: // 前右 NE
						break;
					case 4: // 正右 E
						t = forwards;
						forwards = -sideways;
						sideways = t;
						break;
					case 6: // 右后 SE
						break;
					case 8: // 正后 S
						sideways = -sideways;
						break;
					case 10: // 后左 SW
					case 12: // 正左 W
						t = forwards;
						forwards = sideways;
						sideways = -t;
						break;
					case 14: // 前左 NW
						break;
					}
					pStand->AngleRotatedForwards = forwards;
					pStand->AngleRotatedSideways = sideways;
					pStand->RockingForwardsPerFrame = forwards;
					pStand->RockingSidewaysPerFrame = sideways;

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
			}
		}
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
	if (IsDead(pStand))
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
	if (IsDead(pStand))
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

void StandEffect::OnGuardCommand()
{
	if (IsDead(pStand))
	{
		return;
	}
	if (!pStand->IsSelected)
	{
		// 执行替身的OnStop
		if (auto ext = TechnoExt::ExtMap.Find(pStand))
			ext->_GameObject->Foreach([](Component* c)
				{ if (auto cc = dynamic_cast<ITechnoScript*>(c)) { cc->OnGuardCommand(); } });
	}
}

void StandEffect::OnStopCommand()
{
	if (IsDead(pStand))
	{
		return;
	}
	ClearAllTarget(pStand);
	onStopCommand = true;
	if (!pStand->IsSelected)
	{
		pStand->ClickedEvent(NetworkEvents::Idle);
		// 执行替身的OnStop
		if (auto ext = TechnoExt::ExtMap.Find(pStand))
			ext->_GameObject->Foreach([](Component* c)
				{ if (auto cc = dynamic_cast<ITechnoScript*>(c)) { cc->OnStopCommand(); } });
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

