#include <exception>
#include <Windows.h>

#include <TechnoClass.h>
#include <UnitClass.h>

#include <Extension.h>
#include <Utilities/Macro.h>

#include <Extension/TechnoExt.h>
#include <Extension/TechnoTypeExt.h>

#include <Ext/Helper/FLH.h>
#include <Ext/Helper/Scripts.h>

#include <Ext/TechnoType/DisguiseData.h>
#include <Ext/TechnoType/JumpjetCarryall.h>
#include <Ext/TechnoType/SHPFVTurretData.h>
#include <Ext/TechnoType/Spawn.h>
#include <Ext/TechnoType/TechnoStatus.h>
#include <Ext/TechnoType/TurretAngle.h>

#pragma region Unit Deploy
DEFINE_HOOK(0x6FF8F1, TechnoClass_DeployFire, 0x6)
{
	GET(TechnoClass*, pTechno, ESI);
	if (TechnoStatus* status = GetStatus<TechnoExt, TechnoStatus>(pTechno))
	{
		if (status->DeployState == DeployState::Deploying)
		{
			status->DeployState = DeployState::Deployed;
		}
		else if (status->DeployState == DeployState::Undeploying)
		{
			status->DeployState = DeployState::Undeployed;
		}
	}
	return 0;
}

DEFINE_HOOK(0x6FF923, TechnoClass_DeployFire_FireOnce, 0x6)
{
	GET(TechnoClass*, pTechno, ESI);
	if (pTechno->CurrentMission == Mission::Unload)
	{
		pTechno->QueueMission(Mission::Stop, true);
	}
	return 0;
}

// Phobos Skip this.
// New logic in TechnoClass_Update_DeployTo(), so don't need this.
// DEFINE_HOOK(0x739C7A, UnitClass_SimpleDeployer_Deployed, 0x6)
// {
// 	GET(TechnoClass*, pTechno, ESI);
// 	if (TechnoStatus* status = GetStatus<TechnoExt, TechnoStatus>(pTechno))
// 	{
// 		// Debug::Log("UnitClass_Deployed\n");
// 		status->DeployState = DeployState::Deployed;
// 	}
// 	return 0;
// }
#pragma endregion

#pragma region FLH free
DEFINE_HOOK(0x6F3B5C, UnitClassClass_GetFLH_UnbindTurret, 0x6)
{
	enum { turretMitrix = 0x6F3C1A, nextCheck = 0x6F3B62, takeOver = 0x6F3C52 };
	GET(TechnoClass*, pTechno, EBX);
	if (pTechno)
	{
		if (TechnoStatus* status = GetStatus<TechnoExt, TechnoStatus>(pTechno))
		{
			GET_STACK(int, weaponIdx, 0xE0);
			status->FLHIndex = weaponIdx;
			if (pTechno->HasTurret() && status->IsFLHOnBody(weaponIdx))
			{
				Matrix3D matrix = GetMatrix3D(pTechno);
				R->Stack(0x48, matrix);
				return takeOver;
			}
		}
		return nextCheck;
	}
	return turretMitrix;
}

DEFINE_HOOK(0x6F3D2F, UnitClassClass_GetFLH_OnTarget, 0x5)
{
	GET(TechnoClass*, pTechno, EBX);
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTechno, status) && status->IsFLHOnTarget())
	{
		GET(CoordStruct*, pRenderCoord, EAX);
		*pRenderCoord = pTechno->Target->GetCoords();
	}
	return 0;
}
#pragma endregion

#pragma region Unit explosion anims
DEFINE_HOOK(0x738749, UnitClass_Destroy_Explosion_Remap, 0x6)
{
	GET(TechnoClass*, pTechno, ESI);
	GET(AnimClass*, pAnim, EAX);
	if (pAnim)
	{
		pAnim->Owner = pTechno->Owner;
	}
	return 0;
}

// Take over to Create DestroyAnim Anim
DEFINE_HOOK(0x738801, UnitClass_Destroy_DestroyAnim_Remap, 0x6)
{
	GET(TechnoClass*, pTechno, ESI);
	if (TechnoStatus* status = GetStatus<TechnoExt, TechnoStatus>(pTechno))
	{
		if (status->PlayDestroyAnims())
		{

			return 0x73887E;
		}
	}
	return 0;
}
#pragma endregion

