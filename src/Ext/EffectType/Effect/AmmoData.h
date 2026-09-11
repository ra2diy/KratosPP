#pragma once

#include <string>
#include <vector>
#include <map>

#include <GeneralStructures.h>

#include <Common/INI/INIConfig.h>

#include <Ext/EffectType/Effect/EffectData.h>
#include <Ext/Helper/MathEx.h>

enum class AmmoAction : int
{
	INIT = 0,
	ADD = 1,
	SUB = 2,
	MUL = 3,
};

template <>
inline bool Parser<AmmoAction>::TryParse(const char* pValue, AmmoAction* outValue)
{
	switch (toupper(static_cast<unsigned char>(*pValue)))
	{
	case 'I':
		if (outValue) { *outValue = AmmoAction::INIT; }
		return true;
	case 'A':
		if (outValue) { *outValue = AmmoAction::ADD; }
		return true;
	case 'S':
		if (outValue) { *outValue = AmmoAction::SUB; }
		return true;
	case 'M':
		if (outValue) { *outValue = AmmoAction::MUL; }
		return true;
	default:
		if (outValue) { *outValue = AmmoAction::ADD; }
		return true;
	}
}

enum class AmmoReaction : int
{
	NORMAL = 0,
	HITPOINT = 1,
	SHIELD = 2,
};

template <>
inline bool Parser<AmmoReaction>::TryParse(const char* pValue, AmmoReaction* outValue)
{
	switch (toupper(static_cast<unsigned char>(*pValue)))
	{
	case 'H':
		if (outValue) { *outValue = AmmoReaction::HITPOINT; }
		return true;
	case 'S':
		if (outValue) { *outValue = AmmoReaction::SHIELD; }
		return true;
	default:
		if (outValue) { *outValue = AmmoReaction::NORMAL; }
		return true;
	}
}

enum class AmmoType : int
{
	Number = 0,
	HP = 1,
	MaxHP = 2,
	Ammo = 3,
	MaxAmmo = 4,
};

class AmmoEntity
{
public:
	bool Enable = false;
	Point2D RemoveWhenNum = { 0, -1 };

	virtual void Read(INIBufferReader* reader, std::string title)
	{
		RemoveWhenNum = reader->Get(title + "RemoveWhenNum", RemoveWhenNum);
		Enable = RemoveWhenNum.Y >= 0 && RemoveWhenNum.Y >= RemoveWhenNum.X;
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->Enable)
			.Process(this->RemoveWhenNum)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange)
	{
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const
	{
		return const_cast<AmmoEntity*>(this)->Serialize(stream);
	}
#pragma endregion
};

class AmmoReactionEntity
{
public:
	int Limit = -1;

	double Protect = 1;
	double Percent = 1;

	std::vector<std::string> OnlyReactionWarheads{};
	std::vector<std::string> NotReactionWarheads{};

	virtual void Read(INIBufferReader* reader, std::string title)
	{
		Limit = reader->Get(title + "Limit", Limit);

		Protect = reader->GetPercent(title + "Protect", Protect);
		Protect = std::clamp(Protect, 0.0, 1.0);
		Percent = reader->GetPercent(title + "Percent", Percent);
		Percent = std::max(0.0, Percent);

		OnlyReactionWarheads = reader->GetList(title + "OnlyReactionWarheads", OnlyReactionWarheads);
		ClearIfGetNone(OnlyReactionWarheads);

		NotReactionWarheads = reader->GetList(title + "NotReactionWarheads", NotReactionWarheads);
		ClearIfGetNone(NotReactionWarheads);
	}

	bool WarheadOnMark(const char* warheadId)
	{
		bool hasWhiteList = !OnlyReactionWarheads.empty();
		bool hasBlackList = !NotReactionWarheads.empty();
		bool mark = !hasWhiteList;
		if (hasWhiteList)
		{
			for (const std::string id : OnlyReactionWarheads)
			{
				if (id == warheadId)
				{
					mark = true;
					break;
				}
			}
		}
		if ((!mark || !hasWhiteList) && hasBlackList)
		{
			for (const std::string id : NotReactionWarheads)
			{
				if (id == warheadId)
				{
					mark = false;
					break;
				}
			}
		}
		return mark;
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->Limit)
			.Process(this->Protect)
			.Process(this->Percent)
			.Process(this->OnlyReactionWarheads)
			.Process(this->NotReactionWarheads)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange)
	{
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const
	{
		return const_cast<AmmoReactionEntity*>(this)->Serialize(stream);
	}
#pragma endregion
};

