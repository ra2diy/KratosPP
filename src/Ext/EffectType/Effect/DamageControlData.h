#pragma once

#include <string>
#include <vector>

#include <GeneralStructures.h>

#include <Ext/EffectType/Effect/EffectData.h>
#include <Ext/Helper/CastEx.h>
// 类别取值沿用 DamageReactionMode：弹头侧的 IgnoreDamageReaction.Modes 用的就是这套枚举，
// 共用它可以保证"哪些弹头穿透哪一类响应"的配置在两个机制之间语义一致
#include <Ext/StateType/State/DamageReactionData.h>


/// @brief 闪避率的合成方式，决定这一段的数值怎么并入总闪避率
enum class DodgeStackMode : int
{
	ADD = 0, // 加算：率 = 率 + 本段值
	MUL = 1  // 乘算：率 = 1 - (1 - 率) * (1 - 本段值)
};

template <>
inline bool Parser<DodgeStackMode>::TryParse(const char* pValue, DodgeStackMode* outValue)
{
	switch (toupper(static_cast<unsigned char>(*pValue)))
	{
	case 'A':
		if (outValue)
		{
			*outValue = DodgeStackMode::ADD;
		}
		return true;
	case 'M':
		if (outValue)
		{
			*outValue = DodgeStackMode::MUL;
		}
		return true;
	default:
		// 解析失败时保留调用方的默认值，避免拼写错误被静默当成另一个合法取值
		return false;
	}
}

/// @brief 刚毅的条件判定方式，决定当前伤害与判定值之间满足什么关系才算条件成立
enum class FortitudeCompare : int
{
	EQ = 0, // 等于
	NE = 1, // 不等于
	GT = 2, // 大于
	LT = 3, // 小于
	GE = 4, // 大于等于
	LE = 5  // 小于等于
};

template <>
inline bool Parser<FortitudeCompare>::TryParse(const char* pValue, FortitudeCompare* outValue)
{
	// GT 与 GE、LT 与 LE 的首字母相同，必须比到第二个字符才能区分
	int first = toupper(static_cast<unsigned char>(pValue[0]));
	int second = (pValue[0] && pValue[1]) ? toupper(static_cast<unsigned char>(pValue[1])) : '\0';
	switch (first)
	{
	case 'E':
		if (second == 'Q')
		{
			if (outValue)
			{
				*outValue = FortitudeCompare::EQ;
			}
			return true;
		}
		break;
	case 'N':
		if (second == 'E')
		{
			if (outValue)
			{
				*outValue = FortitudeCompare::NE;
			}
			return true;
		}
		break;
	case 'G':
		if (second == 'T')
		{
			if (outValue)
			{
				*outValue = FortitudeCompare::GT;
			}
			return true;
		}
		if (second == 'E')
		{
			if (outValue)
			{
				*outValue = FortitudeCompare::GE;
			}
			return true;
		}
		break;
	case 'L':
		if (second == 'T')
		{
			if (outValue)
			{
				*outValue = FortitudeCompare::LT;
			}
			return true;
		}
		if (second == 'E')
		{
			if (outValue)
			{
				*outValue = FortitudeCompare::LE;
			}
			return true;
		}
		break;
	default:
		break;
	}
	// 解析失败时保留调用方的默认值，避免拼写错误被静默当成另一个合法取值
	return false;
}

/// @brief 刚毅的条件成立之后对伤害值做什么
enum class FortitudeAction : int
{
	NONE = 0,   // 只做比较，不改数值
	SET = 1,    // 把伤害强制改成变化目标值
	DISCARD = 2 // 把伤害归零并直接到管线出口
};

template <>
inline bool Parser<FortitudeAction>::TryParse(const char* pValue, FortitudeAction* outValue)
{
	switch (toupper(static_cast<unsigned char>(*pValue)))
	{
	case 'N':
		if (outValue)
		{
			*outValue = FortitudeAction::NONE;
		}
		return true;
	case 'S':
		if (outValue)
		{
			*outValue = FortitudeAction::SET;
		}
		return true;
	case 'D':
		if (outValue)
		{
			*outValue = FortitudeAction::DISCARD;
		}
		return true;
	default:
		return false;
	}
}


