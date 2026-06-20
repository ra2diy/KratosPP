#pragma once

#include <algorithm>
#include <string>
#include <vector>
#include <set>

#include "Effect/EffectData.h"
// TODO Add new Effects
#include "Effect/AnimationData.h"
#include "Effect/AttackBeaconData.h"
#include "Effect/AutoWeaponData.h"
#include "Effect/BroadcastData.h"
#include "Effect/CrateBuffData.h"
#include "Effect/CounterData.h"
#include "Effect/CountTriggerData.h"
#include "Effect/AmmoData.h"
#include "Effect/AmmoTriggerData.h"

#include "Effect/DamageSelfData.h"
#include "Effect/ExtraFireData.h"
#include "Effect/FireSuperData.h"
#include "Effect/HostData.h"
#include "Effect/ImmuneData.h"
#include "Effect/InfoData.h"
#include "Effect/MarkData.h"
#include "Effect/RevengeData.h"
#include "Effect/StackData.h"
#include "Effect/StandData.h"
#include "Effect/VectorData.h"
#include "Effect/TurretSpinData.h"
#include "Effect/BodySpinData.h"
#include "Effect/VampireData.h"
// State Effects
#include <Ext/StateType/State/AntiBulletData.h>
#include <Ext/StateType/State/BlackHoleData.h>
#include <Ext/StateType/State/DamageReactionData.h>
#include <Ext/StateType/State/DeselectData.h>
#include <Ext/StateType/State/DestroyAnimData.h>
#include <Ext/StateType/State/DestroySelfData.h>
#include <Ext/StateType/State/DisableWeaponData.h>
#include <Ext/StateType/State/ECMData.h>
#include <Ext/StateType/State/FreezeData.h>
#include <Ext/StateType/State/GiftBoxData.h>
#include <Ext/StateType/State/NoMoneyNoTalkData.h>
#include <Ext/StateType/State/OverrideWeaponData.h>
#include <Ext/StateType/State/PaintballData.h>
#include <Ext/StateType/State/PumpData.h>
#include <Ext/StateType/State/ScatterData.h>
#include <Ext/StateType/State/TeleportData.h>
#include <Ext/StateType/State/TransformData.h>


enum class CumulativeMode
{
	NO = 0, YES = 1, ATTACKER = 2, HOUSE = 3,
};

template <>
inline bool Parser<CumulativeMode>::TryParse(const char* pValue, CumulativeMode* outValue)
{
	switch (toupper(static_cast<unsigned char>(*pValue)))
	{
	case '1':
	case 'T':
	case 'Y':
		if (outValue)
		{
			*outValue = CumulativeMode::YES;
		}
		return true;
	case '0':
	case 'F':
	case 'N':
		if (outValue)
		{
			*outValue = CumulativeMode::NO;
		}
		return true;
	case 'A':
		if (outValue)
		{
			*outValue = CumulativeMode::ATTACKER;
		}
		return true;
	case 'H':
		if (outValue)
		{
			*outValue = CumulativeMode::HOUSE;
		}
		return true;
	}
	return false;
}

enum class AttachOwnerType
{
	TECHONO = 0, BULLET = 1,
};

// 定义变量
#define EFFECT_VAR_DEFINE(EFFECT_DATA_NAME) \
	EFFECT_DATA_NAME ## Data EFFECT_DATA_NAME{}; \

// 从ini中读取内容
#define EFFECT_VAR_READ(EFFECT_DATA_NAME) \
	EFFECT_DATA_NAME.Read(reader); \

// 读取脚本名称
#define EFFECT_VAR_SCRIPT_NAME(EFFECT_DATA_NAME) \
	if (EFFECT_DATA_NAME.Enable) \
	{ \
		names.push_back(EFFECT_DATA_NAME.GetEffectScriptName()); \
	} \

// 保存读取
#define EFFECT_VAR_PROCESS(EFFECT_DATA_NAME) \
	.Process(this->EFFECT_DATA_NAME) \

class AttachEffectData : public EffectData
{
public:
	std::string Name{ "" };

	int Duration = -1; // 持续时间
	bool HoldDuration = true; // 无限时间

