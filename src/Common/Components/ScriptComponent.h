#pragma once

#include <YRPP.h>
#include <GeneralDefinitions.h>
#include <SpecificStructures.h>

#include "Component.h"
#include "GameObject.h"
#include "Scriptable.h"

#include <Utilities/Container.h>

#include <Extension/AnimExt.h>
#include <Extension/BulletExt.h>
#include <Extension/EBoltExt.h>
#include <Extension/TechnoExt.h>
#include <Extension/SuperWeaponExt.h>

#include <Ext/Helper/Status.h>

#include <Ext/BulletType/Trajectory/TrajectoryData.h>

/// @brief 所有的脚本都位于GameObject下
class ScriptComponent : public Component
{
public:
	ScriptComponent() : Component() {}

	virtual void Clean() override
	{
		Component::Clean();
	}

	virtual GameObject* GetGameObject() = 0;
	__declspec(property(get = GetGameObject)) GameObject* _gameObject;
};

#define SCRIPT_COMPONENT(SCRIPT_TYPE, TBASE, TEXT, P_NAME) \
	SCRIPT_TYPE() : ScriptComponent() {} \
	\
	virtual GameObject* GetGameObject() override \
	{ \
	return ((TEXT::ExtData*)_extData)->_GameObject; \
	}\
	\
	TBASE* GetOwner() \
	{ \
		return ((TEXT::ExtData*)_extData)->OwnerObject(); \
	} \
	__declspec(property(get = GetOwner)) TBASE* P_NAME; \

class ObjectScript : public ScriptComponent, public ITechnoScript, public IBulletScript
{
public:
	enum class OwnerType { None, Techno, Bullet };

	ObjectScript() : ScriptComponent() {}

	OwnerType DetectOwnerType()
	{
		if (_ownerType == OwnerType::None && _extData)
		{
			if (dynamic_cast<TechnoExt::ExtData*>(_extData))
				_ownerType = OwnerType::Techno;
			else if (dynamic_cast<BulletExt::ExtData*>(_extData))
				_ownerType = OwnerType::Bullet;
		}
		return _ownerType;
	}

	virtual void Awake() override
	{
		DetectOwnerType();
	}

	virtual GameObject* GetGameObject() override
	{
		switch (DetectOwnerType())
		{
		case OwnerType::Techno:
			return static_cast<TechnoExt::ExtData*>(_extData)->_GameObject;
		case OwnerType::Bullet:
			return static_cast<BulletExt::ExtData*>(_extData)->_GameObject;
		}
		return nullptr;
	}

	TechnoClass* GetTechno()
	{
		if (DetectOwnerType() == OwnerType::Techno)
		{
			return static_cast<TechnoExt::ExtData*>(_extData)->OwnerObject();
		}
		return nullptr;
	}

	__declspec(property(get = GetTechno)) TechnoClass* pTechno;

	BulletClass* GetBullet()
	{
		if (DetectOwnerType() == OwnerType::Bullet)
		{
			return static_cast<BulletExt::ExtData*>(_extData)->OwnerObject();
		}
		return nullptr;
	}

	__declspec(property(get = GetBullet)) BulletClass* pBullet;

	ObjectClass* GetOwner()
	{
		ObjectClass* pObject = pTechno;
		if (!pTechno)
		{
			pObject = pBullet;
		}
		if (!pObject)
		{
			Debug::Log("Warning: ObjectScript \"%s\" got a unknown ExtData!\n", Name.c_str());
		}
		return pObject;
	}

	__declspec(property(get = GetOwner)) ObjectClass* pObject;

	AbstractType GetAbsType()
	{
		if (_absType == AbstractType::None)
		{
			_absType = pObject->WhatAmI();
		}
		return _absType;
	}

	bool IsBullet()
	{
		return pBullet != nullptr;
	}

	bool IsBuilding()
	{
		return pTechno && GetAbsType() == AbstractType::Building;
	}

	bool IsInfantry()
	{
		return pTechno && GetAbsType() == AbstractType::Infantry;
	}

	bool IsUnit()
	{
		return pTechno && GetAbsType() == AbstractType::Unit;
	}

	bool IsAircraft()
	{
		return pTechno && GetAbsType() == AbstractType::Aircraft;
	}

	bool IsFoot()
	{
		return !IsBullet() && !IsBuilding();
	}

	bool InBuilding()
	{
		if (IsBuilding())
		{
			BuildingClass* pBuilding = abstract_cast<BuildingClass*, true>(pTechno);
			return pBuilding->BState == BStateType::Construction && pBuilding->CurrentMission != Mission::Selling;
		}
		return false;
	}

	bool InSelling()
	{
		if (IsBuilding())
		{
			BuildingClass* pBuilding = abstract_cast<BuildingClass*, true>(pTechno);
			return pBuilding->BState == BStateType::Construction && pBuilding->CurrentMission == Mission::Selling && pBuilding->MissionStatus > 0;
		}
		return false;
	}

