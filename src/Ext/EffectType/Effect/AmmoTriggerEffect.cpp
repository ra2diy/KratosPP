#include "AmmoTriggerEffect.h"

#include <Ext/Helper/Finder.h>
#include <Ext/Helper/FLH.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>

bool AmmoTriggerEffect::CanActive(double num, Point2D range)
{
	if (range.Y >= range.X)
	{
		return num >= range.X && (range.Y < 0 || num <= range.Y);
	}
	return true;
}

void AmmoTriggerEffect::Watch()
{
	TechnoClass* pTechno = abstract_cast<TechnoClass*, true>(pObject);
	if (!pTechno) return;

	double ammoValue = static_cast<double>(pTechno->Ammo);

	// key：0 = 无序号触发器（与 AmmoTrigger0 同槽），i = AmmoTrigger<i>；map 升序遍历
	for (auto& [actionIdx, entity] : Data->Actions)
	{
		if (CanActive(ammoValue, entity.Range))
		{
			// 命中时，按当前弹药数值缓存本次动作执行次数，后续修改弹药不影响本次执行
			int times = entity.TriggerNumTimes ? (int)ammoValue : 1;
			// 操作弹药
			if (entity.Num != 0)
			{
				double num = entity.Num;
				if (entity.NumType != AmmoType::Number)
				{
					ObjectClass* pFrom = pObject;
					switch (entity.NumFrom)
					{
					case AmmoTriggerWho::SOURCE:
						pFrom = abstract_cast<ObjectClass*>(AE->pSource);
						break;
					}
					if (pFrom && !IsDeadOrInvisible(pFrom))
					{
						switch (entity.NumType)
						{
						case AmmoType::HP:
							num = pFrom->Health;
							break;
						case AmmoType::MaxHP:
							if (entity.NumFrom != AmmoTriggerWho::ME)
							{
								num = pFrom->GetType()->Strength;
							}
							else
							{
								num = IsBullet() ? pFrom->Health : pFrom->GetType()->Strength;
							}
							break;
						case AmmoType::Ammo:
						{
							TechnoClass* pFromTech = abstract_cast<TechnoClass*, true>(pFrom);
							if (pFromTech) num = pFromTech->Ammo;
							break;
						}
						case AmmoType::MaxAmmo:
						{
							TechnoClass* pFromTech = abstract_cast<TechnoClass*, true>(pFrom);
							if (pFromTech) num = pFromTech->GetTechnoType()->Ammo;
							break;
						}
						}
					}
				}

				// 直接修改 pTechno->Ammo
				int intNum = static_cast<int>(num);
				int maxAmmo = pTechno->GetTechnoType()->Ammo;
				if (maxAmmo > 0)
				{
					AmmoAction action = entity.Action;
					if (entity.ResetNum) action = AmmoAction::INIT;
					switch (action)
					{
					case AmmoAction::INIT:
						pTechno->Ammo = intNum;
						break;
					case AmmoAction::ADD:
						pTechno->Ammo += intNum;
						break;
					case AmmoAction::SUB:
						pTechno->Ammo -= intNum;
						break;
					case AmmoAction::MUL:
						pTechno->Ammo = static_cast<int>(pTechno->Ammo * num);
						break;
					}
					// 钳位
					if (pTechno->Ammo < 0) pTechno->Ammo = 0;
					if (pTechno->Ammo > maxAmmo) pTechno->Ammo = maxAmmo;
				}
			}

			// 附加AE
			if (entity.Attach)
			{
				AttachEffect* aeManager = AE->AEManager;
				switch (entity.AttachTo)
				{
				case AmmoTriggerWho::SOURCE:
					aeManager = GetAEManager<TechnoExt>(AE->pSource);
					break;
				}

				if (aeManager)
				{
					TechnoClass* pSource = AE->pSource;
					HouseClass* pSourceHouse = AE->pSourceHouse;
					switch (entity.AttachFrom)
					{
					case AmmoTriggerWho::ME:
						pSource = nullptr;
						pSourceHouse = nullptr;
						break;
					}
					// 附加AE，按当前弹药数值执行N次，能否附加由AE自身判断
					for (int i = 0; i < times; i++)
					{
						aeManager->Attach(entity.AttachEffects, entity.AttachChances, false, pSource, pSourceHouse);
					}
				}
			}

			// 移除AE
			if (entity.Remove)
			{
				AttachEffect* aeManager = AE->AEManager;
				switch (entity.RemoveWho)
				{
				case AmmoTriggerWho::SOURCE:
					aeManager = GetAEManager<TechnoExt>(AE->pSource);
					break;
				}

				if (aeManager)
				{
					// 移除AE，按当前弹药数值执行N次，能否移除由AE自身判断
					for (int i = 0; i < times; i++)
					{
						if (!entity.RemoveEffects.empty())
						{
							if (!entity.RemoveEffectsLevel.empty())
							{
								std::map<std::string, int> aeTypes;
								int idx = 0;
								int count = entity.RemoveEffects.size();
								for (std::string removeAE : entity.RemoveEffects)
								{
									int level = -1;
									if (idx < count)
									{
										level = entity.RemoveEffectsLevel[idx];
									}
									if (level > 0)
									{
										aeTypes[removeAE] = level;
									}
								}
								if (!aeTypes.empty())
								{
									aeManager->DetachByName(aeTypes, entity.RemoveEffectsSkipNext);
								}
							}
							else
							{
								aeManager->DetachByName(entity.RemoveEffects, entity.RemoveEffectsSkipNext);
							}
						}
						if (!entity.RemoveEffectsWithMarks.empty())
						{
							aeManager->DetachByMarks(entity.RemoveEffectsWithMarks, entity.RemoveEffectsSkipNext);
						}
					}
				}
			}

			// 触发次数限制
			if (entity.TriggeredTimes > 0)
			{
				_count[actionIdx]++;
				if (_count[actionIdx] >= entity.TriggeredTimes)
				{
					Deactivate();
					AE->TimeToDie();
					return;
				}
			}
		}
	}
}

void AmmoTriggerEffect::OnUpdate()
{
	if (!AE->OwnerIsDead())
	{
		Watch();
	}
}

void AmmoTriggerEffect::OnWarpUpdate()
{
	if (!AE->OwnerIsDead())
	{
		Watch();
	}
}