/// @brief 一条 DamageControl AE 的全部配置。
/// 一条 AE 只声明一个类别（Mode）与它需要的数值，多个类别靠挂多条 AE 共存，
/// 由 AE 管理器按 Priority 把它们收拢成一条伤害管线（见 AttachEffect::ApplyDamageControl）。
class DamageControlEntity
{
public:
	bool Enable = false;

	DamageReactionMode Mode = DamageReactionMode::NONE; // 本条段的类别
	int Priority = 0; // 执行优先级，数值越大越先执行；运行时不可改

	double DodgeRate = 0; // EVASION：闪避率，按百分比解析，允许为负、允许超过 1
	DodgeStackMode DodgeStack = DodgeStackMode::ADD; // EVASION：本段数值怎么并入总闪避率

	int DamageValue = 0; // FORTITUDE：判定值；REDUCE：固定增减量（正=减少，负=增加）
	FortitudeCompare Compare = FortitudeCompare::GT; // FORTITUDE：条件判定方式，默认大于
	FortitudeAction Action = FortitudeAction::SET; // FORTITUDE：条件成立后的动作
	int FortitudeTarget = 0; // FORTITUDE：SET 时强制改成的值；不写则取判定值

	bool IMustRegroupMyForces = false; // PREVENT：yes 时免死触发后把血量直接设为 1

	double Chance = 1; // 本段触发概率，各段独立判定
	int Delay = 0; // 成功触发后到下一次可用的冷却帧数
	bool ActiveOnce = false; // 触发之后在当前帧结束后结束
	int TriggeredTimes = -1; // 可触发次数，用尽即结束
	bool ResetTimes = false; // 新兵与精英互相切换时是否清零次数

	bool InfiniteTimesPerFrame = true; // yes 时触发次数每帧只变化一次，效果在本帧内持续
	bool InfiniteAttachPerFrame = true; // yes 时本帧内附属 AE 可以贴无数次

	std::vector<std::string> TriggeredAttachEffects{}; // 触发时给自己附加的 AE 清单
	std::vector<double> TriggeredAttachEffectChances{}; // 附加效果的成功率
	bool TriggeredAttachEffectsFromAttacker = false; // 附加的 AE 记攻击者为来源

	std::vector<std::string> OnlyReactionWarheads{}; // 只对这些弹头响应
	std::vector<std::string> NotReactionWarheads{}; // 不对这些弹头响应

	std::string Anim{ "" };
	CoordStruct AnimFLH = CoordStruct::Empty;
	int AnimDelay = 0;

	/// @brief 读取一条段的配置
	/// @param reader INI 读取器
	/// @param title 标签前缀（普通配置是 "DamageControl."，精英配置是 "DamageControl.Elite"）
	virtual void Read(INIBufferReader* reader, std::string title)
	{
		Mode = reader->Get(title + "Mode", Mode);
		if (Mode == DamageReactionMode::NONE)
		{
			// 没有声明类别就当成空段，直接结束
			Enable = false;
			return;
		}

		Priority = reader->Get(title + "Priority", Priority);
		if (Priority < 0)
		{
			Priority = 0;
		}

		// 同一个 Value 标签在不同类别下含义不同，按类别取正确的解析方式：
		// 闪避率是百分比（带 % 时会被解析器乘 0.01），其余两类是伤害整数
		switch (Mode)
		{
		case DamageReactionMode::EVASION:
			DodgeRate = reader->GetPercent(title + "Value", DodgeRate);
			DodgeStack = reader->Get(title + "DodgeStack", DodgeStack);
			break;
		case DamageReactionMode::FORTITUDE:
			DamageValue = reader->Get(title + "Value", DamageValue);
			FortitudeTarget = DamageValue;
			reader->TryGet(title + "FortitudeTarget", FortitudeTarget);
			Compare = reader->Get(title + "FortitudeCompare", Compare);
			Action = reader->Get(title + "FortitudeAction", Action);
			break;
		case DamageReactionMode::REDUCE:
			DamageValue = reader->Get(title + "Value", DamageValue);
			break;
		case DamageReactionMode::PREVENT:
			IMustRegroupMyForces = reader->Get(title + "IMustRegroupMyForces", IMustRegroupMyForces);
			break;
		default:
			break;
		}

		Chance = reader->GetPercent(title + "Chance", Chance);
		Delay = reader->Get(title + "Delay", Delay);
		ActiveOnce = reader->Get(title + "ActiveOnce", ActiveOnce);
		TriggeredTimes = reader->Get(title + "TriggeredTimes", TriggeredTimes);
		ResetTimes = reader->Get(title + "ResetTimes", ResetTimes);
		InfiniteTimesPerFrame = reader->Get(title + "InfiniteTimesPerFrame", InfiniteTimesPerFrame);
		InfiniteAttachPerFrame = reader->Get(title + "InfiniteAttachPerFrame", InfiniteAttachPerFrame);

		TriggeredAttachEffects = reader->GetList(title + "TriggeredAttachEffects", TriggeredAttachEffects);
		ClearIfGetNone(TriggeredAttachEffects);
		TriggeredAttachEffectChances = reader->GetChanceList(title + "TriggeredAttachEffectChances", TriggeredAttachEffectChances);
		TriggeredAttachEffectsFromAttacker = reader->Get(title + "TriggeredAttachEffectsFromAttacker", TriggeredAttachEffectsFromAttacker);

		OnlyReactionWarheads = reader->GetList(title + "OnlyReactionWarheads", OnlyReactionWarheads);
		ClearIfGetNone(OnlyReactionWarheads);
		NotReactionWarheads = reader->GetList(title + "NotReactionWarheads", NotReactionWarheads);
		ClearIfGetNone(NotReactionWarheads);

		Anim = reader->Get(title + "Anim", Anim);
		AnimFLH = reader->Get(title + "AnimFLH", AnimFLH);
		AnimDelay = reader->Get(title + "AnimDelay", AnimDelay);

		// 写了 Mode 就视为开启，显式写 Enable=no 可以关掉
		bool enable = true;
		reader->TryGet(title + "Enable", enable);
		Enable = enable;
	}

