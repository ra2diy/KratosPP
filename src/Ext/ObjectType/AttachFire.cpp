#include "AttachFire.h"

void AttachFire::FireMissionDone()
{
	_delayFires.clear();
	_simulateBurst.clear();
}

void AttachFire::FireOwnWeapon(int weaponIdx, AbstractClass* pTarget, int delay, int count)
{
	DelayFire delayFire{ weaponIdx, pTarget, delay, count };
	_delayFires.emplace_back(delayFire);
}

bool AttachFire::FireCustomWeapon(TechnoClass* pAttacker, AbstractClass* pTarget, HouseClass* pAttackingHouse,
	WeaponTypeClass* pWeapon, WeaponTypeExt::TypeData weaponTypeData,
	CoordStruct flh, bool isOnBody, bool isOnTarget, FireBulletToTarget callback, CoordStruct sourcePos)
{
	// 不允许朝这个目标发射
	if (!weaponTypeData.CanFireToTarget(pTarget, abstract_cast<ObjectClass*>(pObject), pAttacker, pAttackingHouse, pWeapon))
	{
		return false;
	}
	int burst = pWeapon->Burst;
	int minRange = pWeapon->MinimumRange;
	int maxRange = pWeapon->Range;
	// 检查抛射体是否具有AA
	if (pTarget->IsInAir())
	{
		if (weaponTypeData.CheckAA && !pWeapon->Projectile->AA)
		{
			// 抛射体没有AA，终止发射
			return false;
		}
		if (pTechno)
		{
			maxRange += pTechno->GetTechnoType()->AirRangeBonus;
		}
	}
	else if (weaponTypeData.CheckAAOnly && pWeapon->Projectile->AA)
	{
		// 目标停在地上，抛射体是AAOnly，终止发射
		TechnoTypeClass* pTechnoType = pAttacker->GetTechnoType();
		if (pTechnoType && pTechnoType->LandTargeting != LandTargetingType::Land_Not_OK)
		{
			BulletTypeExt::TypeData* bulletData = GetTypeData<BulletTypeExt, BulletTypeExt::TypeData>(pWeapon->Projectile);
			if (bulletData && bulletData->AAOnly)
			{
				return false;
			}
		}
	}
	// 检查射程
	if (!weaponTypeData.CheckRange || InRange(pObject, pTarget, pWeapon, minRange, maxRange))
	{
		// 可以发射武器
		if (burst > 1 && weaponTypeData.SimulateBurst)
		{
			// burst 发射
			int flipY = 1;
			BulletTypeClass* pBulletType = pWeapon->Projectile;
			if (pBulletType)
			{
				// 翻转抛射体的速度，左右对调
				TrajectoryData* trajectoryData = INI::GetConfig<TrajectoryData>(INI::Rules, pBulletType->ID)->Data;
				if (trajectoryData->ReverseVelocity)
				{
					flipY = -1;
				}
			}
			// 模拟burst发射武器
			SimulateBurst newBurst{ pTarget, pAttacker, pAttackingHouse, pWeapon, weaponTypeData, minRange, maxRange, flh, isOnBody, isOnTarget, burst, flipY, callback };
			SimulateBurstFire(newBurst);
			// 入队
			_simulateBurst.emplace_back(newBurst);
		}
		else
		{
			// 直接发射武器
			DirStruct facingDir{};
			CoordStruct srcPos = sourcePos;
			CoordStruct targetPos = pTarget->GetCoords();
			if (srcPos.IsEmpty())
			{
				if (isOnTarget)
				{
					CoordStruct location = pObject->GetCoords(); // 射手的位置
					srcPos = GetSourcePosOnTarget(location, targetPos, flh, facingDir);
				}
				else
				{
					srcPos = GetSourcePos(flh, facingDir);
				}
			}
			// 扇形攻击
			RadialFire radialFire{ facingDir, burst, weaponTypeData.RadialAngle };
			BulletVelocity bulletVelocity = GetBulletVelocity(srcPos, targetPos);
			for (int i = 0; i < burst; i++)
			{
				if (weaponTypeData.RadialFire)
				{
					bulletVelocity = radialFire.GetBulletVelocity(i, weaponTypeData.RadialZ);
				}
				// 发射武器，全射出去
				BulletClass* pBullet = FireBulletTo(abstract_cast<ObjectClass*>(pObject), pAttacker, pTarget, pAttackingHouse, pWeapon, srcPos, targetPos, bulletVelocity, flh, !isOnBody);
				// 记录下子机发射器的开火坐标
				if (pWeapon->Spawner)
				{
					SpawnerBurstFLH[i] = srcPos;
				}
				if (callback != nullptr)
				{
					callback(i, burst, pBullet, pTarget);
				}
			}
		}
		return true;
	}
	return false;
}

