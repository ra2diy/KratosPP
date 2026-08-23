#include <exception>
#include <Windows.h>

#include <GeneralStructures.h>
#include <TechnoClass.h>
#include <FootClass.h>
#include <AircraftClass.h>

#include <Extension.h>
#include <Utilities/Macro.h>
#include <Utilities/EnumFunctions.h>

#include <Extension/TechnoTypeExt.h>
#include <Extension/WarheadTypeExt.h>

#include <Extension/TechnoExt.h>

#include <Ext/Common/CommonStatus.h>

#include <Ext/Helper/Scripts.h>

#include <Ext/TechnoType/TechnoStatus.h>
#include <Ext/TechnoType/AirstrikeData.h>

// 鲍里斯的副武器使用空袭是强制指定目标类型必须为建筑
DEFINE_HOOK(0x6F3477, TechnoClass_SelectWeapon_Airstrike, 0xA)
{
	// 跳过副武器空袭弹头的检查，执行常规检查
	return 0x6F3528;
}

// 武器的空袭指针强制只对建筑显示
DEFINE_HOOK(0x51EAE0, InfantryClass_WhatAction_Cursor, 0x7)
{
	return 0x51EB06;
}

// 兼容Phobos对空袭的目标控制
DEFINE_HOOK(0x6F348F, TechnoClass_WhatWeaponShouldIUse_Airstrike, 0x7)
{
	enum { Primary = 0x6F37AD, Secondary = 0x6F3807 };

	GET(TechnoClass*, pTargetTechno, EBP);
	GET(WarheadTypeClass*, pSecondaryWH, ECX);

	if (!pTargetTechno)
	{
		return Primary;
	}

	WarheadTypeExt::TypeData* warheadTypeData = GetTypeData<WarheadTypeExt, WarheadTypeExt::TypeData>(pSecondaryWH);

	if (!EnumFunctions::IsTechnoEligible(pTargetTechno, warheadTypeData->AirstrikeTargets))
	{
		return Primary;
	}

	TechnoTypeClass* pTargetType = pTargetTechno->GetTechnoType();
	TechnoTypeExt::TypeData* pTargetTypeData = GetTypeData<TechnoTypeExt, TechnoTypeExt::TypeData>(pTargetType);

	if (pTargetTechno->AbstractFlags & AbstractFlags::Foot)
	{
		return pTargetTypeData->AllowAirstrike ? Secondary : Primary;
	}

	return (pTargetTypeData->AllowAirstrike && (!pTargetType->ResourceDestination || !pTargetType->ResourceGatherer))
		? Secondary : Primary;
}

// 首次添加空袭管理器，跳过建筑检查
DEFINE_HOOK(0x41D97B, AirstrikeClass_Setup_SkipBuildingCheck, 0x7)
{
	GET(TechnoClass*, pTarget, ESI);
	GET(AirstrikeClass*, pAirstrike, EDI);
	// Debug::Log("将持有管理器 %d 的目标[%s]%d, 设置给空袭管理器 %d 的飞机\n", pTarget->Airstrike, pTarget->GetTechnoType()->ID, pTarget, pAirstrike);
	FootClass* pAircraft = pAirstrike->FirstObject;
	do
	{
		if (pAircraft)
		{
			pAircraft->SetTarget(pTarget);
		}
	} while (pAircraft && (pAircraft = pAircraft->NextTeamMember) != nullptr);
	return 0x41D98B;
}

// 兼容Phobos
DEFINE_HOOK(0x41DA52, AirstrikeClass_ResetTarget_OriginalTarget, 0x6)
{
	enum { SkipGameCode = 0x41DA7C };

	R->EDI(R->ESI());

	return SkipGameCode;
}

// 兼容Phobos
DEFINE_HOOK(0x41DA80, AirstrikeClass_ResetTarget_NewTarget, 0x6)
{
	enum { SkipGameCode = 0x41DA9C };

	R->ESI(R->EBX());

	return SkipGameCode;
}

// 兼容Phobos
DEFINE_HOOK(0x41DAA4, AirstrikeClass_ResetTarget_ResetForOldTarget, 0xA)
{
	enum { SkipGameCode = 0x41DAAE };

	GET(AirstrikeClass*, pAirstrike, EBP);
	GET(TechnoClass*, pTarget, EDI);

	// Debug::Log("空袭管理器 %d 清除旧目标，当前旧目标[%s]%d\n", pAirstrike, pTarget->GetTechnoType()->ID, pTarget);
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTarget, status))
	{
		status->CancelAirstrike(pAirstrike);
	}

	return SkipGameCode;
}

