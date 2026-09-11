#pragma once
// ============================================================================
// CopyData — Copy 与 Trace 的公共数据基础（同源族）
//
// 两个效果器共用同一套机制：观察指定单位身上正在生效的 AE（Watch/Ignore 名单），
// 把观察结果换成"要附加的内容"（Copy = 命中 AE 本身；Trace = INI 固定名单），
// 按 AttachTo（贴给谁）/ AttachFrom（来源记谁）分发到选定目标，附加一律走现有 Attach 基建。
//
// 本文件定义三组取值枚举、观察组结构与 Copy 的数据类：
//   CopyAttachTo    贴给谁（Copy 支持 Return；Trace 经 CopyParseAttachTo 限三值）
//   CopyAttachFrom  来源记谁（带死亡回退链）
//   CopyFrom        观察谁身上的 AE
//   CopyWatchConfig 观察名单组（Watch/WatchMarks 白名单 + IgnoreTypes/IgnoreMarks 黑名单；
//                   白名单空 = 全量观察，无额外开关）
// 机制解耦（每个标签只管自己定义的那件事）：
//   Watch/Ignore   只管"观察哪些 AE 当线索"
//   AttachTo       只管"贴给谁"（目标死活 -> 是否贴）
//   AttachFrom     只管"来源记谁"（死亡回退链恒常，与贴判定无关）
//   Discard        只管"死源丢弃 / 走回退"
//   Cut（仅 Copy） 只管"贴前移除观察源身上命中名单的源 AE"
//   Affect*/Marks  只管"发放对象检查"（基类 FilterData 字段）
// ============================================================================

#include <string>
#include <vector>

#include <GeneralStructures.h>

#include <Common/INI/INIConfig.h>

#include <Ext/EffectType/Effect/EffectData.h>
#include <Ext/Helper/StringEx.h> // ClearIfGetNone

// 粘贴对象选择（AttachTo 的取值）
enum class CopyAttachTo : int
{
	Source = 0,        // 整份内容 -> 挂效果器 AE 的单位（默认）
	Target = 1,        // 整份内容 -> 宿主自己
	InitialSource = 2, // 广播：先出观察命中 AE 的来源去重清单，每个来源都收到整份内容
	Return = 3,        // 逐条返还：每条命中 AE 只发回它自己的初始来源（Copy 专属）
};

// 按首字母解析 AttachTo 取值；allowReturn=false 时不接受 Return（Trace 用）
inline bool CopyParseAttachTo(const char* pValue, bool allowReturn, CopyAttachTo* outValue)
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
		if (allowReturn)
		{
			if (outValue)
			{
				*outValue = CopyAttachTo::Return;
			}
			return true;
		}
		break;
	default:
		break;
	}
	if (outValue)
	{
		*outValue = CopyAttachTo::Source;
	}
	return true;
}

template <>
inline bool Parser<CopyAttachTo>::TryParse(const char* pValue, CopyAttachTo* outValue)
{
	return CopyParseAttachTo(pValue, true, outValue);
}

// 附加来源设置（AttachFrom 的取值）
enum class CopyAttachFrom : int
{
	InitialSource = 0, // 各条保持观察命中 AE 自己的来源（默认）
	Source = 1,        // 全部来源强制记挂效果器 AE 的单位
	Target = 2,        // 全部来源强制记宿主
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

// 观察谁身上的 AE（From 的取值）
enum class CopyFrom : int
{
	Target = 0, // 效果器 AE 的附着对象（宿主，默认）
	Source = 1, // 挂效果器 AE 的单位
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

// 观察名单组（主效果器与 Trace 各持一份；过滤判定逻辑共用）
//   Watch / WatchMarks      白名单（AE 名 / AE 自带标记）；空 = 全量
//   IgnoreTypes / IgnoreMarks 黑名单（AE 名 / AE 自带标记，优先）
struct CopyWatchConfig
{
	std::vector<std::string> Watch{};
	std::vector<std::string> WatchMarks{};
	std::vector<std::string> IgnoreTypes{};
	std::vector<std::string> IgnoreMarks{};

	void Read(INIBufferReader* reader, std::string title)
	{
		Watch = reader->GetList(title + "Watch", Watch);
		ClearIfGetNone(Watch);
		WatchMarks = reader->GetList(title + "WatchMarks", WatchMarks);
		ClearIfGetNone(WatchMarks);
		IgnoreTypes = reader->GetList(title + "IgnoreTypes", IgnoreTypes);
		ClearIfGetNone(IgnoreTypes);
		IgnoreMarks = reader->GetList(title + "IgnoreMarks", IgnoreMarks);
		ClearIfGetNone(IgnoreMarks);
	}
};

class CopyData : public EffectData
{
public:
	EFFECT_DATA(Copy);

	// ---- 主效果器（前缀 Copy.*）----
	CopyFrom From = CopyFrom::Target;                       // INI: Copy.From（观察谁身上的 AE）
	CopyWatchConfig Watch{};                                // INI: Copy.Watch / WatchMarks / IgnoreTypes / IgnoreMarks（观察名单组）
	CopyAttachTo AttachTo = CopyAttachTo::Source;           // INI: Copy.AttachTo（贴给谁，含 Return）
	CopyAttachFrom AttachFrom = CopyAttachFrom::InitialSource; // INI: Copy.AttachFrom（来源记谁）
	bool DiscardOnInitialSourceDead = false;                // INI: Copy.DiscardOnInitialSourceDead（死源清单级丢弃 / 走回退）
	bool Cut = false;                                       // INI: Copy.Cut（贴前移除观察源身上命中清单的源 AE）
	int Delay = 0;                                          // INI: Copy.Delay（执行间隔帧，<=0 按 1 帧）

	CopyData() : EffectData()
	{
		// TriggeredTimes（基类键 Copy.TriggeredTimes）= 总执行次数，默认 1（激活执行 1 次即耗尽）
		this->TriggeredTimes = 1;
	}

	virtual void Read(INIBufferReader* reader) override
	{
		Read(reader, "Copy.");
	}

	virtual void Read(INIBufferReader* reader, std::string title) override
	{
		// 直接复用基类读取（Copy.Enable / Copy.TriggeredTimes / Copy.AffectTypes 等发放过滤键）
		EffectData::Read(reader, title);

		From = reader->Get(title + "From", From);
		Watch.Read(reader, title);
		AttachTo = reader->Get(title + "AttachTo", AttachTo);
		AttachFrom = reader->Get(title + "AttachFrom", AttachFrom);
		DiscardOnInitialSourceDead = reader->Get(title + "DiscardOnInitialSourceDead", DiscardOnInitialSourceDead);
		Cut = reader->Get(title + "Cut", Cut);
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
			.Process(this->Cut)
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
		return const_cast<CopyData*>(this)->Serialize(stream);
	}
#pragma endregion
};
