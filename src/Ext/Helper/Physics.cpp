#include "Physics.h"

bool CanHit(BuildingClass* pBuilding, int targetZ, bool blade, int zOffset)
{
	if (!blade)
	{
		int height = pBuilding->Type->Height;
		int sourceZ = pBuilding->GetCoords().Z;
		// Logger.Log($"Building Height {height}, {sourceZ + height * Game.LevelHeight + zOffset}");
		return targetZ <= (sourceZ + height * Unsorted::LevelHeight + zOffset);
	}
	return blade;
}

PassError CanMoveTo(CoordStruct sourcePos, CoordStruct nextPos, bool passBuilding, CoordStruct& nextCellPos, bool& onBridge)
{
	PassError canPass = PassError::PASS;
	nextCellPos = sourcePos;
	int deltaZ = sourcePos.Z - nextPos.Z;
	// 检查地面
	if (CellClass* pTargetCell = MapClass::Instance->TryGetCellAt(nextPos))
	{
		nextCellPos = pTargetCell->GetCoordsWithBridge();
		onBridge = pTargetCell->ContainsBridge();
		if (nextCellPos.Z >= nextPos.Z)
		{
			// 沉入地面
			nextPos.Z = nextCellPos.Z;
			canPass = PassError::UNDERGROUND;
			// 检查悬崖
			switch (pTargetCell->GetTileType())
			{
			case TileType::Cliff:
			case TileType::DestroyableCliff:
				// 悬崖上可以往悬崖下移动
				if (deltaZ <= 0)
				{
					canPass = PassError::HITWALL;
				}
				break;
			}
		}
		// 检查桥
		if (canPass == PassError::UNDERGROUND && onBridge)
		{
			int bridgeHeight = nextCellPos.Z;
			if (sourcePos.Z > bridgeHeight && nextPos.Z <= bridgeHeight)
			{
				// 桥上砸桥下
				canPass = PassError::DOWNBRIDGE;
			}
			else if (sourcePos.Z < bridgeHeight && nextPos.Z >= bridgeHeight)
			{
				// 桥下穿桥上
				canPass = PassError::UPBRIDEG;
			}
		}
		// 检查建筑
		if (!passBuilding)
		{
			BuildingClass* pBuilding = pTargetCell->GetBuilding();
			if (pBuilding)
			{
				if (CanHit(pBuilding, nextPos.Z))
				{
					canPass = PassError::HITBUILDING;
				}
			}
		}
	}
	return canPass;
}

bool CanPassUnder(TechnoClass* pTechno, CoordStruct& targetPos, CellClass*& pCell, bool& isWater)
{
	TechnoTypeClass* pType = pTechno->GetTechnoType();
	// 检查是否在悬崖上摔死
	isWater = false;
	bool canPass = true;
	CoordStruct location = pTechno->GetCoords();
	targetPos = location;
	pCell = MapClass::Instance->TryGetCellAt(location);
	if (pCell)
	{
		CoordStruct cellPos = pCell->GetCoordsWithBridge();
		pTechno->OnBridge = pCell->ContainsBridge();

		if (cellPos.Z >= location.Z)
		{
			targetPos.Z = cellPos.Z;
			pTechno->SetLocation(targetPos);
		}
		// 当前格子所在的位置不可通行，炸了它
		canPass = pCell->IsClearToMove(pType->SpeedType, pType->MovementZone, true, true);
		if (canPass && pCell->GetBuilding() != nullptr)
		{
			canPass = false;
		}
		if (!canPass)
		{
			isWater = pCell->Tile_Is_Water();
		}
	}
	return canPass;
}

