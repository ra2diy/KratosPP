#include "AutoWeaponEffect.h"

#include <Ext/Helper/Finder.h>
#include <Ext/Helper/FLH.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>
#include <Ext/Helper/Weapon.h>

#include <Extension/BulletExt.h>
#include <Extension/TechnoExt.h>

#include <Ext/BulletType/BulletStatus.h>
#include <Ext/TechnoType/TechnoStatus.h>
#include <Ext/ObjectType/AttachFire.h>

void AutoWeaponEffect::SetupFakeTargetToBullet(int index, int burst, BulletClass*& pBullet, AbstractClass*& pTarget)
{
	if (ObjectClass* pObj = abstract_cast<ObjectClass*>(pTarget))
	{
		if (BulletStatus* status = GetStatus<BulletExt, BulletStatus>(pBullet))
		{
			status->SetFakeTarget(pObj);
		}
	}
}

bool AutoWeaponEffect::CheckROF(WeaponTypeClass* pWeapon, WeaponTypeExt::TypeData* weaponData)
{
	bool canFire = false;
	std::string weaponId = pWeapon->ID;
	// 进行ROF检查
	canFire = !weaponData->CheckROF;
	if (!canFire)
	{
		auto it = _rof.find(weaponId);
		if (it != _rof.end())
		{
			canFire = it->second.Expired();
		}
		else
		{
			canFire = true;
		}
	}
	return canFire;
}

void AutoWeaponEffect::ResetROF(WeaponTypeClass* pWeapon, WeaponTypeExt::TypeData* weaponData, double rofMultip)
{
	std::string weaponId = pWeapon->ID;
	int rof = (int)(pWeapon->ROF * rofMultip);
	auto it = _rof.find(weaponId);
	if (it != _rof.end())
	{
		it->second.Start(rof);
	}
	else
	{
		CDTimerClass timer{ rof };
		_rof[weaponId] = timer;
	}
}

bool AutoWeaponEffect::TryGetShooterAndTarget(TechnoClass* pReceiverOwner, HouseClass* pReceiverHouse, AbstractClass* pReceiverTarget,
	ObjectClass*& pShooter, TechnoClass*& pAttacker, HouseClass*& pAttackingHouse, AbstractClass*& pTarget,
	bool& dontMakeFakeTarget)
{
	// 默认情况下，由标记持有者朝向预设位置开火
	pShooter = pObject;
	pAttacker = pReceiverOwner;
	pAttackingHouse = pReceiverHouse;
	pTarget = nullptr;
	dontMakeFakeTarget = false;
	// 更改射手
	if (!Data->ReceiverAttack)
	{
		// IsAttackerMark=yes时ReceiverAttack和ReceiverOwnBullet默认值为no
		// 若无显式修改，此时应为攻击者朝AE附属对象进行攻击
		// 由攻击者开火，朝向AE附属对象进行攻击
		pShooter = AE->pSource;
		pAttacker = AE->pSource;
		pAttackingHouse = AE->pSourceHouse;
		pTarget = pObject;
	}
	// 更改所属
	if (Data->ReceiverOwnBullet)
	{
		pAttacker = pReceiverOwner;
		pAttackingHouse = pReceiverHouse;
	}
	else
	{
		pAttacker = AE->pSource;
		pAttackingHouse = AE->pSourceHouse;
	}
	// 设定目标
	if (Data->IsAttackerMark)
	{
		// IsAttackerMark=yes时ReceiverAttack和ReceiverOwnBullet默认值为no
		// 若无显式修改，此时应为攻击者朝AE附属对象进行攻击
		// 只有显式修改 ReceiverAttack时，说明是由AE附属对象朝向攻击者攻击
		// 修改目标为攻击者
		if (Data->ReceiverAttack)
		{
			pTarget = AE->pSource;

		}
	}
	else if (Data->FireToTarget)
	{
		pTarget = pReceiverTarget;
		// 如果附属对象的目标不存在，此时应为无法开火，固定返回true不创建假目标
		dontMakeFakeTarget = true;
	}
	// 检查攻击目标是否藏在载具内，设定攻击目标为载具
	TechnoClass* pTagretTechno = nullptr;
	if (pTarget && CastToTechno(pTarget, pTagretTechno))
	{
		pTarget = WhoIsShooter(pTagretTechno);

	}
	return pTarget == nullptr;
}

