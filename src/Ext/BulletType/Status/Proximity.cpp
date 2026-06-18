#include "../BulletStatus.h"

#include <Ext/Helper/Finder.h>
#include <Ext/Helper/Physics.h>
#include <Ext/Helper/Status.h>

#include <Ext/WeaponType/ProximityRangeData.h>

ProximityData* BulletStatus::GetProximityData()
{
	if (!_proximityData)
	{
		_proximityData = INI::GetConfig<ProximityData>(INI::Rules, pBullet->GetType()->ID)->Data;
	}
	return _proximityData;
}

void BulletStatus::ActiveProximity()
{
	this->_proximity = { pBullet->Type->CourseLockDuration, GetProximityData()->PenetrationTimes };
	this->_activeProximity = true;
}

void BulletStatus::InitState_Proximity() {
	// 设置提前引爆抛射体
	WeaponTypeClass* pWeapon = pBullet->WeaponType;
	if (pWeapon)
	{
		const char* weaponId = pWeapon->ID;
		ProximityRangeData* tempData = INI::GetConfig<ProximityRangeData>(INI::Rules, weaponId)->Data;
		int range = tempData->GetRange();
		if (range > 0)
		{
			this->_proximityRange = range;
			AbstractClass* pTarget = pBullet->Target;
			if (pTarget && pTarget->WhatAmI() == AbstractType::Building)
			{
				BuildingTypeClass* pTargetBuildingType = ((BuildingClass*)pTarget)->Type;
				int distOffset = (pTargetBuildingType->GetFoundationWidth() + pTargetBuildingType->GetFoundationHeight(false)) << 6;
				this->_proximityRange += distOffset;
			}
		}
	}
	// 设置碰触引擎
	if (GetProximityData()->Force)
	{
		ActiveProximity();
	}
};

void BulletStatus::OnUpdateEnd_Proximity(CoordStruct& sourcePos)
{
	if (_proximityRange > 0 && sourcePos.DistanceFrom(pBullet->GetTargetCoords()) <= _proximityRange)
	{
		ManualDetonation(sourcePos);
	}

	// 计算碰触引信
	if (_proximity.active && !_proximity.IsSafe())
	{
		ProximityData* data = GetProximityData();
		CoordStruct checkPos = sourcePos; // 碰撞检测的原点，默认为抛射体当前位置
		// 调整碰撞检测点位置
		if (!data->POffset.IsEmpty())
		{
			checkPos = GetFLHAbsoluteCoords(pBullet, data->POffset);
		}
		if (CellClass* pSourceCell = MapClass::Instance->TryGetCellAt(checkPos))
		{
			// 读取预定目标格子上的建筑
			CellClass* pSourceTargetCell = MapClass::Instance->TryGetCellAt(pBullet->TargetCoords);
			BuildingClass* pSourceTargetBuilding = nullptr;
			if (pSourceTargetCell)
			{
				pSourceTargetBuilding = pSourceTargetCell->GetBuilding();
				// Logger.Log($"{Game.CurrentFrame} 导弹预定目标格子中有建筑 {pSourceTargetBuilding}");
			}
			// 当前所处的位置距离预定飞行目标过近，且在同一格内，跳过碰撞检测
			if (checkPos.DistanceFrom(pBullet->TargetCoords) <= 256)
			{
				if (pSourceCell == pSourceTargetCell)
				{
					return;
				}
			}
			// 计算碰撞的半径，超过1格，确定搜索范围
			int cellSpread = (data->Arm / 256) + 1;
			// 每个格子只检查一次
			if (pSourceCell != _proximity.pCheckedCell)
			{
				_proximity.pCheckedCell = pSourceCell;
				CoordStruct cellPos = pSourceCell->GetCoordsWithBridge();
				CellStruct currentCell = pSourceCell->MapCoords;
				// 获取这个格子上的所有对象
				std::vector<TechnoClass*> pTechnoList = GetCellSpreadTechnos(currentCell, checkPos, cellSpread,
					data->Blade, data->Blade || data->Arm > Unsorted::LevelHeight, false,
					pSourceHouse,
					data->AffectsOwner, data->AffectsAllies, data->AffectsEnemies, data->AffectsEnemies);
				// 筛选并处理找到的对象
				for (TechnoClass* pTarget : pTechnoList)
				{
					if (!IsDeadOrInvisible(pTarget))
					{
						CoordStruct	targetPos = pTarget->GetCoords();
						bool hit = false;
						// 无视高度和距离，格子内的对象都算碰撞目标
						if (pTarget->WhatAmI() == AbstractType::Building)
						{
							if (BuildingClass* pBuilding = abstract_cast<BuildingClass*, true>(pTarget))
							{
								// 检查建筑在范围内
								hit = CanHit(pBuilding, checkPos.Z, data->Blade, data->ZOffset);
								// 检查建筑是否被炸过
								if (hit && data->PenetrationBuildingOnce)
								{
									hit = pBuilding != pSourceTargetBuilding && _proximity.CheckAndMarkBuilding(pBuilding);
								}
							}
						}
						else
						{
							// 使用抛射体所经过的格子的中心点作为判定原点
							CoordStruct sourceTestPos = cellPos;
							// 判定原点抬升至与抛射体同高
							sourceTestPos.Z = checkPos.Z;
							// 目标点在脚下，加上高度修正偏移值
							CoordStruct targetTestPos = targetPos + CoordStruct{ 0, 0, data->ZOffset };
							if (data->Blade)
							{
								// 无视高度，只检查横向距离
								targetTestPos.Z = sourceTestPos.Z;
							}
							double dist = targetTestPos.DistanceFrom(sourceTestPos);
							hit = dist <= data->Arm;
						}

						// 敌我识别并引爆
						if (hit && CanAffectHouse(pTarget->Owner, pSourceHouse, data->AffectsOwner, data->AffectsAllies, data->AffectsEnemies, data->AffectsEnemies))
						{
							// 引爆
							CoordStruct detonatePos = targetPos; // 爆点在与目标位置
							if (ManualDetonation(sourcePos, !data->Penetration, pTarget, detonatePos))
							{
								// 爆了就结束了
								break;
							}
						}
					}
				}
			}
		}
	}
}

