#include "SyncLogging.h"
#include <Helpers/Macro.h>
#include <Utilities/Debug.h>

#include <InfantryClass.h>
#include <BuildingClass.h>
#include <BulletClass.h>
#include <CellClass.h>
#include <AircraftClass.h>
#include <FootClass.h>
#include <UnitClass.h>
#include <TechnoClass.h>
#include <Dir.h>
#include <Unsorted.h>
#include <HouseClass.h>

bool SyncLogger::Enabled = true;

SyncLogEventBuffer<RNGCallSyncLogEvent, RNGCalls_Size> SyncLogger::RNGCalls;
SyncLogEventBuffer<FacingChangeSyncLogEvent, FacingChanges_Size> SyncLogger::FacingChanges;
SyncLogEventBuffer<TargetChangeSyncLogEvent, TargetChanges_Size> SyncLogger::TargetChanges;
SyncLogEventBuffer<TargetChangeSyncLogEvent, DestinationChanges_Size> SyncLogger::DestinationChanges;
SyncLogEventBuffer<MissionOverrideSyncLogEvent, MissionOverrides_Size> SyncLogger::MissionOverrides;
SyncLogEventBuffer<RangeStatusSyncLogEvent, RangeStatus_Size> SyncLogger::RangeStatus;

void SyncLogger::AddRNGCallSyncLogEvent(Randomizer* pRandomizer, int type, unsigned int callerAddress, int min, int max)
{
	if (!Enabled) return;
	if (pRandomizer == &ScenarioClass::Instance->Random)
		SyncLogger::RNGCalls.Add(RNGCallSyncLogEvent(type, true, pRandomizer->Next1, pRandomizer->Next2, callerAddress, Unsorted::CurrentFrame, min, max));
}

void SyncLogger::AddFacingChangeSyncLogEvent(unsigned short facing, unsigned int callerAddress)
{
	if (!Enabled) return;
	SyncLogger::FacingChanges.Add(FacingChangeSyncLogEvent(facing, callerAddress, Unsorted::CurrentFrame));
}

void SyncLogger::AddTargetChangeSyncLogEvent(AbstractClass* pObject, AbstractClass* pTarget, unsigned int callerAddress)
{
	if (!Enabled) return;
	if (!pObject)
		return;

	auto targetRTTI = AbstractType::None;
	unsigned int targetID = 0;

	if (pTarget)
	{
		targetRTTI = pTarget->WhatAmI();
		targetID = pTarget->UniqueID;
	}

	SyncLogger::TargetChanges.Add(TargetChangeSyncLogEvent(pObject->WhatAmI(), pObject->UniqueID, targetRTTI, targetID, callerAddress, Unsorted::CurrentFrame));
}

void SyncLogger::AddDestinationChangeSyncLogEvent(AbstractClass* pObject, AbstractClass* pTarget, unsigned int callerAddress)
{
	if (!Enabled) return;
	if (!pObject)
		return;

	auto targetRTTI = AbstractType::None;
	unsigned int targetID = 0;

	if (pTarget)
	{
		targetRTTI = pTarget->WhatAmI();
		targetID = pTarget->UniqueID;
	}

	SyncLogger::DestinationChanges.Add(TargetChangeSyncLogEvent(pObject->WhatAmI(), pObject->UniqueID, targetRTTI, targetID, callerAddress, Unsorted::CurrentFrame));
}

void SyncLogger::AddMissionOverrideSyncLogEvent(AbstractClass* pObject, int mission, unsigned int callerAddress)
{
	if (!Enabled) return;
	SyncLogger::MissionOverrides.Add(MissionOverrideSyncLogEvent(pObject->WhatAmI(), pObject->UniqueID, mission, callerAddress, Unsorted::CurrentFrame));
}

