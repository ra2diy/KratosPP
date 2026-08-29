#pragma once

#include <string>
#include <vector>

#include <GeneralStructures.h>

#include <Common/INI/INIConfig.h>

#include <Ext/EffectType/Effect/EffectData.h>
#include <Ext/Helper/MathEx.h>

enum class Condition : int
{
	EQ = 0,
	NE = 1,
	GT = 2,
	LT = 3,
	GE = 4,
	LE = 5,
};

static std::map<std::string, Condition> ConditionTypeStrings
{
	{ "eq", Condition::EQ },
	{ "ne", Condition::NE },
	{ "gt", Condition::GT },
	{ "lt", Condition::LT },
	{ "ge", Condition::GE },
	{ "le", Condition::LE }
};

template <>
inline bool Parser<Condition>::TryParse(const char* pValue, Condition* outValue)
{
	std::string key = lowercase(std::string(pValue));
	auto it = ConditionTypeStrings.find(key);
	if (it != ConditionTypeStrings.end())
	{
		*outValue = it->second;
		return true;
	}
	return false;
}

enum class StackActionMode : int
{
	OR = 0, AND = 1,
};

template <>
inline bool Parser<StackActionMode>::TryParse(const char* pValue, StackActionMode* outValue)
{
	switch (toupper(static_cast<unsigned char>(*pValue)))
	{
	case '1':
	case 'T':
	case 'Y':
	case 'A':
		if (outValue)
		{
			*outValue = StackActionMode::AND;
		}
		return true;
	default:
		if (outValue)
		{
			*outValue = StackActionMode::OR;
		}
		return true;
	}
}

class StackData : public EffectData
{
public:
	EFFECT_DATA(Stack);

	std::vector<std::string> Watch{};

	Mission WatchMission = Mission::None;

	std::vector<int> Level{};
	std::vector<Condition> Condition{};

	StackActionMode ActionMode = StackActionMode::OR;

	bool Attach = false;
	std::vector<std::string> AttachEffects{};
	std::vector<double> AttachChances{};
	bool AttachToSource = false;

	std::vector<std::string> SourceGetEffects{};
	std::vector<double> SourceGetEffectChances{};

	bool Remove = false;
	std::vector<std::string> RemoveEffects{};
	std::vector<int> RemoveEffectsLevel{};
	std::vector<std::string> RemoveEffectsWithMarks{};
	bool RemoveEffectsSkipNext = false;
	bool RemoveToSource = false;

	std::vector<std::string> SourceRemoveEffects{};

	std::vector<int> RemoveLevel{};
	bool RemoveAll = true;
	bool RemoveSkipNext = false;

	virtual void Read(INIBufferReader* reader) override
	{
		Read(reader, "Stack.");
	}

	virtual void Read(INIBufferReader* reader, std::string title) override
	{
		EffectData::Read(reader, title);

		Watch = reader->GetList(title + "Watch", Watch);
		WatchMission = reader->Get(title + "WatchMission", WatchMission);

		Level = reader->GetList(title + "Level", Level);
		Condition = reader->GetList(title + "Condition", Condition);

		ActionMode = reader->Get(title + "ActionMode", ActionMode);

		AttachEffects = reader->GetList(title + "AttachEffects", AttachEffects);
		ClearIfGetNone(AttachEffects);
		AttachChances = reader->GetChanceList(title + "AttachChances", AttachChances);
		Attach = !AttachEffects.empty();
		AttachToSource = reader->Get(title + "AttachToSource", AttachToSource);

		SourceGetEffects = reader->GetList(title + "SourceGetEffects", SourceGetEffects);
		ClearIfGetNone(SourceGetEffects);
		SourceGetEffectChances = reader->GetChanceList(title + "SourceGetEffectChances", SourceGetEffectChances);

		RemoveEffects = reader->GetList(title + "RemoveEffects", RemoveEffects);
		ClearIfGetNone(RemoveEffects);
		RemoveEffectsLevel = reader->GetList(title + "RemoveEffectsLevel", RemoveEffectsLevel);
		RemoveEffectsWithMarks = reader->GetList(title + "RemoveEffectsWithMarks", RemoveEffectsWithMarks);
		ClearIfGetNone(RemoveEffectsWithMarks);
		Remove = !RemoveEffects.empty() || !RemoveEffectsWithMarks.empty();
		RemoveEffectsSkipNext = reader->Get(title + "RemoveEffectsSkipNext", RemoveEffectsSkipNext);
		RemoveToSource = reader->Get(title + "RemoveToSource", RemoveToSource);

		SourceRemoveEffects = reader->GetList(title + "SourceRemoveEffects", SourceRemoveEffects);
		ClearIfGetNone(SourceRemoveEffects);

		RemoveLevel = reader->GetList(title + "RemoveLevel", RemoveLevel);
		RemoveAll = reader->Get(title + "RemoveAll", RemoveAll);
		RemoveSkipNext = reader->Get(title + "RemoveSkipNext", RemoveSkipNext);

		Enable = (!Watch.empty() || WatchMission != Mission::None) && (Attach || Remove || !SourceGetEffects.empty() || !SourceRemoveEffects.empty());
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->Watch)
			.Process(this->WatchMission)
			.Process(this->Level)
			.Process(this->Condition)
			.Process(this->ActionMode)

			.Process(this->Attach)
			.Process(this->AttachEffects)
			.Process(this->AttachChances)
			.Process(this->AttachToSource)
			.Process(this->SourceGetEffects)
			.Process(this->SourceGetEffectChances)

			.Process(this->Remove)
			.Process(this->RemoveEffects)
			.Process(this->RemoveEffectsLevel)
			.Process(this->RemoveEffectsWithMarks)
			.Process(this->RemoveEffectsSkipNext)
			.Process(this->RemoveToSource)
			.Process(this->SourceRemoveEffects)

			.Process(this->RemoveLevel)
			.Process(this->RemoveAll)
			.Process(this->RemoveSkipNext)
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
		return const_cast<StackData*>(this)->Serialize(stream);
	}
#pragma endregion
};
