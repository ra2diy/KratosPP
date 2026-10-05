#include "../TechnoStatus.h"

#include <Extension/WarheadTypeExt.h>

#include <Ext/Helper/Scripts.h>

#include <Ext/EffectType/AttachEffectScript.h>
#include <Ext/EffectType/Effect/StandEffect.h>
#include <Ext/SyncEventType/TechnoScriptCommandEvent.h>

void TechnoStatus::SetupStand(StandData data, TechnoClass* pMaster)
{
	MyStandData = data;
	pMyMaster = pMaster;

	EventSystems::General.AddHandler(Events::ObjectUnInitEvent, this, &TechnoStatus::OnTechnoDelete);
}

bool TechnoStatus::AmIStand()
{
	// 抛射体上的替身可能会因为抛射体的攻击者已经死亡而MyMaster为空
	return pMyMaster || MyMasterIsAnim || MyStandData.Enable;
}

void TechnoStatus::OnPut_Stand(CoordStruct* pCoord, DirType dirType)
{
	if (AmIStand())
	{
		if (MyStandData.Immune)
		{
			TechnoExt::ImmuneStandArray[pTechno] = MyStandData;
		}
		else
		{
			TechnoExt::StandArray[pTechno] = MyStandData;
		}
	}
	else if (VirtualUnit)
	{
		// 单位既是替身又是虚单位，只加入替身清单
		TechnoExt::VirtualUnitArray.push_back(pTechno);
	}
	// 虚单位拔地而起
	if (VirtualUnit)
	{
		pTechno->UpdatePlacement(PlacementType::Remove);
	}
}

void TechnoStatus::OnRemove_Stand()
{
	auto it = TechnoExt::StandArray.find(pTechno);
	if (it != TechnoExt::StandArray.end())
	{
		TechnoExt::StandArray.erase(it);
	}

	auto it2 = TechnoExt::ImmuneStandArray.find(pTechno);
	if (it2 != TechnoExt::ImmuneStandArray.end())
	{
		TechnoExt::ImmuneStandArray.erase(it2);
	}

	auto it3 = std::find(TechnoExt::VirtualUnitArray.begin(), TechnoExt::VirtualUnitArray.end(), pTechno);
	if (it3 != TechnoExt::VirtualUnitArray.end())
	{
		TechnoExt::VirtualUnitArray.erase(it3);
	}
}

void TechnoStatus::OnReceiveDamage_Stand(args_ReceiveDamage* args)
{
	// 无视防御的真实伤害不做任何分摊
	WarheadTypeExt::TypeData* whData = nullptr;
	if (!args->IgnoreDefenses && !MyMasterIsAnim && TryGetTypeData<WarheadTypeExt, WarheadTypeExt::TypeData>(args->WH, whData) && !whData->IgnoreStandShareDamage)
	{
		if (pMyMaster)
		{
			// I'm stand
			if (*args->Damage >= 0 || MyStandData.AllowShareRepair)
			{
				if (MyStandData.Immune)
				{
					*args->Damage = 0;
				}
				else if (MyStandData.DamageToMaster > 0 && !IsDeadOrInvisible(pMyMaster))
				{
					// 伤害分摊给jojo
					int damage = *args->Damage;
					int to = (int)(damage * MyStandData.DamageToMaster);
					*args->Damage = damage - to;
					pMyMaster->ReceiveDamage(&to, args->DistanceToEpicenter, args->WH, args->Attacker, args->IgnoreDefenses, args->PreventsPassengerEscape, args->SourceHouse);
				}
			}
		}
		else
		{
			// I'm master
			int damage = *args->Damage;
			AttachEffect* aem = AEManager();
			aem->ForeachChild([&](Component* c) {
				if (auto ae = dynamic_cast<AttachEffectScript*>(c))
				{
					if (ae->AEData.Stand.Enable
						&& !ae->AEData.Stand.Immune
						&& (damage >= 0 || ae->AEData.Stand.AllowShareRepair)
						&& ae->AEData.Stand.DamageFromMaster > 0)
					{
						if (StandEffect* standEffect = ae->GetComponent<StandEffect>())
						{
							if (!IsDead(standEffect->pStand))
							{
								int to = (int)(damage * standEffect->Data->DamageFromMaster);
								damage -= to;
								standEffect->pStand->ReceiveDamage(&to, args->DistanceToEpicenter, args->WH, args->Attacker, args->IgnoreDefenses, args->PreventsPassengerEscape, args->SourceHouse);
							}
						}
					}
				}
				});
			*args->Damage = damage;
		}
	}
}