FallingError Falling(TechnoClass* pTechno, CoordStruct targetPos, int fallingDestroyHeight, bool hasParachute, bool isWater, bool& canPass)
{
	FallingError drop = FallingError::UNCHANGED;
	bool sinking = false;
	// 检查下方是不是水
	if (isWater)
	{
		LocoType locoType = GetLocoType(pTechno);
		switch (locoType)
		{
		case LocoType::Hover:
		case LocoType::Ship:
			// 船和悬浮不下沉
			canPass = true;
			break;
		case LocoType::Jumpjet:
			if (!pTechno->GetTechnoType()->BalloonHover)
			{
				sinking = true;
			}
			break;
		default:
			sinking = true;
			break;
		}
	}
	// 高度大于一定值时强制摔死
	int height = pTechno->GetHeight();
	if (fallingDestroyHeight > 0 && height >= fallingDestroyHeight)
	{
		canPass = false;
	}
	if (canPass)
	{
		if (height > 0)
		{
			// 离地
			pTechno->IsFallingDown = true;
			drop = FallingError::FALLING;
		}
		else
		{
			// 贴地
			drop = FallingError::UNCHANGED;
		}
	}
	else
	{
		// 摔死
		if (height <= 0 && sinking)
		{
			pTechno->IsSinking = true;
			drop = FallingError::SINKING;
		}
		else
		{
			pTechno->DropAsBomb();
			drop = FallingError::BOMB;
		}
	}
	switch (drop)
	{
	case FallingError::FALLING:
	case FallingError::BOMB:
		if (hasParachute && height >= Unsorted::LevelHeight * 2)
		{
			// ObjectClass.SpawnParachuted(Coords)需要检查单位Unlimbo成功，此处手动添加降落伞动画
			if (AnimTypeClass* pAnimType = RulesClass::Instance->Parachute)
			{
				CoordStruct parachutePos = targetPos;
				parachutePos.Z += 75;
				AnimClass* pAnim = GameCreate<AnimClass>(pAnimType, parachutePos);
				pTechno->Parachute = pAnim;
				pAnim->SetOwnerObject(pTechno);
				pAnim->Owner = pTechno->Owner;
			}
		}
		if (pTechno->WhatAmI() == AbstractType::Infantry)
		{
			abstract_cast<InfantryClass*, true>(pTechno)->PlayAnim(Sequence::Paradrop);
		}
		break;
	}
	return drop;
}

FallingError FallingDown(TechnoClass* pTechno, int fallingDestroyHeight, bool hasParachute)
{
	CoordStruct targetPos = CoordStruct::Empty;
	CellClass* pCell = nullptr;
	bool isWater = false;
	bool canPass = CanPassUnder(pTechno, targetPos, pCell, isWater);
	return Falling(pTechno, targetPos, fallingDestroyHeight, hasParachute, isWater, canPass);
}

FallingError FallingExceptAircraft(TechnoClass* pTechno, int fallingDestroyHeight, bool hasParachute)
{
	FallingError drop = FallingError::UNCHANGED;
	if (pTechno->IsInAir() && pTechno->GetTechnoType()->ConsideredAircraft)
	{
		// 飞行器在天上，免死
		drop = FallingError::FLY;
		CellClass* pCell = MapClass::Instance->TryGetCellAt(pTechno->GetCoords());
		pTechno->SetDestination(pCell, true);
		if (pTechno->Target)
		{
			pTechno->QueueMission(Mission::Attack, false);
		}
		else
		{
			if (pTechno->WhatAmI() == AbstractType::Aircraft)
			{
				pTechno->QueueMission(Mission::Enter, false);
			}
			else
			{
				pTechno->QueueMission(Mission::Move, false);
			}
		}
	}
	else
	{
		CoordStruct targetPos = CoordStruct::Empty;
		CellClass* pCell = nullptr;
		bool isWater = false;
		bool canPass = CanPassUnder(pTechno, targetPos, pCell, isWater);
		drop = Falling(pTechno, targetPos, fallingDestroyHeight, hasParachute, isWater, canPass);
		if (drop == FallingError::UNCHANGED)
		{
			// 贴地
			pTechno->Scatter(targetPos, true, true);
		}
	}
	return drop;
}

void OccupyCell(TechnoClass* pTechno, const CoordStruct& coords)
{
	void** vtable = *reinterpret_cast<void***>(pTechno);
	reinterpret_cast<void(__thiscall*)(void*, const CoordStruct*)>(vtable[0xF0 / 4])(pTechno, &coords);
}

void ReleaseCell(TechnoClass* pTechno, const CoordStruct& coords)
{
	void** vtable = *reinterpret_cast<void***>(pTechno);
	reinterpret_cast<void(__thiscall*)(void*, const CoordStruct*)>(vtable[0xF4 / 4])(pTechno, &coords);
}