#pragma region UnitClass Disguise
DEFINE_HOOK(0x6F4222, TechnoClass_Init_PermaDisguise, 0x7)
{
	GET(TechnoClass*, pTechno, ESI);
	GET(HouseClass*, pHouse, EAX);
	bool custom = false;
	ObjectTypeClass* pDisguise = nullptr;
	switch (pTechno->WhatAmI())
	{
	case AbstractType::Unit:
	{
		// 伪装成自己
		pDisguise = pTechno->GetType();
		// 检查微观设置
		DisguiseData* unitData = INI::GetConfig<DisguiseData>(INI::Rules, pDisguise->ID)->Data;
		if (IsNotNone(unitData->DefaultUnitDisguise))
		{
			UnitTypeClass* pType = UnitTypeClass::Find(unitData->DefaultUnitDisguise.c_str());
			if (pType)
			{
				// 伪装成指定的坦克
				pDisguise = dynamic_cast<ObjectTypeClass*>(pType);
				custom = true;
			}
		}
		if (!custom)
		{
			// 阵营所属的伪装坦克
			int sideIndex = pHouse->SideIndex;
			if (sideIndex >= 0 && SideClass::Array->Count > sideIndex)
			{
				std::string sideId = SideClass::Array->GetItem(sideIndex)->ID;
				DisguiseData* sideData = INI::GetConfig<DisguiseData>(INI::Rules, sideId.c_str())->Data;
				if (IsNotNone(sideData->DefaultUnitDisguise))
				{
					UnitTypeClass* pType = UnitTypeClass::Find(sideData->DefaultUnitDisguise.c_str());
					if (pType)
					{
						// 伪装成指定的坦克
						pDisguise = dynamic_cast<ObjectTypeClass*>(pType);
						custom = true;
					}
				}
			}
		}
		custom = true;
		break;
	}
	case AbstractType::Infantry:
	{
		// 阵营设置，Ares接管，不理会，只管微观设置
		DisguiseData* infData = INI::GetConfig<DisguiseData>(INI::Rules, pTechno->GetTechnoType()->ID)->Data;
		if (IsNotNone(infData->DefaultDisguise))
		{
			InfantryTypeClass* pType = InfantryTypeClass::Find(infData->DefaultDisguise.c_str());
			if (pType)
			{
				// 伪装成指定的步兵
				pDisguise = dynamic_cast<ObjectTypeClass*>(pType);
				custom = true;
			}
		}
		break;
	}
	}
	if (custom && pDisguise)
	{
		pTechno->Disguised = true;
		pTechno->DisguisedAsHouse = pHouse;
		pTechno->Disguise = pDisguise;
		return 0x6F4277;
	}
	return 0;
}

DEFINE_HOOK(0x7466D8, UnitClass_Set_Disguise, 0xA)
{
	GET(AbstractClass*, pTarget, ESI);
	if (abstract_cast<ObjectClass*, true>(pTarget)->IsDisguised())
	{
		// 伪装成目标单位的伪装
		return 0x7466E6;
	}
	else if (pTarget->WhatAmI() == AbstractType::Unit)
	{
		// 我自己来
		TechnoClass* pTargetTechno = abstract_cast<TechnoClass*, true>(pTarget);
		GET(TechnoClass*, pTechno, EDI);
		pTechno->Disguise = pTargetTechno->GetType();
		R->EAX(reinterpret_cast<unsigned int>(pTarget->GetOwningHouse()));
		return 0x746704;
	}
	return 0x746712;
}

DEFINE_HOOK(0x746AFF, UnitClass_Disguise_Update_MoveToClear, 0xA)
{
	GET(TechnoClass*, pTechno, ESI);
	ObjectTypeClass* pDisguise = pTechno->Disguise;
	// Don't clear UnitType
	if (!pDisguise || pDisguise->WhatAmI() != AbstractType::UnitType)
	{
		pTechno->ClearDisguise();
	}
	return 0x746A9C;
}

