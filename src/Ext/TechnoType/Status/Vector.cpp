#include "../TechnoStatus.h"

#include <JumpjetLocomotionClass.h>

#include <Ext/Helper/FLH.h>
#include <Ext/Helper/Physics.h>
#include <Ext/Helper/Weapon.h>
#include <Ext/Helper/Scripts.h>

void TechnoStatus::VectorCancel()
{
	// 保存当前攻击目标及移动目标，Vector结束后恢复
	AbstractClass* savedTarget = pTechno->Target;
	AbstractClass* savedFocus = pTechno->Focus;
	FootClass* pFoot = abstract_cast<FootClass*>(pTechno);
	Debug::Log("[VectorReach] END VectorCancel pos=(%d,%d,%d)\n",
		pTechno->GetCoords().X, pTechno->GetCoords().Y, pTechno->GetCoords().Z);

	if (!IsBuilding() && !IsDeadOrInvisible(pTechno))
	{
		pFoot->Locomotor->Unlock();
		// 恢复原 Locomotor
		if (dynamic_cast<JumpjetLocomotionClass*>(pFoot->Locomotor.get()))
		{
			if (_vectorSavedLocomotor != GUID{})
				LocomotionClass::ChangeLocomotorTo(pFoot, _vectorSavedLocomotor);
			else
				pFoot->Locomotor->Unlock();
		}
		// 恢复可控制
		if (_lostControl)
		{
			pFoot->ForceMission(Mission::Guard);
		}
		// 摔死
		int fallingDestroyHeight = 0;
		if (_vectorResult.AllowFallingDestroy)
		{
			fallingDestroyHeight = _vectorResult.FallingDestroyHeight;
		}
		FallingExceptAircraft(pTechno, fallingDestroyHeight, false);
	}
	VectorForced = false;
	VectorFreezeActive = false;
	_vectorDesiredPos = CoordStruct::Empty;
	_vectorResult = {};
	_savedVectorTarget = nullptr;

	// 恢复被清空的攻击目标和移动目标
	if (savedTarget && !IsDeadOrInvisible(abstract_cast<ObjectClass*>(savedTarget)))
	{
		pTechno->SetTarget(savedTarget);
		pTechno->QueueMission(Mission::Attack, false);
	}
	else if (savedFocus && !IsDeadOrInvisible(abstract_cast<ObjectClass*>(savedFocus)))
	{
		pTechno->SetFocus(savedFocus);
	}
}


