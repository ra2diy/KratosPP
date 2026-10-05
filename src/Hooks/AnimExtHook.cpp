#include <exception>
#include <Windows.h>

#include <AnimTypeClass.h>
#include <GeneralDefinitions.h>
#include <SpecificStructures.h>

#include <Extension.h>
#include <Utilities/Macro.h>

#include <Extension/AnimExt.h>
#include <Extension/TechnoExt.h>

#include <Ext/Helper/CastEx.h>
#include <Ext/Helper/Scripts.h>

#include <Ext/AnimType/ExpireAnimData.h>
#include <Ext/AnimType/AnimStatus.h>
#include <Ext/Common/CommonStatus.h>
#include <Ext/TechnoType/TechnoStatus.h>

// ----------------
// Extension
// ----------------


DEFINE_HOOK_AGAIN(0x422126, AnimClass_CTOR, 0x5)
DEFINE_HOOK_AGAIN(0x422707, AnimClass_CTOR, 0x5)
DEFINE_HOOK(0x4228D2, AnimClass_CTOR, 0x5)
{
	if (!Common::IsLoadGame)
	{
		GET(AnimClass*, pItem, ESI);

		AnimExt::ExtMap.TryAllocate(pItem);
	}
	return 0;
}

DEFINE_HOOK(0x422967, AnimClass_DTOR, 0x6)
{
	GET(AnimClass*, pItem, ESI);

	AnimExt::ExtMap.Remove(pItem);

	return 0;
}

/*Crash when Anim called with GameDelete()
DEFINE_HOOK(0x426598, AnimClass_SDDTOR, 0x7)
{
	GET(AnimClass*, pItem, ESI);

	if(AnimExt::ExtMap.Find(pItem))
	{
		AnimExt::ExtMap.Remove(pItem);
	}

	return 0;
}
*/

DEFINE_HOOK_AGAIN(0x425280, AnimClass_SaveLoad_Prefix, 0x5)
DEFINE_HOOK(0x4253B0, AnimClass_SaveLoad_Prefix, 0x5)
{
	GET_STACK(AnimClass*, pItem, 0x4);
	GET_STACK(IStream*, pStm, 0x8);

	AnimExt::ExtMap.PrepareStream(pItem, pStm);

	return 0;
}

DEFINE_HOOK_AGAIN(0x425391, AnimClass_Load_Suffix, 0x7)
DEFINE_HOOK_AGAIN(0x4253A2, AnimClass_Load_Suffix, 0x7)
DEFINE_HOOK(0x425358, AnimClass_Load_Suffix, 0x7)
{
	AnimExt::ExtMap.LoadStatic();
	return 0;
}

DEFINE_HOOK(0x4253FF, AnimClass_Save_Suffix, 0x5)
{
	AnimExt::ExtMap.SaveStatic();
	return 0;
}

// ----------------
// Component
// ----------------

DEFINE_HOOK(0x423AC0, AnimClass_Update, 0x6)
{
	GET(AnimClass*, pThis, ECX);

	if (auto pExt = AnimExt::ExtMap.Find(pThis))
	{
		pExt->_GameObject->Foreach([](Component* c)
			{ c->OnUpdate(); });
	}

	return 0;
}

DEFINE_HOOK_AGAIN(0x42429E, AnimClass_UpdateEnd, 0x6)
DEFINE_HOOK_AGAIN(0x42437E, AnimClass_UpdateEnd, 0x6)
DEFINE_HOOK_AGAIN(0x4243A6, AnimClass_UpdateEnd, 0x6)
DEFINE_HOOK_AGAIN(0x424567, AnimClass_UpdateEnd, 0x6)
DEFINE_HOOK_AGAIN(0x4246DC, AnimClass_UpdateEnd, 0x6)
DEFINE_HOOK_AGAIN(0x424B42, AnimClass_UpdateEnd, 0x6)
DEFINE_HOOK_AGAIN(0x4247EB, AnimClass_UpdateEnd, 0x6)
DEFINE_HOOK_AGAIN(0x42492A, AnimClass_UpdateEnd, 0x6)
DEFINE_HOOK_AGAIN(0x424B29, AnimClass_UpdateEnd, 0x6)
DEFINE_HOOK(0x424B1B, AnimClass_UpdateEnd, 0x6)
{
	GET(AnimClass*, pThis, ESI);

	if (auto pExt = AnimExt::ExtMap.Find(pThis))
		pExt->_GameObject->Foreach([](Component* c)
			{ c->OnUpdateEnd(); });

	return 0;
}

