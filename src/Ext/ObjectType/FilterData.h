#pragma once

#include <algorithm>
#include <string>
#include <vector>
#include <set>

#include <GeneralStructures.h>
#include <HouseClass.h>
#include <ObjectClass.h>
#include <TechnoClass.h>
#include <BulletClass.h>

#include <Common/INI/INIConfig.h>

#include <Ext/Helper/CastEx.h>
#include <Ext/Helper/MathEx.h>
#include <Ext/Helper/Status.h>
#include <Ext/Helper/StringEx.h>

class FilterData : public INIConfig
{
public:
	std::vector<std::string> AffectTypes{};
	std::vector<std::string> NotAffectTypes{};

	bool AffectTechno = true;
	bool AffectBuilding = true;
	bool AffectInfantry = true;
	bool AffectUnit = true;
	bool AffectAircraft = true;

	bool AffectBullet = true;
	bool AffectMissile = true;
	bool AffectTorpedo = true;
	bool AffectCannon = true;
	bool AffectBomb = true;

	bool AffectStand = false;
	bool AffectSelf = false;
	bool AffectInAir = true;
	std::vector<std::string> NotAffectMarks{};
	std::vector<std::string> OnlyAffectMarks{};

	bool AffectsOwner = true;
	bool AffectsAllies = true;
	bool AffectsEnemies = true;
	bool AffectsCivilian = true;

	bool CheckBuildings = false;
	std::vector<std::string> AuxBuildings{};
	std::vector<std::string> NegBuildings{};


	virtual void Read(INIBufferReader* reader) override
	{
		Read(reader, "");
	}

	virtual void Read(INIBufferReader* reader, std::string title)
	{
		AffectTypes = reader->GetList<std::string>(title + "AffectTypes", AffectTypes);
		ClearIfGetNone(AffectTypes);
		NotAffectTypes = reader->GetList<std::string>(title + "NotAffectTypes", NotAffectTypes);
		ClearIfGetNone(NotAffectTypes);

		AffectTechno = reader->Get(title + "AffectTechno", AffectTechno);
		AffectBuilding = reader->Get(title + "AffectBuilding", AffectBuilding);
		AffectInfantry = reader->Get(title + "AffectInfantry", AffectInfantry);
		AffectUnit = reader->Get(title + "AffectUnit", AffectUnit);
		AffectAircraft = reader->Get(title + "AffectAircraft", AffectAircraft);
		if (!AffectBuilding && !AffectInfantry && !AffectUnit && !AffectAircraft)
		{
			AffectTechno = false;
		}

		AffectBullet = reader->Get(title + "AffectBullet", AffectBullet);
		AffectMissile = reader->Get(title + "AffectMissile", AffectMissile);
		AffectTorpedo = reader->Get(title + "AffectTorpedo", AffectTorpedo);
		AffectCannon = reader->Get(title + "AffectCannon", AffectCannon);
		AffectBomb = reader->Get(title + "AffectBomb", AffectBomb);
		if (!AffectMissile && !AffectCannon && !AffectBomb)
		{
			AffectBullet = false;
		}

		AffectStand = reader->Get(title + "AffectStand", AffectStand);
		AffectSelf = reader->Get(title + "AffectSelf", AffectSelf);
		AffectInAir = reader->Get(title + "AffectInAir", AffectInAir);

		NotAffectMarks = reader->GetList(title + "NotAffectMarks", NotAffectMarks);
		ClearIfGetNone(NotAffectMarks);
		OnlyAffectMarks = reader->GetList(title + "OnlyAffectMarks", OnlyAffectMarks);
		ClearIfGetNone(OnlyAffectMarks);

		bool affectsAllies = true;
		if (reader->TryGet(title + "AffectsAllies", affectsAllies))
		{
			AffectsAllies = affectsAllies;
			AffectsOwner = affectsAllies;
		}

		bool affectsOwner = true;
		if (reader->TryGet(title + "AffectsOwner", affectsOwner))
		{
			AffectsOwner = affectsOwner;
		}
		AffectsEnemies = reader->Get(title + "AffectsEnemies", AffectsEnemies);
		AffectsCivilian = reader->Get(title + "AffectsCivilian", AffectsCivilian);

		AuxBuildings = reader->GetList(title + "AuxBuildings", AuxBuildings);
		ClearIfGetNone(AuxBuildings);
		NegBuildings = reader->GetList(title + "NegBuildings", NegBuildings);
		ClearIfGetNone(NegBuildings);
		CheckBuildings = !AuxBuildings.empty() || !NegBuildings.empty();
	}