bool AttachFire::FireCustomWeapon(TechnoClass* pAttacker, AbstractClass* pTarget, HouseClass* pAttackingHouse,
	std::string weaponId,
	CoordStruct flh, bool isOnBody, bool isOnTarget, FireBulletToTarget callback, CoordStruct sourcePos)
{
	bool isFire = false;
	WeaponTypeClass* pWeapon = WeaponTypeClass::Find(weaponId.c_str());
	if (pWeapon)
	{
		WeaponTypeExt::TypeData weaponTypeData = *GetTypeData<WeaponTypeExt, WeaponTypeExt::TypeData>(pWeapon);
		isFire = FireCustomWeapon(pAttacker, pTarget, pAttackingHouse, pWeapon, weaponTypeData, flh, isOnBody, isOnTarget, callback, sourcePos);
	}
	return isFire;
}

CoordStruct AttachFire::GetSourcePos(CoordStruct flh, DirStruct& facingDir, bool isOnTurret, int flipY)
{
	CoordStruct sourcePos = pObject->GetCoords();
	if (pTechno)
	{
		sourcePos = GetFLHAbsoluteCoords(pTechno, flh, isOnTurret, flipY);
		facingDir = pTechno->GetRealFacing().Current();
	}
	else if (pBullet)
	{
		facingDir = Facing(pBullet, sourcePos);
		CoordStruct tempFLH = flh;
		tempFLH.Y *= flipY;
		sourcePos = GetFLHAbsoluteCoords(sourcePos, tempFLH, facingDir);
	}
	return sourcePos;
}

CoordStruct AttachFire::GetSourcePosOnTarget(CoordStruct sourcePos, CoordStruct targetPos, CoordStruct flh, DirStruct& facingDir, int flipY)
{
	facingDir = Point2Dir(sourcePos, targetPos);
	CoordStruct tempFLH = flh;
	tempFLH.Y *= flipY;
	return GetFLHAbsoluteCoords(targetPos, tempFLH, facingDir);
}

bool AttachFire::IsInRange(AbstractClass* pTarget, SimulateBurst burst)
{
	return InRange(pObject, pTarget, burst.pWeapon, burst.MinRange, burst.MaxRange);
}


void AttachFire::SimulateBurstFire(SimulateBurst& burst)
{
	// burst模式3，双发
	if (burst.WeaponTypeData.SimulateBurstMode == 3)
	{
		SimulateBurst b2 = burst;
		b2.FlipY *= -1;
		SimulateBurstFireOnce(b2);
	}
	SimulateBurstFireOnce(burst);
}