DEFINE_HOOK(0x424785, AnimClass_Loop, 0x6)
{
	GET(AnimClass*, pThis, ESI);

	if (auto pExt = AnimExt::ExtMap.Find(pThis))
	{
		pExt->_GameObject->Foreach([](Component* c)
			{if (auto cc = dynamic_cast<IAnimScript*>(c)) { cc->OnLoop(); } });
	}

	return 0;
}

DEFINE_HOOK_AGAIN(0x4247F3, AnimClass_Done, 0x6)
DEFINE_HOOK(0x424298, AnimClass_Done, 0x6)
{
	GET(AnimClass*, pThis, ESI);

	if (auto pExt = AnimExt::ExtMap.Find(pThis))
	{
		pExt->_GameObject->Foreach([](Component* c)
			{ if (auto cc = dynamic_cast<IAnimScript*>(c)) { cc->OnDone(); } });
	}

	return 0;
}

DEFINE_HOOK(0x424807, AnimClass_Next, 0x6)
{
	GET(AnimClass*, pThis, ESI);
	GET(AnimTypeClass*, pNextAnimType, ECX);

	if (auto pExt = AnimExt::ExtMap.Find(pThis))
	{
		pExt->_GameObject->Foreach([pNextAnimType](Component* c)
			{ if (auto cc = dynamic_cast<IAnimScript*>(c)) { cc->OnNext(pNextAnimType); } });
	}

	return 0;
}

// ----------------
// Feature
// ----------------

#pragma region remap

DEFINE_HOOK(0x42312A, AnimClass_Draw_Remap, 0x6)
{
	GET(AnimClass*, pThis, ESI);
	if (pThis && pThis->Type->AltPalette && pThis->Owner)
	{
		return 0x423130;
	}
	return 0x4231F3;
}

DEFINE_HOOK(0x423136, AnimClass_Draw_Remap2, 0x6)
{
	GET(AnimClass*, pThis, ESI);
	if (pThis && pThis->Type->AltPalette && pThis->Owner)
	{
		R->ECX(pThis->Owner);
	}
	return 0;
}

DEFINE_HOOK(0x423E75, AnimClass_Extras_Remap, 0x6)
{
	GET(AnimClass*, pThis, ESI);
	GET(AnimClass*, pNewAnim, EDI);

	pNewAnim->Owner = pThis->Owner;

	return 0;
}

// Take over to Create Bounce Anim
DEFINE_HOOK(0x423991, AnimClass_Bounce_Remap, 0x5)
{
	GET(AnimClass*, pThis, EBP);
	if (pThis->Type && pThis->Type->BounceAnim)
	{
		AnimClass* pNewAnim = GameCreate<AnimClass>(pThis->Type->BounceAnim, pThis->GetCoords());
		pNewAnim->Owner = pThis->Owner;
		return 0x4239D3;
	}

	return 0;
}

// Take over to Create Spawn Anim
DEFINE_HOOK(0x423F8C, AnimClass_Spawn_Remap, 0x5)
{
	GET(AnimClass*, pThis, ESI);
	if (pThis->Type && pThis->Type->Spawns)
	{
		AnimClass* pNewAnim = GameCreate<AnimClass>(pThis->Type->Spawns, pThis->GetCoords());
		pNewAnim->Owner = pThis->Owner;
		return 0x423FC3;
	}

	return 0;
}

// Take over to Create Trailer Anim
DEFINE_HOOK(0x4242E1, AnimClass_Trailer_Remap, 0x5)
{
	GET(AnimClass*, pThis, ESI);
	if (pThis->Type && pThis->Type->TrailerAnim)
	{
		AnimClass* pNewAnim = GameCreate<AnimClass>(pThis->Type->TrailerAnim, pThis->GetCoords());
		pNewAnim->Owner = pThis->Owner;
		return 0x424322;
	}

	return 0;
}

DEFINE_HOOK(0x45197B, BuildingClass_UpdateAnim_SetOwner, 0x6)
{
	GET(AnimClass*, pThis, EBP);
	if (pThis)
	{
		GET(TechnoClass*, pBuilding, ESI);
		SetAnimOwner(pThis, pBuilding);
		// Building Anim is not attach to the building
		AnimStatus* status = nullptr;
		if (TryGetStatus<AnimExt>(pThis, status))
		{
			GET(CoordStruct*, pOffset, EBX);
			status->Offset = *pOffset;
			status->pAttachOwner = pBuilding;
			status->pCreater = pBuilding; // Building's anim bind to building
		}
	}
	return 0;
}