DEFINE_HOOK(0x746B6D, UnitClass_Disguise_FullName, 0x5)
{
	GET(TechnoClass*, pTechno, ESI);
	if (pTechno->IsDisguised())
	{
		HouseClass* pPlayer = HouseClass::CurrentPlayer;
		HouseClass* pHouse = pTechno->Owner;
		if (!pHouse
			|| (pHouse != pPlayer && !pHouse->IsAlliedWith(pPlayer)))
		{
			// 显示伪装对象的名字
			return 0x746B48;
		}
	}
	if (pTechno->GetTechnoType()->HasMultipleTurrets())
	{
		// 显示乘客的名字
		return 0x746B7A;
	}
	// 显示单位自己的名字
	return 0x746C7A;
}


DEFINE_HOOK(0x736A26, UnitClass_Rotation_SetTurretFacingToTarget_Skip, 0x6)
{
	GET(TechnoClass*, pTechno, ESI);

	// TurretSpin AE 激活时炮塔始终自旋，跳过引擎朝目标的瞄准写入（有目标也覆盖瞄准）
	TechnoStatus* tStatus = nullptr;
	if (TryGetStatus<TechnoExt, TechnoStatus>(pTechno, tStatus) && tStatus->_turretSpinActive)
	{
		return 0x736A8E;
	}

	TurretAngle* status = nullptr;
	if (TryGetScript<TechnoExt, TurretAngle>(pTechno, status) && status->LockTurret)
	{
		// 不允许转炮塔
		R->EDX(&status->LockTurretDir);
	}
	return 0;
}

DEFINE_HOOK(0x736BCA, UnitClass_Rotation_SetTurretFacing_NoTargetAndStanding, 0x5)
{
	GET(TechnoClass*, pTechno, ESI);

	// TurretSpin AE 激活时炮塔始终自旋，跳过引擎无目标时的回正写入
	TechnoStatus* tStatus = nullptr;
	if (TryGetStatus<TechnoExt, TechnoStatus>(pTechno, tStatus) && tStatus->_turretSpinActive)
	{
		return 0x736BE2;
	}

	TurretAngle* status = nullptr;
	if (TryGetScript<TechnoExt, TurretAngle>(pTechno, status) && status->ChangeDefaultDir)
	{
		// 炮塔转到指定角度，非车体前方
		pTechno->SecondaryFacing.SetDesired(status->LockTurretDir);
		return 0x736BE2;
	}
	return 0;
}

DEFINE_HOOK(0x736BBB, UnitClass_Rotation_SetTurretFacing_NoTargetAndMoving, 0x5)
{
	GET(TechnoClass*, pTechno, ESI);

	// TurretSpin AE 激活时炮塔始终自旋，跳过引擎无目标时的回正写入
	TechnoStatus* tStatus = nullptr;
	if (TryGetStatus<TechnoExt, TechnoStatus>(pTechno, tStatus) && tStatus->_turretSpinActive)
	{
		return 0x736BE2;
	}

	TurretAngle* status = nullptr;
	if (TryGetScript<TechnoExt, TurretAngle>(pTechno, status) && status->LockTurret)
	{
		// 炮塔转到指定角度，非车体前方
		pTechno->SecondaryFacing.SetDesired(status->LockTurretDir);
		return 0x736BE2;
	}
	return 0;
}

DEFINE_HOOK(0x736B7E, UnitClass_Rotation_SetTurretFacing_Skip, 0xA)
{
	GET(TechnoClass*, pTechno, ESI);
	if (pTechno->IsUnderEMP())
	{
		// 不允许转炮塔
		return 0x736BE2;
	}
	return 0;
}


DEFINE_HOOK(0x73CD01, UnitClass_Draw_ChangeTurret_SHPIFV, 0x5)
{
	GET(TechnoClass*, pTechno, EBP);
	if (!pTechno->IsVoxel() && pTechno->GetTechnoType()->Gunner && pTechno->Passengers.NumPassengers > 0)
	{
		// 最后的乘客也就是第一个乘客就是枪手
		ObjectClass* pPassenger = pTechno->Passengers.FirstPassenger;
		ObjectClass* pGunner = pPassenger;
		do
		{
			if (pPassenger)
			{
				pGunner = pPassenger;
			}
		} while (pPassenger && (pPassenger = pPassenger->NextObject) != nullptr);
		int ifvMode = pGunner->GetTechnoType()->IFVMode + 1;
		if (SHPFVTurretData* typeData = INI::GetConfig<SHPFVTurretData>(INI::Rules, pTechno->GetTechnoType()->ID)->Data)
		{
			if (typeData->Datas.find(ifvMode) != typeData->Datas.end())
			{
				SHPFVTurretEntity data = typeData->Datas[ifvMode];
				GET(int, index, EAX);
				index += data.WeaponTurretFrameIndex;
				R->EAX(index);
				if (IsNotNone(data.WeaponTurretCustomSHP))
				{
					if (SHPStruct* pCustomSHP = FileSystem::LoadSHPFile(data.WeaponTurretCustomSHP.c_str()))
					{
						R->EDI(pCustomSHP);
					}
				}
				R->ECX(R->EBP());
				return 0x73CD06;
			}
		}
	}
	return 0;
}

