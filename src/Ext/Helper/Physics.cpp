#include "Physics.h"

#include <LocomotionClass.h>
#include <algorithm>

#include <Utilities/Debug.h>
#include <Ext/Common/PreOccupancyManager.h>

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
			// GetBuilding() 只返回建筑对象所在格（中心格）；footprint 的其余格
			// 没有建筑对象，靠 OccupationFlags 0x80 标记。人间大炮飞行中如果被
			// UpdatePlacement(Put) 塞进 footprint 格，引擎会直接压死单位，
			// 所以这里把 0x80 也当作撞建筑处理（canLand 和 Pump 飞行共用本函数）。
			else if ((pTargetCell->OccupationFlags & 0x80) != 0)
			{
				canPass = PassError::HITBUILDING;
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
		// 诊断日志（排查落地摔死时取消注释，能看落点格与可通行性）：
		// Debug::Log("CanPassUnder: pos{%d,%d,%d} cell{%d,%d} clear=%d water=%d\n",
		// 	location.X / 256, location.Y / 256, location.Z,
		// 	pCell->MapCoords.X, pCell->MapCoords.Y, canPass, isWater);
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
		// 诊断日志（排查落地摔死时取消注释）：
		// Debug::Log("FallingExceptAircraft: h=%d canPass=%d isWater=%d\n",
		// 	pTechno->GetHeight(), canPass, isWater);
		drop = Falling(pTechno, targetPos, fallingDestroyHeight, hasParachute, isWater, canPass);
		if (drop == FallingError::UNCHANGED)
		{
			// 贴地
			pTechno->Scatter(targetPos, true, true);
		}
	}
	return drop;
}

