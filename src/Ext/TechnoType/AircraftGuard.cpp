#include "AircraftGuard.h"

#include <FlyLocomotionClass.h>

#include <Ext/Helper/DrawEx.h>
#include <Ext/Helper/Finder.h>
#include <Ext/Helper/FLH.h>
#include <Ext/Helper/MathEx.h>

AircraftGuardData* AircraftGuard::GetAircraftGuardData()
{
	if (!_data)
	{
		_data = INI::GetConfig<AircraftGuardData>(INI::Rules, pTechno->GetTechnoType()->ID)->Data;
	}
	return _data;
}

void AircraftGuard::Setup()
{
	_data = nullptr;
	if (!IsAircraft() || !IsFly() || pTechno->GetTechnoType()->MissileSpawn || !pTechno->IsVoxel() || !GetAircraftGuardData()->Enable)
	{
		Disable();
	}
	_destCenter = CoordStruct::Empty;
	_destList.clear();
	_destIndex = 0;
	_onStopCommand = false;
}

bool AircraftGuard::IsAreaGuardRolling()
{
	switch (State)
	{
	case AircraftGuardStatus::GUARD:
	case AircraftGuardStatus::ROLLING:
		return true;
	}
	return false;
}

void AircraftGuard::StartAreaGuard()
{
	switch (State)
	{
	case AircraftGuardStatus::READY:
	case AircraftGuardStatus::GUARD:
	case AircraftGuardStatus::ROLLING:
	{
		// 设定新的航点
		CoordStruct dest = CoordStruct::Empty;
		FootClass* pFoot = abstract_cast<FootClass*, true>(pTechno);
		AbstractClass* pDest = pFoot->Destination;
		if (pDest)
		{
			dest = pDest->GetCoords();
		}
		else
		{
			pFoot->Locomotor->Destination(&dest);
		}
		// 获取到单位的目的地
		if (SetupDestination(dest))
		{
			//  开始警戒巡航
			State = AircraftGuardStatus::GUARD;
		}
		break;
	}
	}
}

bool AircraftGuard::SetupDestination()
{
	CoordStruct dest = CoordStruct::Empty;
	abstract_cast<FootClass*, true>(pTechno)->Locomotor->Destination(&dest);
	return SetupDestination(dest);
}

bool AircraftGuard::SetupDestination(CoordStruct dest)
{
	if (!dest.IsEmpty() && dest != _destCenter && std::find(_destList.begin(), _destList.end(), dest) == _destList.end())
	{
		AircraftGuardData* data = GetAircraftGuardData();
		// 设定巡航点，目的地, 巡航中心点
		_destCenter = dest;
		// 新建巡航点，以飞机所在的位置和目标的位置的朝向为参考方向，画16个点
		CoordStruct location = pTechno->GetCoords();
		// BulletEffectHelper.GreenLine(dest, location, 1, 450);
		DirStruct sourceDir = Point2Dir(location, dest);
		double sourceRad = sourceDir.GetRadian();
		CoordStruct flh{ data->GuardRadius * Unsorted::LeptonsPerCell, 0, 0 };
		_destList.clear();
		bool clockwise = data->Clockwise;
		if (data->Randomwise)
		{
			clockwise = Random::RandomRanged(0, 1) == 0;
		}
		Clockwise = clockwise; // 顺时针还是逆时针巡航
		for (int i = 0; i < 16; i++)
		{
			DirStruct targetDir = DirNormalized(i, 16);
			double targetRad = targetDir.GetRadian();
			float angle = (float)(sourceRad - targetRad);
			targetDir = Radians2Dir(angle);
			CoordStruct newDest = GetFLHAbsoluteCoords(dest, flh, targetDir);
			if (i == 0)
			{
				// 第一个肯定是队列头，位于飞机前进方向正前方
				_destList.push_back(newDest);
			}
			else
			{
				// 顺序添加为逆时针，插入第二位为顺时针
				if (clockwise)
				{
					_destList.insert(_destList.begin(), newDest);
				}
				else
				{
					_destList.push_back(newDest);
				}
			}
		}
		_destIndex = 0;
		return true;
	}
	return false;
}