void SyncLogger::AddRangeStatusSyncLogEvent(AbstractClass* pObject, int baseRange, double rangeMultiplier, int rangeCell, int finalRange, AbstractClass* pTarget, unsigned int callerAddress)
{
	if (!Enabled) return;
	if (!pObject) return;

	auto targetRTTI = AbstractType::None;
	unsigned int targetID = 0;
	if (pTarget)
	{
		targetRTTI = pTarget->WhatAmI();
		targetID = pTarget->UniqueID;
	}

	SyncLogger::RangeStatus.Add(RangeStatusSyncLogEvent(pObject->WhatAmI(), pObject->UniqueID, baseRange, rangeMultiplier, rangeCell, finalRange, targetRTTI, targetID, callerAddress, Unsorted::CurrentFrame));
}

void SyncLogger::WriteSyncLog(const char* logFilename)
{
	auto const pLogFile = fopen(logFilename, "at");

	if (pLogFile)
	{
		Debug::Log("Writing to sync log file '%s'.\n", logFilename);

		fprintf(pLogFile, "\nKratos synchronization log:\n\n");

		int frameDigits = 5;

		WriteRNGCalls(pLogFile, frameDigits);
		WriteFacingChanges(pLogFile, frameDigits);
		WriteTargetChanges(pLogFile, frameDigits);
		WriteDestinationChanges(pLogFile, frameDigits);
		WriteMissionOverrides(pLogFile, frameDigits);
		WriteRangeStatus(pLogFile, frameDigits);
		WriteBuildings(pLogFile);

		fclose(pLogFile);
	}
	else
	{
		Debug::Log("Failed to open sync log file '%s'.\n", logFilename);
	}
}

void SyncLogger::WriteRNGCalls(FILE* const pLogFile, int frameDigits)
{
	fprintf(pLogFile, "RNG Calls:\n");

	for (size_t i = 0; i < SyncLogger::RNGCalls.Size(); i++)
	{
		auto const& rngCall = SyncLogger::RNGCalls.Get();

		if (!rngCall.Initialized)
			continue;

		if (rngCall.Type == 1)
		{
			fprintf(pLogFile, "#%05d: Single | Caller: %08x | Frame: %*d | Index1: %3d | Index2: %3d\n",
				i, rngCall.Caller, frameDigits, rngCall.Frame, rngCall.Index1, rngCall.Index2);
		}
		else if (rngCall.Type == 2)
		{
			fprintf(pLogFile, "#%05d: Ranged | Caller: %08x | Frame: %*d | Index1: %3d | Index2: %3d | Min: %d | Max: %d\n",
				i, rngCall.Caller, frameDigits, rngCall.Frame, rngCall.Index1, rngCall.Index2, rngCall.Min, rngCall.Max);
		}
	}

	fprintf(pLogFile, "\n");
}

void SyncLogger::WriteFacingChanges(FILE* const pLogFile, int frameDigits)
{
	fprintf(pLogFile, "Facing changes:\n");

	for (size_t i = 0; i < SyncLogger::FacingChanges.Size(); i++)
	{
		auto const& facingChange = SyncLogger::FacingChanges.Get();

		if (!facingChange.Initialized)
			continue;

		fprintf(pLogFile, "#%05d: Facing: %5d | Caller: %08x | Frame: %*d\n",
			i, facingChange.Facing, facingChange.Caller, frameDigits, facingChange.Frame);
	}

	fprintf(pLogFile, "\n");
}

void SyncLogger::WriteTargetChanges(FILE* const pLogFile, int frameDigits)
{
	fprintf(pLogFile, "Target changes:\n");

	for (size_t i = 0; i < SyncLogger::TargetChanges.Size(); i++)
	{
		auto const& targetChange = SyncLogger::TargetChanges.Get();

		if (!targetChange.Initialized)
			continue;

		fprintf(pLogFile, "#%05d: RTTI: %02d | ID: %08d | TargetRTTI: %02d | TargetID: %08d | Caller: %08x | Frame: %*d\n",
			i, targetChange.Type, targetChange.ID, targetChange.TargetType, targetChange.TargetID, targetChange.Caller, frameDigits, targetChange.Frame);
	}

	fprintf(pLogFile, "\n");
}

