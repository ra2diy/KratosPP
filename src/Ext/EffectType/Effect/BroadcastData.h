#pragma once

#include <string>
#include <vector>

#include <GeneralStructures.h>

#include <Common/INI/INIConfig.h>

#include <Ext/Helper/MathEx.h>

#include "EffectData.h"

class BroadcastEntity
{
public:
	bool Enable = false;

	std::vector<std::string> Types{};
	std::vector<double> AttachChances{};
	std::vector<std::string> GetTypes{};
	std::vector<double> GetChances{};
	int Rate = 15;
	float RangeMin = 0;
	float RangeMax = -1;
	bool FullAirspace = false;

	virtual void Read(INIBufferReader* reader, std::string title)
	{
		Types = reader->GetList(title + "Types", Types);
		AttachChances = reader->GetChanceList(title + "AttachChances", AttachChances);
		GetTypes = reader->GetList(title + "GetTypes", GetTypes);
		GetChances = reader->GetChanceList(title + "GetChances", GetChances);
		Rate = reader->Get(title + "Rate", Rate);
		RangeMin = reader->Get(title + "RangeMin", RangeMin);
		RangeMax = reader->Get(title + "RangeMax", RangeMax);
		FullAirspace = reader->Get(title + "FullAirspace", FullAirspace);

		// 0 时关闭，-1全地图
		Enable = (!Types.empty() || !GetTypes.empty()) && RangeMax != 0;
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->Enable)
			.Process(this->Types)
			.Process(this->AttachChances)
			.Process(this->GetTypes)
			.Process(this->GetChances)
			.Process(this->Rate)
			.Process(this->RangeMin)
			.Process(this->RangeMax)
			.Process(this->FullAirspace)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange)
	{
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const
	{
		return const_cast<BroadcastEntity*>(this)->Serialize(stream);
	}
#pragma endregion
};

class BroadcastData : public EffectData
{
public:
	EFFECT_DATA(Broadcast);

	BroadcastEntity Data{};
	BroadcastEntity EliteData{};

	int MaxAttachTechno = -1;
	int MaxAttachBullet = -1;

	BroadcastData() : EffectData()
	{
		AffectBullet = false;

		AffectSelf = false;
		AffectStand = false;

		AffectsOwner = true;
		AffectsAllies = false;
		AffectsEnemies = false;
		AffectsCivilian = false;
	}

	virtual void Read(INIBufferReader* reader) override
	{
		Read(reader, "Broadcast.");
	}

	virtual void Read(INIBufferReader* reader, std::string title) override
	{
		EffectData::Read(reader, title);

		BroadcastEntity data;
		data.Read(reader, title);
		if (data.Enable)
		{
			Data = data;
			EliteData = Data;
		}

		BroadcastEntity eliteData;
		eliteData.Read(reader, title + "Elite");
		if (eliteData.Enable)
		{
			EliteData = eliteData;
		}

		MaxAttachTechno = reader->Get(title + "MaxAttachTechno", MaxAttachTechno);
		MaxAttachBullet = reader->Get(title + "MaxAttachBullet", MaxAttachBullet);

		Enable = Data.Enable || EliteData.Enable;
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->Data)
			.Process(this->EliteData)
			.Process(this->MaxAttachTechno)
			.Process(this->MaxAttachBullet)
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
		return const_cast<BroadcastData*>(this)->Serialize(stream);
	}
#pragma endregion
};