	int Delay = 0; // 不可获得同名的延迟
	Point2D RandomDelay{ 0, 0 }; // 随机最小值

	int InitialDelay = -1; // 生效前的初始延迟
	Point2D InitialRandomDelay{ 0, 0 }; // 随机初始延迟

	bool DiscardOnEntry = false; // 离开地图则失效
	bool DiscardOnTransform = true; // 发生类型改变时失效
	bool DiscardOnUndeploying = false; // 是否在建筑反部署时直接结束
	bool DiscardOnSelling = false; // 是否在建筑出售时直接结束
	bool PenetratesIronCurtain = false; // 弹头附加，影响铁幕
	bool FromTransporter = true; // 弹头附加，乘客附加时，视为载具
	bool ReceiverOwn = false; // 弹头附加，属于被赋予对象

	bool TreatSellingAsDeath = true; // 将建筑出售视为死亡
	bool StartAfterBuildup = true; // 建筑拔起期间不生效

	CumulativeMode Cumulative = CumulativeMode::NO; // 可叠加
	int MaxStack = -1; // 叠加上限
	bool ResetDurationOnReapply = false; // 不可叠加时，重复获得时是否重置计时器
	std::string Group = { "" }; // 分组，同一个分组的效果互相影响，削减或增加持续时间
	bool OverrideSameGroup = false; // 是否覆盖同一个分组
	std::vector<std::string> Next{}; // 结束后附加下一个AE

	std::vector<std::string> AttachWithOutTypes{}; // 有这些AE存在则不可赋予
	bool AttachOnceInTechnoType = false; // 写在TechnoType上只在创建时赋予一次
	int AttachNeedMoney = 0; // 来源需要够钱才可以赋予
	int ReceiverNeedMoney = 0; // 接受者需要够钱才可以赋予
	bool Inheritable = true; // 是否可以被礼盒礼物继承

	// TODO Add new Effects
	EFFECT_VAR_DEFINE(Animation);
	EFFECT_VAR_DEFINE(AttackBeacon);
	EFFECT_VAR_DEFINE(AutoWeapon);
	EFFECT_VAR_DEFINE(Broadcast);
	EFFECT_VAR_DEFINE(CrateBuff);
	EFFECT_VAR_DEFINE(Counter);
	EFFECT_VAR_DEFINE(CountTrigger);
	EFFECT_VAR_DEFINE(Ammo);
	EFFECT_VAR_DEFINE(AmmoTrigger);

	EFFECT_VAR_DEFINE(DamageSelf);
	EFFECT_VAR_DEFINE(ExtraFire);
	EFFECT_VAR_DEFINE(FireSuper);
	EFFECT_VAR_DEFINE(Host);
	EFFECT_VAR_DEFINE(Immune);
	EFFECT_VAR_DEFINE(Info);
	EFFECT_VAR_DEFINE(Mark);
	EFFECT_VAR_DEFINE(Revenge);
	EFFECT_VAR_DEFINE(Stack);
	EFFECT_VAR_DEFINE(Stand);
	EFFECT_VAR_DEFINE(Vector);
	EFFECT_VAR_DEFINE(TurretSpin);
	EFFECT_VAR_DEFINE(BodySpin);
	EFFECT_VAR_DEFINE(Vampire);
	// State Effects
	EFFECT_VAR_DEFINE(AntiBullet);
	EFFECT_VAR_DEFINE(BlackHole);
	EFFECT_VAR_DEFINE(DamageReaction);
	EFFECT_VAR_DEFINE(Deselect);
	EFFECT_VAR_DEFINE(DestroyAnim);
	EFFECT_VAR_DEFINE(DestroySelf);
	EFFECT_VAR_DEFINE(DisableWeapon);
	EFFECT_VAR_DEFINE(ECM);
	EFFECT_VAR_DEFINE(Freeze);
	EFFECT_VAR_DEFINE(GiftBox);
	EFFECT_VAR_DEFINE(NoMoneyNoTalk);
	EFFECT_VAR_DEFINE(OverrideWeapon);
	EFFECT_VAR_DEFINE(Paintball);
	EFFECT_VAR_DEFINE(Pump);
	EFFECT_VAR_DEFINE(Scatter);
	EFFECT_VAR_DEFINE(Teleport);
	EFFECT_VAR_DEFINE(Transform);

