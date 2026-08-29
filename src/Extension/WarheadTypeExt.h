#pragma once
#include <string>
#include <vector>
#include <stack>

#include <Conversions.h>
#include <GeneralDefinitions.h>
#include <GeneralStructures.h>
#include <WarheadTypeClass.h>
#include <CCINIClass.h>

#include <Utilities/Enum.h>

#include <Common/INI/INI.h>

#include <Ext/Helper/StringEx.h>
#include <Ext/Helper/Scripts.h>

#include <Ext/StateType/State/DamageReactionData.h>

#include "TypeExtension.h"

enum class ExpLevel
{
	None = 0, Rookie = 1, Veteran = 2, Elite = 3
};

template <>
inline bool Parser<ExpLevel>::TryParse(const char* pValue, ExpLevel* outValue)
{
	switch (toupper(static_cast<unsigned char>(*pValue)))
	{
	case 'R':
		if (outValue)
		{
			*outValue = ExpLevel::Rookie;
		}
		return true;
	case 'V':
		if (outValue)
		{
			*outValue = ExpLevel::Veteran;
		}
		return true;
	case 'E':
		if (outValue)
		{
			*outValue = ExpLevel::Elite;
		}
		return true;
	default:
		if (outValue)
		{
			*outValue = ExpLevel::None;
		}
		return true;
	}
}

/*
enum class Armor : unsigned int
{
	None = 0,
	Flak = 1,
	Plate = 2,
	Light = 3,
	Medium = 4,
	Heavy = 5,
	Wood = 6,
	Steel = 7,
	Concrete = 8,
	Special_1 = 9,
	Special_2 = 10
};*/

static std::map<std::string, Armor> ArmorTypeStrings
{
	{ "none", Armor::None },
	{ "flak", Armor::Flak },
	{ "plate", Armor::Plate },
	{ "light", Armor::Light },
	{ "medium", Armor::Medium },
	{ "heavy", Armor::Heavy },
	{ "wood", Armor::Wood },
	{ "steel", Armor::Steel },
	{ "concrete", Armor::Concrete },
	{ "special_1", Armor::Special_1 },
	{ "special_2", Armor::Special_2 }
};

struct AresVersus : public WarheadFlags
{
public:
	AresVersus(double versus = 1.0, bool forceFire = true, bool retaliate = true, bool passiveAcquire = true) : Versus(versus), WarheadFlags(forceFire, retaliate, passiveAcquire) {}

	bool operator ==(const AresVersus& RHS) const {
		return (CLOSE_ENOUGH(this->Versus, RHS.Versus));
	}

	void Parse(const char* str) {
		this->Versus = Conversions::Str2Armor(str, this);
	}

	void Read(INIBufferReader* reader, std::string title, std::string armor)
	{
		if (reader->TryGet(title + armor, Versus))
		{
			PassiveAcquire = !LESS_EQUAL(Versus, 0.02);
			Retaliate = !LESS_EQUAL(Versus, 0.01);
			ForceFire = !LESS_EQUAL(Versus, 0.00);
		}
		ForceFire = reader->Get(title + armor + ".ForceFire", ForceFire);
		Retaliate = reader->Get(title + armor + ".Retaliate", Retaliate);
		PassiveAcquire = reader->Get(title + armor + ".PassiveAcquire", PassiveAcquire);
	}

	double Versus = 1.0;
};

class WarheadTypeExt : public TypeExtension<WarheadTypeClass, WarheadTypeExt>
{
public:
	/// @brief 储存一些通用设置或者其他平台的设置
	class TypeData : public INIConfig
	{
	public:
		// YR
		std::vector<double> Versus{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }; // YRPP获取的比例全是1，只能自己维护
		// Ares
		std::vector<AresVersus> AresVersusArray{}; // Ares自定义护甲的参数

		bool AffectsOwner = true;
		bool AffectsAllies = true;
		bool AffectsEnemies = true;
		bool EffectsRequireDamage = false;
		bool EffectsRequireVerses = true;
		bool AllowZeroDamage = false;
		std::string PreImpactAnim{ "" };

