#include <Helpers/Macro.h>
#include <Utilities/Macro.h>

#include <FootClass.h>
#include <HouseClass.h>
#include <UnitClass.h>
#include <OverlayTypeClass.h>
#include <DriveLocomotionClass.h>
#include <ShipLocomotionClass.h>
#include <MechLocomotionClass.h>

#include <Extension.h>

#include <Ext/Common/CommonStatus.h>
#include <Ext/Helper/Crush.h>

/// @file CrushHook.cpp
/// @brief 碾压等级系统：把原版"是不是 Crusher"的布尔判断，统一改成按等级裁决。
///
/// 判定规则：碾压者 CrushLevel > 被碾压者 CrushableLevel（严格大于）。
/// 等级来源见 Ext/Helper/Crush.h（类型/全局基准 + Status.* 的 AE 加成）。

#pragma region 碾压者判定
// 移动类里"能不能碾压"的内联判断：Crusher 标记 -> 碾压等级 > 0
DEFINE_HOOK(0x4B3504, DriveLocomotionClass_PassableCheck_IsCrusher, 0x8)
{
	GET(FootClass*, pThis, ECX);
	return Crush::GetCrushLevel(pThis) > 0 ? 0x4B3516 : 0x4B3525;
}

DEFINE_HOOK(0x4B414F, DriveLocomotionClass_PassableCheck_IsCrusher2, 0x8)
{
	GET(FootClass*, pThis, ECX);
	return Crush::GetCrushLevel(pThis) > 0 ? 0x4B4161 : 0x4B4179;
}

DEFINE_HOOK(0x5B098E, MechLocomotionClass_ProcessMoving_IsCrusher, 0x8)
{
	GET(FootClass*, pThis, ECX);
	return Crush::GetCrushLevel(pThis) > 0 ? 0x5B09A0 : 0x5B09AF;
}

DEFINE_HOOK(0x5B1034, MechLocomotionClass_ProcessMoving_IsCrusher2, 0x8)
{
	GET(FootClass*, pThis, ECX);
	return Crush::GetCrushLevel(pThis) > 0 ? 0x5B1054 : 0x5B108A;
}

DEFINE_HOOK(0x5B1418, MechLocomotionClass_ProcessMoving_IsCrusher3, 0x8)
{
	GET(FootClass*, pThis, ECX);
	return Crush::GetCrushLevel(pThis) > 0 ? 0x5B1438 : 0x5B146E;
}

DEFINE_HOOK(0x6A1044, ShipLocomotionClass_ProcessMoving_IsCrusher, 0x8)
{
	GET(FootClass*, pThis, ECX);
	return Crush::GetCrushLevel(pThis) > 0 ? 0x6A1064 : 0x6A10B9;
}

DEFINE_HOOK(0x6A2B53, ShipLocomotionClass_PassableCheck_IsCrusher, 0x8)
{
	GET(FootClass*, pThis, ECX);
	return Crush::GetCrushLevel(pThis) > 0 ? 0x6A2B65 : 0x6A2B74;
}

DEFINE_HOOK(0x6A377B, ShipLocomotionClass_PassableCheck_IsCrusher2, 0x8)
{
	GET(FootClass*, pThis, ECX);
	return Crush::GetCrushLevel(pThis) > 0 ? 0x6A378D : 0x6A37A5;
}

DEFINE_HOOK(0x73AFEB, UnitClass_PerCellProcess_IsCrusher, 0x6)
{
	GET(FootClass*, pThis, EBP);
	return Crush::GetCrushLevel(pThis) > 0 ? 0x73B002 : 0x73B074;
}

DEFINE_HOOK(0x73FC6C, UnitClass_IsCellOccupied_IsCrusher, 0x6)
{
	GET(FootClass*, pThis, EBX);
	return Crush::GetCrushLevel(pThis) > 0 ? 0x73FD37 : 0x73FC91;
}

DEFINE_HOOK(0x741733, UnitClass_CrushCell_IsCrusher, 0x6)
{
	GET(FootClass*, pThis, EDI);
	return Crush::GetCrushLevel(pThis) > 0 ? 0x741754 : 0x74195E;
}
#pragma endregion

// 跳过引擎在调用 ObjectClass::IsCrushable 之前的 Crusher 前置判断，
// 让"能不能压"完全由 IsCrushable 按等级裁决（否则等级够了也会被布尔门挡回去）
DEFINE_JUMP(LJMP, 0x73FB2A, 0x73FB47);
DEFINE_JUMP(LJMP, 0x73FE5F, 0x73FE7C);
DEFINE_JUMP(LJMP, 0x741529, 0x741546);
DEFINE_JUMP(LJMP, 0x7438F7, 0x743918);

