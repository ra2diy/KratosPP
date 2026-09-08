#pragma once
// ============================================================================
// AddByExistingData — AddByExisting 数据层（Copy 同源族）
//
// 与 Copy 共用同一套观察/分发机制，差异仅两处：
//   附加内容 = 本效果器自己的固定名单 AttachEffects（整串按名附加，Copy 附加观察命中的 AE）
//   无 Cut / 无 AttachTo=Return（不是搬移命中 AE，没有可剪之物与可返对象）
// 其余标签（From / Watch 名单组 / AttachTo 前三值 / AttachFrom / Discard /
// Delay / TriggeredTimes / 发放过滤）与 Copy 完全一致，默认值对齐。
// 前缀 AddByExisting.；机制解耦规则见 CopyData.h 头注释。
// ============================================================================

#include <string>
#include <vector>

#include <GeneralStructures.h>

#include <Common/INI/INIConfig.h>

#include <Ext/EffectType/Effect/EffectData.h>
#include <Ext/Helper/StringEx.h> // ClearIfGetNone

#include "CopyData.h" // 取值枚举 / CopyWatchConfig / CopyParseAttachTo

class AddByExistingData : public EffectData
{
public:
	EFFECT_DATA(AddByExisting);

	// ---- 观察与分发（前缀 AddByExisting.*，与 Copy 同构）----
	CopyFrom From = CopyFrom::Target;                       // INI: AddByExisting.From（观察谁身上的 AE）
	CopyWatchConfig Watch{};                                // INI: AddByExisting.Watch / WatchMarks / IgnoreTypes / IgnoreMarks（观察名单组）
	CopyAttachTo AttachTo = CopyAttachTo::Source;           // INI: AddByExisting.AttachTo（贴给谁，限 Source/Target/InitialSource，无 Return）
	CopyAttachFrom AttachFrom = CopyAttachFrom::InitialSource; // INI: AddByExisting.AttachFrom（来源记谁）
	bool DiscardOnInitialSourceDead = false;                // INI: AddByExisting.DiscardOnInitialSourceDead（名单单位死亡：丢弃该轮 / 走完整回退）
	std::vector<std::string> AttachEffects{};               // INI: AddByExisting.AttachEffects（按名附加名单，空=执行时无动作）
	int Delay = 0;                                          // INI: AddByExisting.Delay（执行间隔帧，<=0 按 1 帧）

	AddByExistingData() : EffectData()
	{
		// TriggeredTimes（基类键 AddByExisting.TriggeredTimes）= 总执行次数，默认 1（激活执行 1 次即耗尽）
		this->TriggeredTimes = 1;
	}

	virtual void Read(INIBufferReader* reader) override
	{
		Read(reader, "AddByExisting.");
	}

	virtual void Read(INIBufferReader* reader, std::string title) override
	{
		// 直接复用基类读取（AddByExisting.Enable / TriggeredTimes / AffectTypes 等发放过滤键）
		EffectData::Read(reader, title);

		From = reader->Get(title + "From", From);
		Watch.Read(reader, title);
		std::string raw;
		if (reader->TryGet(title + "AttachTo", raw))
		{
			CopyParseAttachTo(raw.c_str(), false, &this->AttachTo); // 限三值：Return 不接受
		}
		AttachFrom = reader->Get(title + "AttachFrom", AttachFrom);
		DiscardOnInitialSourceDead = reader->Get(title + "DiscardOnInitialSourceDead", DiscardOnInitialSourceDead);
		AttachEffects = reader->GetList(title + "AttachEffects", AttachEffects);
		ClearIfGetNone(AttachEffects);
		Delay = reader->Get(title + "Delay", Delay);
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->From)
			.Process(this->Watch.Watch)
			.Process(this->Watch.WatchMarks)
			.Process(this->Watch.IgnoreTypes)
			.Process(this->Watch.IgnoreMarks)
			.Process(this->AttachTo)
			.Process(this->AttachFrom)
			.Process(this->DiscardOnInitialSourceDead)
			.Process(this->AttachEffects)
			.Process(this->Delay)
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
		return const_cast<AddByExistingData*>(this)->Serialize(stream);
	}
#pragma endregion
};