ObjectClass* AutoWeaponEffect::MakeFakeTarget(HouseClass* pHouse, ObjectClass* pShooter, CoordStruct fireFLH, CoordStruct targetFLH)
{
	CoordStruct targetPos;
	CoordStruct location = pShooter->GetCoords();
	DirStruct dir;
	// 确定假想敌的位置
	if (Data->IsOnWorld)
	{
		// 绑定世界坐标，以射手为参考移动位置
		targetPos = GetFLHAbsoluteCoords(location, targetFLH, dir);
	}
	else
	{
		TechnoClass* pShooterTechno = nullptr;
		BulletClass* pShooterBullet = nullptr;
		if (CastToTechno(pShooter, pShooterTechno))
		{
			// 以射手为参考获取相对位置
			targetPos = GetFLHAbsoluteCoords(pShooterTechno, targetFLH, Data->IsOnTurret);
		}
		else if (CastToBullet(pShooter, pShooterBullet))
		{
			dir = Facing(pBullet, location);
			targetPos = GetFLHAbsoluteCoords(location, targetFLH, dir);
		}
	}
	if (!targetPos.IsEmpty())
	{
		// 创建假想敌
		if (OverlayTypeClass* pOverlayType = OverlayTypeClass::Array.get()->GetItem(0))
		{
			ObjectClass* pFakeTarget = pOverlayType->CreateObject(pHouse);
			pFakeTarget->SetLocation(targetPos);
			pFakeTarget->InLimbo = false;
			pFakeTarget->IsVisible = false;
			return pFakeTarget;
		}
	}
	return nullptr;
}