		// Phobos
		std::vector<std::string> SplashList{}; // 水花动画列表
		bool SplashListRandom = false; // 水花动画列表是否随机
		AffectedTarget AirstrikeTargets = AffectedTarget::Building; // 空袭目标类型

		// Kratos
		bool AffectInAir = true;
		bool AffectShooter = true;
		bool AffectStand = false;

		int MaxAttachTechno = -1;
		int MaxAttachBullet = -1;

		bool ClearTarget = false;
		bool ClearDisguise = false;

		Mission ForceMission = Mission::None;

		int ExpCost = 0;
		ExpLevel ExpLevel = ExpLevel::None;

		int ShowMeTheMoney = 0;
		bool ShowMeTheMoneyDisplay = false;

		bool IsToy = false; // 玩具弹头

		bool Lueluelue = false; // 欠揍弹头

		bool Teleporter = false; // 弹头传送标记
		bool Capturer = false; // 弹头黑洞捕获标记
		bool IgnoreRevenge = false; // 不触发复仇
		bool IgnoreDamageReaction = false; // 不触发伤害响应
		std::vector<DamageReactionMode> IgnoreDamageReactionModes{}; // 不触发特定的伤害响应类型
		bool IgnoreStandShareDamage = false; // 替身不分担伤害
		bool IgnoreCounterReaction = false; // 不触发计数器响应

		virtual void Read(INIBufferReader* reader) override
		{
			Versus = reader->GetList("Verses", Versus);
			// Ares
			ReadAresVersus(reader);
			bool affectsAllies = true;
			if (reader->TryGet("AffectsAllies", affectsAllies))
			{
				AffectsAllies = affectsAllies;
				AffectsOwner = affectsAllies;
			}
			bool affectsOwner = AffectsOwner;
			if (reader->TryGet("AffectsOwner", affectsOwner))
			{
				AffectsOwner = affectsOwner;
			}
			AffectsEnemies = reader->Get("AffectsEnemies", AffectsEnemies);
			EffectsRequireDamage = reader->Get("EffectsRequireDamage", EffectsRequireDamage);
			EffectsRequireVerses = reader->Get("EffectsRequireVerses", EffectsRequireVerses);
			AllowZeroDamage = reader->Get("AllowZeroDamage", AllowZeroDamage);
			PreImpactAnim = reader->Get("PreImpactAnim", PreImpactAnim);

			// Phobos
			SplashList = reader->GetList("SplashList", SplashList);
			SplashListRandom = reader->Get("SplashList.PickRandom", SplashListRandom);
			AirstrikeTargets = reader->Get("AirstrikeTargets", AirstrikeTargets);

			// Kratos
			AffectInAir = reader->Get("AffectInAir", AffectInAir);
			AffectShooter = reader->Get("AffectShooter", AffectShooter);
			AffectStand = reader->Get("AffectStand", AffectStand);

			MaxAttachTechno = reader->Get("CellSpread.MaxAttachTechno", MaxAttachTechno);
			MaxAttachBullet = reader->Get("CellSpread.MaxAttachBullet", MaxAttachBullet);

			ClearTarget = reader->Get("ClearTarget", ClearTarget);
			ClearDisguise = reader->Get("ClearDisguise", ClearDisguise);

			ForceMission = reader->Get("ForceMission", ForceMission);

			ExpCost = reader->Get("ExpCost", ExpCost);
			ExpLevel = reader->Get("ExpLevel", ExpLevel);

			ShowMeTheMoney = reader->Get("ShowMeTheMoney", ShowMeTheMoney);
			ShowMeTheMoneyDisplay = reader->Get("ShowMeTheMoneyDisplay", ShowMeTheMoneyDisplay);

			IsToy = reader->Get("IsToy", IsToy);

			Lueluelue = reader->Get("Lueluelue", Lueluelue);

			Teleporter = reader->Get("Teleporter", Teleporter);
			Capturer = reader->Get("Capturer", Capturer);
			IgnoreRevenge = reader->Get("IgnoreRevenge", IgnoreRevenge);
			IgnoreDamageReaction = reader->Get("IgnoreDamageReaction", IgnoreDamageReaction);
			IgnoreDamageReactionModes = reader->GetList("IgnoreDamageReaction.Modes", IgnoreDamageReactionModes);
			if (!IgnoreDamageReactionModes.empty())
			{
				IgnoreDamageReaction = true;
			}
			IgnoreStandShareDamage = reader->Get("IgnoreStandShareDamage", IgnoreStandShareDamage);
			IgnoreCounterReaction = reader->Get("IgnoreCounterReaction", IgnoreCounterReaction);
		}

