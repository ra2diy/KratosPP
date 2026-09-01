#pragma once

#include <TechnoClass.h>

#include "GOExtension.h"

#include <Ext/Common/PreOccupancyManager.h>

#include <Ext/TechnoType/HealthTextData.h>
#include <Ext/EffectType/Effect/StandData.h>

struct TechnoClassLess
{
	bool operator()(TechnoClass* a, TechnoClass* b) const
	{
		return a->UniqueID < b->UniqueID;
	}
};

class TechnoExt : public GOExtension<TechnoClass, TechnoExt>
{
public:
	static constexpr DWORD Canary = 0x5A5A5A5A;
	// static constexpr size_t ExtPointerOffset = 0x34C;

	/**
	 * @brief 创建Component实例，并加入到GameObject中.
	 * 创建时需要使用new，不能使用GameCreate.
	 * @param globalScripts 待附加的脚本列表
	 * @param ext ExtData
	 */
	static void AddGlobalScripts(std::list<std::string>& globalScripts, ExtData* ext);

	static TechnoExt::ExtContainer ExtMap;

	/**
	 *@brief 在游戏读取时，手动清理缓存的清单。
	 * 在游戏内直接读取游戏时，游戏会先执行OnRemove，
	 * OnRemove会遍历Array移除自身，因而导致游戏在读档时卡住，剩余几个Techno无法继续读档，原因不明。
	 *
	 * @param sender
	 * @param e
	 * @param args
	 */
	static void ClearAllArray(EventSystem* sender, Event e, void* args);

	/**
	 *@brief 储存可以作为基地建造节点的单位.
	 * key = 单位, value = 做友军的基地建造节点
	 */
	static std::map<TechnoClass*, bool, TechnoClassLess> BaseUnitArray;

	/**
	 *@brief 储存可以作为基地建造节点的替身单位.
	 * key = 单位, value = 做友军的基地建造节点
	 */
	static std::map<TechnoClass*, bool, TechnoClassLess> BaseStandArray;

	static std::map<TechnoClass*, StandData, TechnoClassLess> StandArray;
	static std::map<TechnoClass*, StandData, TechnoClassLess> ImmuneStandArray;

	static std::vector<TechnoClass*> VirtualUnitArray;

	static PreOccupancyManager HumanConnonPreOcc;

	/**
	 *@brief 血量数字的全局设置
	 *
	 */
	static HealthTextControlData HealthTextControlData;
};