void TechnoStatus::OnRegisterDestruction_Stand(TechnoClass* pKiller, int cost, bool& skip)
{
	if (cost != 0 && !MyMasterIsAnim && !IsDead(pKiller))
	{
		// Killer是Stand，而且Master可训练
		TechnoStatus* killerStatue = nullptr;
		if (TryGetStatus<TechnoExt>(pKiller, killerStatue) && killerStatue->AmIStand() && !IsDead(killerStatue->pMyMaster))
		{
			TechnoClass* pMaster = killerStatue->pMyMaster;
			if (killerStatue->MyMasterIsSpawned && killerStatue->MyStandData.ExperienceToSpawnOwner && !IsDead(pMaster->SpawnOwner))
			{
				pMaster = pMaster->SpawnOwner;
			}
			if (pMaster->GetTechnoType()->Trainable)
			{
				int transExp = 0;
				if (pKiller->GetTechnoType()->Trainable)
				{
					// 替身可以训练，经验部分转给使者
					int exp = cost;
					// 替身已经满级
					if (!pKiller->Veterancy.IsElite())
					{
						transExp = cost;
						exp = 0;
					}
					if (!pMaster->Veterancy.IsElite())
					{
						// 使者还能获得经验，转移部分给使者
						transExp = (int)(cost * killerStatue->MyStandData.ExperienceToMaster);
						exp -= transExp;
					}
					// 剩余部分自己享用
					if (exp != 0)
					{
						int technoCost = pKiller->GetTechnoType()->GetActualCost(pKiller->Owner);
						pKiller->Veterancy.Add(technoCost, exp);
					}
				}
				else
				{
					// 替身不能训练，经验全部转给使者
					transExp = cost;
				}
				if (transExp != 0)
				{
					int technoCost = pMaster->GetTechnoType()->GetActualCost(pMaster->Owner);
					pMaster->Veterancy.Add(technoCost, transExp);
				}

				skip = true;
			}
		}
	}
}

bool TechnoStatus::OnSelect_VirtualUnit()
{
	if (pMyMaster && MyStandData.Enable && MyStandData.SelectToMaster)
	{
		pMyMaster->Select();
	}
	return !VirtualUnit;
}

void TechnoStatus::OnGuardCommand_Stand()
{
	if (AmIStand())
	{
		// 替身收到 Guard：补发**原版** Guard 命令（＝"它也被选中按了 G"）。
		// ★ 恰好一条、且两端判定可复现：只在"本机是发起者"（`IsCurrentInitiator()` ＝
		//   事件 house == 本机 CurrentPlayer，由 Respond 派发前设置）**且**"引擎没发过"
		//   （载荷 bit7，`IsEngineAlreadySent()`：对象当时在选中集里）时才调用。
		//   发起端正是**原版热键会调用这一句的那台客户端**（原版 0x730DEB/0x730E56 就在按键端的
		//   v_CurrentObjects 循环里调 `ClickedMission`+`GetCellAgain`）⇒ 每台机器的行为都等价于
		//   "原版按下 G 时它该做的事" ⇒ 对称。该论证**不依赖** `ClickedMission` 没有本地副作用
		//   （它在 planning 分支只构造+入队，正常分支还会播语音并 `Queue_Mission`）。
		if (!TechnoScriptCommandEvent::IsEngineAlreadySent() && TechnoScriptCommandEvent::IsCurrentInitiator())
		{
			CellClass* pCell = pTechno->GetCellAgain();
			pTechno->ClickedMission(Mission::Area_Guard, reinterpret_cast<ObjectClass*>(pCell), nullptr, nullptr);
		}
	}
}

void TechnoStatus::OnStopCommand_Stand()
{
	if (AmIStand())
	{
		// 同 OnGuardCommand_Stand：Stop 的原版命令是那条 IDLE（vtable+0x374）。
		if (!TechnoScriptCommandEvent::IsEngineAlreadySent() && TechnoScriptCommandEvent::IsCurrentInitiator())
		{
			pTechno->ClickedEvent(NetworkEvents::Idle);
		}
	}
}
