#pragma once

#include <string>
#include <vector>

#include <GeneralStructures.h>

#include <Common/INI/INIConfig.h>

#include <Ext/EffectType/Effect/EffectData.h>
#include <Ext/Helper/MathEx.h>


class AutoWeaponEntity
{
public:
	bool Enable = false;

	int WeaponIndex = -1; // 使用单位自身的武器
	std::vector<std::string> WeaponTypes{}; // 武器类型
	int RandomTypesNum = 0; // 随机使用几个武器
	std::vector<int> RandomWeights; // 随机从列表中选区类型，对应列表中每个类型的权重值
	CoordStruct FireFLH = CoordStruct::Empty; // 开火相对位置
	CoordStruct TargetFLH = CoordStruct::Empty; // 目标相对位置
	CoordStruct MoveTo = CoordStruct::Empty; // 以开火位置为坐标0点，计算TargetFLH

	virtual void Read(INIBufferReader* reader, std::string title)
	{
		WeaponIndex = reader->Get(title + "WeaponIndex", WeaponIndex);
		WeaponTypes = reader->GetList(title + "Types", WeaponTypes);
		ClearIfGetNone(WeaponTypes);

		RandomTypesNum = reader->Get(title + "RandomTypesNum", RandomTypesNum);
		RandomWeights = reader->GetList(title + "RandomWeights", RandomWeights);
		FireFLH = reader->Get(title + "FireFLH", FireFLH);
		TargetFLH = reader->Get(title + "TargetFLH", TargetFLH);
		MoveTo = reader->Get(title + "MoveTo", MoveTo);
		if (!MoveTo.IsEmpty())
		{
			TargetFLH = FireFLH + MoveTo;
		}

		Enable = WeaponIndex >= 0 || !WeaponTypes.empty();
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->Enable)
			.Process(this->WeaponIndex)
			.Process(this->WeaponTypes)
			.Process(this->RandomTypesNum)
			.Process(this->RandomWeights)
			.Process(this->FireFLH)
			.Process(this->TargetFLH)
			.Process(this->MoveTo)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange)
	{
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const
	{
		return const_cast<AutoWeaponEntity*>(this)->Serialize(stream);
	}
#pragma endregion
};

class AutoWeaponData : public EffectData
{
public:
	EFFECT_DATA(AutoWeapon);

	AutoWeaponEntity Data{};
	AutoWeaponEntity EliteData{};

	bool FireOnce = false; // 发射后销毁
	bool FireToTarget = false; // 朝附加对象的目标开火，如果附加的对象没有目标，不开火
	bool IsOnTarget = false; // 相对目标
	bool IsOnTurret = true; // 相对炮塔或者身体
	bool IsOnWorld = false; // 相对世界

	// 攻击者标记
	bool IsAttackerMark = false; // 允许附加对象和攻击者进行交互
	bool ReceiverAttack = true; // 武器由AE的接受者发射
	bool ReceiverOwnBullet = true; // 武器所属是AE的接受者

	bool AttackFromSpawnOwner = false; // 武器所属是母鸡

	bool LaunchFromProjectile = false; // 武器从弹头附着点发射（广播专用）


	virtual void Read(INIBufferReader* reader) override
	{
		Read(reader, "AutoWeapon.");
	}

	virtual void Read(INIBufferReader* reader, std::string title) override
	{
		EffectData::Read(reader, title);

		AutoWeaponEntity data;
		data.Read(reader, title);
		if (data.Enable)
		{
			Data = data;
			EliteData = Data;
		}

		AutoWeaponEntity eliteData;
		eliteData.Read(reader, title + "Elite");
		if (eliteData.Enable)
		{
			EliteData = eliteData;
		}

		FireOnce = reader->Get(title + "FireOnce", FireOnce);
		FireToTarget = reader->Get(title + "FireToTarget", FireToTarget);
		IsOnTarget = reader->Get(title + "IsOnTarget", IsOnTarget);
		IsOnTurret = reader->Get(title + "IsOnTurret", IsOnTurret);
		IsOnWorld = reader->Get(title + "IsOnWorld", IsOnWorld);

		// 攻击者标记
		IsAttackerMark = reader->Get(title + "IsAttackerMark", IsAttackerMark);
		ReceiverAttack = reader->Get(title + "ReceiverAttack", ReceiverAttack);
		if (!ReceiverAttack)
		{
			ReceiverOwnBullet = false;
		}
		ReceiverOwnBullet = reader->Get(title + "ReceiverOwnBullet", ReceiverOwnBullet);

		AttackFromSpawnOwner = reader->Get(title + "AttackFromSpawnOwner", AttackFromSpawnOwner);

		LaunchFromProjectile = reader->Get(title + "LaunchFromProjectile", LaunchFromProjectile);

		Enable = Data.Enable || EliteData.Enable;
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->Data)
			.Process(this->EliteData)
			.Process(this->FireOnce)
			.Process(this->FireToTarget)
			.Process(this->IsOnTarget)
			.Process(this->IsOnTurret)
			.Process(this->IsOnWorld)
			.Process(this->IsAttackerMark)
			.Process(this->ReceiverAttack)
			.Process(this->ReceiverOwnBullet)
			.Process(this->AttackFromSpawnOwner)
			.Process(this->LaunchFromProjectile)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		EffectData::Load(stream, registerForChange);
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		EffectData::Save(stream);
		return const_cast<AutoWeaponData*>(this)->Serialize(stream);
	}
#pragma endregion
};