void AircraftGuard::CancelAreaGuard()
{
	_destCenter = CoordStruct::Empty;
	_destList.clear();
	_destIndex = 0;
}

void AircraftGuard::BackToAirport()
{
	pTechno->SetTarget(nullptr);
	pTechno->ForceMission(Mission::Enter);
}

bool AircraftGuard::FoundAndAttack(CoordStruct location)
{
	AircraftGuardData* data = GetAircraftGuardData();
	if (data->AutoFire)
	{
		// 飞机有武器
		WeaponStruct* pPrimary = pTechno->GetWeapon(0);
		bool hasPrimary = pPrimary && pPrimary->WeaponType && !pPrimary->WeaponType->NeverUse;
		WeaponStruct* pSecondary = pTechno->GetWeapon(1);
		bool hasSecondary = pSecondary && pSecondary->WeaponType && !pSecondary->WeaponType->NeverUse;
		if (hasPrimary || hasSecondary)
		{
			CoordStruct sourcePos = location;
			if (!data->FindRangeAroundSelf)
			{
				// 以航点为中心搜索可以攻击的目标
				sourcePos = _destCenter;
			}
			int cellSpread = data->GuardRange;
			// 搜索可以攻击的目标
			AbstractClass* pTarget = nullptr;
			// 使用Cell搜索目标
			bool canAA = (hasPrimary && pPrimary->WeaponType->Projectile->AA) || (hasSecondary && pSecondary->WeaponType->Projectile->AA);
			// 检索范围内的单位类型
			std::vector<TechnoDistance> pTechnoList = GetCellSpreadTechnos(sourcePos, cellSpread, false, canAA, false);
			// TODO 对检索到的单位按威胁值排序
			for (TechnoDistance& item : pTechnoList)
			{
				TechnoClass* pTargetTechno = item.pTechno;
				// 检查能否攻击
				if (CheckTarget(pTargetTechno))
				{
					pTarget = pTargetTechno;
					break;
				}
			}
			// 检索不到常规目标，检索替身
			if (!pTarget)
			{
				double dist = (cellSpread <= 0 ? 1 : cellSpread) * Unsorted::LeptonsPerCell;
				for (auto standExt : TechnoExt::StandArray)
				{

					TechnoClass* pStand = standExt.first;
					StandData standData = standExt.second;
					if (!standData.Immune && pStand != pTechno
						&& sourcePos.DistanceFrom(pStand->GetCoords()) <= dist
						&& CheckTarget(pStand))
					{
						pTarget = pStand;
						break;
					}
				}
			}
			// 检索到一个目标
			if (pTarget)
			{
				pTechno->SetTarget(pTarget);
				pTechno->QueueMission(Mission::Attack, true);
				return true;
			}
		}
	}
	return false;
}

bool AircraftGuard::CheckTarget(TechnoClass* pTarget)
{
	bool pick = false;
	HouseClass* pHouse = pTechno->Owner;
	HouseClass* pTargetHouse = pTarget->Owner;
	if (!IsDeadOrInvisibleOrCloaked(pTarget)
		&& !pTarget->GetTechnoType()->Insignificant
		&& !pTargetHouse->IsAlliedWith(pHouse) // 本身不是盟友
		// 伪装后也不是盟友
		&& (!pTarget->IsDisguised() || pTarget->IsClearlyVisibleTo(pHouse) || (pTargetHouse = pTarget->GetDisguiseHouse(true)) != nullptr)
		&& !pTargetHouse->IsAlliedWith(pHouse))
	{
		pick = true;
		// 检查平民
		if (IsCivilian(pTarget->Owner))
		{
			// Ares 的平民敌对目标
			pick = INI::GetSection(INI::Rules, pTarget->GetTechnoType()->ID)->Get("CivilianEnemy", false);
			// Ares 的反击平民
			TechnoClass* pTargetTechno = nullptr;
			if (!pick && AutoRepel(pHouse) && pTarget->Target && CastToTechno(pTarget->Target, pTargetTechno))
			{
				pick = pHouse->IsAlliedWith(pTargetTechno->Owner);
			}
		}
	}
	if (pick)
	{
		// 能否对其进行攻击
		pick = CanAttack(pTechno, pTarget, false, -1, true);
	}
	return pick;
}