	virtual void Clean() override
	{
		ScriptComponent::Clean();

		_ownerType = OwnerType::None;
		_absType = AbstractType::None;
	}
protected:
	OwnerType _ownerType = OwnerType::None;
	AbstractType _absType = AbstractType::None;
};

class TechnoScript : public ScriptComponent, public ITechnoScript
{
public:
	SCRIPT_COMPONENT(TechnoScript, TechnoClass, TechnoExt, pTechno);

	AbstractType GetAbsType()
	{
		if (_absType == AbstractType::None)
		{
			_absType = pTechno->WhatAmI();
		}
		return _absType;
	}

	bool IsBuilding()
	{
		return GetAbsType() == AbstractType::Building;
	}
	bool IsInfantry()
	{
		return GetAbsType() == AbstractType::Infantry;
	}
	bool IsUnit()
	{
		return GetAbsType() == AbstractType::Unit;
	}
	bool IsAircraft()
	{
		return GetAbsType() == AbstractType::Aircraft;
	}

	LocoType GetThisLocoType()
	{
		if (!IsBuilding())
		{
			if (_locoType == LocoType::None)
			{
				_locoType = GetLocoType(pTechno);
			}
		}
		return _locoType;
	}

	bool IsFly()
	{
		return GetThisLocoType() == LocoType::Fly;
	}
	bool IsJumpjet()
	{
		return GetThisLocoType() == LocoType::Jumpjet;
	}
	bool IsShip()
	{
		return GetThisLocoType() == LocoType::Ship;
	}

	bool IsRocket()
	{
		return IsAircraft() && GetThisLocoType() == LocoType::Rocket;
	}

	virtual void Clean() override
	{
		ScriptComponent::Clean();

		_absType = AbstractType::None;
		_locoType = LocoType::None;
	}

protected:
	// 单位类型
	AbstractType _absType = AbstractType::None;
	LocoType _locoType = LocoType::None;
};

class BulletScript : public ScriptComponent, public IBulletScript
{
public:
	SCRIPT_COMPONENT(BulletScript, BulletClass, BulletExt, pBullet);

	BulletType GetBulletType()
	{
		if (_bulletType == BulletType::UNKNOWN)
		{
			_bulletType = WhatAmI(pBullet);
			if (_bulletType != BulletType::ROCKET && trajectoryData->IsStraight())
			{
				_bulletType = BulletType::ROCKET;
			}
		}
		return _bulletType;
	}

	bool IsArcing()
	{
		return GetBulletType() == BulletType::ARCING;
	}
	bool IsMissile()
	{
		return GetBulletType() == BulletType::MISSILE;
	}
	bool IsRocket()
	{
		return GetBulletType() == BulletType::ROCKET;
	}
	bool IsBomb()
	{
		return GetBulletType() == BulletType::BOMB;
	}

	virtual void Clean() override
	{
		ScriptComponent::Clean();

		_bulletType = BulletType::UNKNOWN;
		_trajectoryData = nullptr;
	}

protected:
	// 抛射体类型
	BulletType _bulletType = BulletType::UNKNOWN;
	// 弹道配置
	TrajectoryData* _trajectoryData = nullptr;
	TrajectoryData* GetTrajectoryData()
	{
		if (!_trajectoryData)
		{
			_trajectoryData = INI::GetConfig<TrajectoryData>(INI::Rules, pBullet->GetType()->ID)->Data;
		}
		return _trajectoryData;
	}
	__declspec(property(get = GetTrajectoryData)) TrajectoryData* trajectoryData;
};

class AnimScript : public ScriptComponent, public IAnimScript
{
public:
	SCRIPT_COMPONENT(AnimScript, AnimClass, AnimExt, pAnim);

	virtual void Clean() override { ScriptComponent::Clean(); }
};

class SuperWeaponScript : public ScriptComponent, public ISuperScript
{
public:
	SCRIPT_COMPONENT(SuperWeaponScript, SuperClass, SuperWeaponExt, pSuper);

	virtual void Clean() override { ScriptComponent::Clean(); }
};

class EBoltScript : public ScriptComponent
{
public:
	SCRIPT_COMPONENT(EBoltScript, EBolt, EBoltExt, pBolt);

	virtual void Clean() override { ScriptComponent::Clean(); }
};

#define OBJECT_SCRIPT(CLASS_NAME) \
	DECLARE_COMPONENT(CLASS_NAME, ObjectScript) \

#define TECHNO_SCRIPT(CLASS_NAME) \
	DECLARE_COMPONENT(CLASS_NAME, TechnoScript) \

#define BULLET_SCRIPT(CLASS_NAME) \
	DECLARE_COMPONENT(CLASS_NAME, BulletScript) \

#define ANIM_SCRIPT(CLASS_NAME) \
	DECLARE_COMPONENT(CLASS_NAME, AnimScript) \

#define SUPER_SCRIPT(CLASS_NAME) \
	DECLARE_COMPONENT(CLASS_NAME, SuperWeaponScript) \

#define EBOLT_SCRIPT(CLASS_NAME) \
	DECLARE_COMPONENT(CLASS_NAME, EBoltScript) \