	void ReadEffects(INIBufferReader* reader)
	{
		EFFECT_VAR_READ(Animation);
		EFFECT_VAR_READ(AttackBeacon);
		EFFECT_VAR_READ(AutoWeapon);
		EFFECT_VAR_READ(Broadcast);
		EFFECT_VAR_READ(CrateBuff);
		EFFECT_VAR_READ(Counter);
		EFFECT_VAR_READ(CountTrigger);
		EFFECT_VAR_READ(Ammo);
		EFFECT_VAR_READ(AmmoTrigger);

		EFFECT_VAR_READ(DamageSelf);
		EFFECT_VAR_READ(ExtraFire);
		EFFECT_VAR_READ(FireSuper);
		EFFECT_VAR_READ(Host);
		EFFECT_VAR_READ(Immune);
		EFFECT_VAR_READ(Info);
		EFFECT_VAR_READ(Mark);
		EFFECT_VAR_READ(Revenge);
		EFFECT_VAR_READ(Stack);
		EFFECT_VAR_READ(Stand);
		EFFECT_VAR_READ(Vector);
		EFFECT_VAR_READ(TurretSpin);
		EFFECT_VAR_READ(BodySpin);
		EFFECT_VAR_READ(Vampire);
		// State Effects
		EFFECT_VAR_READ(AntiBullet);
		EFFECT_VAR_READ(BlackHole);
		EFFECT_VAR_READ(DamageReaction);
		EFFECT_VAR_READ(Deselect);
		EFFECT_VAR_READ(DestroyAnim);
		EFFECT_VAR_READ(DestroySelf);
		EFFECT_VAR_READ(DisableWeapon);
		EFFECT_VAR_READ(ECM);
		EFFECT_VAR_READ(Freeze);
		EFFECT_VAR_READ(GiftBox);
		EFFECT_VAR_READ(NoMoneyNoTalk);
		EFFECT_VAR_READ(OverrideWeapon);
		EFFECT_VAR_READ(Paintball);
		EFFECT_VAR_READ(Pump);
		EFFECT_VAR_READ(Scatter);
		EFFECT_VAR_READ(Teleport);
		EFFECT_VAR_READ(Transform);
	}

	std::vector<std::string> GetScriptNames()
	{
		std::vector<std::string> names{};
		// 替身要放在第一位，状态类型会附加状态给替身，此时若替身没有初始化，整个AE都会被判定为失效
		EFFECT_VAR_SCRIPT_NAME(Stand);
		EFFECT_VAR_SCRIPT_NAME(Vector);
		EFFECT_VAR_SCRIPT_NAME(TurretSpin);
		EFFECT_VAR_SCRIPT_NAME(BodySpin);

		EFFECT_VAR_SCRIPT_NAME(Animation);
		EFFECT_VAR_SCRIPT_NAME(AttackBeacon);
		EFFECT_VAR_SCRIPT_NAME(AutoWeapon);
		EFFECT_VAR_SCRIPT_NAME(Broadcast);
		EFFECT_VAR_SCRIPT_NAME(CrateBuff);
		EFFECT_VAR_SCRIPT_NAME(Counter);
		EFFECT_VAR_SCRIPT_NAME(CountTrigger);
		EFFECT_VAR_SCRIPT_NAME(Ammo);
		EFFECT_VAR_SCRIPT_NAME(AmmoTrigger);
		EFFECT_VAR_SCRIPT_NAME(DamageSelf);
		EFFECT_VAR_SCRIPT_NAME(ExtraFire);
		EFFECT_VAR_SCRIPT_NAME(FireSuper);
		EFFECT_VAR_SCRIPT_NAME(Host);
		EFFECT_VAR_SCRIPT_NAME(Immune);
		EFFECT_VAR_SCRIPT_NAME(Info);
		EFFECT_VAR_SCRIPT_NAME(Mark);
		EFFECT_VAR_SCRIPT_NAME(Revenge);
		EFFECT_VAR_SCRIPT_NAME(Stack);
		EFFECT_VAR_SCRIPT_NAME(Vampire);
		// State Effects
		EFFECT_VAR_SCRIPT_NAME(AntiBullet);
		EFFECT_VAR_SCRIPT_NAME(BlackHole);
		EFFECT_VAR_SCRIPT_NAME(DamageReaction);
		EFFECT_VAR_SCRIPT_NAME(Deselect);
		EFFECT_VAR_SCRIPT_NAME(DestroyAnim);
		EFFECT_VAR_SCRIPT_NAME(DestroySelf);
		EFFECT_VAR_SCRIPT_NAME(DisableWeapon);
		EFFECT_VAR_SCRIPT_NAME(ECM);
		EFFECT_VAR_SCRIPT_NAME(Freeze);
		EFFECT_VAR_SCRIPT_NAME(GiftBox);
		EFFECT_VAR_SCRIPT_NAME(NoMoneyNoTalk);
		EFFECT_VAR_SCRIPT_NAME(OverrideWeapon);
		EFFECT_VAR_SCRIPT_NAME(Paintball);
		EFFECT_VAR_SCRIPT_NAME(Pump);
		EFFECT_VAR_SCRIPT_NAME(Scatter);
		EFFECT_VAR_SCRIPT_NAME(Teleport);
		EFFECT_VAR_SCRIPT_NAME(Transform);

		return names;
	}

