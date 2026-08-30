#include <exception>
#include <Windows.h>

#include <GeneralStructures.h>
#include <EBolt.h>

#include <Extension.h>
#include <Utilities/Macro.h>
#include <Utilities/Debug.h>

#include <Extension/EBoltExt.h>
#include <Extension/WeaponTypeExt.h>

#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/DrawEx.h>

#include <Ext/EBoltType/EBoltStatus.h>

// ----------------
// Extension
// ----------------

DEFINE_HOOK(0x4C1E42, EBolt_CTOR, 0x5)
{
	GET(EBolt*, pItem, EAX);
	EBoltExt::ExtMap.TryAllocate(pItem);

	return 0;
}

DEFINE_HOOK(0x4C2951, EBolt_DTOR, 0x5)
{
	GET(EBolt*, pItem, ECX);

	EBoltExt::ExtMap.Remove(pItem);

	return 0;
}

// ----------------
// Feature
// ----------------

DEFINE_HOOK(0x6FD494, TechnoClass_FireEBolt_SetWeaponData, 0x7)
{
	GET(EBolt*, pThis, EAX);
	GET_STACK(WeaponTypeClass*, pWeapon, 0x30 + 0x8);

	if (WeaponTypeExt::TypeData* weaponData = GetTypeData<WeaponTypeExt, WeaponTypeExt::TypeData>(pWeapon))
	{
		if (EBoltStatus* status = GetStatus<EBoltExt, EBoltStatus>(pThis))
		{
			status->ArcCount = weaponData->BoltArcCount;

			status->Color1 = weaponData->BoltColor1 == Colors::Empty ? BoltType::GetDefaultColor1(pWeapon->IsAlternateColor) : weaponData->BoltColor1;
			status->Color2 = weaponData->BoltColor2 == Colors::Empty ? BoltType::GetDefaultColor2(pWeapon->IsAlternateColor) : weaponData->BoltColor2;
			status->Color3 = weaponData->BoltColor3 == Colors::Empty ? BoltType::GetDefaultColor3(pWeapon->IsAlternateColor) : weaponData->BoltColor3;

			status->Disable1 = weaponData->BoltDisable1;
			status->Disable2 = weaponData->BoltDisable2;
			status->Disable3 = weaponData->BoltDisable3;

			status->DisableParticle = weaponData->BoltDisableParticle;
		}
	}
	return 0;
}

namespace BoltTemp
{
	EBolt* pBolt = nullptr;
	EBoltStatus* pStatus = nullptr;
	int color1;
	int color2;
	int color3;
	bool disable1;
	bool disable2;
	bool disable3;
}

DEFINE_HOOK(0x4C28B6, EBolt_Draw_Update, 0x6)
{
	GET(EBolt*, pThis, EAX);
	EBoltStatus* status = GetStatus<EBoltExt, EBoltStatus>(pThis);
	BoltTemp::pBolt = pThis;
	BoltTemp::pStatus = status;
	if (status)
	{
		// Debug::Log("EBolt_Draw_Update %d, color1 = {%d, %d, %d}, color2 = {%d, %d, %d}, color3 = {%d, %d, %d}\n", pThis, status->Color1.R, status->Color1.G, status->Color1.B, status->Color2.R, status->Color2.G, status->Color2.B, status->Color3.R, status->Color3.G, status->Color3.B);
		BoltTemp::color1 = Drawing::RGB_To_Int(status->Color1);
		BoltTemp::color2 = Drawing::RGB_To_Int(status->Color2);
		BoltTemp::color3 = Drawing::RGB_To_Int(status->Color3);
		BoltTemp::disable1 = status->Disable1;
		BoltTemp::disable2 = status->Disable2;
		BoltTemp::disable3 = status->Disable3;
		status->OnDraw();
	}
	else
	{
		BoltTemp::color1 = 0;
		BoltTemp::color2 = 0;
		BoltTemp::color3 = 0;
		BoltTemp::disable1 = false;
		BoltTemp::disable2 = false;
		BoltTemp::disable3 = false;
	}
	return 0;
}

DEFINE_HOOK(0x4C20BC, EBolt_Draw_Arcs, 0xB)
{
	enum { DoLoop = 0x4C20C7, Break = 0x4C2400 };

	GET_STACK(int, plotIndex, 0x408 - 0x3E0);
	const int arcCount = BoltTemp::pStatus ? BoltTemp::pStatus->ArcCount : 8;
	return plotIndex < arcCount ? DoLoop : Break;
}

DEFINE_JUMP(LJMP, 0x4C24BE, 0x4C24C3)// Disable Ares's hook EBolt_Draw_Color1
DEFINE_HOOK(0x4C24C3, EBolt_DrawFirst_Color, 0x9)// copy from Phobos
{
	if (BoltTemp::disable1)
		return 0x4C2515;

	R->EAX(BoltTemp::color1);
	return 0x4C24E4;
}

DEFINE_JUMP(LJMP, 0x4C25CB, 0x4C25D0)// Disable Ares's hook EBolt_Draw_Color2
DEFINE_HOOK(0x4C25D0, EBolt_DrawSecond_Color, 0x6)// copy from Phobos
{
	if (BoltTemp::disable2)
		return 0x4C262A;

	R->Stack(STACK_OFFSET(0x424, -0x40C), BoltTemp::color2);
	return 0x4C25FD;
}

DEFINE_JUMP(LJMP, 0x4C26CF, 0x4C26D5)// Disable Ares's hook EBolt_Draw_Color3
DEFINE_HOOK(0x4C26D5, EBolt_DrawThird_Color, 0x6)// copy from Phobos
{
	if (BoltTemp::disable3)
		return 0x4C2710;

	R->EAX(BoltTemp::color3);
	return 0x4C26EE;
}

// Can not hook in 0x4C2AFF, maybe it was skipped by Ares
DEFINE_HOOK(0x4C2AE7, EBolt_Fire_DisableParticle, 0x6)
{
	GET(EBolt*, pThis, ESI);

	if (EBoltStatus* status = GetStatus<EBoltExt, EBoltStatus>(pThis))
	{
		if (status->DisableParticle)
		{
			EBolt::Array->AddItem(pThis);
			R->EAX(0);
			return 0x4C2B0C;
		}
	}
	return 0;
}