// 接管空袭管理器向目标设置自己
DEFINE_HOOK(0x41D994, AirstrikeClass_Setup_SetToTarget, 0x6)
{
	GET(TechnoClass*, pTarget, ESI);
	GET(AirstrikeClass*, pAirstrike, EDI);
	// Debug::Log("接管将空袭管理器 %d 设置给[%s]%d\n", pAirstrike, pTarget->GetTechnoType()->ID, pTarget);
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTarget, status))
	{
		status->SetAirstrike(pAirstrike);
		return 0x41D99A;
	}
	return 0;
}

// 接管空袭管理器清空旧目标
DEFINE_HOOK(0x41DA68, AirstrikeClass_Reset_Stopwhat_SkipBuildingCheck, 0x7)
{
	GET(TechnoClass*, pTarget, ESI);
	GET(AirstrikeClass*, pAirstrike, EBP);
	// Debug::Log("空袭管理器 %d 设置新目标，当前旧目标[%s]%d\n", pAirstrike, pTarget->GetTechnoType()->ID, pTarget);
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTarget, status))
	{
		status->CancelAirstrike(pAirstrike);
	}
	R->EDI(pTarget);
	return 0x41DA7C;
}

// 接管空袭管理器重设新目标
DEFINE_HOOK(0x41DA88, AirstrikeClass_Reset_Startwhat_SkipBuildingCheck, 0x7)
{
	GET(TechnoClass*, pTarget, EBX);
	// GET(AirstrikeClass*, pAirstrike, EBP);
	// Debug::Log("空袭管理器 %d 设置新目标，当前新目标[%s]%d\n", pAirstrike, pTarget->GetTechnoType()->ID, pTarget);
	R->ESI(pTarget);
	return 0x41DA9C;
}

// 接管空袭管理器向目标设置自己
DEFINE_HOOK(0x41DAD4, AirstrikeClass_Reset, 0x6)
{
	GET(TechnoClass*, pTarget, ESI);
	GET(AirstrikeClass*, pAirstrike, EBP);
	// Debug::Log("接管将空袭管理器 %d 重新设置给[%s]%d\n", pAirstrike, pTarget->GetTechnoType()->ID, pTarget);
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTarget, status))
	{
		status->SetAirstrike(pAirstrike);
		return 0x41DADA;
	}
	return 0;
}

DEFINE_HOOK(0x41DBD4, AirstrikeClass_ClearTarget, 0x7)
{
	enum { resetTarget = 0x41DBE8, clearTarget = 0x41DC3A };
	GET(TechnoClass*, pTarget, ESI); // AirstrikeClass->Target
	GET(AirstrikeClass*, pAirstrike, EBP);
	// Debug::Log("不满足空袭条件，空袭管理器 %d 清空目标[%s]%d\n", pAirstrike, pTarget->GetTechnoType()->ID, pTarget);
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTarget, status))
	{
		status->CancelAirstrike(pAirstrike);
	}
	// 该函数遍历所有的空袭管理器清空挂载，因为接管了目标身上的管理器，所以永久返回clearTarget
	if (pTarget->Airstrike == pAirstrike)
	{
		return resetTarget;
	}
	return clearTarget;
}

// 上一个Hook永久跳过该步骤
// DEFINE_HOOK(0x41DC10, AirstrikeClass_ResetTarget_SetToTarget, 0x6)
// {
// 	GET(TechnoClass*, pTarget, ESI);
// 	GET(AirstrikeClass*, pAirstrike, EBP);
// 	TechnoStatus* status = nullptr;
// 	if (TryGetStatus<TechnoExt>(pTarget, status))
// 	{
// 		status->SetAirstrike(pAirstrike);
// 		return 0x41DC2C;
// 	}
// 	return 0;
// }

DEFINE_HOOK(0x41D604, AirstrikeClass_PointerGotInvalid_ResetForTarget, 0x6)
{
	enum { SkipGameCode = 0x41D634 };

	GET(AirstrikeClass*, pAirstrike, ESI);
	GET(TechnoClass*, pTarget, EAX);

	// Debug::Log("空袭管理器 %d 清除旧目标，当前旧目标[%s]%d\n", pAirstrike, pTarget->GetTechnoType()->ID, pTarget);
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTarget, status))
	{
		status->CancelAirstrike(pAirstrike);
	}

	return SkipGameCode;
}

