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
}


TechnoExt::ExtContainer TechnoExt::ExtMap{};
std::map<TechnoClass*, bool, TechnoClassLess> TechnoExt::BaseUnitArray{};
std::map<TechnoClass*, bool, TechnoClassLess> TechnoExt::BaseStandArray{};

std::map<TechnoClass*, StandData, TechnoClassLess> TechnoExt::StandArray{};
std::map<TechnoClass*, StandData, TechnoClassLess> TechnoExt::ImmuneStandArray{};
std::vector<TechnoClass*> TechnoExt::VirtualUnitArray{};

PreOccupancyManager TechnoExt::HumanConnonPreOcc{};

HealthTextControlData TechnoExt::HealthTextControlData{};