void SyncLogger::WriteDestinationChanges(FILE* const pLogFile, int frameDigits)
{
	fprintf(pLogFile, "Destination changes:\n");

	for (size_t i = 0; i < SyncLogger::DestinationChanges.Size(); i++)
	{
		auto const& destChange = SyncLogger::DestinationChanges.Get();

		if (!destChange.Initialized)
			continue;

		fprintf(pLogFile, "#%05d: RTTI: %02d | ID: %08d | TargetRTTI: %02d | TargetID: %08d | Caller: %08x | Frame: %*d\n",
			i, destChange.Type, destChange.ID, destChange.TargetType, destChange.TargetID, destChange.Caller, frameDigits, destChange.Frame);
	}

	fprintf(pLogFile, "\n");
}

void SyncLogger::WriteMissionOverrides(FILE* const pLogFile, int frameDigits)
{
	fprintf(pLogFile, "Mission overrides:\n");

	for (size_t i = 0; i < SyncLogger::MissionOverrides.Size(); i++)
	{
		auto const& missionOverride = SyncLogger::MissionOverrides.Get();

		if (!missionOverride.Initialized)
			continue;

		fprintf(pLogFile, "#%05d: RTTI: %02d | ID: %08d | Mission: %02d | Caller: %08x | Frame: %*d\n",
			i, missionOverride.Type, missionOverride.ID, missionOverride.Mission, missionOverride.Caller, frameDigits, missionOverride.Frame);
	}

	fprintf(pLogFile, "\n");
}

// === Hooks ===

// Sync file writing — called when desync is detected

DEFINE_HOOK(0x64736D, Queue_AI_WriteDesyncLog, 0x5)
{
	GET(const int, frame, ECX);

	char logFilename[0x40];
	_snprintf_s(logFilename, _TRUNCATE, "SYNC%01d.TXT", HouseClass::CurrentPlayer->ArrayIndex);

	SyncLogger::WriteSyncLog(logFilename);

	// Replace overridden instructions.
	CALL(0x6BEC60);

	return 0x647372;
}

DEFINE_HOOK(0x64CD11, ExecuteDoList_WriteDesyncLog, 0x8)
{
	char logFilename[0x40];
	_snprintf_s(logFilename, _TRUNCATE, "SYNC%01d.TXT", HouseClass::CurrentPlayer->ArrayIndex);
	SyncLogger::WriteSyncLog(logFilename);

	return 0;
}

// RNG call logging

DEFINE_HOOK(0x65C7D0, Random2Class_Random_SyncLog, 0x1)
{
	GET(Randomizer*, pThis, ECX);
	GET_STACK(const unsigned int, callerAddress, 0x0);

	SyncLogger::AddRNGCallSyncLogEvent(pThis, 1, callerAddress);

	return 0;
}

DEFINE_HOOK(0x65C88A, Random2Class_RandomRanged_SyncLog, 0x3)
{
	GET(Randomizer*, pThis, EDX);
	GET_STACK(const unsigned int, callerAddress, 0x0);
	GET_STACK(const int, min, 0x4);
	GET_STACK(const int, max, 0x8);

	SyncLogger::AddRNGCallSyncLogEvent(pThis, 2, callerAddress, min, max);

	return 0;
}

// Facing change logging

DEFINE_HOOK(0x4C9300, FacingClass_Set_SyncLog, 0x5)
{
	GET_STACK(DirStruct*, facing, 0x4);
	GET_STACK(const unsigned int, callerAddress, 0x0);

	SyncLogger::AddFacingChangeSyncLogEvent(facing->Raw, callerAddress);

	return 0;
}

// Target change logging

DEFINE_HOOK(0x51B1F0, InfantryClass_AssignTarget_SyncLog, 0x5)
{
	GET(InfantryClass*, pThis, ECX);
	GET_STACK(AbstractClass*, pTarget, 0x4);
	GET_STACK(const unsigned int, callerAddress, 0x0);

	SyncLogger::AddTargetChangeSyncLogEvent(pThis, pTarget, callerAddress);

	return 0;
}

