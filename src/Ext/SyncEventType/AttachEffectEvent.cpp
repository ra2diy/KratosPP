#include <string>
#include <vector>

#include <ObjectClass.h>
#include <TechnoClass.h>
#include <HouseClass.h>

#include <Extension/TechnoExt.h>

#include <Ext/Common/SyncEventManager.h>
#include <Ext/Helper/CastEx.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>
#include <Ext/ObjectType/AttachEffect.h>
#include <Ext/SyncEventType/AttachEffectEvent.h>
#include <Ext/TechnoType/HotKeyAttachData.h>

namespace
{
	/// <summary>
	/// 收端与发端预筛共用：对"目标 × 组号"跑一遍配置与过滤器。
	/// dryRun = true 时只判定（不附加），用于发端少发无用事件。
	/// 判据全部只读同步状态（目标类型/存活、AE 管理器标记、house 关系），两端结果必然一致。
	/// </summary>
	bool ApplyHotKeyAttach(TechnoClass* pTechno, HouseClass* pHouse, int group, bool dryRun)
	{
		if (!pTechno || !pHouse)
		{
			return false;
		}
		if (!pTechno->IsAlive || pTechno->Health <= 0 || pTechno->InLimbo)
		{
			return false;
		}
		TechnoTypeClass* pType = pTechno->GetTechnoType();
		if (!pType)
		{
			return false;
		}

		HotKeyAttachTypeData* typeData = HotKeyAttachTypeData::Get(pType->ID);
		if (!typeData || !typeData->Enable)
		{
			return false;
		}

		AttachEffect* aeManager = GetAEManager<TechnoExt>(pTechno);
		if (!aeManager)
		{
			return false;
		}

		std::vector<std::string> marks = aeManager->GetMarks();

		for (auto& kv : typeData->Datas)
		{
			HotKeyAttachData& data = kv.second;
			if (!data.IsEnable || !data.IsOnKey(group))
			{
				continue;
			}
			if (!data.AffectInAir && pTechno->IsInAir())
			{
				continue;
			}
			if (!data.AffectStand && AmIStand(pTechno))
			{
				continue;
			}
			if (!data.CanAffectHouse(pHouse, pTechno->Owner))
			{
				continue;
			}
			if (!data.CanAffectType(pTechno))
			{
				continue;
			}
			if (!data.OnMark(marks))
			{
				continue;
			}
			if (dryRun)
			{
				return true;
			}
			// 附加AE
			aeManager->Attach(data.AttachEffects, data.AttachChances, false, nullptr, pHouse);
		}
		return false;
	}

	/// <summary>发端：对单个目标发起（内含与收端同判据的预筛）。</summary>
	bool RaiseToTargetInternal(HouseClass* pHouse, TechnoClass* pTarget, int group)
	{
		if (!pHouse || !pTarget)
		{
			return false;
		}
		if (!ApplyHotKeyAttach(pTarget, pHouse, group, true))
		{
			return false;
		}

		AttachEffectEvent::EventData data{ TargetClass(pTarget), (uint8_t)group };
		return SyncEventManager::Raise(AttachEffectEvent::Id, pHouse, data);
	}
}

namespace AttachEffectEvent
{
	void Respond(SyncEventManager::Event* pEvent)
	{
		HouseClass* pHouse = SyncEventManager::GetEventHouse(pEvent);
		if (!pHouse)
		{
			return;
		}
		EventData* data = SyncEventManager::GetPayload<EventData>(pEvent);
		// 用 RTTI + ID 重新取对象，绝不使用发起端的本地指针
		TechnoClass* pTechno = data->Target.As_Techno();
		if (!pTechno)
		{
			return;
		}
		ApplyHotKeyAttach(pTechno, pHouse, (int)data->Group, false);
	}

	void Raise(int group)
	{
		HouseClass* pPlayer = HouseClass::CurrentPlayer;
		if (!pPlayer || pPlayer->Defeated)
		{
			return;
		}

		for (int i = 0; i < ObjectClass::CurrentObjects->Count; i++)
		{
			ObjectClass* pObject = ObjectClass::CurrentObjects->GetItem(i);
			if (!pObject)
			{
				continue;
			}
			TechnoClass* pTechno = abstract_cast<TechnoClass*>(pObject);
			if (pTechno)
			{
				RaiseToTargetInternal(pPlayer, pTechno, group);
			}
		}
	}
}

namespace AttachEffectEvent
{
	// 供后续触发器（点击 / 图标）复用：对单个目标发起
	void RaiseToTarget(HouseClass* pHouse, TechnoClass* pTarget, int group)
	{
		RaiseToTargetInternal(pHouse, pTarget, group);
	}
}

// 注册给同步事件通道（号 0x50，载荷 EventData，收端 AttachEffectEvent::Respond）
namespace
{
	const SyncEventManager::Registrar _attachEffectReg(AttachEffectEvent::Id, sizeof(AttachEffectEvent::EventData), &AttachEffectEvent::Respond);
}