void AttachFire::SimulateBurstFireOnce(SimulateBurst& burst)
{
	DirStruct facingDir{};
	CoordStruct sourcePos = CoordStruct::Empty;
	CoordStruct targetPos = burst.pTarget->GetCoords();
	if (burst.IsOnTarget)
	{
		CoordStruct location = pObject->GetCoords(); // 射手的位置
		sourcePos = GetSourcePosOnTarget(location, targetPos, burst.FLH, facingDir, burst.FlipY);
	}
	else
	{
		sourcePos = GetSourcePos(burst.FLH, facingDir, !burst.IsOnBody, burst.FlipY);
	}

	BulletVelocity bulletVelocity = BulletVelocity::Empty;
	// 扇形攻击
	if (burst.WeaponTypeData.RadialFire)
	{
		RadialFire radialFire{ facingDir, burst.Burst, burst.WeaponTypeData.RadialAngle };
		bulletVelocity = radialFire.GetBulletVelocity(burst.Index, burst.WeaponTypeData.RadialZ);
	}
	else
	{
		bulletVelocity = GetBulletVelocity(sourcePos, targetPos);
	}
	// 发射武器
	WeaponTypeClass* pWeapon = burst.pWeapon;
	CoordStruct tempFLH = burst.FLH;
	tempFLH.Y *= burst.FlipY;
	BulletClass* pBullet = FireBulletTo(abstract_cast<ObjectClass*>(pObject), burst.pAttacker, burst.pTarget, burst.pAttackingHouse, pWeapon, sourcePos, targetPos, bulletVelocity, tempFLH, !burst.IsOnBody);
	// 记录下子机发射器的开火坐标
	if (pWeapon->Spawner)
	{
		SpawnerBurstFLH[burst.Index] = sourcePos;
	}
	if (burst.Callback != nullptr)
	{
		burst.Callback(burst.Index, burst.Burst, pBullet, burst.pTarget);
	}
	burst.CountOne();
}

void AttachFire::OnUpdate()
{
	if (IsDeadOrInvisible(pObject))
	{
		FireMissionDone();
		return;
	}
	// 发射自身武器
	if (pTechno)
	{
		TechnoClass* pShooter = pTechno;
		int size = _delayFires.size();
		for (int i = 0; i < size; i++)
		{
			auto it = _delayFires.begin();
			DelayFire delayFire = *it;
			_delayFires.erase(it);
			if (!delayFire.Invalid)
			{
				if (delayFire.TimesUp())
				{
					// 发射武器
					if (delayFire.FireOwnWeapon)
					{
						pShooter->Fire_IgnoreType(delayFire.pTarget, delayFire.WeaponIndex);
					}
					else
					{
						if (!FireCustomWeapon(pShooter, delayFire.pTarget, pShooter->Owner, delayFire.pWeapon, delayFire.WeaponTypeData, delayFire.FLH))
						{
							delayFire.Done();
						}
					}
					delayFire.RecuceOnce();
				}
			}
			if (delayFire.NotDone())
			{
				_delayFires.emplace_back(delayFire);
			}
		}
	}
	else
	{
		_delayFires.clear();
	}
	// 模拟Burst发射
	int size = _simulateBurst.size();
	for (int i = 0; i < size; i++)
	{
		auto it = _simulateBurst.begin();
		SimulateBurst burst = *it;
		_simulateBurst.erase(it);
		if (!burst.Invalid)
		{
			// 检查余弹
			if (burst.Index < burst.Burst)
			{
				// 检查延迟
				if (burst.CanFire())
				{
					AbstractClass* pTarget = burst.pTarget; // 武器的目标
					WeaponTypeClass* pWeapon = burst.pWeapon;

					TechnoClass* pTargetTechno = nullptr;
					// 检查目标幸存和射程
					if (pWeapon && pTarget
						&& (!CastToTechno(pTarget, pTargetTechno) || (!IsDeadOrInvisible(pTargetTechno) && !pTargetTechno->IsFallingDown)) // 如果目标是单位，检查存活情况
						&& (!burst.WeaponTypeData.CheckRange || IsInRange(pTarget, burst)) // 射程之内
						&& (!burst.WeaponTypeData.CheckAA || !pTarget->IsInAir() || pWeapon->Projectile->AA) // 检查AA
						)
					{
						// 发射
						SimulateBurstFire(burst);
					}
					else
					{
						// 武器失效，任务取消
						continue;
					}
				}
				// 归队
				_simulateBurst.emplace_back(burst);
			}
		}
	}
}