DEFINE_HOOK(0x65E97F, HouseClass_CreateAirstrike_SetTargetForUnit, 0x6)
{
	enum { SkipGameCode = 0x65E992 };

	GET(AircraftClass*, pFirer, ESI);
	GET_STACK(AirstrikeClass*, pThis, STACK_OFFSET(0x38, 0x1C));

	const auto pOwner = pThis->Owner;

	if (!pOwner)
		return 0;

	if (const auto pTarget = abstract_cast<TechnoClass*>(pOwner->Target))
	{
		pFirer->SetTarget(pTarget);

		return SkipGameCode;
	}

	return 0;
}

namespace AirstrikePutOffset
{
	int flipY = -1;
	int GetFlipY()
	{
		flipY = -flipY;
		return flipY;
	}
}

// Phobos hook here and skip gamecode
DEFINE_HOOK(0x65E997, Airstrike_Supported_Reinforcements_Put, 0x6)
{
	enum { SkipGameCode = 0x65E9EE, SkipGameCodeNoSuccess = 0x65EA8B };

	GET(AircraftClass*, pAircraft, ESI);
	AirstrikeClass* pAirstrike = pAircraft->Airstrike;
	if (pAirstrike && pAirstrike->Owner)
	{
		TechnoClass* pTechno = pAirstrike->Owner;
		AirstrikeData* data = INI::GetConfig<AirstrikeData>(INI::Rules, pTechno->GetTechnoType()->ID)->Data;
		CoordStruct offset = data->AirstrikePutOffset;
		if (!offset.IsEmpty())
		{
			GET(CellStruct, edgeCell, EDI);
			GET_STACK(AbstractClass*, pTarget, 0x44);
			CoordStruct sourcePos = CellClass::Cell2Coord(edgeCell);
			CoordStruct targetPos = pTarget->GetCoords();
			DirStruct dir = Point2Dir(sourcePos, targetPos);
			offset.Y *= AirstrikePutOffset::GetFlipY();
			CoordStruct newPos = GetFLHAbsoluteCoords(targetPos, offset, dir);
			bool result = TryPutTechno(pAircraft, newPos);
			pAircraft->SetHeight(offset.Z + pAircraft->Type->GetFlightLevel());
			pAircraft->PrimaryFacing.SetCurrent(dir);
			pAircraft->SecondaryFacing.SetCurrent(dir);
			return result ? SkipGameCode : SkipGameCodeNoSuccess;
		}
	}
	return 0;
}

#pragma region GetEffectTintIntensity

// 兼容Phobos，只能使用Phobos的方式控制线条和点的颜色
// namespace AirstrikeLaser
// {
// 	bool CustomColor = false;
// 	ColorStruct Color = Colors::Red;
// }

// 是否绘制激光
DEFINE_HOOK(0x6D481D, TacticalClass_Draw_AirstrikeLaser_SkipBuildingCheck, 0x7)
{
	enum { draw = 0x6D482D, skip = 0x6D48FA };
	GET(TechnoClass*, pTechno, ESI);
	// Debug::Log("单位[%s]%d绘制空袭激光指示\n", pTechno->GetTechnoType()->ID, pTechno);
	AirstrikeData* data = INI::GetConfig<AirstrikeData>(INI::Rules, pTechno->GetTechnoType()->ID)->Data;
	if (data->AirstrikeDisableLine)
	{
		return skip;
	}
	// 自定义颜色，为了兼容Phobos，只能使用Phobos的方式控制线条和点的颜色
	// AirstrikeLaser::CustomColor = true;
	// if (data->AirstrikeTargetLaser >= 0)
	// {
	// 	ColorStruct add = RulesClass::Instance->ColorAdd[data->AirstrikeTargetLaser];
	// 	AirstrikeLaser::Color = Drawing::Int_To_RGB(Add2RGB565(add));
	// }
	// else
	// {
	// 	AirstrikeLaser::Color = data->AirstrikeLineColor;
	// }
	return draw;
}