	bool CanAffectHouse(HouseClass* pHouse, HouseClass* pTargetHouse)
	{
		return !pHouse || !pTargetHouse || (pTargetHouse == pHouse ? AffectsOwner : (IsCivilian(pTargetHouse) ? AffectsCivilian : pTargetHouse->IsAlliedWith(pHouse) ? AffectsAllies : AffectsEnemies));
	}

	bool CanAffectType(const char* ID) const
	{
		if (!NotAffectTypes.empty() && std::find(NotAffectTypes.begin(), NotAffectTypes.end(), ID) != NotAffectTypes.end())
		{
			return false;
		}
		bool can = AffectTypes.empty();
		if (!can)
		{
			can = std::find(AffectTypes.begin(), AffectTypes.end(), ID) != AffectTypes.end();
		}
		return can;
	}

	bool CanAffectType(AbstractType absType) const
	{
		switch (absType)
		{
		case AbstractType::Building:
			return AffectBuilding;
		case AbstractType::Infantry:
			return AffectInfantry;
		case AbstractType::Unit:
			return AffectUnit;
		case AbstractType::Aircraft:
			return AffectAircraft;
		}
		return false;
	}

	bool CanAffectType(BulletType bulletType, bool isLevel) const
	{
		switch (bulletType)
		{
		case BulletType::INVISO:
			return true;
		case BulletType::ARCING:
			return AffectCannon;
		case BulletType::BOMB:
			return AffectBomb;
		case BulletType::ROCKET:
		case BulletType::MISSILE:
			// 导弹和直线导弹都算Missile
			if (isLevel)
			{
				return AffectTorpedo;
			}
			return AffectMissile;
		}
		return false;
	}

	bool CanAffectType(BulletClass* pBullet) const
	{
		return CanAffectType(pBullet->Type->ID) && CanAffectType(WhatAmI(pBullet), pBullet->Type->Level);
	}

	bool CanAffectType(TechnoClass* pTechno) const
	{
		return CanAffectType(pTechno->GetTechnoType()->ID) && CanAffectType(pTechno->WhatAmI());
	}

	bool CanAffectType(ObjectClass* pObject) const
	{
		TechnoClass* pTechno = nullptr;
		BulletClass* pBullet = nullptr;
		if (CastToTechno(pObject, pTechno))
		{
			return CanAffectType(pTechno);
		}
		else if (CastToBullet(pObject, pBullet))
		{
			return CanAffectType(pBullet);
		}
		return false;
	}

	bool HasMarks() const
	{
		return !OnlyAffectMarks.empty() || !NotAffectMarks.empty();
	}

	bool OnMark(std::vector<std::string> marks) const
	{
		bool hasWhiteList = !OnlyAffectMarks.empty();
		bool hasBlackList = !NotAffectMarks.empty();
		return (!hasWhiteList || (!marks.empty() && CheckOnMarks(OnlyAffectMarks, marks)))
				&& (!hasBlackList || (marks.empty() || !CheckOnMarks(NotAffectMarks, marks)));
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->AffectTypes)
			.Process(this->NotAffectTypes)

			.Process(this->AffectTechno)
			.Process(this->AffectBuilding)
			.Process(this->AffectInfantry)
			.Process(this->AffectUnit)
			.Process(this->AffectAircraft)

			.Process(this->AffectBullet)
			.Process(this->AffectMissile)
			.Process(this->AffectTorpedo)
			.Process(this->AffectCannon)
			.Process(this->AffectBomb)

			.Process(this->AffectStand)
			.Process(this->AffectSelf)
			.Process(this->AffectInAir)
			.Process(this->NotAffectMarks)
			.Process(this->OnlyAffectMarks)

			.Process(this->AffectsOwner)
			.Process(this->AffectsAllies)
			.Process(this->AffectsEnemies)
			.Process(this->AffectsCivilian)

			.Process(this->AuxBuildings)
			.Process(this->NegBuildings)
			.Process(this->CheckBuildings)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		INIConfig::Load(stream, registerForChange);
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		INIConfig::Save(stream);
		return const_cast<FilterData*>(this)->Serialize(stream);
	}
#pragma endregion
};