DEFINE_HOOK(0x73C71D, UnitClass_DrawSHP_FacingDir, 0x6)
{
	GET(TechnoClass*, pTechno, EBP);
	if (pTechno->IsDisguised() && !pTechno->IsClearlyVisibleTo(HouseClass::CurrentPlayer))
	{
		// WWSB 自己算起始帧
		if (UnitTypeClass* pTargetType = dynamic_cast<UnitTypeClass*>(pTechno->GetDisguise(true)))
		{
			if (pTargetType->WhatAmI() == AbstractType::UnitType)
			{
				int facing = pTargetType->Facings;
				// 0的方向是游戏中的北方，是↗，素材0帧是朝向0点，是↑
				int index = Dir2FrameIndex(pTechno->PrimaryFacing.Current(), facing);
				// Logger.Log($"{Game.CurrentFrame} OOXX dirIndex = {index}, facing = {facing}, walk = {pTargetType->WalkFrames}, fire = {pTargetType->FiringFrames}, {R->EDX} {R->EBX} x {R->ESI}");
				// EDX是播放的帧序号
				GET(int, frameOffset, EDX);
				if (frameOffset == 0)
				{
					frameOffset += index;
					// 站立状态
					R->EDX(frameOffset);
				}
				else
				{
					// 移动状态
					// ???, UnitTypeClass.WalkFrames拿到的不是WalkFrames
					int walkFrames = INI::GetSection(INI::Art, pTargetType->ID)->Get("WalkFrames", 1);
					frameOffset += index * walkFrames + pTargetType->StartWalkFrame;
					R->EDX(frameOffset);
				}
			}
		}
	}
	return 0;
}

DEFINE_HOOK(0x73C655, UnitClass_DrawSHP_TechnoType, 0x6)
{
	GET(TechnoClass*, pTechno, EBP);
	ObjectTypeClass* pTargetType = pTechno->GetDisguise(true);
	if (pTechno->IsDisguised() && !pTechno->IsClearlyVisibleTo(HouseClass::CurrentPlayer)
		&& pTargetType->WhatAmI() == AbstractType::UnitType)
	{
		R->ECX(reinterpret_cast<unsigned int>(pTargetType));
		return 0x73C65B;
	}
	return 0;
}

DEFINE_HOOK(0x73C69D, UnitClass_DrawSHP_TechnoType2, 0x6)
{
	GET(TechnoClass*, pTechno, EBP);
	ObjectTypeClass* pTargetType = pTechno->GetDisguise(true);
	if (pTechno->IsDisguised() && !pTechno->IsClearlyVisibleTo(HouseClass::CurrentPlayer)
		&& pTargetType->WhatAmI() == AbstractType::UnitType)
	{
		R->ECX(reinterpret_cast<unsigned int>(pTargetType));
		return 0x73C6A3;
	}
	// Stand is Moving
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTechno, status) && status->StandIsMoving)
	{
		return 0x73C702;
	}
	return 0;
}

DEFINE_HOOK(0x73C702, UnitClass_DrawSHP_TechnoType3, 0x6)
{
	GET(TechnoClass*, pTechno, EBP);
	ObjectTypeClass* pTargetType = pTechno->GetDisguise(true);
	if (pTechno->IsDisguised() && !pTechno->IsClearlyVisibleTo(HouseClass::CurrentPlayer)
		&& pTargetType->WhatAmI() == AbstractType::UnitType)
	{
		R->ECX(reinterpret_cast<unsigned int>(pTargetType));
		return 0x73C708;
	}
	return 0;
}