// DEFINE_HOOK(0x705976, TechnoClass_Draw_AirstrikeLaser_LineColor, 0x8)
// {
// 	if (!AirstrikeLaser::CustomColor)
// 	{
// 		AirstrikeLaser::Color = Drawing::Int_To_RGB(Add2RGB565(RulesClass::Instance->ColorAdd[RulesClass::Instance->LaserTargetColor]));
// 	}
// 	ColorStruct c = AirstrikeLaser::Color;
// 	BYTE rand = (BYTE)Random::RandomRanged(0, 14);
// 	if (c.R > rand)
// 	{
// 		c.R -= rand;
// 	}
// 	if (c.G > rand)
// 	{
// 		c.G -= rand;
// 	}
// 	if (c.B > rand)
// 	{
// 		c.B -= rand;
// 	}
// 	// 线的颜色
// 	R->Stack(0x10, c);
// 	return 0;
// }

// DEFINE_HOOK(0x705986, TechnoClass_Draw_AirstrikeLaser_PointColor, 0x6)
// {
// 	if (!AirstrikeLaser::CustomColor)
// 	{
// 		AirstrikeLaser::Color = Drawing::Int_To_RGB(Add2RGB565(RulesClass::Instance->ColorAdd[RulesClass::Instance->LaserTargetColor]));
// 	}
// 	ColorStruct c = AirstrikeLaser::Color;
// 	// 点的颜色
// 	R->ESI(Drawing::RGB_To_Int(c));
// 	return 0x7059C7;
// }

// from Phobos, 绘制空袭闪光线和点的颜色
namespace DrawAirstrikeFlareTemp
{
	TechnoClass* pTechno = nullptr;
}

DEFINE_HOOK(0x705860, TechnoClass_DrawAirstrikeFlare_SetContext, 0x8)
{
	GET(TechnoClass*, pThis, ECX);

	// This is not used in vanilla function so ECX gets overwritten later.
	DrawAirstrikeFlareTemp::pTechno = pThis;

	return 0;
}

DEFINE_HOOK(0x7058F6, TechnoClass_DrawAirstrikeFlare_LineColor, 0x5)
{
	enum { SkipGameCode = 0x705976 };

	GET(int, zSrc, EBP);
	GET(int, zDest, EBX);
	REF_STACK(ColorStruct, color, STACK_OFFSET(0x70, -0x60));

	// Fix depth buffer value.
	int zValue = Math::min(zSrc, zDest) + AudioVisual::Data()->AirstrikeLineZAdjust;
	R->EBP(zValue);
	R->EBX(zValue);

	// Allow custom colors.
	auto const pTechno = DrawAirstrikeFlareTemp::pTechno;
	// 自定义颜色，点的颜色与此处相同
	AirstrikeData* data = INI::GetConfig<AirstrikeData>(INI::Rules, pTechno->GetTechnoType()->ID)->Data;
	auto const baseColor = data->AirstrikeLineColor;
	// 渲染路径，使用本地随机数，避免联机分叉
	double percentage = VisualRandomRanged(745, 1000) / 1000.0; // 随机色差
	// Debug::Log(" - 绘制空袭光纤颜色: {%d, %d, %d} %d\n", baseColor.R, baseColor.G, baseColor.B, percentage);
	color = { (BYTE)(baseColor.R * percentage), (BYTE)(baseColor.G * percentage), (BYTE)(baseColor.B * percentage) };
	R->ESI(Drawing::RGB_To_Int(baseColor));

	return SkipGameCode;
}

// Always draw the dot and skip setting color, it is already done in previous hook.
DEFINE_HOOK(0x70597A, TechnoClass_DrawAirstrikeFlare_DotColor, 0x6)
{
	enum { SkipGameCode = 0x7059C7 };

	GET_STACK(int, xCoord, STACK_OFFSET(0x70, -0x38));

	// Restore overridden instructions.
	R->ECX(xCoord);

	return SkipGameCode;
}

// 更新被空袭时的闪光变化
DEFINE_HOOK(0x70E92F, TechnoClass_Update_Airstrike_Tint_Timer, 0x5)
{
	enum { ContinueTintIntensity = 0x70E96E, NonAirstrike = 0x70EC9F };

	GET(TechnoClass*, pTechno, ESI);
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTechno, status) && status->AnyAirstrike())
	{
		return ContinueTintIntensity;
	}
	return NonAirstrike;
}

