#include "JumpjetCarryall.h"

#include <JumpjetLocomotionClass.h>

#include <Ext/Helper/FLH.h>
#include <Ext/Helper/Physics.h>
#include <Ext/Helper/Scripts.h>

TechnoStatus* JumpjetCarryall::GetTechnoStatus()
{
	TechnoStatus* status = nullptr;
	if (pTechno && _parent)
	{
		status = _parent->GetComponent<TechnoStatus>();
	}
	return status;
}

TechnoTypeExt::TypeData* JumpjetCarryall::GetJJCarryallData()
{
	if (!_jjCarryallData)
	{
		_jjCarryallData = GetTypeData<TechnoTypeExt, TechnoTypeExt::TypeData>(pTechno->GetTechnoType());
	}
	return _jjCarryallData;
}

void JumpjetCarryall::SetupJJCarryall()
{
	_jjCarryallData = nullptr;
	if (!GetJJCarryallData()->Carryall || !IsJumpjet())
	{
		Disable();
	}
}

void JumpjetCarryall::CancelMission(bool reset)
{
	_pTarget = nullptr;
	_pTargetCell = nullptr;
	_toPayload = false;
	_status = reset ? CarryStatus::READY : CarryStatus::STOP;
	GetTechnoStatus()->CarryallLanding = false;
}

void JumpjetCarryall::StartMission(FootClass* pTarget, bool toPayload)
{
	_pTarget = nullptr;
	_pTargetCell = nullptr;
	_toPayload = false;
	if (CellClass* pCell = MapClass::Instance->TryGetCellAt(pTarget->GetCoords()))
	{
		_toPayload = toPayload;
		_pTarget = pTarget;
		_pTargetCell = pCell;
		pTechno->SetDestination(pCell, true);
		pTechno->QueueMission(Mission::Move, true);
		_status = CarryStatus::FLYTO;
	}
}

bool JumpjetCarryall::InMission()
{
	switch (_status)
	{
	case CarryStatus::STOP:
	case CarryStatus::READY:
		return false;
	default:
		return true;
	}
}

bool JumpjetCarryall::CanLift(TechnoClass* pTarget, bool& toPayload)
{
	FootClass* pTargetFoot = abstract_cast<FootClass*>(pTarget);
	// ★ 判据必须是"全端一致"的（失同步优先）：
	//   原写法 `pTechno->Owner->IsControlledByCurrentPlayer()` 在联机下等价于
	//   `HouseClass::IsCurrentPlayer()`（见 YRpp/HouseClass.h:461）—— **各客户端结果不同**：
	//   同一个运输机，在 A 客户端返回 true、在 B 客户端返回 false。
	//   而 `CanLift` 经 `UnitClass::What_Action`(`0x73FD50..0x7404AA` 内的 `0x74041B`，
	//   是虚函数、由 vtable `0x7F5CE4` 派发) 到达，`What_Action` 的返回值会决定
	//   `Action::Tote`(吊运) 还是 `Action::Select` —— 一旦两端结果不同，
	//   同一帧的吊运判定分叉 ⇒ 目标/任务分叉 ⇒ 失同步。
	//   改用 `IsControlledByHuman()`：联机下 == `IsHumanPlayer`（House 级属性，全端一致；
	//   战役下 == `IsHumanPlayer || IsInPlayerControl`，与原写法**完全相同**）。
	//   对本地玩家的体验零影响（本机自己的运输机两种判据都为真）。
	if (pTechno && pTargetFoot && pTechno != pTargetFoot
		&& pTechno->Owner->IsControlledByHuman() && pTechno->Owner->IsAlliedWith(pTargetFoot->GetOwningHouse())
		&& !pTargetFoot->ParasiteEatingMe && !pTargetFoot->IsIronCurtained() && !pTargetFoot->WarpingOut && !pTargetFoot->IsImmobilized && !pTargetFoot->IsAttackedByLocomotor)
	{
		TechnoTypeClass* pType = pTechno->GetTechnoType();

		TechnoTypeClass* pTargetType = pTarget->GetTechnoType();
		TechnoTypeExt::TypeData* pTechnoData = GetTypeData<TechnoTypeExt, TechnoTypeExt::TypeData>(pType);

		int maxPassengers = pType->Passengers;
		double targetSize = pTargetType->Size;

		if (pTechnoData->Carryall
			&& (pTechnoData->CarryallSizeLimit < 0 || pTechnoData->CarryallSizeLimit >= targetSize))
		{
			if (maxPassengers == 0 || pType->SizeLimit < targetSize)
			{
				toPayload = true;
				// 塞不进乘客舱，检查是否可以吊运
				if (!pPayload && pTarget->WhatAmI() != AbstractType::Infantry)
				{
					TechnoTypeExt::TypeData* pTargetData = GetTypeData<TechnoTypeExt, TechnoTypeExt::TypeData>(pTargetType);
					return pTargetData->CarryallAllowed;
				}
			}
			else
			{
				toPayload = false;
				// 可以塞进乘客舱，检查是否有足够的空位
				return _remainingSpace >= targetSize;
			}
		}
	}
	return false;
}