DEFINE_HOOK(0x443B90, BuildingClass_AssignTarget_SyncLog, 0xB)
{
	GET(BuildingClass*, pThis, ECX);
	GET_STACK(AbstractClass*, pTarget, 0x4);
	GET_STACK(const unsigned int, callerAddress, 0x0);

	SyncLogger::AddTargetChangeSyncLogEvent(pThis, pTarget, callerAddress);

	return 0;
}

DEFINE_HOOK(0x6FCDB0, TechnoClass_AssignTarget_SyncLog, 0x5)
{
	GET(TechnoClass*, pThis, ECX);
	GET_STACK(AbstractClass*, pTarget, 0x4);
	GET_STACK(const unsigned int, callerAddress, 0x0);

	auto const RTTI = pThis->WhatAmI();

	if (RTTI != AbstractType::Building && RTTI != AbstractType::Infantry)
		SyncLogger::AddTargetChangeSyncLogEvent(pThis, pTarget, callerAddress);

	return 0;
}

// Destination change logging

DEFINE_HOOK(0x41AA80, AircraftClass_AssignDestination_SyncLog, 0x7)
{
	GET(AircraftClass*, pThis, ECX);
	GET_STACK(AbstractClass*, pDest, 0x4);
	GET_STACK(const unsigned int, callerAddress, 0x0);

	SyncLogger::AddDestinationChangeSyncLogEvent(pThis, pDest, callerAddress);

	return 0;
}

DEFINE_HOOK(0x455D50, BuildingClass_AssignDestination_SyncLog, 0xA)
{
	GET(BuildingClass*, pThis, ECX);
	GET_STACK(AbstractClass*, pDest, 0x4);
	GET_STACK(const unsigned int, callerAddress, 0x0);

	SyncLogger::AddDestinationChangeSyncLogEvent(pThis, pDest, callerAddress);

	return 0;
}

DEFINE_HOOK(0x51AA40, InfantryClass_AssignDestination_SyncLog, 0x5)
{
	GET(InfantryClass*, pThis, ECX);
	GET_STACK(AbstractClass*, pDest, 0x4);
	GET_STACK(const unsigned int, callerAddress, 0x0);

	SyncLogger::AddDestinationChangeSyncLogEvent(pThis, pDest, callerAddress);

	return 0;
}

DEFINE_HOOK(0x741970, UnitClass_AssignDestination_SyncLog, 0x6)
{
	GET(UnitClass*, pThis, ECX);
	GET_STACK(AbstractClass*, pDest, 0x4);
	GET_STACK(const unsigned int, callerAddress, 0x0);

	SyncLogger::AddDestinationChangeSyncLogEvent(pThis, pDest, callerAddress);

	return 0;
}

// Mission override logging

DEFINE_HOOK(0x41BB30, AircraftClass_OverrideMission_SyncLog, 0x6)
{
	GET(AircraftClass*, pThis, ECX);
	GET_STACK(const int, mission, 0x4);
	GET_STACK(const unsigned int, callerAddress, 0x0);

	SyncLogger::AddMissionOverrideSyncLogEvent(pThis, mission, callerAddress);

	return 0;
}

DEFINE_HOOK(0x4D8F40, FootClass_OverrideMission_SyncLog, 0x5)
{
	GET(FootClass*, pThis, ECX);
	GET_STACK(const int, mission, 0x4);
	GET_STACK(const unsigned int, callerAddress, 0x0);

	SyncLogger::AddMissionOverrideSyncLogEvent(pThis, mission, callerAddress);

	return 0;
}

DEFINE_HOOK(0x7013A0, TechnoClass_OverrideMission_SyncLog, 0x5)
{
	GET(TechnoClass*, pThis, ECX);
	GET_STACK(const int, mission, 0x4);
	GET_STACK(const unsigned int, callerAddress, 0x0);

	if (pThis->WhatAmI() == AbstractType::Building)
		SyncLogger::AddMissionOverrideSyncLogEvent(pThis, mission, callerAddress);

	return 0;
}