	template <typename T>
	void ProcessEffects(T& stream)
	{
		stream
			EFFECT_VAR_PROCESS(Animation)
			EFFECT_VAR_PROCESS(AttackBeacon)
			EFFECT_VAR_PROCESS(AutoWeapon)
			EFFECT_VAR_PROCESS(Broadcast)
			EFFECT_VAR_PROCESS(CrateBuff)
			EFFECT_VAR_PROCESS(Counter)
			EFFECT_VAR_PROCESS(CountTrigger)
			EFFECT_VAR_PROCESS(Ammo)
			EFFECT_VAR_PROCESS(AmmoTrigger)

			EFFECT_VAR_PROCESS(DamageSelf)
			EFFECT_VAR_PROCESS(ExtraFire)
			EFFECT_VAR_PROCESS(FireSuper)
			EFFECT_VAR_PROCESS(Host)
			EFFECT_VAR_PROCESS(Immune)
			EFFECT_VAR_PROCESS(Info)
			EFFECT_VAR_PROCESS(Mark)
			EFFECT_VAR_PROCESS(Revenge)
			EFFECT_VAR_PROCESS(Stack)
			EFFECT_VAR_PROCESS(Stand)
			EFFECT_VAR_PROCESS(Vector)
			EFFECT_VAR_PROCESS(TurretSpin)
			EFFECT_VAR_PROCESS(BodySpin)
			EFFECT_VAR_PROCESS(Vampire)
			// State Effects
			EFFECT_VAR_PROCESS(AntiBullet)
			EFFECT_VAR_PROCESS(BlackHole)
			EFFECT_VAR_PROCESS(DamageReaction)
			EFFECT_VAR_PROCESS(Deselect)
			EFFECT_VAR_PROCESS(DestroyAnim)
			EFFECT_VAR_PROCESS(DestroySelf)
			EFFECT_VAR_PROCESS(DisableWeapon)
			EFFECT_VAR_PROCESS(GiftBox)
			EFFECT_VAR_PROCESS(NoMoneyNoTalk)
			EFFECT_VAR_PROCESS(OverrideWeapon)
			EFFECT_VAR_PROCESS(ECM)
			EFFECT_VAR_PROCESS(Freeze)
			EFFECT_VAR_PROCESS(Paintball)
			EFFECT_VAR_PROCESS(Pump)
			EFFECT_VAR_PROCESS(Scatter)
			EFFECT_VAR_PROCESS(Teleport)
			EFFECT_VAR_PROCESS(Transform)
			;
	}

	AttachEffectData() : EffectData()
	{
		this->AffectBullet = false;
	}

	virtual void Read(INIBufferReader* reader) override
	{
		EffectData::Read(reader);
		Read(reader, "");
	}

