#pragma once

#include <TechnoTypeClass.h>

#include "TypeExtension.h"

class TechnoTypeExt : public TypeExtension<TechnoTypeClass, TechnoTypeExt>
{
public:
	/// @brief 储存一些通用设置或者其他平台的设置
	class TypeData : public INIConfig
	{
	public:
		// Ares
		bool AllowCloakable = true;
		bool Carryall = false;
		int CarryallSizeLimit = -1;
		bool CarryallAllowed = true;
		int NoAmmoAmount = 0; // 弹药数量小于等于此值时切换无弹武器
		int NoAmmoWeapon = -1; // 无弹药武器索引

		// Phobos
		CoordStruct TurretOffset = CoordStruct::Empty;
		std::string WarpIn{ "" };
		std::string WarpOut{ "" };
		std::string WarpAway{ "" };
		int ChronoDelay = 60;
		int ChronoDistanceFactor = 32;
		bool ChronoTrigger = true;
		int ChronoMinimumDelay = 0;
		int ChronoRangeMinimum = 0;
		bool AllowAirstrike = true; // 空袭是否允许该类型单位为目标
		bool NoSecondaryWeaponFallback = false; // 禁用二级武器自动切换
		bool NoSecondaryWeaponFallbackForAA = false; // 对防空禁用二级武器自动切换
		bool AllowWeaponSelectAgainstWalls = false; // 允许对墙壁使用二级武器

		// Kratos
		bool SelectWeaponUseRange = false; // 按射程选择武器
		CoordStruct CarryallOffset{ 0, 0, Unsorted::LevelHeight };
		std::string CarryallImage{ "" };
		bool DisableNoFighterFireTwice = false; // 禁用NoFighter飞机开火两次
		bool CurleyShuffle = true; // NoFighter飞机开火后换位置

		virtual void Read(INIBufferReader* reader) override
		{
			// 读取全局设置
			INIBufferReader* cdReader = INI::GetSection(INI::Rules, INI::SectionCombatDamage);

			AllowCloakable = reader->Get("Cloakable.Allowed", AllowCloakable);
			Carryall = reader->Get("Carryall", Carryall);
			CarryallSizeLimit = reader->Get("Carryall.SizeLimit", CarryallSizeLimit);
			CarryallAllowed = reader->Get("Carryall.Allowed", CarryallAllowed);
			NoAmmoAmount = reader->Get("NoAmmoAmount", NoAmmoAmount);
			NoAmmoWeapon = reader->Get("NoAmmoWeapon", NoAmmoWeapon);

			TurretOffset = reader->Get("TurretOffset", TurretOffset);

			WarpIn = reader->Get("WarpIn", WarpIn);
			WarpOut = reader->Get("WarpOut", WarpOut);
			WarpAway = reader->Get("WarpAway", WarpAway);
			// 在游戏开始后再访问，读取全局没问题
			RulesClass* rules = RulesClass::Instance;
			ChronoDelay = reader->Get("ChronoDelay", rules->ChronoDelay);
			ChronoDistanceFactor = reader->Get("ChronoDistanceFactor", rules->ChronoDistanceFactor);
			ChronoTrigger = reader->Get("ChronoTrigger", rules->ChronoTrigger);
			ChronoMinimumDelay = reader->Get("ChronoMinimumDelay", rules->ChronoMinimumDelay);
			ChronoRangeMinimum = reader->Get("ChronoRangeMinimum", rules->ChronoRangeMinimum);
			AllowAirstrike = reader->Get("CanC4", AllowAirstrike); // 默认值与CanC4相同
			AllowAirstrike = reader->Get("AllowAirstrike", AllowAirstrike);
			NoSecondaryWeaponFallback = reader->Get("NoSecondaryWeaponFallback", NoSecondaryWeaponFallback);
			NoSecondaryWeaponFallbackForAA = reader->Get("NoSecondaryWeaponFallback.AllowAA", NoSecondaryWeaponFallbackForAA);
			// INIBufferReader* avReader = INI::GetSection(INI::Rules, INI::SectionAudioVisual);
			INIBufferReader* combatReader = INI::GetSection(INI::Rules, INI::SectionCombatDamage);
			AllowWeaponSelectAgainstWalls = combatReader->Get("AllowWeaponSelectAgainstWalls", AllowWeaponSelectAgainstWalls);
			AllowWeaponSelectAgainstWalls = reader->Get("AllowWeaponSelectAgainstWalls", AllowWeaponSelectAgainstWalls);

			SelectWeaponUseRange = reader->Get("SelectWeaponUseRange", SelectWeaponUseRange);
			CarryallOffset = reader->Get("Carryall.Offset", CarryallOffset);
			CarryallImage = reader->Get("Carryall.Image", CarryallImage);
			DisableNoFighterFireTwice = cdReader->Get("DisableNoFighterFireTwice", DisableNoFighterFireTwice);
			DisableNoFighterFireTwice = reader->Get("DisableNoFighterFireTwice", DisableNoFighterFireTwice);
			CurleyShuffle = reader->Get("CurleyShuffle", rules->CurleyShuffle);
		}
	};

	static constexpr DWORD Canary = 0x33333333;
	// static constexpr size_t ExtPointerOffset = 0xDF4;

	static TechnoTypeExt::ExtContainer ExtMap;
};