class AmmoData : public EffectData
{
public:
	EFFECT_DATA(Ammo);

	double Num = 0;
	AmmoType NumType = AmmoType::Number;
	bool NumFromSource = false;

	double Min = 0;
	double Max = -1;
	AmmoAction Action = AmmoAction::ADD; // 附着时对当前弹药执行的动作，默认增加

	// 弹药归零自动移除列表，key：0 = 无序号（与 Ammo0 同槽，后写覆盖），i = Ammo<i> 序号。
	// 用 map 按键覆写（FeedbackAttach 同款机制）：INI 依赖链多文件重复 Read 同一配置时，
	// 同 key 后写覆盖先写，天然去重；不同 key 并存。
	std::map<int, AmmoEntity> RemoveWhenNums{};

	AmmoReaction ReactionMode = AmmoReaction::NORMAL;

	AmmoReactionEntity Reaction{};

	virtual void Read(INIBufferReader* reader) override
	{
		Read(reader, "Ammo.");
	}

	virtual void Read(INIBufferReader* reader, std::string title) override
	{
		EffectData::Read(reader, title);

		std::string numStr{ "" };
		numStr = reader->Get(title + "Num", numStr);
		if (IsNotNone(numStr))
		{
			if (std::regex_match(numStr, INIReader::Number))
			{
				int buffer = 0;
				const char* pFmt = "%d";
				if (sscanf_s(numStr.c_str(), pFmt, &buffer) == 1)
				{
					Num = buffer;
					NumType = AmmoType::Number;
				}
			}
			else
			{
				std::string upper = uppercase(numStr);
				if (upper.substr(0, 6) == "MAXAMM")
				{
					NumType = AmmoType::MaxAmmo;
				}
				else
				{
					const char v = upper[0];
					switch (v)
					{
					case 'H':
						NumType = AmmoType::HP;
						break;
					case 'M':
						NumType = AmmoType::MaxHP;
						break;
					case 'A':
						NumType = AmmoType::Ammo;
						break;
					}
				}
			}
		}
		NumFromSource = reader->Get(title + "NumFromSource", NumFromSource);

		Min = reader->Get(title + "Min", Min);
		Max = reader->Get(title + "Max", Max);
		Action = reader->Get(title + "Action", Action);

		// 读取无序号的，挂在 key 0
		AmmoEntity defaultEntity;
		defaultEntity.Read(reader, title);
		if (defaultEntity.Enable)
		{
			RemoveWhenNums[0] = defaultEntity;
		}
		// 读取有序号的，序号即 key
		for (int i = 0; i < 128; i++)
		{
			AmmoEntity entity{};
			entity.Read(reader, "Ammo" + std::to_string(i) + ".");
			if (entity.Enable)
			{
				RemoveWhenNums[i] = entity;
			}
		}

		ReactionMode = reader->Get(title + "Reaction", ReactionMode);
		ReactionMode = reader->Get(title + "ReactionMode", ReactionMode);
		Reaction.Read(reader, title + "Reaction.");

		// 有实际 Ammo 配置时才启用
		Enable = (NumType != AmmoType::Number) // 读取了特殊值
			|| (ReactionMode != AmmoReaction::NORMAL) // 配置了受击响应
			|| !RemoveWhenNums.empty(); // 配置了自动移除
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->Num)
			.Process(this->NumType)
			.Process(this->NumFromSource)
			.Process(this->Min)
			.Process(this->Max)
			.Process(this->Action)
			.Process(this->RemoveWhenNums)
			.Process(this->ReactionMode)
			.Process(this->Reaction)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		EffectData::Load(stream, registerForChange);
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		return const_cast<AmmoData*>(this)->Serialize(stream);
	}
#pragma endregion
};
