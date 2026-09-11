#pragma once
// ============================================================================
// TraceData — Trace 数据层（Copy 同源族）
//
// 与 Copy 共用同一套观察/分发机制，差异仅两处：
//   附加内容 = 本效果器自己的固定名单 AttachEffects（整串按名附加，Copy 附加观察命中的 AE）
//   无 Cut / 无 AttachTo=Return（不是搬移命中 AE，没有可剪之物与可返对象）
// 其余标签（From / Watch 名单组 / AttachTo 前三值 / AttachFrom / Discard /
// Delay / TriggeredTimes / 发放过滤）与 Copy 完全一致，默认值对齐。
// 前缀 Trace.；机制解耦规则见 CopyData.h 头注释。
// ============================================================================

#include <string>
#include <vector>

#include <GeneralStructures.h>

#include <Common/INI/INIConfig.h>

#include <Ext/EffectType/Effect/EffectData.h>
#include <Ext/Helper/StringEx.h> // ClearIfGetNone

#include "CopyData.h" // 取值枚举 / CopyWatchConfig / CopyParseAttachTo

class TraceData : public EffectData
{
public:
	EFFECT_DATA(Trace);

	// ---- 观察与分发（前缀 Trace.*，与 Copy 同构）----
	CopyFrom From = CopyFrom::Target;                       // INI: Trace.From（观察谁身上的 AE）
	CopyWatchConfig Watch{};                                // INI: Trace.Watch / WatchMarks / IgnoreTypes / IgnoreMarks（观察名单组）
	CopyAttachTo AttachTo = CopyAttachTo::Source;           // INI: Trace.AttachTo（贴给谁，限 Source/Target/InitialSource，无 Return）
	CopyAttachFrom AttachFrom = CopyAttachFrom::InitialSource; // INI: Trace.AttachFrom（来源记谁）
	bool DiscardOnInitialSourceDead = false;                // INI: Trace.DiscardOnInitialSourceDead（名单单位死亡：丢弃该轮 / 走完整回退）
	std::vector<std::string> AttachEffects{};               // INI: Trace.AttachEffects（按名附加名单，空=执行时无动作）
	int Delay = 0;                                          // INI: Trace.Delay（执行间隔帧，<=0 按 1 帧）

	// ---- 移除（可独立使用，不要求 AttachEffects；执行在附加前）----
	bool RemoveEffectsSkipNext = false;                     // INI: Trace.RemoveEffectsSkipNext（移除时是否跳过 Next 链，对齐 AmmoTrigger 先例）
	std::vector<std::string> RemoveEffectsTarget{};         // INI: Trace.RemoveEffectsTarget（从宿主身上按名移除）
	std::vector<std::string> RemoveEffectsTargetMarks{};    // INI: Trace.RemoveEffectsTargetMarks（宿主身上按标记移除）
	std::vector<std::string> RemoveEffectsSource{};         // INI: Trace.RemoveEffectsSource（从挂本 AE 的单位身上按名移除）
	std::vector<std::string> RemoveEffectsSourceMarks{};    // INI: Trace.RemoveEffectsSourceMarks（挂本 AE 单位身上按标记移除）
	std::vector<std::string> RemoveEffectsInitialSource{};  // INI: Trace.RemoveEffectsInitialSource（从追溯出的各来源单位身上按名移除）
	std::vector<std::string> RemoveEffectsInitialSourceMarks{}; // INI: Trace.RemoveEffectsInitialSourceMarks（各来源单位身上按标记移除）

	TraceData() : EffectData()
	{
		// TriggeredTimes（基类键 Trace.TriggeredTimes）= 总执行次数，默认 1（激活执行 1 次即耗尽）
		this->TriggeredTimes = 1;
	}

	virtual void Read(INIBufferReader* reader) override
	{
		Read(reader, "Trace.");
	}

	virtual void Read(INIBufferReader* reader, std::string title) override
	{
		// 直接复用基类读取（Trace.Enable / TriggeredTimes / AffectTypes 等发放过滤键）
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

		RemoveEffectsSkipNext = reader->Get(title + "RemoveEffectsSkipNext", RemoveEffectsSkipNext);
		RemoveEffectsTarget = reader->GetList(title + "RemoveEffectsTarget", RemoveEffectsTarget);
		ClearIfGetNone(RemoveEffectsTarget);
		RemoveEffectsTargetMarks = reader->GetList(title + "RemoveEffectsTargetMarks", RemoveEffectsTargetMarks);
		ClearIfGetNone(RemoveEffectsTargetMarks);
		RemoveEffectsSource = reader->GetList(title + "RemoveEffectsSource", RemoveEffectsSource);
		ClearIfGetNone(RemoveEffectsSource);
		RemoveEffectsSourceMarks = reader->GetList(title + "RemoveEffectsSourceMarks", RemoveEffectsSourceMarks);
		ClearIfGetNone(RemoveEffectsSourceMarks);
		RemoveEffectsInitialSource = reader->GetList(title + "RemoveEffectsInitialSource", RemoveEffectsInitialSource);
		ClearIfGetNone(RemoveEffectsInitialSource);
		RemoveEffectsInitialSourceMarks = reader->GetList(title + "RemoveEffectsInitialSourceMarks", RemoveEffectsInitialSourceMarks);
		ClearIfGetNone(RemoveEffectsInitialSourceMarks);
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
			.Process(this->RemoveEffectsSkipNext)
			.Process(this->RemoveEffectsTarget)
			.Process(this->RemoveEffectsTargetMarks)
			.Process(this->RemoveEffectsSource)
			.Process(this->RemoveEffectsSourceMarks)
			.Process(this->RemoveEffectsInitialSource)
			.Process(this->RemoveEffectsInitialSourceMarks)
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
		return const_cast<TraceData*>(this)->Serialize(stream);
	}
#pragma endregion
};