DEFINE_HOOK(0x73C725, UnitClass_DrawSHP_HasTurret, 0x6)
{
	GET(TechnoClass*, pTechno, EBP);
	if (pTechno->IsDisguised() && !pTechno->IsClearlyVisibleTo(HouseClass::CurrentPlayer))
	{
		ObjectTypeClass* pTargetType = pTechno->GetDisguise(true);
		auto const pTargetTechnoType = dynamic_cast<TechnoTypeClass*>(pTargetType);
		if (pTargetType && pTargetTechnoType && !pTargetTechnoType->Turret)
		{
			// no turret
			return 0x73CE0D;
		}
	}
	return 0;
}

DEFINE_HOOK_AGAIN(0x73B765, UnitClass_DrawVoxel_TurretFacing, 0x5)
DEFINE_HOOK_AGAIN(0x73BA78, UnitClass_DrawVoxel_TurretFacing, 0x6)
DEFINE_HOOK_AGAIN(0x73BD8B, UnitClass_DrawVoxel_TurretFacing, 0x5)
DEFINE_HOOK(0x73BDA3, UnitClass_DrawVoxel_TurretFacing, 0x5)
{
	GET(TechnoClass*, pTechno, EBP);
	if (!pTechno->GetTechnoType()->Turret && pTechno->IsDisguised() && !pTechno->IsClearlyVisibleTo(HouseClass::CurrentPlayer))
	{
		// 本体没有炮塔
		ObjectTypeClass* pTargetType = pTechno->GetDisguise(true);
		if (pTargetType && dynamic_cast<TechnoTypeClass*>(pTargetType)->Turret)
		{
			// 伪装的对象有炮塔，将身体的朝向赋给炮塔
			GET(DirStruct*, pDir, EAX);
			DirStruct dir = pTechno->PrimaryFacing.Current();
			pDir->SetValue(dir.GetValue());
		}
	}
	return 0;
}

DEFINE_HOOK(0x73B8DB, UnitClass_DrawVoxel_HasChargeTurret, 0x8)
{
	GET(TechnoClass*, pTechno, EBP);
	GET(TechnoTypeClass*, pTechnoType, EBX);
	// 渲染伪装对象
	if (pTechnoType != pTechno->GetTechnoType())
	{
		if (!pTechnoType->HasMultipleTurrets() || pTechnoType->IsGattling)
		{
			// 渲染炮管
			return 0x73B92F;
		}
		else
		{
			// 继续检查
			return 0x73B8FC;
		}
	}
	return 0;
}

// DEFINE_HOOK_AGAIN(0x73BA53, UnitClass_DrawVoxel_HasChargeTurret2, 0x7) // TurretOffset
DEFINE_HOOK(0x73BC1D, UnitClass_DrawVoxel_HasChargeTurret2, 0x7)
{
	GET(TechnoClass*, pTechno, EBP);
	GET(TechnoTypeClass*, pTechnoType, EBX);
	// 渲染伪装对象
	if (pTechnoType != pTechno->GetTechnoType())
	{
		if (pTechnoType->TurretCount > 0 && !pTechnoType->IsGattling)
		{
			if (pTechno->CurrentTurretNumber < 0)
			{
				R->Stack<int>(0x1C, 0);
			}
			return 0x73BC35;
		}
		else
		{
			return 0x73BD79;
		}
	}
	return 0;
}

DEFINE_HOOK(0x706724, TechnoClass_Draw_VXL_Disguise_Blit_Flags, 0x5)
{
	return 0x706731;
}
#pragma endregion


DEFINE_HOOK(0x73C485, UnitClass_Draw_Voxel_Shadow_WO_Skip, 0x8)
{
	GET(TechnoClass*, pTechno, EBP);
	SpawnManagerClass* pSpawnManager = pTechno->SpawnManager;
	Spawn* spawnEx = nullptr;
	if (pSpawnManager
		&& pSpawnManager->CountDockedSpawns() < pSpawnManager->SpawnCount
		&& pTechno->GetTechnoType()->NoSpawnAlt
		&& TryGetScript<TechnoExt>(pTechno, spawnEx)
		&& spawnEx->GetSpawnData()->NoShadowSpawnAlt)
	{
		// skip draw shadow
		return 0x73C5C9;
	}
	return 0;
}