		double GetVersus(Armor armor, bool& forceFire, bool& retaliate, bool& passiveAcquire)
		{
			double versus = 1;
			forceFire = true;
			retaliate = true;
			passiveAcquire = true;

			int index = static_cast<int>(armor);
			if (index >= 0)
			{
				int size = AresVersusArray.size();
				if (index < 11)
				{
					// 原始护甲
					versus = Versus[index];
					forceFire = versus > 0.0;
					retaliate = versus > 0.1;
					passiveAcquire = versus > 0.2;
				}
				else
				{
					index -= 11;
					if (index < size)
					{
						// 扩展护甲
						AresVersus aresVersus = AresVersusArray[index];
						versus = aresVersus.Versus;
						forceFire = aresVersus.ForceFire;
						retaliate = aresVersus.Retaliate;
						passiveAcquire = aresVersus.PassiveAcquire;
					}
				}
			}
			return versus;
		}

		static std::string GetArmorName(Armor armor)
		{
			int armorIndex = (int)armor;
			std::string armorName = "unknown";
			if (armorIndex < 11)
			{
				// 默认护甲
				for (auto it : ArmorTypeStrings)
				{
					if (it.second == armor)
					{
						armorName = it.first;
						break;
					}
				}
			}
			else
			{
				// 自定义护甲
				auto array = GetAresArmorArray();
				if ((armorIndex -= 11) < (int)array.size())
				{
					auto it = array.begin();
					if (armorIndex > 0)
					{
						std::advance(it, armorIndex);
					}
					if (it != array.end())
					{
						armorName = it->first;
					}
				}
			}
			return armorName;
		}