#pragma region 全压 / 墙
// 靠近目标时判定"是不是全压者"：原版比较 OmniCrusher 标记
DEFINE_HOOK(0x741603, UnitClass_ApproachTarget_OmniCrusher, 0x6)
{
	enum { IsOmniCrusher = 0x741613, NotOmniCrusher = 0x741685 };

	GET(UnitClass*, pThis, ESI);

	return Crush::GetCrushLevel(pThis) >= General::Data()->OmniCrusherLevel ? IsOmniCrusher : NotOmniCrusher;
}

// 压过墙/沙袋：原版看 overlay 的 Crushable / Wall 标记，这里叠加墙的碾压等级
DEFINE_HOOK(0x73B013, UnitClass_PerCellProcess_CrusherWall, 0x6)
{
	enum { CanCrush = 0x73B036, CannotCrush = 0x73B074 };

	GET(OverlayTypeClass*, pOverlay, ESI);

	// 原版逻辑：ArrayIndex == 0（GASAND 沙袋）默认可压
	if (!pOverlay->ArrayIndex || pOverlay->Crushable)
		return CanCrush;

	if (!pOverlay->Wall)
		return CannotCrush;

	GET(UnitClass*, pThis, EBP);

	return pThis->Type->MovementZone == MovementZone::CrusherAll
		|| Crush::GetCrushLevel(pThis) > General::Data()->WallCrushableLevel ? CanCrush : CannotCrush;
}

DEFINE_HOOK(0x73F42E, UnitClass_IsCellOccupied_CrushWall, 0x6)
{
	enum { CanCrush = 0x73F46E, CannotCrush = 0x73F483 };

	GET(UnitClass*, pThis, EBX);
	int crushLevel = Crush::GetCrushLevel(pThis);

	if (crushLevel <= 0)
		return CannotCrush;

	GET(OverlayTypeClass*, pOverlayType, ESI);

	if (pOverlayType->Crushable
		|| (pOverlayType->Wall
			&& (pThis->Type->MovementZone == MovementZone::CrusherAll
				|| crushLevel > General::Data()->WallCrushableLevel)))
		return CanCrush;

	return CannotCrush;
}

DEFINE_HOOK(0x4B19B8, DriveLocomotionClass_ProcessMoving_CrushWall, 0x8)
{
	enum { CanCrush = 0x4B19E2, CannotCrush = 0x4B1A04 };

	GET(FootClass*, pLinkedTo, ECX);
	GET(OverlayTypeClass*, pOverlay, ESI);

	return pOverlay->Wall && Crush::GetCrushLevel(pLinkedTo) > General::Data()->WallCrushableLevel
		? CanCrush : CannotCrush;
}

DEFINE_HOOK(0x4B1A1B, DriveLocomotionClass_ProcessMoving_CrusherAll, 0x8)
{
	enum { CanCrush = 0x4B1A2C, CannotCrush = 0x4B1A77 };

	GET(FootClass*, pLinkedTo, ECX);

	return pLinkedTo->GetTechnoType()->MovementZone == MovementZone::CrusherAll
		|| Crush::GetCrushLevel(pLinkedTo) > General::Data()->OmniCrusherLevel ? CanCrush : CannotCrush;
}

DEFINE_HOOK_AGAIN(0x6A2BB8, LocomotionClass_PassableCheck_CrusherAll, 0x8) // Ship
DEFINE_HOOK(0x4B3569, LocomotionClass_PassableCheck_CrusherAll, 0x8) // Drive
{
	GET(FootClass*, pLinkedTo, ECX);

	return R->Origin() + (pLinkedTo->GetTechnoType()->MovementZone == MovementZone::CrusherAll
		|| Crush::GetCrushLevel(pLinkedTo) > General::Data()->WallCrushableLevel ? 0x11 : 0x16);
}
#pragma endregion

#pragma region IsCrushable
// 整函数替换：原版只比布尔，这里按等级裁决
DEFINE_HOOK(0x5F6CD0, ObjectClass_IsCrushable_CrushLevel, 0x6)
{
	enum { SkipGameCode = 0x5F6D90 };

	GET(ObjectClass*, pThis, ECX);
	GET_STACK(FootClass*, pCrusher, 0x4);

	bool result = false;

	if (pThis && pCrusher && pThis != pCrusher)
	{
		if (TechnoClass* pTechno = abstract_cast<TechnoClass*, true>(pThis))
		{
			// 友军不可压、铁幕不可压（与原版一致）
			if (!pCrusher->Owner->IsAlliedWith(pTechno) && !pTechno->IsIronCurtained())
				result = Crush::GetCrushLevel(pCrusher) > Crush::GetCrushableLevel(pTechno);
		}
		else
		{
			result = pThis->GetType()->Crushable;
		}
	}

	R->AL(result);
	return SkipGameCode;
}
#pragma endregion