// TechnoClass::Fire logging
DEFINE_HOOK(0x6FF08B, TechnoClass_Fire_SyncLog, 0x6)
{
	GET(BulletClass*, pBullet, EBX);

	if (SyncLogger::Enabled && pBullet)
	{
		TechnoClass* pOwner = pBullet->Owner;
		AbstractClass* pTarget = pBullet->Target;
		if (pOwner)
		{
			SyncLogger::AddTargetChangeSyncLogEvent(pOwner, pTarget,
				reinterpret_cast<unsigned int>(_ReturnAddress()));
		}
	}

	return 0;
}

// TechnoClass::SelectAutoTarget logging
DEFINE_HOOK(0x6F9B7E, TechnoClass_SelectAutoTarget_SyncLog, 0x5)
{
	if (SyncLogger::Enabled)
	{
		GET(TechnoClass*, pThis, ESI);
		GET(CellClass*, pTarget, EAX);

		if (pThis && pTarget)
		{
			SyncLogger::AddTargetChangeSyncLogEvent(pThis, pTarget,
				reinterpret_cast<unsigned int>(_ReturnAddress()));
		}
	}

	return 0;
}

void SyncLogger::WriteRangeStatus(FILE* const pLogFile, int frameDigits)
{
	fprintf(pLogFile, "Range status (target selection):\n");

	for (size_t i = 0; i < SyncLogger::RangeStatus.Size(); i++)
	{
		auto const& rangeStatus = SyncLogger::RangeStatus.Get();

		if (!rangeStatus.Initialized)
			continue;

		fprintf(pLogFile, "#%05d: RTTI: %02d | ID: %08d | BaseRange: %d | AE.RangeMult: %.6f | AE.RangeCell: %d | FinalRange: %d | TargetRTTI: %02d | TargetID: %08d | Caller: %08x | Frame: %*d\n",
			i, rangeStatus.Type, rangeStatus.ID, rangeStatus.BaseRange, rangeStatus.RangeMultiplier, rangeStatus.RangeCell, rangeStatus.FinalRange, rangeStatus.TargetRTTI, rangeStatus.TargetID, rangeStatus.Caller, frameDigits, rangeStatus.Frame);
	}

	fprintf(pLogFile, "\n");
}

void SyncLogger::WriteBuildings(FILE* const pLogFile)
{
	fprintf(pLogFile, "Buildings full state:\n");

	auto const pArray = BuildingClass::Array.get();
	for (int i = 0; i < pArray->Count; i++)
	{
		auto const pBld = pArray->GetItem(i);
		if (!pBld) continue;

		auto const pType = pBld->GetTechnoType();
		auto const loc = pBld->GetCoords();
		auto const facing = pBld->PrimaryFacing.Current();
		auto const turretFacing = pBld->TurretFacing().Current();

		fprintf(pLogFile, "#%04d: Type=%s (%d) Coords=(%d,%d,%d) Health=%d/%d Mission=%d Facing=%d TurretFacing=%d\n",
			i,
			pType ? pType->ID : "NULL",
			pType ? pType->GetArrayIndex() : -1,
			loc.X, loc.Y, loc.Z,
			pBld->Health, pType ? pType->Strength : 0,
			(int)pBld->CurrentMission,
			facing.GetValue<8>(),
			turretFacing.GetValue<8>());

		fprintf(pLogFile, "  Owner=%d Limbo=%d Ammo=%d",
			pBld->Owner ? pBld->Owner->ArrayIndex : -1,
			pBld->InLimbo,
			pBld->Ammo);

		fprintf(pLogFile, " PrimaryWeapon=%s SecondaryWeapon=%s",
			pBld->GetWeapon(0) && pBld->GetWeapon(0)->WeaponType ? pBld->GetWeapon(0)->WeaponType->ID : "NULL",
			pBld->GetWeapon(1) && pBld->GetWeapon(1)->WeaponType ? pBld->GetWeapon(1)->WeaponType->ID : "NULL");

		fprintf(pLogFile, " UpgradeLevel=%d\n", pBld->UpgradeLevel);
	}

	fprintf(pLogFile, "\n");
}