	/// @brief 判断某个弹头是否允许触发本段
	/// @param warheadId 弹头的 ID
	/// @return true = 允许触发；白名单存在时必须在白名单内，黑名单命中时一律不允许
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
			.Process(this->Enable)
			.Process(this->Mode)
			.Process(this->Priority)

			.Process(this->DodgeRate)
			.Process(this->DodgeStack)

			.Process(this->DamageValue)
			.Process(this->Compare)
			.Process(this->Action)
			.Process(this->FortitudeTarget)

			.Process(this->IMustRegroupMyForces)

			.Process(this->Chance)
			.Process(this->Delay)
			.Process(this->ActiveOnce)
			.Process(this->TriggeredTimes)
			.Process(this->ResetTimes)
			.Process(this->InfiniteTimesPerFrame)
			.Process(this->InfiniteAttachPerFrame)

			.Process(this->TriggeredAttachEffects)
			.Process(this->TriggeredAttachEffectChances)
			.Process(this->TriggeredAttachEffectsFromAttacker)

			.Process(this->OnlyReactionWarheads)
			.Process(this->NotReactionWarheads)

			.Process(this->Anim)
			.Process(this->AnimFLH)
			.Process(this->AnimDelay)

			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange)
	{
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const
	{
		return const_cast<DamageControlEntity*>(this)->Serialize(stream);
	}
#pragma endregion
};


/// @brief DamageControl 的 INI 数据，普通配置与精英配置各存一份
class DamageControlData : public EffectData
{
public:
	EFFECT_DATA(DamageControl);

	DamageControlEntity Data{};
	DamageControlEntity EliteData{};

	DamageControlData() : EffectData()
	{
		TriggeredTimes = 1;
	}

	virtual void Read(INIBufferReader* reader) override
	{
		Read(reader, "DamageControl.");
	}

	virtual void Read(INIBufferReader* reader, std::string title) override
	{
		EffectData::Read(reader, title);

		DamageControlEntity data;
		data.Read(reader, title);
		if (data.Enable)
		{
			Data = data;
			EliteData = data;
		}

		DamageControlEntity eliteData;
		eliteData.Read(reader, title + "Elite");
		if (eliteData.Enable)
		{
			EliteData = eliteData;
		}

		Enable = Data.Enable || EliteData.Enable;
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->Data)
			.Process(this->EliteData)
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
		return const_cast<DamageControlData*>(this)->Serialize(stream);
	}
#pragma endregion
};