bool BulletStatus::ManualDetonation(CoordStruct sourcePos, bool KABOOM, AbstractClass* pTarget, CoordStruct detonatePos)
{
	ProximityData * data = GetProximityData();
	// 检查穿透次数是否用完
	KABOOM = KABOOM || !_proximity.active || !data->Penetration || _proximity.TimesDone();

	if (KABOOM)
	{
		// 抛射体原地爆炸
		pBullet->Detonate(sourcePos);
		pBullet->Limbo();
		pBullet->UnInit();
	}
	else if (pTarget)
	{
		// 抛射体原地爆炸，但不销毁，并且启用自定义武器，自定义弹头
		if (detonatePos.IsEmpty())
		{
			detonatePos = sourcePos;
		}

		// 使用自身制造伤害
		int damage = pBullet->Health;
		WarheadTypeClass* pWH = pBullet->WH;

		// 检查自定义武器是否存在，存在则使用自定义武器制造伤害，不存在就用自身制造伤害
		std::string weaponId = data->PenetrationWeapon;
		if (IsNotNone(weaponId))
		{
			// 对敌人造成自定义武器的伤害
			if (WeaponTypeClass* pWeapon = WeaponTypeClass::Find(weaponId.c_str()))
			{
				damage = pWeapon->Damage;
				pWH = pWeapon->Warhead;
			}
		}
		// 检查是否使用其他弹头
		std::string warheadId = data->PenetrationWarhead;
		if (IsNotNone(warheadId))
		{
			if (WarheadTypeClass* pOverrideWH = WarheadTypeClass::Find(warheadId.c_str()))
			{
				pWH = pOverrideWH;
			}
		}

		// 在预定引爆地点引爆弹头
		MapClass::DamageArea(detonatePos, damage, pBullet->Owner, pWH, pWH->Tiberium, pSourceHouse);
		// 播放弹头动画
		LandType landType = _proximity.pCheckedCell ? _proximity.pCheckedCell->LandType : LandType::Clear;
		if (AnimClass* pAnim = PlayWarheadAnim(pWH, sourcePos, damage, landType))
		{
			pAnim->Owner = pSourceHouse;
		}
		// 计数器减1
		_proximity.ThroughOnce();
	}
	return KABOOM;
}
