#include <Helpers/Macro.h>
#include <Utilities/Macro.h>

#include <FootClass.h>
#include <HouseClass.h>
#include <UnitClass.h>
#include <OverlayTypeClass.h>
#include <RulesClass.h>
#include <WarheadTypeClass.h>

#include <Extension.h>
#include <Extension/TechnoExt.h>

#include <Ext/Common/CommonStatus.h>
#include <Ext/Helper/Crush.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/TechnoType/TechnoStatus.h>
#include <Common/Components/Scriptable.h>

/// @file CrushHook.cpp
/// @brief 碾压等级系统（B 方案）：引擎原本用「类型 Crusher 标记」判断的地方，
///        改成用「Techno 实例」判断，结果写回，让引擎自己继续执行。
///
/// 判定规则：碾压者 CrushLevel > 被碾压者 CrushableLevel（严格大于）。
/// 等级来源见 Ext/Helper/Crush.h（类型/全局基准 + Status.* 的 AE 加成）。

namespace
{
	/// @brief 引擎的"碾压减速"标志位（TechnoClass + 0x6B5）。
	///
	/// 置位后 DriveLocomotionClass 每帧把 movementspeed_50 夹到减速系数；
	/// 整个引擎只有"真的压掉一个 overlay"的路径（0x73B067）会清零，
	/// 所有"压不了"的出口都会把位留脏 —— 位一旦卡住就每帧减速、永久不恢复。
	/// 所以我们在拒绝碾压的分支里补一次清零（只是安全网，不参与判定）。
	constexpr size_t CrushSlowdownFlagOffset = 0x6B5;

	void ClearCrushSlowdownFlag(TechnoClass* pThis)
	{
		if (pThis)
			*reinterpret_cast<BYTE*>(reinterpret_cast<BYTE*>(pThis) + CrushSlowdownFlagOffset) = 0;
	}
}

#pragma region HasAbility
// HasAbility 的失败收尾（0x70D148，8 字节：pop edi; pop esi; xor al,al; pop ebx; ret 4）。
// 只要实例的碾压等级 > 0，就把答案改成"有 Crusher 能力"，改走成功收尾（0x70D140）。
// 只读 TechnoStatus 的每帧缓存：不查类型、不读 INI、不递归。
DEFINE_HOOK(0x70D148, TechnoClass_HasAbility_CrushLevel, 0x8)
{
	enum { ReturnTrue = 0x70D140 };

	GET(TechnoClass*, pThis, ESI);
	GET_STACK(Ability, ability, 0x10);

	if (ability == Ability::Crusher && pThis)
	{
		TechnoStatus* pStatus = nullptr;
		if (TryGetStatus<TechnoExt>(pThis, pStatus) && pStatus->CrushLevelReady && pStatus->CrushLevelCache > 0)
			return ReturnTrue;
	}

	return 0; // 执行原 epilogue
}
#pragma endregion

#pragma region 碾压者判定（值替换）
// 引擎在这 5 处直接读 Type->Crusher（+0xD28），然后 test cl,cl 分支。
// 这里把「类型判定」换成「实例判定」：算好结果写回 CL，跳过原 mov（6 字节），
// 让引擎自己的 test/je 去走它原本的两条路 —— 不跳门、不改流程。
#define CRUSH_CRUSHER_SUBSTITUTE(hookAddr, baseReg, baseOffset, name) \
	DEFINE_HOOK(hookAddr, name, 0x6) \
	{ \
		GET(BYTE*, pBase, baseReg); \
		FootClass* pTechno = *reinterpret_cast<FootClass**>(pBase + baseOffset); \
		if (!pTechno) \
			return 0; \
		R->CL(Crush::GetCrushLevel(pTechno) > 0 ? 1 : 0); \
		return R->Origin() + 0x6; \
	}

// [ebp+0xC] = techno（原版 0x4B3501 就是这么取的）
CRUSH_CRUSHER_SUBSTITUTE(0x4B350C, EBP, 0xC, DriveLocomotionClass_PassableCheck_Crusher1)
CRUSH_CRUSHER_SUBSTITUTE(0x4B4157, EBP, 0xC, DriveLocomotionClass_PassableCheck_Crusher2)
// [ebx+0xC] = techno（原版 0x5B098B 取法）
CRUSH_CRUSHER_SUBSTITUTE(0x5B0996, EBX, 0xC, MechLocomotionClass_ProcessMoving_Crusher)
// Ship 两处同 Drive：[ebp+0xC]
CRUSH_CRUSHER_SUBSTITUTE(0x6A2B5B, EBP, 0xC, ShipLocomotionClass_PassableCheck_Crusher1)
CRUSH_CRUSHER_SUBSTITUTE(0x6A3783, EBP, 0xC, ShipLocomotionClass_PassableCheck_Crusher2)
#pragma endregion