	virtual void Read(INIBufferReader* reader, std::string title) override
	{
		Name = reader->Section;


		int duration = reader->Get("Duration", Duration);
		if (duration != 0)
		{
			Duration = duration;

			EffectData::Read(reader, title);

			ReadEffects(reader);

			Enable = !GetScriptNames().empty();

			HoldDuration = Duration == -1; // 只允许-1为特殊值，否则持续时间必须大于0
			HoldDuration = reader->Get("HoldDuration", HoldDuration);

			Delay = reader->Get("Delay", Delay);
			RandomDelay = reader->Get("RandomDelay", RandomDelay);
			InitialDelay = reader->Get("InitialDelay", InitialDelay);
			InitialRandomDelay = reader->Get("InitialRandomDelay", InitialRandomDelay);

			DiscardOnEntry = reader->Get("DiscardOnEntry", DiscardOnEntry);
			DiscardOnTransform = reader->Get("DiscardOnTransform", DiscardOnTransform);
			DiscardOnUndeploying = reader->Get("DiscardOnUndeploying", DiscardOnUndeploying);
			DiscardOnSelling = reader->Get("DiscardOnSelling", DiscardOnSelling);
			PenetratesIronCurtain = reader->Get("PenetratesIronCurtain", PenetratesIronCurtain);
			FromTransporter = reader->Get("FromTransporter", FromTransporter);
			ReceiverOwn = reader->Get("ReceiverOwn", ReceiverOwn);
			TreatSellingAsDeath = reader->Get("TreatSellingAsDeath", TreatSellingAsDeath);
			StartAfterBuildup = reader->Get("StartAfterBuildup", StartAfterBuildup);

			Cumulative = reader->Get("Cumulative", Cumulative);
			MaxStack = reader->Get("MaxStack", MaxStack);
			ResetDurationOnReapply = reader->Get("ResetDurationOnReapply", ResetDurationOnReapply);
			Group = reader->Get("Group", Group);
			OverrideSameGroup = reader->Get("OverrideSameGroup", OverrideSameGroup);
			Next = reader->GetList("Next", Next);

			AttachWithOutTypes = reader->GetList("AttachWithOutTypes", AttachWithOutTypes);

			AttachOnceInTechnoType = reader->Get("AttachOnceInTechnoType", AttachOnceInTechnoType);
			AttachNeedMoney = reader->Get("AttachNeedMoney", AttachNeedMoney);
			ReceiverNeedMoney = reader->Get("ReceiverNeedMoney", ReceiverNeedMoney);
			Inheritable = reader->Get("Inheritable", Inheritable);
		}
	}

	bool HasContradiction(std::vector<std::string> AENames)
	{
		bool has = !AttachWithOutTypes.empty() && !AENames.empty();
		if (has)
		{
			has = CheckOnMarks(AttachWithOutTypes, AENames);
		}
		return has;
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		stream
			.Process(this->Name)

			.Process(this->Duration)
			.Process(this->HoldDuration)

			.Process(this->Delay)
			.Process(this->RandomDelay)
			.Process(this->InitialDelay)
			.Process(this->InitialRandomDelay)

			.Process(this->DiscardOnEntry)
			.Process(this->DiscardOnTransform)
			.Process(this->DiscardOnUndeploying)
			.Process(this->DiscardOnSelling)
			.Process(this->PenetratesIronCurtain)
			.Process(this->FromTransporter)
			.Process(this->ReceiverOwn)
			.Process(this->TreatSellingAsDeath)
			.Process(this->StartAfterBuildup)

			.Process(this->Cumulative)
			.Process(this->MaxStack)
			.Process(this->ResetDurationOnReapply)
			.Process(this->Group)
			.Process(this->OverrideSameGroup)
			.Process(this->Next)

			.Process(this->AttachWithOutTypes)
			.Process(this->AttachOnceInTechnoType)
			.Process(this->AttachNeedMoney)
			.Process(this->ReceiverNeedMoney)
			.Process(this->Inheritable);
		ProcessEffects(stream);
		return stream.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		EffectData::Load(stream, registerForChange);
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		EffectData::Save(stream);
		return const_cast<AttachEffectData*>(this)->Serialize(stream);
	}
#pragma endregion

	int GetDuration()
	{
		return this->HoldDuration ? -1 : this->Duration;
	}

};


