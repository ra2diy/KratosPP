#pragma once
// ============================================================================
// CopyData — 数据层
//
// Copy 效果器：把宿主身上正在生效的 AE 的配置克隆（附加）到指定对象。
// 实际效果约等于被带 AttachEffectTypes 的弹头击中 / 被 Broadcast 广播而附加 AE，
// 只调用原有 Attach 基建，附加是否成功完全由原机制判定，Copy 不做干扰。
//
// 标签前缀 Copy.；各字段语义见字段注释与下方 Read() 的读取键。
//
// 机制解耦（每个标签只管自己定义的那件事）：
//   Copy.AttachTo   只管"贴给谁"（含贴对象死活 -> 是否贴）
//   Copy.AttachFrom 只管"副本来源记谁"（含死亡回退链，所有 AttachTo 模式下恒常工作）
//   Copy.DiscardOnInitialSourceDead 只管"来源死亡 -> 清单级移除"
//   Copy.Cut        只管"贴前移除宿主身上属于最终清单的源 AE"（与 AttachTo 无关）
// 基类 FilterData 字段（直接复用，不另立语义）：
//   Copy.AffectTypes / Copy.NotAffectTypes = 发放对象（AttachTo 目标）的类型检查名单——
//   只有名单内的对象才允许被 AttachTo（CopyEffect 第 7 步调 CanAffectType 判定）
//   Copy.OnlyAffectMarks / Copy.NotAffectMarks = 发放对象的标记检查名单——
//   目标对象身上汇总标记须通过 OnMark 判定才允许贴（CopyEffect 第 7 步）
// ============================================================================

#include <string>
#include <vector>

#include <GeneralStructures.h>

#include <Common/INI/INIConfig.h>

#include <Ext/EffectType/Effect/EffectData.h>
#include <Ext/Helper/StringEx.h> // ClearIfGetNone / CheckOnMarks

// 粘贴对象选择（Copy.AttachTo 的取值）
// 角色约定：A=给宿主挂 CopyAE 的单位（CopyAE 来源），B=宿主，C=被复制 AE 自己的来源
enum class CopyAttachTo : int
{
	Source = 0,        // 整份清单 -> CopyAE 的来源单位（基线 A，默认）
	Target = 1,        // 整份清单 -> 宿主自己（基线 B，"翻倍"）
	InitialSource = 2, // 广播：先出清单内各 AE 初始来源去重清单，每个来源都收到整份清单
	Return = 3,        // 逐条返还：每条 AE 只发回它自己的初始来源
};

template <>
inline bool Parser<CopyAttachTo>::TryParse(const char* pValue, CopyAttachTo* outValue)
{
	switch (toupper(static_cast<unsigned char>(*pValue)))
	{
	case 'S':
		if (outValue)
		{
			*outValue = CopyAttachTo::Source;
		}
		return true;
	case 'T':
		if (outValue)
		{
			*outValue = CopyAttachTo::Target;
		}
		return true;
	case 'I':
		if (outValue)
		{
			*outValue = CopyAttachTo::InitialSource;
		}
		return true;
	case 'R':
		if (outValue)
		{
			*outValue = CopyAttachTo::Return;
		}
		return true;
	default:
		if (outValue)
		{
			*outValue = CopyAttachTo::Source;
		}
		return true;
	}
}

// 副本来源设置（Copy.AttachFrom 的取值；不含 Return）
enum class CopyAttachFrom : int
{
	InitialSource = 0, // 各副本保持源 AE 自己的来源（默认）
	Source = 1,        // 全部副本来源强制记 CopyAE 来源（A）
	Target = 2,        // 全部副本来源强制记宿主（B）
};

template <>
inline bool Parser<CopyAttachFrom>::TryParse(const char* pValue, CopyAttachFrom* outValue)
{
	switch (toupper(static_cast<unsigned char>(*pValue)))
	{
	case 'S':
		if (outValue)
		{
			*outValue = CopyAttachFrom::Source;
		}
		return true;
	case 'T':
		if (outValue)
		{
			*outValue = CopyAttachFrom::Target;
		}
		return true;
	case 'I':
		if (outValue)
		{
			*outValue = CopyAttachFrom::InitialSource;
		}
		return true;
	default:
		if (outValue)
		{
			*outValue = CopyAttachFrom::InitialSource;
		}
		return true;
	}
}

// 复制谁身上的 AE（Copy.From 的取值）
enum class CopyFrom : int
{
	Target = 0, // CopyAE 的附着对象（宿主 B，默认）
	Source = 1, // CopyAE 的来源（A）
};

template <>
inline bool Parser<CopyFrom>::TryParse(const char* pValue, CopyFrom* outValue)
{
	switch (toupper(static_cast<unsigned char>(*pValue)))
	{
	case 'S':
		if (outValue)
		{
			*outValue = CopyFrom::Source;
		}
		return true;
	case 'T':
		if (outValue)
		{
			*outValue = CopyFrom::Target;
		}
		return true;
	default:
		if (outValue)
		{
			*outValue = CopyFrom::Target;
		}
		return true;
	}
}

// 额外名单贴给谁（Copy.AdditionalAttachTo 的取值；无 Return）
enum class CopyAdditionalAttachTo : int
{
	Source = 0,        // CopyAE 的来源（A）
	Target = 1,        // 宿主（B）
	InitialSource = 2, // 按来源名单逐单位分发（名单里有几个来源就附加几次）
};