void TechnoStatus::OnUpdate_Vector()
{
	if (CaptureByBlackHole || IsBuilding() || IsDeadOrInvisible(pTechno))
		return;

	bool wasVectorForced = VectorForced;

	// 纠正上一帧引擎自主运动造成的偏离（下一帧开头强行搬回Vector计算的位置）
	if (wasVectorForced && !_vectorDesiredPos.IsEmpty())
	{
		CoordStruct currentPos = pTechno->GetCoords();
		if (currentPos != _vectorDesiredPos)
		{
			bool onBridge = pTechno->OnBridge;
			pTechno->UpdatePlacement(PlacementType::Remove);
			pTechno->OnBridge = onBridge;
			pTechno->SetLocation(_vectorDesiredPos);
			pTechno->UpdatePlacement(PlacementType::Put);
		}
	}

	_vectorResult = AEManager()->MarginVectorOffset();

	if (!_vectorResult.MoveDisp.IsEmpty() || _vectorResult.Freeze)
	{
		VectorForced = true;
		VectorPendingFall = false;

		// 首次进入 Vector：锁定当前目标
		if (!wasVectorForced)
		{
			_savedVectorTarget = pTechno->Target;
		}
		// 每帧恢复：引擎可能清Target，强制重设
		if (_savedVectorTarget && !IsDeadOrInvisible(abstract_cast<ObjectClass*>(_savedVectorTarget)))
		{
			pTechno->SetTarget(_savedVectorTarget);
			pTechno->QueueMission(Mission::Attack, false);
		}

		if (_vectorResult.Freeze)
			VectorFreezeActive = true;

		// 首次进入 Vector 控制：切换 Locomotor 为 Jumpjet（允许地面单位浮空）
		if (!wasVectorForced)
		{
			FootClass* pFoot = abstract_cast<FootClass*>(pTechno);
			if (pFoot && !dynamic_cast<JumpjetLocomotionClass*>(pFoot->Locomotor.get()) && !IsAircraft())
			{
				_vectorSavedLocomotor = pTechno->GetTechnoType()->Locomotor;
				LocomotionClass::ChangeLocomotorTo(pFoot, LocomotionClass::CLSIDs::Jumpjet);
			}
		}
		CoordStruct location = pTechno->GetCoords();

		if (IsDeadOrInvisible(pTechno))
		{
			VectorCancel();
		}
		else
		{
			// 移动位置
			// 从占据的格子中移除自己
			pTechno->UnmarkAllOccupationBits(location);
			FootClass* pFoot = abstract_cast<FootClass*, true>(pTechno);
			// 停止移动
			ForceStopMoving(pFoot);
			// 计算下一个坐标点
			CoordStruct nextPos = location + _vectorResult.MoveDisp;
			bool onBridge = pTechno->OnBridge;
			if (VectorForced)
			{
				// Force 模式：跳过路径校验，直接 SetLocation（允许高空/穿墙等非地面位）
				_vectorDesiredPos = nextPos;
				pTechno->UpdatePlacement(PlacementType::Remove);
				pTechno->OnBridge = onBridge;
				pTechno->SetLocation(nextPos);
				pTechno->UpdatePlacement(PlacementType::Put);
				MapClass::Instance->RevealArea2(&nextPos, pTechno->LastSightRange, pTechno->Owner, false, false, false, true, 0);
				MapClass::Instance->RevealArea2(&nextPos, pTechno->LastSightRange, pTechno->Owner, false, false, false, true, 1);
			}
			else
			{
			CoordStruct nextCellPos = CoordStruct::Empty;
			PassError passError = CanMoveTo(location, nextPos, _vectorResult.CanPassBuilding, nextCellPos, onBridge);
			switch (passError)
			{
			case PassError::HITWALL:
			case PassError::HITBUILDING:
			case PassError::UPBRIDEG:
				// 反弹回移动前的格子
				if (CellClass* pSourceCell = MapClass::Instance->TryGetCellAt(location))
				{
					CoordStruct cellPos = pSourceCell->GetCoordsWithBridge();
					nextPos.X = cellPos.X;
					nextPos.Y = cellPos.Y;
					if (nextPos.Z < cellPos.Z)
					{
						nextPos.Z = cellPos.Z;
					}
				}
				break;
			case PassError::UNDERGROUND:
			case PassError::DOWNBRIDGE:
				// 卡在地表
				nextPos.Z = nextCellPos.Z;
				break;
			} // switch
				_vectorDesiredPos = nextPos;
				// 被黑洞吸走
				pTechno->UpdatePlacement(PlacementType::Remove);
				// 是否在桥上
				pTechno->OnBridge = onBridge;
				pTechno->SetLocation(nextPos);
				pTechno->UpdatePlacement(PlacementType::Put);
			} // else (非 Force 模式)
			// 移除黑幕
			MapClass::Instance->RevealArea2(&nextPos, pTechno->LastSightRange, pTechno->Owner, false, false, false, true, 0);
			MapClass::Instance->RevealArea2(&nextPos, pTechno->LastSightRange, pTechno->Owner, false, false, false, true, 1);
			// 设置动作
			if (_vectorResult.AllowCrawl && IsInfantry())
			{
				abstract_cast<InfantryClass*, true>(pTechno)->PlayAnim(Sequence::Crawl);
			}
			// 成熟机制，别乱动：单位 SyncFacing 朝向同步
		if (_vectorResult.AllowRotateUnit)
		{
			CoordStruct p1 = nextPos;
			CoordStruct p2 = location;
			p1.Z = 0;
			p2.Z = 0;
			if (p1.DistanceFrom(p2) > 0)
			{
				DirStruct facingDir = Point2Dir(location, nextPos); // 官方API，不得修改
				pTechno->PrimaryFacing.SetDesired(facingDir);
				if (IsJumpjet())
				{
					if (JumpjetLocomotionClass* jjLoco = dynamic_cast<JumpjetLocomotionClass*>(pFoot->Locomotor.get()))
						jjLoco->LocomotionFacing.SetDesired(facingDir);
				}
				else if (IsAircraft())
				{
					pTechno->SecondaryFacing.SetDesired(facingDir);
				}
			}
		}
		}
	}
	else if (wasVectorForced)
	{
		// 延后 1 帧判定坠落，等待 Next= 链条衔接
		VectorPendingFall = true;
		VectorForced = false;
	}
	else if (VectorPendingFall)
	{
		VectorCancel();
		VectorPendingFall = false;
	}
}

void TechnoStatus::OnUpdateEnd_Vector()
{
	// VectorForced 或过渡帧（VectorPendingFall，AE 已移除但未交还）都锁位到目标点，
	// 防止 Aircraft 等引擎在 Vector 结束瞬间接管飞离目标
	if ((VectorForced || VectorPendingFall) && !IsBuilding() && !IsDeadOrInvisible(pTechno) && !_vectorDesiredPos.IsEmpty())
	{
		CoordStruct currentPos = pTechno->GetCoords();
		if (currentPos != _vectorDesiredPos)
		{
			bool onBridge = pTechno->OnBridge;
			pTechno->UpdatePlacement(PlacementType::Remove);
			pTechno->OnBridge = onBridge;
			pTechno->SetLocation(_vectorDesiredPos);
			pTechno->UpdatePlacement(PlacementType::Put);
		}
	}
}

