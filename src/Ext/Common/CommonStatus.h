#pragma once

#include <Common/INI/INI.h>
#include <Common/INI/INIConfig.h>

class GeneralData : public INIConfig
{
public:
	// YR
	std::vector<std::string> PadAircraft{};
	CoordStruct NoHelipadPutOffset{};
	bool ForcePutOffset = false;

	// Phobos
	bool BuildingWaypoints = false; // 允许建筑使用路径点

	virtual void Read(INIBufferReader* reader) override
	{
		PadAircraft = reader->GetList("PadAircraft", PadAircraft);
		NoHelipadPutOffset = reader->Get("AircraftNoHelipadPutOffset", NoHelipadPutOffset);
		ForcePutOffset = reader->Get("AircraftForcePutOffset", ForcePutOffset);

		BuildingWaypoints = reader->Get("BuildingWaypoints", BuildingWaypoints);
	}
};

class General
{
public:
	static GeneralData* Data()
	{
		if (!_data)
		{
			_data = INI::GetConfig<GeneralData>(INI::Rules, INI::SectionGeneral)->Data;
		}
		return _data;
	};

	// 使惰性缓存失效。ScenarioClearClassesEvent 上 INI::ClearBuffer 会 GameDelete 掉这些
	// INIConfig（Common/INI/INIConfigManager.h:23），缓存指针必须同时清空，否则下一次 Data()
	// 返回悬垂指针（跨局残留 / UAF）。
	static void ClearCache()
	{
		_data = nullptr;
	}

private:
	static GeneralData* _data;
};


class CombatDamageData : public INIConfig
{
public:
	// Ares
	bool AutoRepel = false;
	bool PlayerAutoRepel = false;

	// 动画
	bool AllowAnimDamageTakeOverByKratos = true;
	bool AllowDamageIfDebrisHitWater = true;

	// 替身
	bool AllowAutoPickStandAsTarget = true;
	bool AllowUnitAsBaseNormal = false;
	bool AllowJumpjetAsBaseNormal = false;
	bool AllowStandAsBaseNormal = false;

	// AI
	bool AllowAIAttackFriendlies = false;

	virtual void Read(INIBufferReader* reader) override
	{
		AutoRepel = reader->Get("AutoRepel", AutoRepel);
		PlayerAutoRepel = reader->Get("PlayerAutoRepel", PlayerAutoRepel);

		AllowAnimDamageTakeOverByKratos = reader->Get("AllowAnimDamageTakeOverByKratos", AllowAnimDamageTakeOverByKratos);
		AllowDamageIfDebrisHitWater = reader->Get("AllowDamageIfDebrisHitWater", AllowDamageIfDebrisHitWater);

		AllowAutoPickStandAsTarget = reader->Get("AllowAutoPickStandAsTarget", AllowAutoPickStandAsTarget);
		AllowUnitAsBaseNormal = reader->Get("AllowUnitAsBaseNormal", AllowUnitAsBaseNormal);
		AllowJumpjetAsBaseNormal = reader->Get("AllowJumpjetAsBaseNormal", AllowJumpjetAsBaseNormal);
		AllowStandAsBaseNormal = reader->Get("AllowStandAsBaseNormal", AllowStandAsBaseNormal);

		AllowAIAttackFriendlies = reader->Get("AllowAIAttackFriendlies", AllowAIAttackFriendlies);
	}
};

class CombatDamage
{
public:
	static CombatDamageData* Data()
	{
		if (!_data)
		{
			_data = INI::GetConfig<CombatDamageData>(INI::Rules, INI::SectionCombatDamage)->Data;
		}
		return _data;
	};

	// 见 General::ClearCache
	static void ClearCache()
	{
		_data = nullptr;
	}

private:
	static CombatDamageData* _data;
};

class AudioVisualData : public INIConfig
{
public:
	// Ares
	float DeactivateDimEMP = 0.8f;
	float DeactivateDimPowered = 0.5f;

	// Phobos
	int AirstrikeLineZAdjust = 0;

	// Kratos
	bool AllowMakeVoxelDebrisByKratos = true;

	bool AllowTakeoverPhobosShadowMaker = true;
	float VoxelShadowScaleInAir = 1.0f;

	virtual void Read(INIBufferReader* reader) override
	{
		DeactivateDimEMP = reader->Get("DeactivateDimEMP", DeactivateDimEMP);
		DeactivateDimPowered = reader->Get("DeactivateDimPowered", DeactivateDimPowered);

		AirstrikeLineZAdjust = reader->Get("AirstrikeLineZAdjust", AirstrikeLineZAdjust);

		AllowMakeVoxelDebrisByKratos = reader->Get("AllowMakeVoxelDebrisByKratos", AllowMakeVoxelDebrisByKratos);

		AllowTakeoverPhobosShadowMaker = reader->Get("AllowTakeoverPhobosShadowMaker", AllowTakeoverPhobosShadowMaker);
		VoxelShadowScaleInAir = reader->Get("VoxelShadowScaleInAir", VoxelShadowScaleInAir);
	}

};

class AudioVisual
{
public:
	static AudioVisualData* Data()
	{
		if (!_data)
		{
			_data = INI::GetConfig<AudioVisualData>(INI::Rules, INI::SectionAudioVisual)->Data;
		}
		return _data;
	};

	// 见 General::ClearCache
	static void ClearCache()
	{
		_data = nullptr;
	}

private:
	static AudioVisualData* _data;
};

class AIConfigData : public INIConfig
{
public:
	// Phobos
	int EnablePowerSurplus = 0;

	virtual void Read(INIBufferReader* reader) override
	{
		EnablePowerSurplus = reader->Get("EnablePowerSurplus", EnablePowerSurplus);
	}

};

class AIConfig
{
public:
	static AIConfigData* Data()
	{
		if (!_data)
		{
			_data = INI::GetConfig<AIConfigData>(INI::Rules, INI::SectionAI)->Data;
		}
		return _data;
	};

	// 见 General::ClearCache
	static void ClearCache()
	{
		_data = nullptr;
	}

private:
	static AIConfigData* _data;
};

// ScenarioClearClassesEvent 处理器：清空上述 4 个规则数据的惰性缓存。
// 注册顺序必须排在 INI::ClearBuffer 之后（同一事件按注册顺序执行）。
void ClearCommonStatusCache(EventSystem* sender, Event e, void* args);