DEFINE_HOOK(0x43F9E0, BuildingClass_Mark_Airstrike, 0x6)
{
	enum { ContinueTintIntensity = 0x43FA0F, NonAirstrike = 0x43FA19 };

	GET(BuildingClass*, pThis, EDI);
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pThis, status) && status->AnyAirstrike())
	{
		return ContinueTintIntensity;
	}
	return NonAirstrike;
}

DEFINE_HOOK(0x448DF1, BuildingClass_SetOwningHouse_Airstrike, 0x6)
{
	enum { ContinueTintIntensity = 0x448E0D, NonAirstrike = 0x448E17 };

	GET(BuildingClass*, pThis, ESI);
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pThis, status) && status->AnyAirstrike())
	{
		return ContinueTintIntensity;
	}
	return NonAirstrike;
}

DEFINE_HOOK(0x451ABC, BuildingClass_PlayAnim_Airstrike, 0x6)
{
	enum { ContinueTintIntensity = 0x451AEB, NonAirstrike = 0x451AF5 };

	GET(BuildingClass*, pThis, ESI);
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pThis, status) && status->AnyAirstrike())
	{
		return ContinueTintIntensity;
	}
	return NonAirstrike;
}

DEFINE_HOOK(0x452041, BuildingClass_452000_Airstrike, 0x6)
{
	enum { ContinueTintIntensity = 0x452070, NonAirstrike = 0x45207A };

	GET(BuildingClass*, pThis, ESI);
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pThis, status) && status->AnyAirstrike())
	{
		return ContinueTintIntensity;
	}
	return NonAirstrike;
}

DEFINE_HOOK(0x456E5A, BuildingClass_Flash_Airstrike, 0x6)
{
	enum { ContinueTintIntensity = 0x456E89, NonAirstrike = 0x456E93 };

	GET(BuildingClass*, pThis, ESI);
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pThis, status) && status->AnyAirstrike())
	{
		return ContinueTintIntensity;
	}
	return NonAirstrike;
}

DEFINE_HOOK(0x43D390, BuildingClass_DrawSHP_LaserTargetColor_Skip, 0x6)
{
	return 0x43D430;
}

// 绘制SHP时无敌或者被空袭时，调整亮度
DEFINE_HOOK(0x706342, TechnoClass_DrawSHP_Tint_Airstrike, 0x7)
{
	enum { draw = 0x706377, skip = 0x706389 };
	GET(TechnoClass*, pTechno, ESI);
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTechno, status) && status->AnyAirstrike())
	{
		AirstrikeData* data = INI::GetConfig<AirstrikeData>(INI::Rules, pTechno->GetTechnoType()->ID)->Data;
		if (data->AirstrikeDisableBlink)
		{
			return skip;
		}
		pTechno->NeedsRedraw = true;
		return draw;
	}
	return 0;
}

DEFINE_HOOK(0x43DC2A, BuildingClass_DrawVXL_LaserTargetColor_Skip, 0x6)
{
	return 0x43DCCE;
}

// 绘制VXL时无敌或者被空袭时，调整亮度
DEFINE_HOOK(0x70679B, TechnoClass_DrawVXL_Tint_Airstrike, 0x5)
{
	enum { draw = 0x7067D2, skip = 0x7067E4 };
	GET(TechnoClass*, pTechno, EBP);
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTechno, status) && status->AnyAirstrike())
	{
		AirstrikeData* data = INI::GetConfig<AirstrikeData>(INI::Rules, pTechno->GetTechnoType()->ID)->Data;
		if (data->AirstrikeDisableBlink)
		{
			return skip;
		}
		pTechno->NeedsRedraw = true;
		return draw;
	}
	return 0;
}

// 绘制VXL时无敌或者被空袭时，调整亮度
DEFINE_HOOK(0x73BFA4, UnitClass_DrawVXL_Tint_Airstrike, 0x6)
{
	enum { draw = 0x73BFAA, skip = 0x73C07C };
	GET(TechnoClass*, pTechno, EBP);
	if (pTechno->IsIronCurtained())
	{
		return draw;
	}
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTechno, status) && status->AnyAirstrike())
	{
		AirstrikeData* data = INI::GetConfig<AirstrikeData>(INI::Rules, pTechno->GetTechnoType()->ID)->Data;
		if (data->AirstrikeDisableBlink)
		{
			return skip;
		}
		return draw;
	}
	return skip;
}

#pragma endregion