DEFINE_HOOK(0x423630, AnimClass_Draw_Colour, 0x6)
{
	GET(AnimClass*, pAnim, ESI);

	if (pAnim)
	{
		AnimStatus* animStatus = nullptr;
		if (pAnim->IsBuildingAnim)
		{
			TechnoClass* pCreater = nullptr;
			TechnoStatus* technoStatus = nullptr;
			if (TryGetStatus<AnimExt, AnimStatus>(pAnim, animStatus)
				&& animStatus->TryGetCreater(pCreater)
				&& TryGetStatus<TechnoExt, TechnoStatus>(pCreater, technoStatus))
			{
				technoStatus->DrawSHP_Paintball_BuildingAnim(R);
			}

		}
		else if (TryGetStatus<AnimExt, AnimStatus>(pAnim, animStatus))
		{
			animStatus->DrawSHP_Paintball(R);
		}
	}

	return 0;
}

#pragma endregion

#pragma region AnimType Damage

// Takes over all damage from animations, including Ares
DEFINE_HOOK(0x424513, AnimClass_Update_Explosion, 0x6)
{
	GET(AnimClass*, pThis, ESI);
	AnimStatus* status = nullptr;
	if (CombatDamage::Data()->AllowAnimDamageTakeOverByKratos && TryGetStatus<AnimExt>(pThis, status))
	{
		status->Explosion_Damage();
		return 0x42464C;
	}

	return 0;
}

// 碎片、流星敲地板触发，砸水中不触发
DEFINE_HOOK(0x423E7B, AnimClass_Extras_Explosion, 0xA)
{
	GET(AnimClass*, pThis, ESI);

	AnimStatus* status = nullptr;
	if (CombatDamage::Data()->AllowAnimDamageTakeOverByKratos && TryGetStatus<AnimExt>(pThis, status))
	{
		status->Explosion_Damage(true, true);
		return 0x423EFD;
	}

	return 0;
}

// Take over to create Extras Anim when Meteor/Debris hit the water
// Phobos hook on 0x423CC7 and skip all game code, so it won't work with Phobos
DEFINE_HOOK(0x423CD5, AnimClass_Extras_HitWater, 0x6)
{
	GET(AnimClass*, pThis, ESI);

	AnimStatus* status = nullptr;
	if (TryGetStatus<AnimExt>(pThis, status))
	{
		if (CombatDamage::Data()->AllowAnimDamageTakeOverByKratos && CombatDamage::Data()->AllowDamageIfDebrisHitWater)
		{
			status->Explosion_Damage(true);
		}
	}
	// 接管砸在水中的动画
	ExpireAnimData* data = INI::GetConfig<ExpireAnimData>(INI::Art, pThis->Type->ID)->Data;
	CoordStruct location = pThis->GetCoords();
	// 涟漪
	AnimTypeClass* pWake = RulesClass::Instance->Wake;
	if (IsNotNone(data->WakeAnimOnWater))
	{
		pWake = AnimTypeClass::Find(data->WakeAnimOnWater.c_str());
		if (!pWake)
		{
			Debug::Log("Warning: Anim %s try to create a unknow wake anim %s.\n", pThis->Type->ID, data->WakeAnimOnWater.c_str());
		}
	}
	if (pWake)
	{
		AnimClass* pNewAnim = GameCreate<AnimClass>(pWake, location);
		pNewAnim->Owner = pThis->Owner;
	}
	// 水花
	AnimTypeClass* pSplash = nullptr;
	if (IsNotNone(data->ExpireAnimOnWater))
	{
		pSplash = AnimTypeClass::Find(data->ExpireAnimOnWater.c_str());
		if (!pSplash)
		{
			Debug::Log("Warning: Anim %s try to create a unknow splash anim %s.\n", pThis->Type->ID, data->ExpireAnimOnWater.c_str());
		}
	}
	else
	{
		// 流星是大水花，碎片是小水花
		pSplash = pThis->Type->IsMeteor ? *RulesClass::Instance->SplashList.back() : *RulesClass::Instance->SplashList.front();
	}
	if (pSplash)
	{
		location.Z += 3;
		AnimClass* pNewAnim = GameCreate<AnimClass>(pSplash, location);
		pNewAnim->Owner = pThis->Owner;
	}
	return 0x423EFD;
}

