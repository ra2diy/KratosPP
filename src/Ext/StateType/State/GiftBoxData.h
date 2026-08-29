#pragma once
#include <string>
#include <vector>

#include <GeneralStructures.h>

#include <Ext/EffectType/Effect/EffectData.h>
#include <Ext/Helper/CastEx.h>

class GiftBoxEntity
{
public:
	bool Enable = false;

	bool Dynamic = false;
	bool DynamicFromSource = false;
	std::vector<std::string> Gifts;
	std::vector<int> Nums;
	std::vector<double> Chances;
	bool RandomType;
	std::vector<int> RandomWeights;

	int Delay;
	Point2D RandomDelay;

	virtual void Read(INIBufferReader* reader, std::string title)
	{
		Dynamic = reader->Get(title + "Dynamic", Dynamic);
		DynamicFromSource = reader->Get(title + "DynamicFromSource", DynamicFromSource);
		Gifts = reader->GetList(title + "Types", Gifts);
		Nums = reader->GetList(title + "Nums", Nums);
		Chances = reader->GetChanceList(title + "Chances", Chances);

		RandomType = reader->Get(title + "RandomType", RandomType);
		RandomWeights = reader->GetList(title + "RandomWeights", RandomWeights);

		Delay = reader->Get(title + "Delay", Delay);
		RandomDelay = reader->Get(title + "RandomDelay", RandomDelay);

		Enable = !Gifts.empty() || Dynamic || DynamicFromSource;
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->Enable)
			.Process(this->Dynamic)
			.Process(this->DynamicFromSource)
			.Process(this->Gifts)
			.Process(this->Nums)
			.Process(this->Chances)
			.Process(this->RandomType)
			.Process(this->RandomWeights)
			.Process(this->Delay)
			.Process(this->RandomDelay)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange)
	{
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const
	{
		return const_cast<GiftBoxEntity*>(this)->Serialize(stream);
	}
#pragma endregion
};

class GiftBoxData : public EffectData
{
public:
	EFFECT_DATA(GiftBox);

	bool ForceTransform = false;

	GiftBoxEntity Data{};
	GiftBoxEntity EliteData{};

	bool Remove = true;
	bool Destroy = false;
	bool RealCoords = false;
	int RandomRange = 0;
	bool EmptyCell = false;
	bool OpenWhenDestroyed = false;
	bool OpenWhenHealthPercent = false;
	double OpenHealthPercent = 0;

	std::vector<std::string> OnlyOpenWhenMarks{};

	bool IsTransform = false;
	bool InheritHealth = false;
	double HealthPercent = 1;
	int HealthNumber = 0;
	bool InheritHealthNumber = false;
	bool InheritTarget = true;
	bool InheritExperience = true;
	bool InheritPassenger = false;
	bool InheritROF = false;
	bool InheritAmmo = false;
	bool InheritAE = false;
	Mission ForceMission = Mission::None;

	std::vector<std::string> RemoveEffects{};
	std::vector<std::string> AttachEffects{};
	std::vector<double> AttachChances{};