bool JumpjetCarryall::CanLift(TechnoClass* pTarget)
{
	bool toPayload = false;
	return CanLift(pTarget, toPayload);
}

void JumpjetCarryall::ActionClick(Action action, FootClass* pTarget)
{
	if (action != Action::Tote)
	{
		CancelMission();
	}
	if (_pTarget && pTarget != _pTarget)
	{
		CancelMission(action == Action::Tote);
	}
	bool toPayload = false;
	if (action == Action::Tote && _status == CarryStatus::READY && CanLift(pTarget, toPayload) && (!toPayload || !pPayload))
	{
		StartMission(pTarget, toPayload);
	}
}

void JumpjetCarryall::DropPayload()
{
	TechnoClass* pTarget = pPayload;
	pPayload = nullptr;
	if (pTarget)
	{
		CoordStruct location = pTechno->GetCoords();
		DirStruct dir = pTarget->PrimaryFacing.Current();
		pTarget->IsOnCarryall = false;
		TechnoTypeExt::TypeData* typeData = GetTypeData<TechnoTypeExt, TechnoTypeExt::TypeData>(pTarget->GetTechnoType());
		if (!typeData->CarryallOffset.IsEmpty())
		{
			CoordStruct flh = -typeData->CarryallOffset;
			location = GetFLHAbsoluteCoords(location, flh, dir);
		}
		if (TryPutTechno(pTarget, location))
		{
			pTarget->PrimaryFacing.SetCurrent(dir);
			pTarget->SecondaryFacing.SetCurrent(dir);
			FallingExceptAircraft(pTarget, 0, true);
			int sound = pTechno->GetTechnoType()->LeaveTransportSound;
			if (sound != -1)
			{
				VocClass::PlayAt(sound, location, nullptr);
			}
		}
		else
		{
			pTarget->UnInit();
		}
	}
}

bool JumpjetCarryall::NotToTarget()
{
	if (!IsDeadOrInvisible(_pTarget) && _pTargetCell)
	{
		if (AbstractClass* pDest = abstract_cast<FootClass*, true>(pTechno)->Destination)
		{
			CoordStruct destPos = pDest->GetCoords();
			CellClass* pDestCell = MapClass::Instance->TryGetCellAt(destPos);
			CellStruct t = _pTargetCell->MapCoords;
			CellStruct d = pDestCell->MapCoords;
			return t.X != d.X || t.Y != d.Y;
		}
	}
	return true;
}

void JumpjetCarryall::Awake()
{
	SetupJJCarryall();
	EventSystems::General.AddHandler(Events::ObjectUnInitEvent, this, &JumpjetCarryall::OnTechnoDelete);
}

void JumpjetCarryall::Destroy()
{
	EventSystems::General.RemoveHandler(Events::ObjectUnInitEvent, this, &JumpjetCarryall::OnTechnoDelete);
	if (pPayload)
	{
		pPayload->UnInit();
	}
}

void JumpjetCarryall::ExtChanged()
{
	SetupJJCarryall();
}