template <>
inline bool Parser<CopyAdditionalAttachTo>::TryParse(const char* pValue, CopyAdditionalAttachTo* outValue)
{
	switch (toupper(static_cast<unsigned char>(*pValue)))
	{
	case 'S':
		if (outValue)
		{
			*outValue = CopyAdditionalAttachTo::Source;
		}
		return true;
	case 'T':
		if (outValue)
		{
			*outValue = CopyAdditionalAttachTo::Target;
		}
		return true;
	case 'I':
		if (outValue)
		{
			*outValue = CopyAdditionalAttachTo::InitialSource;
		}
		return true;
	default:
		if (outValue)
		{
			*outValue = CopyAdditionalAttachTo::Source;
		}
		return true;
	}
}

class CopyData : public EffectData
{
public:
	EFFECT_DATA(Copy);

	CopyAttachTo AttachTo = CopyAttachTo::Source;                   // INI: Copy.AttachTo（贴给谁）
	CopyAttachFrom AttachFrom = CopyAttachFrom::InitialSource;      // INI: Copy.AttachFrom（副本来源记谁）
	CopyFrom From = CopyFrom::Target;                               // INI: Copy.From（复制谁身上的 AE：Target=宿主/默认，Source=CopyAE 来源）
	bool DiscardOnInitialSourceDead = false;                              // INI: Copy.DiscardOnInitialSourceDead
	std::vector<std::string> AllowTypes{};                          // INI: Copy.AllowTypes   白名单（AE 名，空=全放行）
	std::vector<std::string> DisallowTypes{};                       // INI: Copy.DisallowTypes 黑名单（AE 名，优先）
	std::vector<std::string> AllowMarks{};                          // INI: Copy.AllowMarks   白名单（AE 自带标记，空=全放行）
	std::vector<std::string> DisallowMarks{};                       // INI: Copy.DisallowMarks 黑名单（AE 自带标记，优先）
	bool Cut = false;                                               // INI: Copy.Cut（贴前移除宿主源 AE）
	int Delay = 0;                                                  // INI: Copy.Delay（周期拷贝间隔帧，<=0 按 1 帧）
	// Additional 附加通道（空名单=通道关闭）
	std::vector<std::string> AdditionalAttachEffects{};             // INI: Copy.AdditionalAttachEffects（额外按名附加的 AE 名单）
	CopyAdditionalAttachTo AdditionalAttachTo = CopyAdditionalAttachTo::Source; // INI: Copy.AdditionalAttachTo（额外名单贴给谁）
	CopyAttachFrom AdditionalAttachFrom = CopyAttachFrom::InitialSource;        // INI: Copy.AdditionalAttachFrom（额外名单来源记谁，复用 CopyAttachFrom 值集）

	/// @brief Additional 涉及 InitialSource（需要来源名单）时必须显式给出白名单，
	/// 不允许"空=读复制源身上全部 AE"
	bool NeedAdditionalSourceList() const
	{
		return !AllowTypes.empty() || !AllowMarks.empty();
	}

	CopyData() : EffectData()
	{
		// 基类 FilterData 的 AffectTypes/NotAffectTypes/OnlyAffectMarks/NotAffectMarks
		// 参与行为（发放对象类型/标记检查，见 CopyEffect::ExecuteOnce 第 7 步）；
		// 其余对象级过滤字段（类别开关默认全开、Affects* 阵营等）读入但 Copy 不查。
		// TriggeredTimes 默认 1（不写 = 激活帧拷 1 次即移除）；基类默认 -1，需覆盖
		this->TriggeredTimes = 1;
	}

	virtual void Read(INIBufferReader* reader) override
	{
		Read(reader, "Copy.");
	}

	virtual void Read(INIBufferReader* reader, std::string title) override
	{
		// 直接复用基类读取（Copy.Enable / Copy.TriggeredTimes / Copy.AffectTypes /
		// Copy.NotAffectTypes 等），不复写基类逻辑。
		EffectData::Read(reader, title);

		AttachTo = reader->Get(title + "AttachTo", AttachTo);
		AttachFrom = reader->Get(title + "AttachFrom", AttachFrom);
		From = reader->Get(title + "From", From);
		DiscardOnInitialSourceDead = reader->Get(title + "DiscardOnInitialSourceDead", DiscardOnInitialSourceDead);

		AllowTypes = reader->GetList(title + "AllowTypes", AllowTypes);
		ClearIfGetNone(AllowTypes);
		DisallowTypes = reader->GetList(title + "DisallowTypes", DisallowTypes);
		ClearIfGetNone(DisallowTypes);
		AllowMarks = reader->GetList(title + "AllowMarks", AllowMarks);
		ClearIfGetNone(AllowMarks);
		DisallowMarks = reader->GetList(title + "DisallowMarks", DisallowMarks);
		ClearIfGetNone(DisallowMarks);

		Cut = reader->Get(title + "Cut", Cut);
		Delay = reader->Get(title + "Delay", Delay);

		AdditionalAttachEffects = reader->GetList(title + "AdditionalAttachEffects", AdditionalAttachEffects);
		ClearIfGetNone(AdditionalAttachEffects);
		AdditionalAttachTo = reader->Get(title + "AdditionalAttachTo", AdditionalAttachTo);
		AdditionalAttachFrom = reader->Get(title + "AdditionalAttachFrom", AdditionalAttachFrom);
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->AttachTo)
			.Process(this->AttachFrom)
			.Process(this->From)
			.Process(this->DiscardOnInitialSourceDead)
			.Process(this->AllowTypes)
			.Process(this->DisallowTypes)
			.Process(this->AllowMarks)
			.Process(this->DisallowMarks)
			.Process(this->Cut)
			.Process(this->Delay)
			.Process(this->AdditionalAttachEffects)
			.Process(this->AdditionalAttachTo)
			.Process(this->AdditionalAttachFrom)
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
		return const_cast<CopyData*>(this)->Serialize(stream);
	}
#pragma endregion
};