	virtual void Read(INIBufferReader* reader, std::string title) override
	{
		EffectData::Read(reader, title);

		Data.Read(reader, title);
		EliteData = Data;
		EliteData.Read(reader, title);

		Enable = Data.Enable || EliteData.Enable;

		// 通用设置
		Remove = reader->Get(title + "Remove", Remove);
		Destroy = reader->Get(title + "Explodes", Destroy);
		RealCoords = reader->Get(title + "RealCoords", RealCoords);
		RandomRange = reader->Get(title + "RandomRange", RandomRange);
		EmptyCell = reader->Get(title + "RandomToEmptyCell", EmptyCell);

		OpenWhenDestroyed = reader->Get(title + "OpenWhenDestroyed", OpenWhenDestroyed);
		OpenHealthPercent = reader->Get(title + "OpenWhenHealthPercent", OpenHealthPercent);
		OpenWhenHealthPercent = OpenHealthPercent > 0 && OpenHealthPercent < 1;

		OnlyOpenWhenMarks = reader->GetList(title + "OnlyOpenWhenMarks", OnlyOpenWhenMarks);

		IsTransform = reader->Get(title + "IsTransform", IsTransform);
		if (IsTransform || ForceTransform)
		{
			ForTransform();
		}

		HealthPercent = reader->Get(title + "HealthPercent", HealthPercent);
		if (HealthPercent <= 0)
		{
			// 设置了0，自动，当IsTransform时，按照礼盒的比例
			HealthPercent = 1;
			InheritHealth = true;
		}
		else
		{
			// 固定比例
			HealthPercent = HealthPercent > 1 ? 1 : HealthPercent;
			InheritHealth = false;
		}

		HealthNumber = reader->Get(title + "HealthNumber", HealthNumber);
		{
			if (HealthNumber > 0)
			{
				// 设置血量数字
				InheritHealth = true;
			}
		}

		InheritHealthNumber = reader->Get(title + "InheritHealthNumber", InheritHealthNumber);
		if (InheritHealthNumber)
		{
			// 继承血量数字
			InheritHealth = true;
		}

		InheritTarget = reader->Get(title + "InheritTarget", InheritTarget);
		InheritExperience = reader->Get(title + "InheritExp", InheritExperience);
		InheritPassenger = reader->Get(title + "InheritPassenger", InheritPassenger);
		InheritROF = reader->Get(title + "InheritROF", InheritROF);
		InheritAmmo = reader->Get(title + "InheritAmmo", InheritAmmo);
		InheritAE = reader->Get(title + "InheritAE", InheritAE);
		ForceMission = reader->Get(title + "ForceMission", ForceMission);

		RemoveEffects = reader->GetList(title + "RemoveEffects", RemoveEffects);
		AttachEffects = reader->GetList(title + "AttachEffects", AttachEffects);
		AttachChances = reader->GetChanceList(title + "AttachChances", AttachChances);
	}

	virtual void Read(INIBufferReader* reader) override
	{
		Read(reader, TITLE);
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->ForceTransform)

			.Process(this->Data)
			.Process(this->EliteData)

			.Process(this->Remove)
			.Process(this->Destroy)

			.Process(this->RealCoords)
			.Process(this->RandomRange)
			.Process(this->EmptyCell)

			.Process(this->OpenWhenDestroyed)
			.Process(this->OpenWhenHealthPercent)
			.Process(this->OpenHealthPercent)
			.Process(this->OnlyOpenWhenMarks)

			.Process(this->IsTransform)
			.Process(this->InheritHealth)
			.Process(this->HealthPercent)
			.Process(this->HealthNumber)
			.Process(this->InheritHealthNumber)
			.Process(this->InheritTarget)
			.Process(this->InheritExperience)
			.Process(this->InheritPassenger)
			.Process(this->InheritROF)
			.Process(this->InheritAmmo)
			.Process(this->InheritAE)
			.Process(this->ForceMission)

			.Process(this->RemoveEffects)
			.Process(this->AttachEffects)
			.Process(this->AttachChances)
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
		return const_cast<GiftBoxData*>(this)->Serialize(stream);
	}
#pragma endregion
private:
	void ForTransform()
	{
		Remove = true; // 释放后移除
		Destroy = false; // 静默删除
		OpenWhenDestroyed = false;
		OpenWhenHealthPercent = false;
		IsTransform = true;
		InheritHealth = true; // 继承血量
		HealthPercent = 0;
		InheritTarget = true; // 继承目标
		InheritExperience = true; // 继承经验
		InheritPassenger = true; // 继承乘客
		InheritROF = true; // 继承ROF计时器
		InheritAmmo = true; // 继承弹药
		InheritAE = true; // 继承AE
	}

	inline static std::string TITLE = "GiftBox.";
};