void JumpjetCarryall::OnUpdate()
{
	if (pPayload && pTechno)
	{
		CoordStruct location = pTechno->GetCoords();
		pPayload->SetLocation(location);
		if (pTechno->IsInAir())
		{
			pPayload->AngleRotatedForwards = 0;
			pPayload->AngleRotatedSideways = 0;
		}
		else
		{
			pPayload->AngleRotatedForwards = pTechno->AngleRotatedForwards;
			pPayload->AngleRotatedSideways = pTechno->AngleRotatedSideways;
		}
		DirStruct dir = pTechno->PrimaryFacing.Current();
		pPayload->PrimaryFacing.SetCurrent(dir);
		pPayload->SecondaryFacing.SetCurrent(dir);
		if (JumpjetLocomotionClass* pPayloadJJ = dynamic_cast<JumpjetLocomotionClass*>(pPayload->Locomotor.get()))
		{
			pPayloadJJ->LocomotionFacing.SetCurrent(dir);
		}
	}
	if (!IsDeadOrInvisible(pTechno))
	{
		TechnoTypeClass* pType = pTechno->GetTechnoType();
		int maxPassengers = pType->Passengers;
		_remainingSpace = maxPassengers == 0 ? 0 : (maxPassengers - pTechno->Passengers.GetTotalSize());
		if (pTechno->Target || (InMission() && NotToTarget()))
		{
			CancelMission();
		}
		switch (_status)
		{
		case CarryStatus::STOP:
		{
			// 满了吗
			if ((!pPayload || _remainingSpace > 0) && pTechno->IsInAir())
			{
				_status = CarryStatus::READY;
			}
			break;
		}
		case CarryStatus::READY:
			// 可以开抓
			break;
		case CarryStatus::FLYTO:
		{
			// 正在前往目的地，随时更新抓取目标的位置信息
			if (CellClass* pCell = MapClass::Instance->TryGetCellAt(_pTarget->GetCoords()))
			{
				if (pCell != _pTargetCell)
				{
					// 更新目标位置
					_pTargetCell = pCell;
					pTechno->SetDestination(pCell, true);
					pTechno->QueueMission(Mission::Move, true);
				}
			}
			if (!_pTargetCell)
			{
				CancelMission();
				break;
			}
			FootClass* pFoot = abstract_cast<FootClass*, true>(pTechno);
			if (pFoot->DistanceFrom(_pTargetCell) <= 16)
			{
				// 抵达目标上空，开始降落
				GetTechnoStatus()->CarryallLanding = true;
				_status = CarryStatus::LAND;
			}
			break;
		}
		case CarryStatus::LAND:
		{
			// 降落，计算和目标的高度差，抓取目标
			CoordStruct targetPos = _pTarget->GetCoords();
			CoordStruct location = pTechno->GetCoords();
			// 检查目标有无跑出范围
			CellStruct t = CellClass::Coord2Cell(targetPos);
			CellStruct s = CellClass::Coord2Cell(location);
			if (t.X != s.X || t.Y != s.Y)
			{
				// 目标润了，中断下降
				GetTechnoStatus()->CarryallLanding = false;
				_status = CarryStatus::FLYTO;
				break;
			}
			TechnoTypeExt::TypeData* typeData = GetTypeData<TechnoTypeExt, TechnoTypeExt::TypeData>(_pTarget->GetTechnoType());
			if (!typeData->CarryallOffset.IsEmpty())
			{
				CoordStruct flh = typeData->CarryallOffset;
				targetPos = GetFLHAbsoluteCoords(_pTarget, flh, false);
			}
			// 下降高度
			int z = targetPos.Z;
			int climb = 20;
			FootClass* pFoot = abstract_cast<FootClass*, true>(pTechno);
			JumpjetLocomotionClass* pJJ = dynamic_cast<JumpjetLocomotionClass*>(pFoot->Locomotor.get());
			if (pJJ)
			{
				climb = (int)pJJ->Climb;
			}
			bool close = (location.Z <= z + climb) || pTechno->GetHeight() <= Unsorted::LevelHeight;
			if (!close)
			{
				location.Z -= climb;
				// 被黑洞吸走
				pTechno->UpdatePlacement(PlacementType::Remove);
				pTechno->SetLocation(location);
				pTechno->UpdatePlacement(PlacementType::Put);
				// 转向目标的dir
				if (_pTarget->WhatAmI() != AbstractType::Infantry)
				{
					DirStruct dir = _pTarget->PrimaryFacing.Current();
					if (pJJ)
					{
						pJJ->LocomotionFacing.SetDesired(dir);
					}
					pTechno->PrimaryFacing.SetDesired(dir);
				}
			}
			if (close)
			{
				if (_pTarget)
				{
					// 抓起目标
					_pTarget->Limbo();
					_pTarget->SetLocation(location);
					_pTarget->OnBridge = false;
					// 吊运还是塞进乘客舱
					if (_toPayload)
					{
						_pTarget->IsOnCarryall = true;
						pPayload = _pTarget;
					}
					else
					{
						// 塞进乘客舱
						if (pType->OpenTopped)
						{
							pTechno->EnteredOpenTopped(_pTarget);
						}
						_pTarget->Transporter = pTechno;
						// WWSB，AddPassenger在这里会造成死循环，手动加入乘客舱
						// pTechno->AddPassenger(_pTarget);
						_pTarget->NextObject = pTechno->Passengers.FirstPassenger;
						pTechno->Passengers.FirstPassenger = _pTarget;
						pTechno->Passengers.NumPassengers++;
					}
					int sound = pTechno->GetTechnoType()->EnterTransportSound;
					if (sound != -1)
					{
						VocClass::PlayAt(sound, location, nullptr);
					}
				}
				_status = CarryStatus::TAKEOFF;
			}
			break;
		}
		case CarryStatus::TAKEOFF:
			// 抓完之后是否需要起飞
			CancelMission();
			break;
		}
	}
	else
	{
		_status = CarryStatus::STOP;
	}
}

void JumpjetCarryall::OnGuardCommand()
{
	CancelMission();
}

void JumpjetCarryall::OnStopCommand()
{
	CancelMission();
}