void AircraftGuard::Awake()
{
	Setup();
}

void AircraftGuard::ExtChanged()
{
	Setup();
}

void AircraftGuard::OnUpdate()
{
	// AI不允许使用这个逻辑
	HouseClass* pHouse = pTechno->Owner;
	if (!pHouse || !pHouse->IsControlledByHuman())
	{
		return;
	}
	if (!IsDeadOrInvisible(pTechno))
	{
		AircraftGuardData* data = GetAircraftGuardData();
		FootClass* pFoot = abstract_cast<FootClass*, true>(pTechno);
		ILocomotion* loco = pFoot->Locomotor.get();
		switch (State)
		{
		case AircraftGuardStatus::READY:
		{
			// 停在机场等待起飞
			if (bool inAir = pTechno->IsInAir())
			{
				// 移动设定巡航点
				if (data->AutoGuard)
				{
					switch (pTechno->CurrentMission)
					{
					case Mission::Attack:
					case Mission::Enter:
						// 攻击或返航途中，什么都不做
						CancelAreaGuard();
						return;
					case Mission::Guard:
					case Mission::Area_Guard:
					case Mission::Move:
						// 返航途中有新的移动指令
						if (SetupDestination())
						{
							if (_onStopCommand)
							{
								// 按下S，跳过一次目的地更改
								_onStopCommand = false;
							}
							else
							{
								State = AircraftGuardStatus::GUARD;
							}
						}
						return;
					}
				}
			}
			else
			{
				_onStopCommand = false;
				CancelAreaGuard();
				// 弹药充足，自动起飞
				if (data->DefaultToGuard && pTechno->Ammo >= data->MaxAmmo)
				{
					CoordStruct sourcePos = pTechno->GetCoords();
					if (CellClass* pCell = MapClass::Instance->TryGetCellAt(sourcePos))
					{
						// 在地面待机中，设置当前格子为新巡航点
						SetupDestination(pCell->GetCoordsWithBridge());
						// 开始警戒巡航
						State = AircraftGuardStatus::GUARD;
					}
				}
			}
			break;
		}
		case AircraftGuardStatus::STOP:
		{
			// 什么都不做，等待飞机降落
			if (pTechno->GetHeight() <= 0)
			{
				State = AircraftGuardStatus::READY;
			}
			// 即使按下S之后，Mission仍然是Area_Guard，此时不能切换至Ready
			switch (pTechno->CurrentMission)
			{
			case Mission::Attack:
			case Mission::Enter:
			case Mission::Move: // 鼠标指令前往别处
				State = AircraftGuardStatus::READY;
				return;
			}
			break;
		}
		case AircraftGuardStatus::ATTACK:
		{
			AbstractClass* pTarget = pTechno->Target;
			if (!pTarget)
			{
				// 目标不存在，带着蛋飞，切换状态
				if (!_destCenter.IsEmpty() && pTechno->Ammo > data->MinAmmo)
				{
					// 返回巡航状态
					State = AircraftGuardStatus::GUARD;
					pTechno->ForceMission(Mission::Area_Guard);
				}
				else
				{
					// 返航
					BackToAirport();
					State = AircraftGuardStatus::RELOAD;
				}
			}
			else if (data->ChaseRange > 0)
			{
				int dist = data->ChaseRange * Unsorted::LeptonsPerCell;
				CoordStruct targetPos = pTarget->GetCoords();
				if (targetPos.DistanceFrom(pTechno->GetCoords()) > dist)
				{
					// 超出追击距离
					pTechno->SetTarget(nullptr);
				}
			}
			break;
		}
		case AircraftGuardStatus::RELOAD:
		{
			// 返航等待油满重新出发
			if (!pTechno->IsInAir() && pTechno->Ammo >= data->MaxAmmo)
			{
				State = AircraftGuardStatus::GUARD;
				loco->Move_To(_destCenter);
				pTechno->ForceMission(Mission::Area_Guard); // Mission 会变成 Guard
			}
			break;
		}
		case AircraftGuardStatus::GUARD:
		case AircraftGuardStatus::ROLLING:
		{
			// 巡航直到进入攻击状态或者按下S键停止
			switch (pTechno->CurrentMission)
			{
			case Mission::Guard: // 重新起飞前往记录的航点
			case Mission::Area_Guard: // 正常巡航
				break;
			case Mission::Move: // 鼠标指令前往别处
				if (data->AutoGuard)
				{
					// 巡航中，获得移动指令，会重设航点
					// Logger.Log($"{Game.CurrentFrame} 巡航中，设置巡航点");
					if (SetupDestination())
					{
						State = AircraftGuardStatus::GUARD;
					}
					break;
				}
				CancelAreaGuard();
				State = AircraftGuardStatus::STOP;
				return;
			case Mission::Enter:
				// Logger.Log($"{Game.CurrentFrame} 从巡航中切换返航");
				if (_onStopCommand && data->DefaultToGuard && pTechno->Ammo >= data->MaxAmmo)
				{
					// 还有蛋，不返航，回基地巡逻
					pTechno->ForceMission(Mission::Area_Guard);
				}
				else
				{
					// 命令返航
					State = AircraftGuardStatus::STOP;
				}
				return;
			default:
				// Logger.Log($"{Game.CurrentFrame} [{section}]{pTechno} 暂停巡航");
				State = AircraftGuardStatus::RELOAD;
				return;
			}
			// 不带蛋飞行
			if (pTechno->Ammo == 0)
			{
				BackToAirport();
				State = AircraftGuardStatus::RELOAD;
				return;
			}
			// 当前飞机需要前往的目的地
			CoordStruct destNow = CoordStruct::Empty;
			loco->Destination(&destNow);
			CoordStruct location = pTechno->GetCoords();
			if (FoundAndAttack(location))
			{
				State = AircraftGuardStatus::ATTACK;
			}
			else
			{
				// 检查是否需要更换巡航点
				if (destNow != _destCenter && std::find(_destList.begin(), _destList.end(), destNow) == _destList.end() && pTechno->CurrentMission != Mission::Area_Guard)
				{
					// WWSB, 机场有飞机停下去之后会召回在外面的飞机
					loco->Move_To(_destList[_destIndex]);
				}
				// 无视Z坐标判断距离
				CoordStruct posA = location;
				posA.Z = 0;
				CoordStruct posB = destNow;
				posB.Z = 0;
				bool changeDest = posA.DistanceFrom(posB) <= 512;
				if (changeDest)
				{
					if (_destIndex > 0)
					{
						// 进入转圈状态
						State = AircraftGuardStatus::ROLLING;
					}
					// 清除飞机的目的地
					pTechno->SetDestination(nullptr, false);
					// 前往下一个巡航点
					CoordStruct destNew = _destList[_destIndex];
					loco->Move_To(destNew);
					if (++_destIndex >= (int)_destList.size())
					{
						_destIndex = 0;
					}
				}
			}
			break;
		}
		}
	}
}

void AircraftGuard::OnStopCommand()
{
	_onStopCommand = true;
	CancelAreaGuard();
	State = AircraftGuardStatus::STOP;
	// 任务改成返航
	pTechno->ForceMission(Mission::Enter);
}