void AutoWeaponEffect::OnUpdate()
{
	if (!AE->OwnerIsDead())
	{
		if (Data->Powered && AE->AEManager->PowerOff)
		{
			// 需要电力，但是没电
			return;
		}

		AutoWeaponEntity data = Data->Data;
		TechnoClass* pReceiverOwner = nullptr; // 附着的对象，如果是Bullet类型，则是Bullet的发射者
		HouseClass* pReceiverHouse = nullptr; // 附着的对象的所属
		AbstractClass* pReceiverTarget = nullptr; // 附着的对象当前的目标

		bool isOnBullet = pBullet != nullptr;
		if (pTechno)
		{
			pReceiverOwner = pTechno;
			pReceiverHouse = pReceiverOwner->Owner;
			pReceiverTarget = pReceiverOwner->Target;
			// 获取武器设置
			if (pReceiverOwner->Veterancy.IsElite())
			{
				data = Data->EliteData;
			}
		}
		else if (pBullet)
		{
			pReceiverOwner = pBullet->Owner;
			pReceiverHouse = GetHouse(pBullet);
			pReceiverTarget = pBullet->Target;
		}
		else
		{
			End(CoordStruct::Empty);
			return;
		}

		if (!data.Enable)
		{
			// 没有可以使用的武器
			return;
		}

		// 检查平民
		if (Data->DeactiveWhenCivilian && IsCivilian(pReceiverHouse))
		{
			return;
		}

		bool attackerInvisible = IsDead(AE->pSource) || AE->pSource->IsImmobilized || IsDeadOrInvisible(WhoIsShooter(AE->pSource));
		bool bulletOwnerInvisible = isOnBullet && (IsDeadOrInvisible(pReceiverOwner) || pReceiverOwner->IsImmobilized || IsDeadOrInvisible(WhoIsShooter(pReceiverOwner)));
		// 攻击者标记下，攻击者死亡或不存在，如果在抛射体上，而抛射体的发射者死亡或不存在，AE结束，没有启用标记，却设置了反向，同样结束AE
		if (Data->IsAttackerMark ? (attackerInvisible || bulletOwnerInvisible) : !Data->ReceiverAttack)
		{
			End(CoordStruct::Empty);
			return;
		}
		// 进入发射流程
		ObjectClass* pShooter = nullptr;
		TechnoClass* pAttacker = nullptr;
		HouseClass* pAttackingHouse = nullptr;
		AbstractClass* pTarget = nullptr;

		bool dontMakeFakeTarget = false;
		bool needFakeTarget = TryGetShooterAndTarget(pReceiverOwner, pReceiverHouse, pReceiverTarget,
			pShooter, pAttacker, pAttackingHouse, pTarget,
			dontMakeFakeTarget);
		if (needFakeTarget && dontMakeFakeTarget)
		{
			// 目标为空，并且不构建假目标，发射终止
			return;
		}
		if (IsDeadOrInvisible(pShooter))
		{
			// 发射武器的对象不存在，发射终止
			return;
		}

		bool weaponLaunch = false;
		TechnoClass* pShooterTechno = nullptr;
		BulletClass* pShooterBullet = nullptr;
		AttachFire* pAttachFire = nullptr;
		if (CastToTechno(pShooter, pShooterTechno))
		{
			pAttachFire = FindOrAttachScript<TechnoExt, AttachFire>(pShooterTechno);
		}
		else if (CastToBullet(pShooter, pShooterBullet))
		{
			pAttachFire = FindOrAttachScript<BulletExt, AttachFire>(pShooterBullet);
		}
		if (!pAttachFire)
		{
			Debug::Log("Error: AE [%s] cannot find the AttachFire componet on Shooter [%s]\n", AEData.Name.c_str(), pShooter->GetType()->ID);
			return;
		}
		// 发射武器是单位本身的武器还是自定义武器
		if (data.WeaponIndex >= 0 && pShooterTechno)
		{
			// 发射单位自身的武器
			// 获取发射单位的ROF加成
			double rofMultip = GetROFMulti(pShooterTechno);
			rofMultip *= AE->AEManager->CountAttachStatusMultiplier().ROFMultiplier;
			// 检查武器是否存在，是否ROF结束
			WeaponStruct* pWeaponStruct = pShooterTechno->GetWeapon(data.WeaponIndex);
			WeaponTypeClass* pWeapon = nullptr;
			if (pWeaponStruct && (pWeapon = pWeaponStruct->WeaponType) != nullptr)
			{
				WeaponTypeExt::TypeData* weaponData = GetTypeData<WeaponTypeExt, WeaponTypeExt::TypeData>(pWeapon);
				if (CheckROF(pWeapon, weaponData))
				{
						FireBulletToTarget callback = nullptr;
						// 可以发射
						if (needFakeTarget && pReceiverHouse)
						{
							pTarget = MakeFakeTarget(pReceiverHouse, pShooter, data.FireFLH, data.TargetFLH);
							callback = SetupFakeTargetToBullet;
						}
						if (pTarget)
						{
							// 如果攻击者是子机，调整攻击者为母鸡
							if (pAttacker && pAttacker->SpawnOwner && Data->AttackFromSpawnOwner)
							{
								pAttacker = pAttacker->SpawnOwner;
							}
							// 发射武器
							weaponLaunch = pAttachFire->FireCustomWeapon(pAttacker, pTarget, pAttackingHouse,
								pWeapon, *weaponData,
								data.FireFLH, !Data->IsOnTurret, Data->IsOnTarget,
								callback,
								AE->FromWarhead ? AE->WarheadLocation : CoordStruct::Empty);
							if (weaponLaunch)
								ResetROF(pWeapon, weaponData, rofMultip);
						}
				}
			}
		}
		else if (!data.WeaponTypes.empty())
		{
			std::vector<std::string> weaponTypes;
			// 发射自定义的武器
			int randomNum = data.RandomTypesNum;
			// 随机发射武器
			if (randomNum > 0)
			{
				// 计算权重
				int maxValue = 0;
				std::map<Point2D, int> targetPad = MakeTargetPad(data.RandomWeights, data.WeaponTypes.size(), maxValue);
				// 算出随机值，确认位置，取得序号，选出武器
				for (int i = 0; i < randomNum; i++)
				{
					int index = Hit(targetPad, maxValue);
					// 选出的武器
					std::string weaponType = data.WeaponTypes[index];
					weaponTypes.push_back(weaponType);
				}
			}
			else
			{
				weaponTypes.assign(data.WeaponTypes.begin(), data.WeaponTypes.end());
			}
			// 获取ROF加成
			double rofMultip = GetROFMulti(pAttacker);
			rofMultip *= AE->AEManager->CountAttachStatusMultiplier().ROFMultiplier;
			// 如果攻击者是子机，调整攻击者为母鸡
			if (pAttacker && pAttacker->SpawnOwner && Data->AttackFromSpawnOwner)
			{
				pAttacker = pAttacker->SpawnOwner;
			}
			// 正式发射武器
			for (std::string weaponId : weaponTypes)
			{
				if (IsNotNone(weaponId))
				{
					WeaponTypeClass* pWeapon = WeaponTypeClass::Find(weaponId.c_str());
					if (pWeapon && pReceiverHouse)
					{
						WeaponTypeExt::TypeData* weaponData = GetTypeData<WeaponTypeExt, WeaponTypeExt::TypeData>(pWeapon);
						if (CheckROF(pWeapon, weaponData))
						{
							AbstractClass* pTempTarget = pTarget;
							FireBulletToTarget callback = nullptr;
							if (needFakeTarget)
							{
								pTempTarget = MakeFakeTarget(pReceiverHouse, pShooter, data.FireFLH, data.TargetFLH);
								callback = SetupFakeTargetToBullet;
							}
							if (pTempTarget)
							{
								// 发射武器
								weaponLaunch = pAttachFire->FireCustomWeapon(pAttacker, pTempTarget, pAttackingHouse,
									pWeapon, *weaponData,
									data.FireFLH, !Data->IsOnTurret, Data->IsOnTarget,
									callback,
									AE->FromWarhead ? AE->WarheadLocation : CoordStruct::Empty);
								if (weaponLaunch)
									ResetROF(pWeapon, weaponData, rofMultip);
							}
						}
					}
				}
			}
		}
		if (weaponLaunch && Data->FireOnce)
		{
			// 武器已成功发射，销毁AE
			End(CoordStruct::Empty);
		}
	}
}

void AutoWeaponEffect::End(CoordStruct location)
{
	Deactivate();
	AE->TimeToDie();
}

void AutoWeaponEffect::OnRemove()
{
	End(CoordStruct::Empty);
}

void AutoWeaponEffect::OnReceiveDamageDestroy()
{
	End(CoordStruct::Empty);
}

void AutoWeaponEffect::OnDetonate(CoordStruct* pCoords, bool& skip)
{
	End(*pCoords);
}