	private:
		/**
		 *@brief 读取弹头上的自定义护甲的设置
		 *
		 * @param reader
		 */
		void ReadAresVersus(INIBufferReader* reader)
		{
			std::vector<std::pair<std::string, AresVersus>> armorValues = GetAresArmorValueArray();
			std::vector<std::pair<std::string, std::string>> armorArray = GetAresArmorArray();

			AresVersusArray.clear();
			AresVersusArray.resize(armorValues.size());

			// 为每个护甲读取值
			for (size_t i = 0; i < armorValues.size(); ++i)
			{
				const std::string& armorName = armorValues[i].first;
				AresVersus& aresVersus = AresVersusArray[i];

				// 尝试从INI读取
				std::string key = "Versus." + armorName;
				if (reader->TryGet(key, aresVersus.Versus))
				{
					// 有明确设置值
					aresVersus.PassiveAcquire = !LESS_EQUAL(aresVersus.Versus, 0.02);
					aresVersus.Retaliate = !LESS_EQUAL(aresVersus.Versus, 0.01);
					aresVersus.ForceFire = !LESS_EQUAL(aresVersus.Versus, 0.00);
				}
				else
				{
					// 没有明确设置值，需要根据引用关系确定
					const std::string& targetName = armorArray[i].second;

					// 检查引用的是什么
					int defaultIndex = 0;
					if (IsDefaultArmor(targetName, defaultIndex))
					{
						// 引用默认护甲，使用弹头对默认护甲的值
						if (defaultIndex >= 0 && defaultIndex < (int)Versus.size())
						{
							aresVersus.Versus = Versus[defaultIndex];
						}
						else
						{
							aresVersus.Versus = 1.0;
						}
					}
					else
					{
						// 引用其他自定义护甲，查找那个护甲的值
						bool found = false;
						for (size_t j = 0; j < armorArray.size(); ++j)
						{
							if (armorArray[j].first == targetName)
							{
								aresVersus = AresVersusArray[j]; // 继承值
								found = true;
								break;
							}
						}
						if (!found)
						{
							aresVersus.Versus = 1.0;
						}
					}

					// 设置标志位
					aresVersus.PassiveAcquire = !LESS_EQUAL(aresVersus.Versus, 0.02);
					aresVersus.Retaliate = !LESS_EQUAL(aresVersus.Versus, 0.01);
					aresVersus.ForceFire = !LESS_EQUAL(aresVersus.Versus, 0.00);
				}

				// 读取覆盖标志
				aresVersus.ForceFire = reader->Get("Versus." + armorName + ".ForceFire", aresVersus.ForceFire);
				aresVersus.Retaliate = reader->Get("Versus." + armorName + ".Retaliate", aresVersus.Retaliate);
				aresVersus.PassiveAcquire = reader->Get("Versus." + armorName + ".PassiveAcquire", aresVersus.PassiveAcquire);
			}
// #ifdef DEBUG
// 			// 输出调试信息
// 			Debug::Log("%s Versus values:\n", reader->Section.c_str());
// 			for (size_t i = 0; i < Versus.size(); ++i)
// 			{
// 				Armor armor = (Armor)i;
// 				const std::string& name = GetArmorName(armor);
// 				Debug::Log("  %d - %s: %.2f\n", i, name.c_str(), Versus[i]);
// 			}
// 			Debug::Log("%s AresVersus values:\n", reader->Section.c_str());
// 			for (size_t i = 0; i < armorValues.size(); ++i)
// 			{
// 				const std::string& name = armorValues[i].first;
// 				const AresVersus& value = AresVersusArray[i];
// 				Debug::Log("  %d - %s: %.2f, %d, %d, %d\n",
// 					i + 11, name.c_str(), value.Versus, value.ForceFire, value.Retaliate, value.PassiveAcquire);
// 			}
// #endif // DEBUG
		}
	};

	static void ReadAresArmorTypes()
	{
		GetAresArmorValueArray();
	}

	static constexpr DWORD Canary = 0x44444444;
	// static constexpr size_t ExtPointerOffset = 0x18;

	static WarheadTypeExt::ExtContainer ExtMap;

private:
	/**
	 *@brief 检查是否是游戏默认的护甲，并返回护甲的序号
	 *
	 * @param armor 待检查的护甲
	 * @param index 护甲的序号
	 * @return true
	 * @return false
	 */
	static bool IsDefaultArmor(std::string armor, int& index);

	/**
	 *@brief 迭代查找嵌套护甲的最底层的数值
	 *
	 * @param key
	 * @param array
	 * @return std::string
	 */
	static std::string GetArmorValue(std::string key, std::vector<std::pair<std::string, std::string>> array);

	/**
	 *@brief 读取Ares护甲注册表，护甲的对应关系.\n
	 * [ArmorTypes]\n
	 * OOXX = 100%\n
	 * XXOO = OOXX\n
	 * @return std::vector<std::pair<std::string, std::string>>
	 */
	static std::vector<std::pair<std::string, std::string>> GetAresArmorArray();

	/**
	 * @brief 读取整理之后的护甲和比例.\n
	 * [ArmorTypes]\n
	 * OOXX = 100%\n
	 * XXOO = 100%\n
	 *
	 * @return std::vector<std::pair<std::string, AresVersus>>
	 */
	static std::vector<std::pair<std::string, AresVersus>> GetAresArmorValueArray();

	// Ares护甲注册表，护甲的对应关系，OOXX=none，XXOO=OOXX
	static std::vector<std::pair<std::string, std::string>> _aresArmorArray;
	// 读取完所有嵌套护甲对应的百分比默认值，OOXX=100%，XXOO=100%
	static std::vector<std::pair<std::string, AresVersus>> _aresArmorValueArray;


};

static bool HasPreImpactAnim(WarheadTypeClass* pWH)
{
	return IsNotNone(GetTypeData<WarheadTypeExt, WarheadTypeExt::TypeData>(pWH)->PreImpactAnim);
}
