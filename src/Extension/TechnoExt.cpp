#include "TechnoExt.h"

#include <Ext/ObjectType/AttachEffect.h>
#include <Ext/TechnoType/AircraftPut.h>
#include <Ext/TechnoType/TechnoStatus.h>

void TechnoExt::AddGlobalScripts(std::list<std::string>& globalScripts, ExtData* ext)
{
	// Base Component
	globalScripts.push_back(AircraftPut::ScriptName);
	globalScripts.push_back(TechnoStatus::ScriptName);
	globalScripts.push_back(AttachEffect::ScriptName);
}

void TechnoExt::ClearAllArray(EventSystem* sender, Event e, void* args)
{
	BaseUnitArray.clear();
	BaseStandArray.clear();

	StandArray.clear();
	ImmuneStandArray.clear();

	VirtualUnitArray.clear();

	// HumanConnonPreOcc 与上面几个表同类：key 都是 TechnoClass*，读档后旧 sim 的
	// 预占没有意义，且 key 全部悬垂。一起清。
	HumanConnonPreOcc.Clear();
}

void TechnoExt::ClearPreOcc(EventSystem* sender, Event e, void* args)
{
	HumanConnonPreOcc.Clear();
}

void TechnoExt::OnObjectUnInit(EventSystem* sender, Event e, void* args)
{
	// args 是即将销毁的 ObjectClass*（Hooks/PointerExpireHook.cpp:36）。
	// 只有 TechnoClass 进过 m_unitToCell，非 techno 指针查不到即返回。
	HumanConnonPreOcc.ReleaseOccupancy(static_cast<TechnoClass*>(args));
}


TechnoExt::ExtContainer TechnoExt::ExtMap{};
std::map<TechnoClass*, bool, TechnoClassLess> TechnoExt::BaseUnitArray{};
std::map<TechnoClass*, bool, TechnoClassLess> TechnoExt::BaseStandArray{};

std::map<TechnoClass*, StandData, TechnoClassLess> TechnoExt::StandArray{};
std::map<TechnoClass*, StandData, TechnoClassLess> TechnoExt::ImmuneStandArray{};
std::vector<TechnoClass*> TechnoExt::VirtualUnitArray{};

PreOccupancyManager TechnoExt::HumanConnonPreOcc{};

HealthTextControlData TechnoExt::HealthTextControlData{};
