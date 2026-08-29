#include <exception>
#include <Windows.h>

#include <GeneralDefinitions.h>
#include <SpecificStructures.h>
#include <AnimClass.h>
#include <AnimTypeClass.h>
#include <BulletClass.h>
#include <BuildingClass.h>
#include <TechnoClass.h>

#include <Extension.h>
#include <Utilities/Macro.h>
#include <Utilities/SyncLogging.h>

#include <Extension/AnimExt.h>
#include <Extension/BulletExt.h>
#include <Extension/TechnoExt.h>
#include <Extension/WeaponTypeExt.h>

#include <Ext/Helper/Scripts.h>

#include <Ext/BulletType/BulletStatus.h>
#include <Ext/Common/CommonStatus.h>
#include <Ext/Common/ExpandAnimsManager.h>


// ----------------
// Extension
// ----------------


// Allow drawing single color lasers with thickness.
DEFINE_HOOK(0x6FD446, TechnoClass_LaserZap_IsSingleColor, 0x7)
{
	GET(WeaponTypeClass* const, pWeapon, ECX);
	GET(LaserDrawClass* const, pLaser, EAX);

	WeaponTypeExt::TypeData* weaponData = GetTypeData<WeaponTypeExt, WeaponTypeExt::TypeData>(pWeapon);
	if (!pLaser->IsHouseColor && weaponData->IsSingleColor)
	{
		pLaser->IsHouseColor = true;
	}

	// Fixes drawing thick lasers for non-PrismSupport building-fired lasers.
	pLaser->IsSupported = pLaser->Thickness > 3;

	return 0;
}



// Contains hooks that fix weapon graphical effects like lasers, railguns, electric bolts, beams and waves not interacting
// correctly with obstacles between firer and target, as well as railgun / railgun particles being cut off by elevation.

namespace FireAtTemp
{
	BulletClass* FireBullet = nullptr;
	CoordStruct OriginalTargetCoords;
	CellClass* pObstacleCell = nullptr;
	AbstractClass* pOriginalTarget = nullptr;
	AbstractClass* pWaveOwnerTarget = nullptr;
	bool IgnoreTargetForWaveAmbientDamage = false;
}

DEFINE_HOOK(0x6FF08B, TechnoClass_Fire_RecordBullet, 0x6)
{
	GET(BulletClass*, pBullet, EBX);
	FireAtTemp::FireBullet = pBullet;

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

// Set obstacle cell.
DEFINE_HOOK(0x6FF15F, TechnoClass_FireAt_ObstacleCellSet, 0x6)
{
	GET(TechnoClass*, pThis, ESI);
	GET(WeaponTypeClass*, pWeapon, EBX);
	GET_BASE(AbstractClass*, pTarget, 0x8);
	LEA_STACK(CoordStruct*, pSourceCoords, STACK_OFFSET(0xB0, -0x6C));

	const auto pBuilding = abstract_cast<BuildingClass*>(pTarget);
	const auto coords = pBuilding ? pBuilding->GetTargetCoords() : pTarget->GetCenterCoords();

	// This is set to a temp variable as well, as accessing it everywhere needed from TechnoExt would be more complicated.
	FireAtTemp::pObstacleCell = TrajectoryHelper::FindFirstObstacle(*pSourceCoords, coords, pWeapon->Projectile, pThis->Owner);
	// TechnoExt::ExtMap.Find(pThis)->FiringObstacleCell = FireAtTemp::pObstacleCell;

	return 0;
}

DEFINE_HOOK_AGAIN(0x6FD70D, TechnoClass_DrawSth_DrawToInvisoFlakScatterLocation, 0x6) // CreateRBeam
DEFINE_HOOK_AGAIN(0x6FD514, TechnoClass_DrawSth_DrawToInvisoFlakScatterLocation, 0x7) // CreateEBolt
DEFINE_HOOK(0x6FD38D, TechnoClass_DrawSth_DrawToInvisoFlakScatterLocation, 0x7) // CreateLaser
{
	GET(CoordStruct*, pTargetCoords, EAX);

	if (const auto pBullet = FireAtTemp::FireBullet)
	{
		// The weapon may not have been set up
		// const auto pWeaponExt = WeaponTypeExt::ExtMap.TryFind(pBullet->WeaponType);
		WeaponTypeExt::TypeData* weaponData = GetTypeData<WeaponTypeExt, WeaponTypeExt::TypeData>(pBullet->WeaponType);
		if (weaponData && weaponData->VisualScatter)
		{
			const auto radius = Random::RandomRanged(weaponData->VisualScatterMin, weaponData->VisualScatterMax);
			*pTargetCoords = MapClass::GetRandomCoordsNear((pBullet->Type->Inviso ? pBullet->Location : pBullet->TargetCoords), radius, true);
		}
		else
		{
			*pTargetCoords = (pBullet->Type->Inviso ? pBullet->Location : pBullet->TargetCoords);
		}
	}
	else if (const auto pObstacleCell = FireAtTemp::pObstacleCell)
	{
		*pTargetCoords = pObstacleCell->GetCoordsWithBridge();
	}

	R->EAX(pTargetCoords);
	return 0;
}

// Adjust target for bolt / beam / wave drawing.
DEFINE_HOOK(0x6FF43F, TechnoClass_FireAt_TargetSet, 0x6)
{
	LEA_STACK(CoordStruct*, pTargetCoords, STACK_OFFSET(0xB0, -0x28));
	GET_BASE(AbstractClass*, pOriginalTarget, 0x8);

	// Store original target & coords
	FireAtTemp::OriginalTargetCoords = *pTargetCoords;
	FireAtTemp::pOriginalTarget = pOriginalTarget;

	if (FireAtTemp::pObstacleCell)
	{
		*pTargetCoords = FireAtTemp::pObstacleCell->GetCoordsWithBridge();
		R->Base(8, FireAtTemp::pObstacleCell); // Replace original target so it gets used by Ares sonic wave stuff etc. as well.
	}

	return 0;
}

// Restore original target values and unset obstacle cell.
DEFINE_HOOK(0x6FF660, TechnoClass_FireAt_ObstacleCellUnset, 0x6)
{
	LEA_STACK(CoordStruct*, pTargetCoords, STACK_OFFSET(0xB0, -0x28));

	// Restore original target & coords
	*pTargetCoords = FireAtTemp::OriginalTargetCoords;
	R->Base(8, FireAtTemp::pOriginalTarget);
	R->EDI(FireAtTemp::pOriginalTarget);

	// Reset temp values
	FireAtTemp::FireBullet = nullptr;
	FireAtTemp::OriginalTargetCoords = CoordStruct::Empty;
	FireAtTemp::pObstacleCell = nullptr;
	FireAtTemp::pOriginalTarget = nullptr;

	return 0;
}