// ★ AE 动画图层跟随（InAir 覆盖修复的第一环）：
//   Kratos 的 AE 动画刻意不设置 OwnerObject（强关联会在移除时产生指针错误），
//   因此引擎原版 AnimClass::In_WhichLayer (0x424CB0) 对这些动画永远返回 Type->Layer。
//   本 hook 让带附着对象（AnimStatus::pAttachOwner，仅 AE Idle/Hit 动画会设置）的动画
//   取 max(Type->Layer, 主身 LastLayer)，只升不降，把动画放进**主身所在的那个层数组**。
//   它只覆盖"跨层"的两类情形：
//     a) 宿主是 Fly 机动的飞机 —— 全引擎唯一 In_Which_Layer 会返回 Top(4) 的机动
//        （实测 0x4CFCF0：GetHeight()>0 ⇒ 4，否则 2；对照 YRpp/GeneralDefinitions.h:885），
//        而 [AnimType] Layer 默认是 Air(3)（实测 0x4276D4: mov [esi+364h],3）；
//     b) 动画 art 里显式写了 Layer=ground/surface/underground 而宿主已升空。
//   ⚠️ 对 Jumpjet 宿主（Kirov/ZEP 等）本 hook **恒为空转**：空中 JJ 被本项目的
//      0x54B8E9 钉死在 Air(3)，与 Layer 默认值 3 相等 ⇒ 条件为假。那种"同层内谁盖谁"
//      的问题由 AnimStatus::OnUpdate 的重提交解决（第二环），两环分工不可互替 ——
//      重提交只能在**同一层数组内**重排，无法把动画搬进宿主所在的层数组。
//
//   ⚠️ 落点选 0x424CCA，而不是被 Phobos 占用的原版 0x424CB0（Phobos/Anim/Hooks.cpp:352）：
//      · 0x424CB0 → 0x424CCA 之间 ECX 只被读、从未改写，pThis 语义完全不变；
//        且该函数入口只经虚表 vt+0x78 进入（0x5F3FFA `mov ecx,esi` ⇒ ECX=对象），
//        实测 0x424CCA 无任何直接控制流引用（只可能从 0x424CB0 顺序落入）；
//      · 该处只在 OwnerObject == nullptr 且 Type != nullptr 的分支上，天然只作用于
//        "没有强关联"的 Kratos AE 动画，不再干扰 vanilla"有 OwnerObject 恒 Ground(2)"的判定；
//      · 不再与 Phobos 同址 —— SyringeEx 的链式 hook 一旦本 hook 返回非 0 就会跳过
//        链上后面的 Phobos hook（SyringeDebugger.cpp:575-588、518-520）。
//      · 6 字节恰好一条完整指令（mov eax,[eax+364h]），回跳契约：`return 0` 重放 6 字节后
//        落在 0x424CCF 的 NOP → 0x424CD0 的 `C3 retn`；覆盖时显式返回 0x424CD0。
//
//   ⚠️ 取 LastLayer(ObjectClass+0x94) 而非 pOwner->InWhichLayer()：
//      LastLayer 是宿主**当前所在层数组**的索引（只由 DisplayClass::Submit 写，
//      0x4A9762），与 AnimStatus 的换层监视器同源。这样不会出现"宿主 InWhichLayer()
//      已变而尚未重提交、动画先跳层"⇒ 宿主后提交把动画压回去 的反向遮挡；
//      同时彻底不触碰 FootClass 的 locomotor(+0x674)，没有额外虚调用与空指针路径。
//   参考：Phobos AnimClass_InWhichLayer_AttachedObjectLayer（走 OwnerObject 路径，机制不同）。
DEFINE_HOOK(0x424CCA, AnimClass_InWhichLayer_FollowAttachOwner, 0x6)
{
	GET(AnimClass*, pThis, ECX);

	if (pThis->Type)
	{
		AnimStatus* status = nullptr;
		if (TryGetStatus<AnimExt>(pThis, status) && status->pAttachOwner)
		{
			TechnoClass* pOwner = nullptr;
			if (CastToTechno(status->pAttachOwner, pOwner) && pOwner->IsAlive)
			{
				const Layer masterLayer = pOwner->LastLayer;
				if (masterLayer > pThis->Type->Layer)
				{
					R->EAX(masterLayer);
					return 0x424CD0;
				}
			}
		}
	}
	return 0;
}

#pragma endregion