#pragma region Balloon Jumpjet Carryall and Landing
// Check CanDeploy when push the DeployCommand button
DEFINE_HOOK(0x700E8B, TechnoClass_CanDeploy_Carryall_Drop, 0x6)
{
	GET(TechnoTypeClass*, pType, EAX);
	if (pType->Passengers == 0 && pType->BalloonHover && pType->Locomotor == LocomotionClass::CLSIDs::Jumpjet)
	{
		GET(TechnoClass*, pTechno, ESI);
		JumpjetCarryall* carry = GetScript<TechnoExt, JumpjetCarryall>(pTechno);
		if (carry && carry->pPayload)
		{
			// skip check other, goto check LandType when pTechno has Passengers
			return 0x700EEC;
		}
	}
	// continue to check !IsSimpleDeploy and other, then return false
	return 0;
}

// Landing to pickup the passengers
DEFINE_HOOK(0x73D6F8, UnitClass_Mission_Unload_Transporter, 0x6)
{
	GET(TechnoClass*, pTechno, ESI);
	if (pTechno->GetTechnoType()->BalloonHover && pTechno->IsInAir())
	{
		bool canFall = true;
		if (JumpjetCarryall* jjCarryall = GetScript<TechnoExt, JumpjetCarryall>(pTechno))
		{
			canFall = !jjCarryall->InMission();
		}
		if (canFall)
		{
			if (TechnoStatus* status = GetStatus<TechnoExt, TechnoStatus>(pTechno))
			{
				status->BalloonFall = true;
			}
		}
		else
		{
			// skip this mission
			return 0x73DFB0;
		}
	}
	return 0;
}

// DEFINE_HOOK(0x73D6E6, UnitClass_Mission_Unload_Carryall_Drop, 0x6) //phobos skipped this address
DEFINE_HOOK(0x73D6EC, UnitClass_Mission_Unload_Carryall_Drop, 0x6)
{
	GET(TechnoClass*, pTechno, ESI);
	JumpjetCarryall* carry = GetScript<TechnoExt, JumpjetCarryall>(pTechno);
	if (carry && carry->pPayload)
	{
		carry->DropPayload();
		bool noPassengers = pTechno->GetTechnoType()->Passengers == 0;
		if (noPassengers)
		{
			pTechno->QueueMission(Mission::Guard, false);
		}
		if (pTechno->Passengers.FirstPassenger || noPassengers)
		{
			// countine to landing nearby and unload passenger
			return 0x73D772;
		}
	}
	return 0;
}

// 选中JJ后鼠标指向其他单位的时候，显示吊运的图标
DEFINE_HOOK(0x74041B, UnitClass_WhatAction_Carryall_Lifting, 0x5)
{
	enum { Attack = 0x740420, Other = 0x74043B };

	GET(Action, action, EBX);
	GET(TechnoClass*, pTechno, ESI);
	GET(TechnoClass*, pTarget, EDI);

	if (action == Action::Select && pTarget != pTechno && !pTarget->IsInAir())
	{
		JumpjetCarryall* carry = GetScript<TechnoExt, JumpjetCarryall>(pTechno);
		if (carry && carry->CanLift(pTarget))
		{
			action = Action::Tote;
			R->EBX(action);
		}
	}
	return action == Action::Attack ? Attack : Other;
}

// 没有乘客位的JJ鼠标指向自己的时候，显示部署图标
DEFINE_HOOK(0x74000B, UnitClass_WhatAction_Carryall_Droping, 0x6)
{
	GET(TechnoClass*, pTechno, ESI);
	JumpjetCarryall* carry = GetScript<TechnoExt, JumpjetCarryall>(pTechno);
	if (carry && carry->pPayload)
	{
		return 0x7400FA; // Action::Self_Deploy
	}
	return 0;
}

// 有乘客位的JJ鼠标指向自己的时候，显示部署图标
DEFINE_HOOK(0x73FFF4, UnitClass_WhatAction_TransportHover, 0x8)
{
	GET(TechnoClass*, pTechno, ESI);
	if (pTechno->GetTechnoType()->BalloonHover && pTechno->IsInAir())
	{
		if (pTechno->CanDeploySlashUnload())
		{
			R->Stack(0x28, Action::Self_Deploy);
			return 0x73FFFC;
		}
	}
	return 0;
}