#pragma region 碾压击杀视为普通伤害
// 原版 CrushCell 是静默击杀（RegisterDestruction -> Remove -> UnInit），不产生伤害事件。
//
// 已验证：在 CrushCell 里用 ReceiveDamage 把目标打死，无论换什么弹头（CrushWarhead/C4Warhead）、
// 放在遍历中还是收尾，都会在死亡后踩到已失效对象崩溃（EIP=0）。所以这里不再让伤害致死：
//   1) 只造成一次带来源的非致死伤害（留 1 血）—— 受击事件/伤害统计能拿到；
//   2) 手动补 FireDeathWeapon(0) —— 死亡武器；
//   3) 手动派发 Kratos 的 OnReceiveDamageDestroy —— 被摧毁事件（与 0x702050 同一套回调）；
//   4) 返回 0，击杀/摘除仍由原版静默流程完成（RegisterDestruction/Remove/UnInit）。
DEFINE_HOOK(0x7418F0, UnitClass_CrushCell_DamageKill, 0x6)
{
	if (!General::Data()->CrushDealsDamage)
		return 0;

	// 弹头：优先用 [General] CrushKillWarhead，留空则用 [CombatDamage] CrushWarhead
	WarheadTypeClass* pWH = nullptr;
	if (!General::Data()->CrushKillWarhead.empty())
		pWH = WarheadTypeClass::Find(General::Data()->CrushKillWarhead.c_str());
	if (!pWH)
		pWH = RulesClass::Instance->CrushWarhead;
	if (!pWH)
		return 0;

	GET(FootClass*, pVictim, ESI);
	GET(TechnoClass*, pCrusher, EDI);

	TechnoClass* pTechnoVictim = abstract_cast<TechnoClass*, true>(pVictim);
	if (!pTechnoVictim || pTechnoVictim == pCrusher)
		return 0;

	// IgnoreDefenses 必须为 false：Revenge 的 OnReceiveDamageReal 会跳过真伤伤害
	int damage = pTechnoVictim->Health - 1;
	if (damage > 0)
		pTechnoVictim->ReceiveDamage(&damage, 0, pWH, pCrusher, false, false, pCrusher->Owner);

	// 死亡武器：按原版规则，仅当 Explodes=yes 或当前主武器（含 IFV 当前模式）Suicide=yes
	TechnoTypeClass* pVictimType = pTechnoVictim->GetTechnoType();
	bool fireDeathWeapon = pVictimType && pVictimType->Explodes;
	if (!fireDeathWeapon)
	{
		WeaponStruct* pWS = pTechnoVictim->GetWeapon(0);
		fireDeathWeapon = pWS && pWS->WeaponType && pWS->WeaponType->Suicide;
	}
	if (fireDeathWeapon && pTechnoVictim->Health > 0)
		pTechnoVictim->FireDeathWeapon(0);

	// 被摧毁事件：与引擎在 0x702050 派发的是同一套回调
	if (auto pExt = TechnoExt::ExtMap.Find(pTechnoVictim))
	{
		pExt->_GameObject->Foreach([](Component* c)
			{ if (auto cc = dynamic_cast<ITechnoScript*>(c)) { cc->OnReceiveDamageDestroy(); } });
	}

	return 0; // 交回原版静默击杀
}
#pragma endregion

#pragma region 全压 / 墙
// 压过墙/沙袋：原版看 overlay 的 Crushable / Wall 标记，这里叠加墙的碾压等级
DEFINE_HOOK(0x73B013, UnitClass_PerCellProcess_CrusherWall, 0x6)
{
	enum { CanCrush = 0x73B036, CannotCrush = 0x73B074 };

	GET(OverlayTypeClass*, pOverlay, ESI);
	GET(UnitClass*, pThis, EBP);

	// overlay 自己配了 CrushableLevel 就以它为准（不配置则完全走原版规则）
	int overlayLevel = Crush::GetOverlayCrushableLevel(pOverlay);
	if (overlayLevel >= 0)
	{
		if (Crush::GetCrushLevel(pThis) > overlayLevel)
			return CanCrush;

		ClearCrushSlowdownFlag(pThis);
		return CannotCrush;
	}

	// 原版逻辑：ArrayIndex == 0（GASAND 沙袋）默认可压
	if (!pOverlay->ArrayIndex || pOverlay->Crushable)
		return CanCrush;

	// 不是墙（矿石之类）或墙的等级不够：压不了，清掉残留的减速位
	if (!pOverlay->Wall
		|| (pThis->Type->MovementZone != MovementZone::CrusherAll
			&& Crush::GetCrushLevel(pThis) <= General::Data()->WallCrushableLevel))
	{
		ClearCrushSlowdownFlag(pThis);
		return CannotCrush;
	}

	return CanCrush;
}

DEFINE_HOOK(0x73F42E, UnitClass_IsCellOccupied_CrushWall, 0x6)
{
	enum { CanCrush = 0x73F46E, CannotCrush = 0x73F483 };

	GET(UnitClass*, pThis, EBX);
	int crushLevel = Crush::GetCrushLevel(pThis);

	if (crushLevel <= 0)
		return CannotCrush;

	GET(OverlayTypeClass*, pOverlayType, ESI);

	// overlay 自己配了 CrushableLevel 就以它为准（不配置则完全走原版规则）
	int overlayLevel = Crush::GetOverlayCrushableLevel(pOverlayType);
	if (overlayLevel >= 0)
		return crushLevel > overlayLevel ? CanCrush : CannotCrush;

	if (pOverlayType->Crushable
		|| (pOverlayType->Wall
			&& (pThis->Type->MovementZone == MovementZone::CrusherAll
				|| crushLevel > General::Data()->WallCrushableLevel)))
		return CanCrush;

	return CannotCrush;
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
			// 非 Techno 对象（overlay/地形）：原版只看 [ObjectType]Crushable。
			// overlay 自己配了 CrushableLevel 就以它为准；否则墙（Wall=yes）按
			// 等级 > WallCrushableLevel 判定，其余保持原版。
			bool crushable = pThis->GetType()->Crushable;
			if (pThis->WhatAmI() == AbstractType::Overlay)
			{
				OverlayTypeClass* pOverlay = static_cast<OverlayTypeClass*>(pThis->GetType());
				int overlayLevel = Crush::GetOverlayCrushableLevel(pOverlay);
				if (overlayLevel >= 0)
					crushable = Crush::GetCrushLevel(pCrusher) > overlayLevel;
				else if (!crushable && pOverlay->Wall)
					crushable = Crush::GetCrushLevel(pCrusher) > General::Data()->WallCrushableLevel;
			}
			result = crushable;
		}
	}

	R->AL(result);
	return SkipGameCode;
}
#pragma endregion
