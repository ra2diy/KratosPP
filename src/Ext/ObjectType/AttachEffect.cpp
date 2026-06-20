#include "AttachEffect.h"

#include <BuildingClass.h>
#include <MissionClass.h>

#include <Common/INI/INI.h>

#include <Ext/Helper/DrawEx.h>
#include <Ext/Helper/Finder.h>
#include <Ext/Helper/MathEx.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>
#include <Ext/Helper/SurfaceEx.h>
#include <Ext/Helper/Weapon.h>
#include <Ext/Common/PrintTextManager.h>

#include <Ext/EffectType/AttachEffectScript.h>
#include <Ext/EffectType/Effect/AttackBeaconEffect.h>
#include <Ext/EffectType/Effect/CounterEffect.h>
#include <Ext/EffectType/Effect/StandEffect.h>
#include <Ext/EffectType/Effect/VectorEffect.h>
#include <Ext/EffectType/Effect/TurretSpinEffect.h>
#include <Ext/EffectType/Effect/BodySpinEffect.h>
#include <Ext/BulletType/BulletStatus.h>
#include <Ext/TechnoType/TechnoStatus.h>
#include <Ext/TechnoType/UploadAttachData.h>
#include <Ext/WeaponType/FeedbackAttachData.h>

#include <Ext/StateType/State/AntiBulletData.h>
#include <Ext/StateType/State/DestroyAnimData.h>
#include <Ext/StateType/State/DestroySelfData.h>
#include <Ext/StateType/State/GiftBoxData.h>
#include <Ext/StateType/State/PaintballData.h>
#include <Ext/StateType/State/TeleportData.h>
#include <Ext/StateType/State/TransformData.h>

BulletStatus* AttachEffect::GetBulletStatus()
{
	BulletStatus* status = nullptr;
	if (pBullet && _parent)
	{
		status = _parent->GetComponent<BulletStatus>();
	}
	return status;
}

TechnoStatus* AttachEffect::GetTechnoStatus()
{
	TechnoStatus* status = nullptr;
	if (pTechno && _parent)
	{
		status = _parent->GetComponent<TechnoStatus>();
	}
	return status;
}

void AttachEffect::OnDestroySelf()
{
	_ownerIsDead = true;
}

bool AttachEffect::OwnerIsDead()
{
	if (!_ownerIsDead)
	{
		if (pBullet && IsDead(pBullet))
		{
			_ownerIsDead = true;
		}
		else if (pTechno && (IsDead(pTechno) || InSelling())) // 变卖了建筑也算死亡
		{
			_ownerIsDead = true;
		}
	}
	return _ownerIsDead;
}

int AttachEffect::Count()
{
	return _children.size();
}

bool AttachEffect::AddCounter(AttachEffectData data, CounterEffect* counter)
{
	std::string mark = data.Counter.Mark;
	auto it = Counters.find(mark);
	if (it == Counters.end())
	{
		Counters[mark] = counter;
		return true;
	}
	return false;
}

void AttachEffect::RemoveCounter(AttachEffectData data)
{
	std::string mark = data.Counter.Mark;
	auto it = Counters.find(mark);
	if (it != Counters.end())
	{
		Counters.erase(it);
	}
}

void AttachEffect::GetMarks(std::vector<std::string>& marks)
{
	ForeachChild([&marks](Component* c) {
		if (auto ae = dynamic_cast<AttachEffectScript*>(c)) {
			ae->GetMarks(marks);
		}
		});
}

std::vector<std::string> AttachEffect::GetMarks()
{
	std::vector<std::string> marks{};
	GetMarks(marks);
	return marks;
}

void AttachEffect::GetAENames(std::vector<std::string>& names)
{
	ForeachChild([&names](Component* c) {
		if (auto ae = dynamic_cast<AttachEffectScript*>(c))
		{
			names.push_back(ae->AEData.Name);
		}
		});
}

bool AttachEffect::HasStand()
{
	bool find = false;
	ForeachChild([&find](Component* c) {
		if (auto ae = dynamic_cast<AttachEffectScript*>(c))
		{
			find = ae->AEData.Stand.Enable;
			if (find)
			{
				ae->Break();
			}
		}
		});
	return find;
}

