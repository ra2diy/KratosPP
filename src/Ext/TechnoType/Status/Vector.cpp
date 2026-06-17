#include "../TechnoStatus.h"

#include <JumpjetLocomotionClass.h>

#include <Ext/Helper/FLH.h>
#include <Ext/Helper/Physics.h>
#include <Ext/Helper/Weapon.h>
#include <Ext/Helper/Scripts.h>

void TechnoStatus::VectorCancel()
{
	if (!IsBuilding() && !IsDeadOrInvisible(pTechno))
	{
		FootClass* pFoot = abstract_cast<FootClass*, true>(pTechno);
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
		// 坠落：取当前高度对地差值或 AllowFallingDestroy 指定高度
		CellClass* pCell = MapClass::Instance->TryGetCellAt(pTechno->GetCoords());
		int heightAboveGround = pCell ? (pTechno->GetCoords().Z - pCell->GetCoordsWithBridge().Z) : 0;
		int fallingDestroyHeight = _vectorResult.AllowFallingDestroy
			? _vectorResult.FallingDestroyHeight
			: 0; // AllowFallingDestroy=no 时不判死，自然坠落
		FallingExceptAircraft(pTechno, fallingDestroyHeight, false);
	}
	VectorForced = false;
	_vectorResult = {};
}


void TechnoStatus::OnUpdate_Vector()
{
	if (CaptureByBlackHole || IsBuilding() || IsDeadOrInvisible(pTechno))
		return;

	bool wasVectorForced = VectorForced;

	_vectorResult = AEManager()->MarginVectorOffset();

	if (!_vectorResult.MoveDisp.IsEmpty())
	{
		VectorForced = true;
		VectorPendingFall = false;

		{
			FILE* f = fopen("I:/KratosAI/techno_vector.log", "a");
			if (f) { fprintf(f, "  VECT_TECHNO disp=(%d,%d,%d) rot=%d\n",
				_vectorResult.MoveDisp.X, _vectorResult.MoveDisp.Y, _vectorResult.MoveDisp.Z,
				(int)_vectorResult.AllowRotateUnit); fclose(f); }
		}

		// 首次进入 Vector 控制：切换 Locomotor 为 Jumpjet（允许地面单位浮空）
		if (!wasVectorForced)
		{
			FootClass* pFoot = abstract_cast<FootClass*>(pTechno);
			if (pFoot && !dynamic_cast<JumpjetLocomotionClass*>(pFoot->Locomotor.get()))
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
			// 设置翻滚
			if (_vectorResult.AllowRotateUnit)
			{
				FILE* ff = fopen("I:/KratosAI/techno_vector.log", "a");
				if (ff) { fprintf(ff, "  ROTATE next=(%d,%d,%d) loc=(%d,%d,%d)\n",
					nextPos.X, nextPos.Y, nextPos.Z, location.X, location.Y, location.Z); fclose(ff); }
				// 设置朝向
				if (_lastMission == Mission::Move || _lastMission == Mission::AttackMove || pTechno->GetTechnoType()->ConsideredAircraft || !pTechno->IsInAir())
				{
					CoordStruct p1 = nextPos;
					CoordStruct p2 = location;
					p1.Z = 0;
					p2.Z = 0;
					if (p1.DistanceFrom(p2) > 0)
					{
						DirStruct facingDir = Point2Dir(location, nextPos);
						{
							FILE* f = fopen("I:/KratosAI/techno_vector.log", "a");
							if (f) { fprintf(f, "  FACING next=(%d,%d) loc=(%d,%d) raw=%d\n",
								nextPos.X, nextPos.Y, location.X, location.Y,
								(int)facingDir.Raw); fclose(f); }
						}
						pTechno->PrimaryFacing.SetDesired(facingDir);
						if (IsJumpjet())
						{
							if (JumpjetLocomotionClass* jjLoco = dynamic_cast<JumpjetLocomotionClass*>(pFoot->Locomotor.get()))
							{
								jjLoco->LocomotionFacing.SetDesired(facingDir);
							}
						}
						else if (IsAircraft())
						{
							pTechno->SecondaryFacing.SetDesired(facingDir);
						}
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