DEFINE_HOOK(0x7388FD, UnitClass_ActionClick_Carryall, 0x5)
{
	GET(Action, action, ECX);
	GET(TechnoClass*, pTechno, ESI);
	if (JumpjetCarryall* jjCarryall = GetScript<TechnoExt, JumpjetCarryall>(pTechno))
	{
		GET(TechnoClass*, pTarget, EDI);
		if (FootClass* pTargetFoot = abstract_cast<FootClass*>(pTarget))
		{
			jjCarryall->ActionClick(action, pTargetFoot);
		}
	}
	return 0;
}

namespace UnitPayloadDraw
{
	// 控制对吊运单位的绘制
	bool SkipCarryallOffset = false;
	TechnoTypeExt::TypeData* TypeData = nullptr;
}

DEFINE_HOOK(0x73CF16, UnitClass_Draw_It_Carryall_HightOffset, 0x7)
{
	GET(TechnoClass*, pTechno, ESI);
	TechnoTypeExt::TypeData* typeData = GetTypeData<TechnoTypeExt, TechnoTypeExt::TypeData>(pTechno->GetTechnoType());
	if (!typeData->CarryallOffset.IsEmpty())
	{
		// 如果是飞机吊运，需要计算偏移
		if (!UnitPayloadDraw::SkipCarryallOffset)
		{
			CoordStruct flh = -typeData->CarryallOffset;
			GET_STACK(int, x, 0x1C);
			GET_STACK(int, y, 0x20);
			Point2D point{ x, y };
			CoordStruct location = ToCoords(point);
			CoordStruct pos = GetFLHAbsoluteCoords(location, flh, pTechno->PrimaryFacing.Current());
			point = ToClientPos(pos);
			R->EDI(point.Y);
			R->Stack(0x1C, point.X);
			R->Stack(0x20, point.Y);
		}
		return 0x73CF1D;
	}
	return 0;
}

DEFINE_HOOK(0x73D317, UnitClass_Draw_It, 0x6)
{
	GET(TechnoClass*, pTechno, ESI);

	// 绘制自身的吊运货物
	JumpjetCarryall* carry = GetScript<TechnoExt, JumpjetCarryall>(pTechno);
	if (carry && carry->pPayload)
	{
		GET_STACK(int, x, 0x1C);
		GET_STACK(int, y, 0x20);
		Point2D point{ x, y };
		GET(RectangleStruct*, rect, EBX);

		TechnoClass* pPayload = carry->pPayload;
		TechnoTypeExt::TypeData* typeData = GetTypeData<TechnoTypeExt, TechnoTypeExt::TypeData>(pPayload->GetTechnoType());
		Point2D newPoint = point;
		if (!typeData->CarryallOffset.IsEmpty())
		{
			CoordStruct flh = -typeData->CarryallOffset;
			CoordStruct newPos = GetFLHAbsoluteCoords(pTechno, flh, false);
			newPoint = ToClientPos(newPos);
		}
		UnitPayloadDraw::SkipCarryallOffset = true;
		UnitPayloadDraw::TypeData = typeData;
		carry->pPayload->DrawIt(&newPoint, rect);
		UnitPayloadDraw::SkipCarryallOffset = false;
		UnitPayloadDraw::TypeData = nullptr;
	}
	return 0;
}

// 被捕获时切换图像
// DEFINE_HOOK(0x4DB157, FootClass_DrawVoxel_Shadow_Carryall, 0x8)
// {
// 	if (UnitCarryall::SkipCarryallOffset && UnitCarryall::TypeData)
// 	{
// 		GET(TechnoClass*, pTechno, EBP);
// 		GET_STACK(VoxelStruct*, pVxl, 0x38 - 0x4);
// 		TechnoTypeClass* pType = pTechno->GetTechnoType();
// 		VoxelStruct vxl = pType->MainVoxel;
// 		Debug::Log("vxl = %d, hva = %d, vxl = %d, hva = %d\n", pVxl->VXL, pVxl, vxl.VXL, vxl.HVA);
// 	}
// 	return 0;
// }

#pragma endregion // Balloon Jumpjet Carryall and Landing