#define GET_STAND_STATE(CLASS_NAME) \
 			else if (dynamic_cast<CLASS_NAME ## Data*>(pData)) \
			{ \
				state = status->CLASS_NAME; \
			} \

void AttachEffect::AEStateToStand(EffectData* pData, int duration, std::string token, bool resetDuration)
{
	ForeachChild([&pData, &duration, &token, &resetDuration](Component* c) {
		if (auto ae = dynamic_cast<AttachEffectScript*>(c)) {
			if (ae->AEData.Stand.Enable && ae->IsAlive())
			{
				if (StandEffect* standEffect = ae->GetComponent<StandEffect>())
				{
					TechnoStatus* status = nullptr;
					if (standEffect->IsAlive() && TryGetStatus<TechnoExt>(standEffect->pStand, status))
					{
						IStateScript* state = nullptr;
						// TODO Get stand's state

						// 状态机
						if (state) {}
						GET_STAND_STATE(AntiBullet)
							GET_STAND_STATE(BlackHole)
							GET_STAND_STATE(DamageReaction)
							GET_STAND_STATE(Deselect)
							GET_STAND_STATE(DestroyAnim)
							GET_STAND_STATE(DestroySelf)
							GET_STAND_STATE(DisableWeapon)
							GET_STAND_STATE(Freeze)
							GET_STAND_STATE(GiftBox)
							GET_STAND_STATE(NoMoneyNoTalk)
							GET_STAND_STATE(OverrideWeapon)
							GET_STAND_STATE(Paintball)
							GET_STAND_STATE(Pump)
							GET_STAND_STATE(Scatter)
							GET_STAND_STATE(Teleport)
							GET_STAND_STATE(Transform)
							if (state)
							{
								if (resetDuration)
								{
									state->ResetDuration(duration, token);
								}
								else
								{
									state->Replace(pData, duration, token, ae);
								}
							}
					}
				}
			}
		}
		});
}

CrateBuffData AttachEffect::CountAttachStatusMultiplier()
{
	CrateBuffData multiplier{};
	// 替身是虚拟炮塔，buff加成取JOJO身上的
	if (pTechno)
	{
		TechnoStatus* status = GetStatus<TechnoExt, TechnoStatus>(pTechno);
		if (status->AmIStand() && status->MyStandData.IsVirtualTurret
			&& !status->MyMasterIsAnim && !IsDead(status->pMyMaster)
			)
		{
			AttachEffect* aem = nullptr;
			if (TryGetAEManager<TechnoExt>(status->pMyMaster, aem))
			{
				return aem->CountAttachStatusMultiplier();
			}
		}
	}
	// 统计AE加成
	ForeachChild([&multiplier](Component* c) {
		auto temp = dynamic_cast<AttachEffectScript*>(c);
		if (temp && temp->IsAlive() && !temp->IsPaused() && temp->AEData.CrateBuff.Enable)
		{
			multiplier.FirepowerMultiplier *= temp->AEData.CrateBuff.FirepowerMultiplier;
			multiplier.ArmorMultiplier *= temp->AEData.CrateBuff.ArmorMultiplier;
			multiplier.SpeedMultiplier *= temp->AEData.CrateBuff.SpeedMultiplier;
			multiplier.ROFMultiplier *= temp->AEData.CrateBuff.ROFMultiplier;
			multiplier.Cloakable |= temp->AEData.CrateBuff.Cloakable;
			multiplier.ForceDecloak |= temp->AEData.CrateBuff.ForceDecloak;
			multiplier.RangeMultiplier *= temp->AEData.CrateBuff.RangeMultiplier;
			multiplier.RangeCell += temp->AEData.CrateBuff.RangeCell;
		}
		});
	return multiplier;
}

VectorResult AttachEffect::MarginVectorOffset()
{
	VectorResult result;
	CoordStruct currentPos = pObject->GetCoords();
	ForeachChild([&result, &currentPos](Component* c) {
		auto temp = dynamic_cast<AttachEffectScript*>(c);
		if (temp && temp->IsAlive() && !temp->IsPaused())
		{
			// Vector（替代 V1，包括 Freeze）
			if (temp->AEData.Vector.Enable)
			{
				if (auto* ve = temp->GetComponent<VectorEffect>())
				{
					VectorResult tempResult = ve->GetVectorResult();
					result.MoveDisp.X += tempResult.MoveDisp.X;
					result.MoveDisp.Y += tempResult.MoveDisp.Y;
					result.MoveDisp.Z += tempResult.MoveDisp.Z;
					result.Freeze |= tempResult.Freeze;
					result.Force |= tempResult.Force;
					result.AllowFallingDestroy |= tempResult.AllowFallingDestroy;
				result.AllowRotateUnit |= tempResult.AllowRotateUnit;
					if (tempResult.FallingDestroyHeight > result.FallingDestroyHeight)
						result.FallingDestroyHeight = tempResult.FallingDestroyHeight;
					if (tempResult.Freeze && !tempResult.FrozenPos.IsEmpty())
					{
						result.FrozenPos = tempResult.FrozenPos;
					}
				}
			}
		}
	});
	return result;
}

ImmuneData AttachEffect::GetImmuneData()
{
	ImmuneData data{};
	// 统计AE加成
	ForeachChild([&data](Component* c) {
		auto temp = dynamic_cast<AttachEffectScript*>(c);
		if (temp && temp->IsAlive() && !temp->IsPaused() && temp->AEData.Immune.Enable)
		{
			data.Psionics |= temp->AEData.Immune.Psionics;
			data.PsionicWeapons |= temp->AEData.Immune.PsionicWeapons;
			data.Radiation |= temp->AEData.Immune.Radiation;
			data.Poison |= temp->AEData.Immune.Poison;
			data.EMP |= temp->AEData.Immune.EMP;
			data.Parasite |= temp->AEData.Immune.Parasite;
			data.Temporal |= temp->AEData.Immune.Temporal;
			data.IsLocomotor |= temp->AEData.Immune.IsLocomotor;
			std::vector<std::string> antiWH = temp->AEData.Immune.AntiWarheads;
			data.AntiWarheads.insert(data.AntiWarheads.end() ,antiWH.begin(), antiWH.end());
			std::vector<std::string> acceptWH = temp->AEData.Immune.AcceptWarheads;
			data.AcceptWarheads.insert(data.AcceptWarheads.end(), acceptWH.begin(), acceptWH.end());
		}
		});
	data.CheckEnable();
	return data;
}

void AttachEffect::SetLocationSpace(int cabinLength)
{
	_locationSpace = cabinLength;
	if (cabinLength < _locationMarkDistance)
	{
		_locationMarkDistance = cabinLength;
	}
}

void AttachEffect::Attach(AttachEffectTypeData* typeData)
{
	if (typeData->Enable)
	{
		Attach(typeData->AttachEffectTypes, {}, _attachOnceFlag, pObject);
	}
	if (typeData->StandTrainCabinLength > 0)
	{
		SetLocationSpace(typeData->StandTrainCabinLength);
	}

}

void AttachEffect::Attach(std::vector<std::string> types, std::vector<double> chances, bool onceCheck,
	ObjectClass* pSource, HouseClass* pSourceHouse,
	CoordStruct warheadLocation, int aeMode, bool fromPassenger)
{
	if (!types.empty())
	{
		int index = 0;
		for (std::string type : types)
		{
			if (Bingo(chances, index))
			{
				Attach(type, onceCheck, pSource, pSourceHouse, warheadLocation, aeMode, fromPassenger);
			}
			index++;
		}
	}
}

void AttachEffect::Attach(std::string type, bool onceCheck,
	ObjectClass* pSource, HouseClass* pSourceHouse,
	CoordStruct warheadLocation, int aeMode, bool fromPassenger)
{
	if (IsNotNone(type))
	{
		AttachEffectData* data = INI::GetConfig<AttachEffectData>(INI::Rules, type.c_str())->Data;
		if (data->Enable)
		{
			if (onceCheck && data->AttachOnceInTechnoType)
			{
				return;
			}
			Attach(*data, pSource, pSourceHouse, warheadLocation, aeMode, fromPassenger);
		}
	}
}

void AttachEffect::Attach(AttachEffectData data,
	ObjectClass* pSource, HouseClass* pSourceHouse,
	CoordStruct warheadLocation, int aeMode, bool fromPassenger)
{
	if (!data.Enable)
	{
		Debug::Log("Warning: Attemp to attach an invalid AE [%s] to [%s]\n", data.Name.c_str(), pObject->GetType()->ID);
		return;
	}
	// 检查附加前提
	if (!IsAvailable(data))
	{
		return;
	}
	// 检查是否穿透铁幕
	if (!data.PenetratesIronCurtain && pObject->IsIronCurtained())
	{
		return;
	}
	// 是否在白名单上
	if (!data.CanAffectType(pObject))
	{
		return;
	}
	// 是否需要标记
	if (!IsOnMark(data))
	{
		return;
	}
	// 是否有排斥的AE
	if (HasContradiction(data))
	{
		return;
	}

	TechnoClass* pAttacker = nullptr;
	HouseClass* pAttackingHouse = pSourceHouse;
	// 调整所属
	if (!pSource)
	{
		pSource = pObject;
	}
	TechnoClass* pSourceTechno = nullptr;
	BulletClass* pSourceBullet = nullptr;
	if (CastToTechno(pSource, pSourceTechno))
	{
		pAttacker = pSourceTechno;
		if (!pAttackingHouse)
		{
			pAttackingHouse = pAttacker->Owner;
		}
	}
	else if (CastToBullet(pSource, pSourceBullet))
	{
		pAttacker = pSourceBullet->Owner;
		if (!pAttackingHouse)
		{
			pAttackingHouse = GetSourceHouse(pSourceBullet);
		}
	}
	else
	{
		Debug::Log("Warning: Attach AE [%s] to [%s] form a unknow source!\n", data.Name.c_str(), pObject->GetType()->ID);
		return;
	}
	// 更改所属，如果需要
	if (data.ReceiverOwn && pSource != pObject)
	{
		// 所属设置为接受者
		if (pSourceTechno)
		{
			pAttackingHouse = pSourceTechno->Owner;
		}
		else if (pSourceBullet)
		{
			pAttackingHouse = GetSourceHouse(pSourceBullet);
		}
	}
	// 调整攻击者
	if (!fromPassenger && data.FromTransporter && !IsDead(pAttacker))
	{
		pAttacker = WhoIsShooter(pAttacker);
	}
	// 检查金币
	int needMoney = data.AttachNeedMoney;
	if (needMoney != 0)
	{
		if (pAttackingHouse)
		{
			int money = pAttackingHouse->Available_Money();
			if (needMoney > 0 ? money < needMoney : money > abs(needMoney))
			{
				return;
			}
		}
		else
		{
			Debug::Log("Warning: Attach AE [%s] to [%s] get a unknow source house when check AttachNeedMoney!\n", data.Name.c_str(), pObject->GetType()->ID);
			return;
		}
	}
	needMoney = data.ReceiverNeedMoney;
	if (needMoney != 0)
	{
		if (HouseClass* myHouse = pObject->GetOwningHouse())
		{
			int money = pAttackingHouse->Available_Money();
			if (needMoney > 0 ? money < needMoney : money > abs(needMoney))
			{
				return;
			}
		}
		else
		{
			Debug::Log("Warning: Attach AE [%s] to [%s] get a unknow source house when check ReceiverNeedMoney!\n", data.Name.c_str(), pObject->GetType()->ID);
			return;
		}
	}

	// 检查叠加
	bool add = data.Cumulative == CumulativeMode::YES;
	if (!add)
	{
		// 不同攻击者是否叠加
		bool isAttackMark = fromPassenger && data.Cumulative != CumulativeMode::NO || data.Cumulative == CumulativeMode::ATTACKER && pAttacker && pAttacker->IsAlive;
		// 不同所属是否叠加
		bool isHouseMark = data.Cumulative == CumulativeMode::HOUSE;
		// 攻击者标记AE名称相同，但可以来自不同的攻击者，可以叠加，不检查Delay
		// 检查冷却计时器
		if (!isAttackMark && !isHouseMark && IsOnDelay(data))
		{
			return;
		}

		bool findSameAE = false; // 是否找到同名AE
		bool findSameGroup = false; // 是否找到同组AE
		bool overrideSameGroup = false; // 是否覆盖同组AE
		bool needToAddAE = true; // 是否需要添加AE

		CounterEffect* targetCounter = nullptr; // 目标计数器

		std::string group = data.Group;
		bool hasGroup = IsNotNone(group);
		bool checkCounter = data.Counter.Enable;

		// 检查持续时间，增减Duration；检查Counter，增减计数器
		ForeachChild([&](Component* c) {
				auto temp = dynamic_cast<AttachEffectScript*>(c);
				if (!temp || !temp->IsAlive())
				{
					return;
				}

				// 检查AE匹配
				if (!hasGroup)
				{
					// 无分组，攻击者标记叠加，或同名重置计时器
					if (!findSameAE && temp->AEData.Name == data.Name)
					{
						// 检查标记匹配
						bool markMatched = false;
						if (!isAttackMark && !isHouseMark)
						{
							markMatched = true;
						}
						else if (isAttackMark && temp->pSource == pAttacker)
						{
							markMatched = true;
						}
						else if (isHouseMark && temp->pSourceHouse == pAttackingHouse)
						{
							markMatched = true;
						}
						if (markMatched)
						{
							findSameAE = true;
							// 重置持续时间，并跳出循环
							if (temp->AEData.ResetDurationOnReapply)
							{
								temp->ResetDuration();
							}

							// 检查这个匹配AE的计数器
							if (checkCounter && !targetCounter)
							{
								targetCounter = temp->GetComponent<CounterEffect>();
								if (targetCounter && targetCounter->Data->Mark != data.Counter.Mark)
								{
									targetCounter = nullptr;
								}
							}
						}
					}
				}
				else
				{
					// 有分组，检查同组AE，调整持续时间
					std::string tempGroup = temp->AEData.Group;
					if (IsNotNone(tempGroup) && group == tempGroup)
					{
						findSameGroup = true;
						if (data.OverrideSameGroup)
						{
							// 与自己不同名的，替换
							if (temp->AEData.Name != data.Name)
							{
								// 不同名同组，执行替换操作，关闭所有的同组AE
								temp->TimeToDie();
								overrideSameGroup = true;
							}
							else if (temp->AEData.ResetDurationOnReapply)
							{
								// 同名且同组，重置持续时间
								temp->ResetDuration();
								findSameAE = true;
							}
						}
						else
						{
							// 不进行覆盖，同组调整持续时间
							temp->MergeDuration(data.Duration);
							findSameAE = true;

							// 检查这个匹配AE的计数器
							if (!targetCounter && checkCounter)
							{
								targetCounter = temp->GetComponent<CounterEffect>();
								if (targetCounter && targetCounter->Data->Mark != data.Counter.Mark)
								{
									targetCounter = nullptr;
								}
							}
						}
					}
				}

				// 同名或同组计数器都没找到，单独查找计数器
				if (checkCounter && !targetCounter)
				{
					if (CounterEffect* counterEffect = temp->GetComponent<CounterEffect>())
					{
						// 找到计数器
						if (counterEffect->Data->Mark == data.Counter.Mark)
						{
							targetCounter = counterEffect;
						}
					}
				}

			});

		// 确定是否需要添加这个AE
		if (hasGroup)
		{
			// 有分组的情况
			needToAddAE = add || !findSameGroup || overrideSameGroup;
			// 检查持续时间要求
			if (!data.HoldDuration && data.GetDuration() <= 0) {
				needToAddAE = false;
			}
		}
		else
		{
			// 无分组的情况
			needToAddAE = add || !findSameAE;
		}

		// 处理计数器
		if (checkCounter)
		{
			if (targetCounter)
			{
				if (needToAddAE)
				{
					// 找到计数器，且AE应该被添加时，执行计数操作
					double num = data.Counter.Num;
					if (data.Counter.NumType != CounterType::Number)
					{
						ObjectClass* pFrom = data.Counter.NumFromSource ? pAttacker : pObject;
						if (pFrom && !IsDeadOrInvisible(pFrom))
						{
							switch (data.Counter.NumType)
							{
							case CounterType::HP:
								num = pFrom->Health;
								break;
							case CounterType::MaxHP:
								if (data.Counter.NumFromSource)
								{
									num = pFrom->GetType()->Strength;
								}
								else
								{
									num = IsBullet() ? pFrom->Health : pFrom->GetType()->Strength;
								}
								break;
							}
						}
					}
					targetCounter->ModifyCount(data.Counter.Action, num);
					add = false;
				}
				else
				{
					add = false;
				}
			}
			else
			{
				// 没有找到计数器
				if (needToAddAE)
				{
					// 允许添加新的AE实例
					add = data.Counter.CanAttach();
				}
				else
				{
					add = false;
				}
			}
		}
		else
		{
			// 没有计数器，直接添加AE
			add = needToAddAE;
		}
	}
	// 可以添加AE，开始执行添加动作
	if (add && data.GetDuration() != 0 && StackNotFull(data))
	{
		int index = FindInsertIndex(data);
		Component* c = AddComponent(AttachEffectScript::ScriptName, index); // 插队
#ifdef DEBUG_AE
		Debug::Log("[%s]%d 添加新的AE[%s]%d\n", pObject->GetType()->ID, pObject, data.Name.c_str(), c);
#endif // DEBUG_AE
		if (c)
		{
			AddStackCount(data); // 叠层计数
			// 初始化AE
			auto ae = dynamic_cast<AttachEffectScript*>(c);
			ae->AEData = data;
			// 激活AE
			ae->EnsureAwaked();
			ae->Start(pAttacker, pAttackingHouse, warheadLocation, aeMode, fromPassenger);
		}

	}
}

bool AttachEffect::IsAvailable(AttachEffectData data)
{
	// 检查是否持有建筑
	if (data.CheckBuildings)
	{
		HouseClass* pHouse = pObject->GetOwningHouse();
		if (pHouse)
		{
			auto IsBuildingPresent = [pHouse](std::string type)
				{
					BuildingTypeClass* pType = BuildingTypeClass::Find(type.c_str());
					if (pType)
					{
						// 检查是否为加载物
						if (BuildingTypeClass::Find(pType->PowersUpBuilding))
						{
							return GetUpgradesAmount(pType, pHouse) > 0;
						}
						else
						{
							return pHouse->CountOwnedAndPresent(pType) > 0;

						}
					}
					return false;
				};

			if (!data.AuxBuildings.empty() && std::none_of(data.AuxBuildings.begin(), data.AuxBuildings.end(), IsBuildingPresent))
			{
				return false;
			}

			if (!data.NegBuildings.empty() && std::any_of(data.NegBuildings.begin(), data.NegBuildings.end(), IsBuildingPresent))
			{
				return false;
			}
		}
	}
	return true;
}

void AttachEffect::FeedbackAttach(WeaponTypeClass* pWeapon)
{
	// Feedback
	if (pWeapon && !IsDeadOrInvisible(pObject))
	{
		FeedbackAttachData* typeData = INI::GetConfig<FeedbackAttachData>(INI::Rules, pWeapon->ID)->Data;
		if (typeData->Enable)
		{
			bool inTransporter = pTechno && pTechno->Transporter != nullptr;
			TechnoClass* pTransporter = nullptr;
			AttachEffect* pTransporterAEM = nullptr;
			if (inTransporter)
			{
				pTransporter = WhoIsShooter(pTechno);
				pTransporterAEM = GetAEManager<TechnoExt>(pTransporter);
			}
			std::map<int, FeedbackAttachEntity> aeTypes = typeData->Datas;
			for (auto it = aeTypes.begin(); it != aeTypes.end(); it++)
			{
				FeedbackAttachEntity data = it->second;
				if (data.Enable)
				{
					// 检查所属是否平民
					if (pTechno && data.DeactiveWhenCivilian && IsCivilian(pTechno->Owner))
					{
						continue;
					}
					if (pTechno)
					{
						if (data.AffectTechno)
						{
							TechnoClass* tempTechno = pTechno;
							AttachEffect* tempAEM = this;
							// 载具内且能赋予载具，则赋予，不能则跳过
							if (inTransporter)
							{
								if (!pTransporterAEM)
								{
									continue;
								}
								tempTechno = pTransporter;
								tempAEM = pTransporterAEM;
							}
							// 检查是否可以影响并赋予AE
							if (data.CanAffectType(tempTechno)
								&& (data.AffectInAir || !tempTechno->IsInAir())
								// && (e.AffectStand || !AmIStand(tempTechno))
								&& tempAEM->IsOnMark(data)
								)
							{
								tempAEM->Attach(data.AttachEffects, data.AttachChances);
							}
						}
					}
					else if (data.AffectBullet && pBullet)
					{
						if (data.CanAffectType(pBullet) && IsOnMark(data))
						{
							Attach(data.AttachEffects, data.AttachChances);
						}
					}
				}
			}
		}
	}
}

void AttachEffect::AttachUploadAE()
{
	// 读取所有的乘客，获取Id清单，并依据乘客的设置，为载具添加AE
	PassengerIds.clear();
	// 坦克碉堡
	if (IsBuilding())
	{
		CheckAndAttachUploadAE(pTechno->BunkerLinkedItem);
	}
	// 乘客舱
	if (pTechno->Passengers.NumPassengers > 0)
	{
		// 有乘客
		ObjectClass* pPassenger = pTechno->Passengers.FirstPassenger;
		do
		{
			CheckAndAttachUploadAE(abstract_cast<TechnoClass*>(pPassenger));
		} while ((pPassenger = pPassenger->NextObject) != nullptr);
	}
}

void AttachEffect::CheckAndAttachUploadAE(TechnoClass* pPassenger)
{
	if (!IsDead(pPassenger))
	{
		// 查找乘客身上的AEMode设置
		AttachEffect* aeManager = nullptr;
		if (TryGetAEManager<TechnoExt>(pPassenger, aeManager))
		{
			int aeMode = aeManager->GetTypeData()->AEMode;
			if (aeMode >= 0)
			{
				PassengerIds.push_back(aeMode);
			}
		}
		// 查找乘客身上的AE设置，赋予载具
		UploadAttachData* uploadData = INI::GetConfig<UploadAttachData>(INI::Rules, pPassenger->GetType()->ID)->Data;
		if (uploadData->Enable)
		{
			std::map<int, UploadAttachEntity> uploadDatas = uploadData->Datas;
			for (auto it = uploadDatas.begin(); it != uploadDatas.end(); it++)
			{
				UploadAttachEntity e = it->second;
				if (e.Enable
					&& e.CanAffectType(pTechno)
					&& (e.AffectInAir || !pTechno->IsInAir())
					// && (e.AffectStand || !AmIStand(pTechno))
					&& this->IsOnMark(e)
					)
				{
					Attach(e.AttachEffects, {}, false, pPassenger, nullptr, CoordStruct::Empty, -1, e.SourceIsPassenger);
				}
			}
		}
	}
}

void AttachEffect::AttachGroupAE()
{
	if (GetGroupData()->Enable)
	{
		std::map<int, AttachEffectTypeData> groupData = GetGroupData()->Datas;
		for (auto it = groupData.begin(); it != groupData.end(); it++)
		{
			AttachEffectTypeData aeType = it->second;
			if (aeType.AttachByPassenger)
			{
				// 该组AE需要乘客进行激活
				int aeMode = aeType.AEModeIndex;
				auto ite = std::find(PassengerIds.begin(), PassengerIds.end(), aeMode);
				if (ite != PassengerIds.end())
				{
					// 乘客中有这个ID
					Attach(aeType.AttachEffectTypes, {}, false, pObject, nullptr, CoordStruct::Empty, aeMode, false);
				}
			}
			else
			{
				// 该组AE不需要乘客激活，直接赋予
				Attach(&aeType);
			}
		}
	}
}

#define ATTACH_EFFECT_ON_TECHNOTYPE(EFFECT_NAME) \
		EFFECT_NAME ## Data* _ ## EFFECT_NAME ## Data = INI::GetConfig<EFFECT_NAME ## Data>(INI::Rules, section.c_str())->Data; \
		if (_ ## EFFECT_NAME ## Data->Enable) \
		{ \
			AttachEffectData aeData; \
			aeData.Enable = true; \
			aeData.Name = section + #EFFECT_NAME; \
			aeData. ## EFFECT_NAME = *_ ## EFFECT_NAME ## Data; \
			Attach(aeData); \
		} \

void AttachEffect::AttachStateEffect()
{
	//TODO Attach Effect on TechnoType
	std::string section = pObject->GetType()->ID;
	if (IsNotNone(section))
	{
		ATTACH_EFFECT_ON_TECHNOTYPE(AttackBeacon);
		ATTACH_EFFECT_ON_TECHNOTYPE(DamageSelf);
		ATTACH_EFFECT_ON_TECHNOTYPE(DestroyAnim);
		ATTACH_EFFECT_ON_TECHNOTYPE(ExtraFire);
		ATTACH_EFFECT_ON_TECHNOTYPE(FireSuper);
		ATTACH_EFFECT_ON_TECHNOTYPE(NoMoneyNoTalk);
	}
}

void AttachEffect::DetachByName(std::vector<std::string> aeTypes, bool skipNext)
{
	ForeachChild([&aeTypes, &skipNext](Component* c) {
		auto ae = dynamic_cast<AttachEffectScript*>(c);
		// 通过名字关闭掉AE
		if (ae && std::find(aeTypes.begin(), aeTypes.end(), ae->AEData.Name) != aeTypes.end())
		{
			ae->SkipNext = skipNext;
			ae->TimeToDie();
		}
		});
}

void AttachEffect::DetachByName(std::map<std::string, int> aeTypes, bool skipNext)
{
	std::vector<std::string> names;
	std::map<std::string, int> levels;
	std::map<std::string, int> counts;
	for (auto ae : aeTypes)
	{
		std::string name = ae.first;
		int level = ae.second;
		names.push_back(name);
		levels[name] = level;
		counts[name] = 0;
	}
	if (!names.empty())
	{
		ForeachChild([&](Component* c) {
			auto ae = dynamic_cast<AttachEffectScript*>(c);
			std::string name = ae->AEData.Name;
			// 通过名字关闭掉AE
			if (ae && std::find(names.begin(), names.end(), name) != names.end())
			{
				int l = levels[name];
				if (l < 0 || counts[name] < l)
				{
					ae->SkipNext = skipNext;
					ae->TimeToDie();
					counts[name]++;
				}
			}
			});
	}
}

void AttachEffect::DetachByMarks(std::vector<std::string> marks, bool skipNext)
{
	ForeachChild([&marks, &skipNext](Component* c) {
		auto ae = dynamic_cast<AttachEffectScript*>(c);
		// 通过标记关闭掉AE
		if (ae->AEData.Mark.Enable && CheckOnMarks(marks, ae->AEData.Mark.Names))
		{
			ae->SkipNext = skipNext;
			ae->TimeToDie();
		}
		});
}

void AttachEffect::DetachByToken(std::string token, bool skipNext)
{
	if (!token.empty())
	{
		ForeachChild([&token, &skipNext](Component* c) {
			auto ae = dynamic_cast<AttachEffectScript*>(c);
			// 通过Token关闭掉AE
			if (ae && ae->Token == token)
			{
				ae->SkipNext = skipNext;
				ae->TimeToDie();
			}
			});
	}
}

void AttachEffect::DetachWhenTransform()
{
	ForeachChild([](Component* c) {
		auto ae = dynamic_cast<AttachEffectScript*>(c);
		// 通过标记关闭掉AE
		if (ae->AEData.DiscardOnTransform && !ae->AEData.Transform.Enable)
		{
			ae->TimeToDie();
		}
		});
}

void AttachEffect::CheckDurationAndDisable(bool silence)
{
	CoordStruct location = _location;
	ForeachChild([&](Component* c) {
		if (auto ae = dynamic_cast<AttachEffectScript*>(c))
		{
			// 执行IsAlive时，检查AE的生命状态，失效的AE会在这里被标记为Deactivate
			if (!ae->IsAlive())
			{
				AttachEffectData data = ae->AEData;
				// 结束AE
				ae->End(location);
				if (!silence)
				{
					// 加入冷却计时器
					StartDelay(data);
#ifdef DEBUG_AE
					Debug::Log("  - [%s]%d 关闭AE [%s]%d ,加入冷却计时 %d, %s附加NextAE[%s]\n", pObject->GetType()->ID, pObject, data.Name.c_str(), c, data.Delay, ae->SkipNext ? "不" : "", data.Next.c_str());
#endif // DEBUG_AE
					if (!ae->SkipNext)
					{
						// 添加NextAE，Next升级为清单
						if (!IsDeadOrInvisible(pObject) && !data.Next.empty())
						{
							for (std::string nextAE : data.Next)
							{
								if (IsNotNone(nextAE))
								{
									Attach(nextAE, false, ae->pSource, ae->pSourceHouse);
								}
							}
						}
					}
				}
				// 减少堆叠计数
				ReduceStackCount(data);
				// 移除计数器
				if (data.Counter.Enable)
				{
					RemoveCounter(data);
				}
				// Deactivate的组件不会再执行Foreach事件，标记为失效，以便父组件将其删除
				ae->Disable();
#ifdef DEBUG_AE
				Debug::Log("  - [%s]%d 移除AE [%s]%d\n", pObject->GetType()->ID, pObject, data.Name.c_str(), c);
#endif // DEBUG_AE
			}
		}
		});
}

AttachEffectTypeData* AttachEffect::GetTypeData()
{
	if (!_typeData)
	{
		_typeData = INI::GetConfig<AttachEffectTypeData>(INI::Rules, pObject->GetType()->ID)->Data;
	}
	return _typeData;
}

AttachEffectGroupData* AttachEffect::GetGroupData()
{
	if (!_groupData)
	{
		_groupData = INI::GetConfig<AttachEffectGroupData>(INI::Rules, pObject->GetType()->ID)->Data;
	}
	return _groupData;
}

bool AttachEffect::IsOnMark(FilterData data)
{
	if (data.HasMarks())
	{
		std::vector<std::string> marks;
		GetMarks(marks);
		return data.OnMark(marks);
	}
	return true;
}

bool AttachEffect::HasContradiction(AttachEffectData data)
{
	std::vector<std::string> names;
	GetAENames(names);
	return data.HasContradiction(names);
}

bool AttachEffect::IsOnDelay(AttachEffectData data)
{
	std::string name = data.Name;
	auto it = DisableDelayTimers.find(name);
	if (it != DisableDelayTimers.end())
	{
		return it->second.InProgress();
	}
	return false;
}

void AttachEffect::StartDelay(AttachEffectData data)
{
	std::string name = data.Name;
	auto it = DisableDelayTimers.find(name);
	if (it == DisableDelayTimers.end() || it->second.Expired())
	{
		int delay = GetRandomValue(data.RandomDelay, data.Delay);
		if (delay > 0)
		{
			CDTimerClass timer{ delay };
			DisableDelayTimers[name] = timer;
		}
	}
}

void AttachEffect::AddStackCount(AttachEffectData data)
{
	std::string name = data.Name;
	auto it = AEStacks.find(name);
	if (it != AEStacks.end())
	{
		it->second++;
	}
	else
	{
		AEStacks[name] = 1;
	}
}

void AttachEffect::ReduceStackCount(AttachEffectData data)
{
	std::string name = data.Name;
	auto it = AEStacks.find(name);
	if (it != AEStacks.end())
	{
		it->second--;
		if (it->second < 1)
		{
			AEStacks.erase(it);
		}
	}
}

bool AttachEffect::StackNotFull(AttachEffectData data)
{
	if (data.MaxStack > 0)
	{
		std::string name = data.Name;
		auto it = AEStacks.find(name);
		if (it != AEStacks.end())
		{
			return it->second < data.MaxStack;
		}
	}
	return true;
}

CoordStruct AttachEffect::StackOffset(AttachEffectData aeData, OffsetData offsetData,
	StackOffsetMap<std::string, CoordStruct>& offsetMarks,
	StackOffsetMap<std::string, CoordStruct>& groupMarks,
	StackOffsetMap<std::string, CoordStruct>& groupFirstMarks)
{
	CoordStruct offset = offsetData.Offset;

	if (IsNotNone(offsetData.StackGroup))
	{
		// 分组堆叠
		std::string stackGroup = offsetData.StackGroup;

		// 线性查找
		auto it = std::find_if(groupMarks.begin(), groupMarks.end(),
			[&stackGroup](const std::pair<std::string, CoordStruct>& pair) {
				return pair.first == stackGroup;
			});
		if (it != groupMarks.end())
		{
			// 有记录，往上堆叠
			CoordStruct offsetMark = it->second;
			offset = offsetMark + offsetData.StackOffset;
			it->second = offset;
		}
		else
		{
			// 没有记录，取最后一个组的初始偏移位置，加上组偏移
			if (!groupFirstMarks.empty())
			{
				offset = groupFirstMarks.back().second + offsetData.StackGroupOffset;
			}
			// 创建新的分组
			groupMarks.emplace_back(stackGroup, offset);
			groupFirstMarks.emplace_back(stackGroup, offset);
		}
	}
	else if (!offsetData.StackOffset.IsEmpty())
	{
		// 无分组堆叠
		std::string aeName = aeData.Name;

		// 线性查找
		auto it = std::find_if(offsetMarks.begin(), offsetMarks.end(),
			[&aeName](const std::pair<std::string, CoordStruct>& pair) {
				return pair.first == aeName;
			});
		if (it != offsetMarks.end())
		{
			// 需要进行偏移
			CoordStruct offsetMark = it->second;
			offset = offsetMark + offsetData.StackOffset;
			it->second = offset;
		}
		else
		{
			offsetMarks.emplace_back(aeName, offset);
		}
	}
	return offset;
}

int AttachEffect::FindInsertIndex(AttachEffectData data)
{
	int index = -1;
	int size = -1;
	if (data.Stand.Enable && data.Stand.IsTrain && (size = _children.size()) > 0)
	{
		// Head or Trail
		if (data.Stand.CabinHead)
		{
			// check group
			if (data.Stand.CabinGroup > -1)
			{
				// Find the first same group cabin but reverse
				for (auto it = _children.rbegin(); it != _children.rend(); it++)
				{
					AttachEffectScript* ae = dynamic_cast<AttachEffectScript*>(*it);
					if (ae && ae->IsAlive() && ae->AEData.Stand.Enable && ae->AEData.Stand.IsTrain)
					{
						if (ae->AEData.Stand.CabinGroup == data.Stand.CabinGroup)
						{
							index = size - 1;
							break;
						}
					}
					size--;
				}

			}
		}
		else
		{
			index = 0;
			// check group
			if (data.Stand.CabinGroup > -1)
			{
				// Find the first same group cabin
				int i = 0;
				for (Component*& c : _children)
				{
					auto ae = dynamic_cast<AttachEffectScript*>(c);
					if (ae && ae->IsAlive() && ae->AEData.Stand.Enable && ae->AEData.Stand.IsTrain)
					{
						if (ae->AEData.Stand.CabinGroup == data.Stand.CabinGroup)
						{
							index = i;
							break;
						}
					}
					i++;
				}
			}
		}
	}
	return index;
}

bool AttachEffect::UpdateTrainStandLocation(AttachEffectScript* ae, int& markIndex)
{
	// 查找可以用的记录点
	double length = 0;
	LocationMark preMark;
	for (std::size_t j = markIndex; j < _locationMarks.size(); j++)
	{
		markIndex = j;
		LocationMark mark = _locationMarks[j];
		if (preMark.IsEmpty())
		{
			preMark = mark;
			continue;
		}
		length += mark.Location.DistanceFrom(preMark.Location);
		preMark = mark;
		if (length >= _locationSpace)
		{
			break;
		}
	}

	if (!preMark.IsEmpty())
	{
		ae->UpdateStandLocation(preMark);
		return true;
	}
	return false;
}

CoordStruct AttachEffect::MarkLocation()
{
	CoordStruct location = pObject->GetCoords();
	if (_lastLocation.IsEmpty())
	{
		_lastLocation = location;
	}
	else
	{
		double mileage = location.DistanceFrom(_lastLocation);
		if (!isnan(mileage) && mileage > _locationMarkDistance)
		{
			_lastLocation = location;
			double tempMileage = _totalMileage + mileage;
			// 记录下当前的位置
			OffsetData offset{};
			LocationMark mark = GetRelativeLocation(pObject, offset);
			// 插入队头
			_locationMarks.insert(_locationMarks.begin(), mark);
			// 检查容量(当前AE数量+1)，弹出队尾
			if (tempMileage > (Count() + 1) * _locationSpace)
			{
				_locationMarks.pop_back();
			}
			else
			{
				_totalMileage = tempMileage;
			}
		}
	}
	return location;
}

void AttachEffect::ClearLocationMarks()
{
	_locationMarks.clear();
	_lastLocation = CoordStruct::Empty;
	_totalMileage = 0;
}

void AttachEffect::OnGScreenRender(EventSystem* sender, Event e, void* args)
{
	if (!pObject)
	{
		return;
	}
	CoordStruct location = pObject->GetCoords();
	if (args)
	{
		// EndRender
		ForeachChild([&location](Component* c) {
			if (auto ae = dynamic_cast<AttachEffectScript*>(c)) { ae->OnGScreenRenderEnd(location); }
			});
#ifdef DEBUG
		if (Common::DebugAE)
		{
			int offsetZ = PrintTextManager::GetFontSize().Y;
			// 打印Component结构
			/// GameObject
			///		|__ TechnoStatus
			///				|__ AntiBullet
			///				|__ BlackHole
			///				|__ GiftBoxState
			///				|__ Paintball
			///				|__ ...
			///				|__ Transform
			///		|__ AttachEffect
			///				|__ AttachEffectScript#0
			///						|__ EffectScript#0
			///						|__ EffectScript#1
			///				|__ AttachEffectScript#1
			///						|__ EffectScript#0
			///						|__ EffectScript#1
			///						|__ EffectScript#2
			///		|__ HealthText
			///		|__ AircraftGuard
			///		|__ ...
			///		|__ TechnoTrail
			if (_gameObject)
			{
				int ii = 0;
				std::vector<Component::ComponentInfo> infos;
				if (pTechno)
				{
					std::string uiName = std::string{ "" }.append("[").append(pTechno->GetTechnoType()->ID).append("]")
						.append(WString2String(pTechno->GetTechnoType()->UIName));
					infos.push_back(Component::ComponentInfo{ uiName, !pTechno->InLimbo });
					ii = -1;
				}
				else if (pBullet)
				{
					std::string uiName = std::string{ "" }.append("[").append(pBullet->GetType()->ID).append("]");
					infos.push_back(Component::ComponentInfo{ uiName, !pBullet->InLimbo });
					ii = -1;
				}
				int level = 0;
				GetComponentInfos(infos, level);
				Point2D pos = ToClientPos(location);
				for (Component::ComponentInfo& info : infos)
				{
					if (ii++ == 0) continue;
					std::string log{ info.Name };
					log.append("\n");
					pos.Y += offsetZ;
					ColorStruct color = Colors::Green;
					if (!info.Active)
					{
						color = Colors::Red;
					}
					PrintTextManager::PrintText(log, color, pos);
				}
			}
			// 打印叠层信息
			Point2D pos2 = ToClientPos(location);
			for (auto it = AEStacks.begin(); it != AEStacks.end(); it++)
			{
				std::string log;
				log.append(it->first).append(" : ").append(std::to_string(it->second)).append("\n");
				pos2.Y -= offsetZ;
				PrintTextManager::PrintText(log, Colors::Red, pos2);
			}
		}
#endif // DEBUG
	}
	else
	{
		// BeginRender
		// 替身的定位偏移
		static StackOffsetMap<std::string, CoordStruct> standMarks{};
		static StackOffsetMap<std::string, CoordStruct> standGroupMarks{};
		static StackOffsetMap<std::string, CoordStruct> standGroupFirstMarks{};
		// 动画的定位偏移
		static StackOffsetMap<std::string, CoordStruct> animMarks{};
		static StackOffsetMap<std::string, CoordStruct> animGroupMarks{};
		static StackOffsetMap<std::string, CoordStruct> animGroupFirstMarks{};
		// 叠层信息
		static StackOffsetMap<std::string, CoordStruct> stackMarks{};
		static StackOffsetMap<std::string, CoordStruct> stackGroupMarks{};
		static StackOffsetMap<std::string, CoordStruct> stackGroupFirstMarks{};

		// 清空偏移记录
		standMarks.clear();
		standGroupMarks.clear();
		standGroupFirstMarks.clear();
		animMarks.clear();
		animGroupMarks.clear();
		animGroupFirstMarks.clear();
		stackMarks.clear();
		stackGroupMarks.clear();
		stackGroupFirstMarks.clear();

		// 火车的位置索引
		int markIndex = 0;
		ForeachChild([&](Component* c) {
			if (auto ae = dynamic_cast<AttachEffectScript*>(c))
			{
				if (ae->IsAlive())
				{
					AttachEffectData aeData = ae->AEData;
					// 调整替身的位置
					if (aeData.Stand.Enable)
					{
						// 调整火车替身的位置
						if (!aeData.Stand.IsTrain || !UpdateTrainStandLocation(ae, markIndex))
						{
							// 堆叠偏移
							OffsetData offsetData = aeData.Stand.Offset;
							CoordStruct standOffset = this->StackOffset(aeData, offsetData, standMarks, standGroupMarks, standGroupFirstMarks);
							LocationMark locationMark = GetRelativeLocation(pObject, offsetData, standOffset);
							ae->UpdateStandLocation(locationMark);
						}
					}
					// 调整动画的位置
					if (aeData.Animation.Enable && aeData.Animation.IdleAnim.Enable)
					{
						OffsetData offsetData = aeData.Animation.IdleAnim.Offset;
						CoordStruct animOffset = this->StackOffset(aeData, offsetData, animMarks, animGroupMarks, animGroupFirstMarks);
						ae->UpdateAnimOffset(animOffset);
					}
					// 调整Info.Stack的位置
					if (aeData.Info.Enable && aeData.Info.Stack.Enable)
					{
						OffsetData offsetData = aeData.Info.Stack.Offset;
						CoordStruct infoOffset = this->StackOffset(aeData, offsetData, stackMarks, stackGroupMarks, stackGroupFirstMarks);
						ae->UpdateInfoOffset(infoOffset);
					}
					ae->OnGScreenRender(location);
				}
			}
			});
	}
}

void AttachEffect::OnUpdate()
{
	// 移除失效的AE，附加Next的AE
	CheckDurationAndDisable();

	// 添加Section上记录的AE
	if (!_ownerIsDead)
	{
		_location = MarkLocation();
		// 检查电力
		if (!IsBullet())
		{
			PowerOff = pTechno->Owner->HasLowPower();
			if (!PowerOff && IsBuilding())
			{
				// 关闭当前建筑电源
				PowerOff = !abstract_cast<BuildingClass*, true>(pTechno)->HasPower;
			}
		}

		// 添加section自带AE
		Attach(GetTypeData());
		// 检查乘客并附加乘客带来的AE
		if (pTechno)
		{
			// 赋予乘客带来的AE，由于乘客不会在非OpenTopped的载具内执行update事件，因此由乘客向载具赋予AE的任务也由载具执行
			AttachUploadAE();
		}
		// 赋予自带的多组AE
		AttachGroupAE();

		this->_attachOnceFlag = true;
	}
}

void AttachEffect::OnWarpUpdate()
{
	// 移除失效的AE，附加Next的AE
	CheckDurationAndDisable();
	if (!_ownerIsDead)
	{
		_location = MarkLocation();
	}
}

void AttachEffect::OnPut(CoordStruct* pCoord, DirType dirType)
{
	_location = *pCoord;
	// 部分AE以状态方式写在TechnoType标签里，在初始化时，自动附加一个AE
	if (!_attachStateEffectFlag)
	{
		_attachStateEffectFlag = true;

		AttachStateEffect();
	}
}

void AttachEffect::OnRemove()
{
	// 从地图移除时关闭AE
	CoordStruct location = pObject->GetCoords();
	_location = location;
	ClearLocationMarks();
	ForeachChild([&location](Component* c) {
		if (auto ae = dynamic_cast<AttachEffectScript*>(c))
		{
			if (ae->AEData.DiscardOnEntry)
			{
				ae->End(location);
			}
		}
		});
}

void AttachEffect::CanFire(AbstractClass* pTarget, WeaponTypeClass* pWeapon, bool& ceaseFire)
{
	if (pTarget && pWeapon && pWeapon->Warhead)
	{
		// 如果目标免疫超时空和磁电，不能攻击
		TechnoClass* pTargetTechno = nullptr;
		AttachEffect* aem = nullptr;
		if (CastToTechno(pTarget, pTargetTechno) && TryGetAEManager<TechnoExt>(pTargetTechno, aem))
		{
			ImmuneData data = aem->GetImmuneData();
			if (data.Enable)
			{
				// 免疫超时空和磁电
				if ((pWeapon->Warhead->Temporal && data.Temporal)
					|| (pWeapon->Warhead->IsLocomotor && data.IsLocomotor))
				{
					ceaseFire = true;
					Break();
				}
			}
		}
	}
}

void AttachEffect::OnFire(AbstractClass* pTarget, int weaponIdx)
{
	WeaponStruct* pWeapon = pTechno->GetWeapon(weaponIdx);
	WeaponTypeClass* pWeaponType = nullptr;
	if (pWeapon && (pWeaponType = pWeapon->WeaponType) != nullptr)
	{
		FeedbackAttach(pWeaponType);
	}
}

void AttachEffect::OnReceiveDamageDestroy()
{
	_ownerIsDead = true;
};

void AttachEffect::OnDetonate(CoordStruct* pCoords, bool& skip)
{
	_ownerIsDead = true;
	_location = *pCoords;
}

void AttachEffect::OnUnInit()
{
	_ownerIsDead = true;
	CoordStruct location = _location;
	ForeachChild([&location](Component* c) {
		if (auto ae = dynamic_cast<AttachEffectScript*>(c))
		{
			ae->TimeToDie();
			ae->End(location);
		}
		});
}

// 造了一个 Kratos 版本的 AE 转移，检查的 DiscardOnTransform 值
void AttachEffect::TransferAttachedEffects(TechnoClass* pSourceTechno, TechnoClass* pTargetTechno)
{
	if (!pSourceTechno || !pTargetTechno)
		return;

	AttachEffect* pSourceAE = nullptr;
	AttachEffect* pTargetAE = nullptr;
	TryGetAEManager<TechnoExt>(pSourceTechno, pSourceAE);
	TryGetAEManager<TechnoExt>(pTargetTechno, pTargetAE);

	if (!pSourceAE || !pTargetAE)
		return;

	struct AEInfo {
		AttachEffectData Data;
		ObjectClass* Source;
		HouseClass* SourceHouse;
		CoordStruct WarheadLocation;
		int Mode;
		bool FromPassenger;
		int RemainingDuration;   // 剩余时间
	};
	std::vector<AEInfo> aeToTransfer;

	pSourceAE->ForeachChild([&](Component* c) {
		auto pAEScript = dynamic_cast<AttachEffectScript*>(c);
		if (!pAEScript || !pAEScript->IsAlive())
			return;

		AttachEffectData data = pAEScript->AEData;

		// 检查 DiscardOnTransform 决定
		if (data.DiscardOnTransform)
			return;

		// 检查目标单位是否可接受此类型
		if (!data.CanAffectType(pTargetTechno))
			return;

		int timeLeft = -1;
		pAEScript->TryGetDurationTimeLeft(timeLeft);  // 获取剩余时间

		aeToTransfer.push_back({ data, pAEScript->pSource, pAEScript->pSourceHouse, pAEScript->WarheadLocation, pAEScript->AEMode, pAEScript->FromPassenger, timeLeft });
		});

	if (aeToTransfer.empty())
		return;

	// 转移剩余时间
	for (auto& info : aeToTransfer)
	{
		if (info.RemainingDuration > -1)
		{
			info.Data.Duration = info.RemainingDuration;
			info.Data.HoldDuration = false;
		}

		pTargetAE->Attach(info.Data, info.Source, info.SourceHouse,
			info.WarheadLocation, info.Mode, info.FromPassenger);
	}

	// 关闭源单位上已转移的 AE
	pSourceAE->ForeachChild([&](Component* c) {
		auto pAEScript = dynamic_cast<AttachEffectScript*>(c);
		if (!pAEScript || !pAEScript->IsAlive())
			return;

		AttachEffectData data = pAEScript->AEData;
		if (data.DiscardOnTransform)
			return;
		if (!data.CanAffectType(pTargetTechno))
			return;

		pAEScript->TimeToDie();
		pAEScript->End(pSourceTechno->GetCoords());
		});

	TechnoStatus* pTargetStatus = nullptr;
	if (TryGetStatus<TechnoExt, TechnoStatus>(pTargetTechno, pTargetStatus)) {
		pTargetStatus->RecalculateStatus();
	}
	pTargetTechno->MarkForRedraw();
}