CoordStruct FindLandingPoint(TechnoClass* pTechno, const CoordStruct& targetPos, bool forceGround, bool usePathPriority, PreOccupancyManager* pPreOccupancyManager)
{
	CellClass* pTargetCell = MapClass::Instance->TryGetCellAt(targetPos);
	if (!pTargetCell)
	{
		return CoordStruct::Empty;
	}

	bool isJumpJet = pTechno->GetTechnoType()->Locomotor == LocomotionClass::CLSIDs::Jumpjet;
	if (!forceGround && pTechno->IsInAir())
	{
		return pTargetCell->GetCoordsWithBridge();
	}

	TechnoTypeClass* pType = pTechno->GetTechnoType();
	CoordStruct location = pTechno->GetCoords();
	CellStruct source = CellClass::Coord2Cell(location);
	CellStruct target = pTargetCell->MapCoords;
	int dx = target.X - source.X;
	int dy = target.Y - source.Y;

	auto makeLandingPos = [&](CellClass* pCell) -> CoordStruct
		{
			CoordStruct landingPos = pCell->GetCoordsWithBridge();
			if (!isJumpJet && pTechno->WhatAmI() == AbstractType::Infantry)
			{
				CoordStruct subcellPos = CoordStruct::Empty;
				if (pPreOccupancyManager)
				{
					// 优先使用预占管理器选择的空闲子格（配合提前占格形成包围圈），
					// 该格没有被预占时内部会回退到引擎的 PickInfantrySublocation
					if (pPreOccupancyManager->GetAvailableSubCell(pTechno, landingPos, subcellPos) && !subcellPos.IsEmpty())
					{
						landingPos = subcellPos;
					}
				}
				else if (MapClass::PickInfantrySublocation(subcellPos, landingPos, false) && !subcellPos.IsEmpty())
				{
					landingPos = subcellPos;
				}
			}
			return landingPos;
		};

	auto canLand = [&](CellClass* pCell) -> bool
		{
			if (isJumpJet)
			{
				if (pCell->Jumpjet != nullptr)
				{
					return false;
				}
			}
			if (pCell->GetBuilding() != nullptr)
			{
				return false;
			}
			CoordStruct cellCenter = pCell->GetCoordsWithBridge();
			CoordStruct nextCellPos = CoordStruct::Empty;
			bool onBridge = false;
			PassError err = CanMoveTo(location, cellCenter, false, nextCellPos, onBridge);
			if (err == PassError::HITBUILDING || err == PassError::HITWALL
				|| err == PassError::DOWNBRIDGE || err == PassError::UPBRIDEG)
			{
				return false;
			}
			if (!pCell->IsClearToMove(pType->SpeedType, pType->MovementZone, true, true))
			{
				return false;
			}
			Move move = pTechno->IsCellOccupied(pCell, -1, -1, nullptr, false);
			if (move != Move::OK)
			{
				return false;
			}
			// —— 伞兵/步兵占格过滤（引擎实测结论，2026-09-01，详见 Physics.h 备忘）——
			// 伞兵投放 AircraftClass::Paradrop (0x415C60) 查找空位的流程是：
			//   IsCellOccupied(cell, -1, -1, 0, 1) → CellClass::PickInfantrySublocation → SpawnParachuting。
			// 其中 IsCellOccupied 对同阵营 1~2 个步兵只累计 v47、不累计 v19，仍返回 Move::OK，
			// 所以"格子里有步兵"本身不会让该函数认为格子被占；真正的子格占用要看
			// CellClass::OccupationFlags（0x124 地面 / 0x128 桥上，即 YRpp 的
			// OccupationFlags / AltOccupationFlags）的三个步兵位 1<<2|1<<3|1<<4 = 0x1C。
			// 另外 PickInfantrySublocation 在 0x20 位置位、或 0x40 载车位且无可穿越建筑时直接失败。
			// 还有关键一点：伞兵空中时（SpawnParachuting→InfantryClass::Put 0x51DFF0 的
			// Z != 地面高度，不会调 F0 写子格位）只存在于格子的 Content 列表里，
			// IsCellOccupied 和 OccupationFlags 都看不到它——这就是之前过滤不掉伞兵格的原因。
			// 所以这里步兵落点额外做两层检查：① 镜像引擎的子格位检查；② 扫 Content，
			// 格子里存在任何步兵（含正在落伞的）就视为已被占用，不允许再落进去。
			if (pTechno->WhatAmI() == AbstractType::Infantry)
			{
				DWORD occ = pCell->OccupationFlags; // 地面占用位；桥上还有 AltOccupationFlags
				if ((occ & 0x1C) == 0x1C // 3 个步兵位全占 = 格子满员
					|| (occ & 0x20) != 0) // 引擎 PickInfantrySublocation 的禁用位
				{
					return false;
				}
				if ((occ & 0x40) != 0) // 载车位：上面 IsCellOccupied 一般已拦下，这里兜底
				{
					return false;
				}
				for (ObjectClass* pObj = pCell->GetContent(); pObj; pObj = pObj->NextObject)
				{
					if (pObj != pTechno && pObj->WhatAmI() == AbstractType::Infantry)
					{
						// 人间大炮飞行途中的乘客（空中、非伞降/坠落）只是路过这个格子，
						// 落点由 PreOccupancyManager 管理，不应把路过格判为已占用；
						// 否则车→目标方向的格子全被"飞过的乘客"占掉，落点只剩建筑另一侧。
						bool flyingPass = pObj->IsInAir() && !pObj->IsFallingDown;
						if (!flyingPass)
						{
							return false;
						}
					}
				}
			}
			if (pPreOccupancyManager)
			{
				if (!pPreOccupancyManager->IsPositionAvailable(pTechno, pCell->MapCoords))
				{
					return false;
				}
			}
			return true;
		};

	// 1. 目标格本身可落脚
	if (canLand(pTargetCell))
	{
		return makeLandingPos(pTargetCell);
	}

	// 路径优先搜索：从目标点沿连线向源点方向逐格搜索，找第一个可落脚点
	// （人间大炮默认不用它，避免乘客全部散落在车与目标之间的连线上；
	//   但当包围圈彻底找不到落点时仍用它兜底，保证不会返回 Empty）
	auto searchAlongLine = [&]() -> CoordStruct
		{
			int dist = std::max(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy);
			int samples = dist * 4 + 1;
			CellStruct last = target;

			for (int i = 1; i <= samples; i++)
			{
				double t = 1.0 - (double)i / samples;
				int cx = (int)(source.X + dx * t);
				int cy = (int)(source.Y + dy * t);

				if (cx < 0 || cx >= MapClass::Instance->MaxWidth ||
					cy < 0 || cy >= MapClass::Instance->MaxHeight)
				{
					continue;
				}

				CellStruct c{ static_cast<short>(cx), static_cast<short>(cy) };

				if (c.X == last.X && c.Y == last.Y)
				{
					continue;
				}
				last = c;

				if (c.X == target.X && c.Y == target.Y)
				{
					continue;
				}

				if (CellClass* pCell = MapClass::Instance->TryGetCellAt(c))
				{
					if (canLand(pCell))
					{
						return makeLandingPos(pCell);
					}
				}
			}
			return CoordStruct::Empty;
		};

	// 2. 路径优先（默认开启；人间大炮 usePathPriority=false 时跳过）
	if (usePathPriority)
	{
		CoordStruct linePos = searchAlongLine();
		if (!linePos.IsEmpty())
		{
			return linePos;
		}
	}

	// 3. 以目标为圆心、以"目标→车"连线方向为起始，向左右展开：
	//	先落连线方向上的格，再向两侧依次铺开，先形成朝向车的半月，
	//	填满后围成完整的一圈圆，然后进入下一圈（全局遍历 radius 1..12）。
	//	评分：连线方向优先（angleScore*100）→ 半径近优先（distToTarget*10）
	//	→ 预占惩罚（50/预占，每格先占 1 个再补满 3 个）。

	// 计算"目标→车"的方向向量（归一化），作为半月/圆的起始方向
	double dirX = -(double)dx;
	double dirY = -(double)dy;
	double dirLen = sqrt(dirX * dirX + dirY * dirY);
	if (dirLen > 0.001)
	{
		dirX /= dirLen;
		dirY /= dirLen;
	}
	else
	{
		// 如果起始点和目标点重合，默认朝下
		dirX = 0;
		dirY = 1;
	}

	// 建筑 4x4 时 radius 1~2 大部分在 footprint 内，只有一侧漏出几个格；
	// 如果"逐圈返回"，radius 2 有可用格就直接返回，radius 3 以后建筑
	// 其他方向的空格永远不会被考虑 → 半月固定在某侧、与朝向无关。
	// 所以这里改为全局遍历（半径 1..12），评分以"角度"为主：
	// 连线方向（目标→车）的格先落，再向左右展开成半月→圆，半径只做次排序。
	CellClass* pBest = nullptr;
	double bestScore = 1e18;

	for (int radius = 1; radius <= 12; radius++)
	{
		for (int ox = -radius; ox <= radius; ox++)
		{
			for (int oy = -radius; oy <= radius; oy++)
			{
				// 只取这一圈的外沿
				if ((ox != -radius && ox != radius) && (oy != -radius && oy != radius))
				{
					continue;
				}

				CellStruct c{ static_cast<short>(target.X + ox), static_cast<short>(target.Y + oy) };

				if (c.X < 0 || c.X >= MapClass::Instance->MaxWidth ||
					c.Y < 0 || c.Y >= MapClass::Instance->MaxHeight)
				{
					continue;
				}

				CellClass* pCell = MapClass::Instance->TryGetCellAt(c);
				if (!pCell || !canLand(pCell))
				{
					continue;
				}

				// 候选格相对目标的方向与"目标→车"方向的夹角：
				// cosA=1 → 连线方向（起始点，优先）；cosA=-1 → 正后方（最后）
				double len = sqrt((double)ox * ox + (double)oy * oy);
				double cosA = len > 0.001 ? (ox * dirX + oy * dirY) / len : 1.0;
				double angleScore = 1.0 - cosA; // 0=连线方向，2=正后方

				// 到目标点的距离（半径平方，同角度下近的优先）
				double distToTarget = (double)ox * ox + (double)oy * oy;

				// 已预占子格数：完全空闲的格优先（每格先占 1 个，
				// 铺成半月/圆后再回头把每格补到 3 个），避免全堆在同一格
				double preOccPenalty = 0;
				if (pPreOccupancyManager)
				{
					preOccPenalty = pPreOccupancyManager->GetOccupiedSubCellCount(pCell->MapCoords) * 50.0;
				}

				// 综合评分：
				// 1. 连线方向优先（angleScore 小，权重100）：先连线点，再左右展开成半月/圆
				// 2. 半径近优先（distToTarget 小，权重10）：同一方向先落近的
				// 3. 无预占的格子优先（preOccPenalty 小，权重50/预占）
				double s = angleScore * 100.0 + distToTarget * 10.0 + preOccPenalty;

				if (s < bestScore)
				{
					bestScore = s;
					pBest = pCell;
				}
			}
		}
	}
	if (pBest)
	{
		// 诊断日志（需要排查落点分布时取消注释）：
		// Debug::Log("FindLanding arc: cell{%d,%d} score=%.0f\n",
		// 	pBest->MapCoords.X, pBest->MapCoords.Y, bestScore);
		return makeLandingPos(pBest);
	}

	// 3b. 包围圈也找不到时，兜底走路径优先，尽量保证有落点
	if (!usePathPriority)
	{
		CoordStruct linePos = searchAlongLine();
		if (!linePos.IsEmpty())
		{
			return linePos;
		}
	}

	return CoordStruct::Empty;
}

bool TryGetLandingPoint(TechnoClass* pTechno, const CoordStruct& targetPos, CoordStruct& landingPos, bool forceGround, bool usePathPriority, PreOccupancyManager* pPreOccupancyManager)
{
	landingPos = FindLandingPoint(pTechno, targetPos, forceGround, usePathPriority, pPreOccupancyManager);
	return !landingPos.IsEmpty();
}

