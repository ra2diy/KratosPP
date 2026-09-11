#include "CountTriggerEffect.h"

#include <Ext/Helper/Finder.h>
#include <Ext/Helper/FLH.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>

#include "CounterEffect.h"

bool CountTriggerEffect::CanActive(double num, Point2D range)
{
	if (range.Y >= range.X)
	{
		return num >= range.X && (range.Y < 0 || num <= range.Y);
	}
	return true;
}

void CountTriggerEffect::Watch()
{
	// 向AE管理器查找计数器，不能使用AE->AEManager->Counters[Data->Watch]，会导致插入一个空指针
	auto it = AE->AEManager->Counters.find(Data->Watch);
	CounterEffect* counter = (it != AE->AEManager->Counters.end()) ? it->second : nullptr;
	if (counter && counter->IsAlive())
	{
		// key：0 = 无序号触发器（与 CountTrigger0 同槽），i = CountTrigger<i>；map 升序遍历
		for (auto& [actionIdx, entity] : Data->Actions)
		{
			if (CanActive(counter->CountNum, entity.Range))
			{
				// 命中时，按计数器数值缓存本次动作执行次数，后续修改计数不影响本次执行
				int times = entity.TriggerNumTimes ? (int)counter->CountNum : 1;
				// 操作计数
				if (entity.Num != 0)
				{
					double num = entity.Num;
					if (entity.NumType != CounterType::Number)
					{
						ObjectClass* pFrom = pObject;
						switch (entity.NumFrom)
						{
						case CountTriggerWho::SOURCE:
							pFrom = abstract_cast<ObjectClass*>(AE->pSource);
							break;
						case CountTriggerWho::COUNTER:
							pFrom = abstract_cast<ObjectClass*>(counter->AE->pSource);
							break;
						}
						if (pFrom && !IsDeadOrInvisible(pFrom))
						{
							switch (entity.NumType)
							{
							case CounterType::HP:
								num = pFrom->Health;
								break;
							case CounterType::MaxHP:
								if (entity.NumFrom != CountTriggerWho::ME)
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
					counter->ModifyCount(entity.Action, num);
				}
				if (entity.ResetNum)
				{
					counter->ResetNum();
				}

				// 添加AE
				if (entity.Attach)
				{
					// To哪个
					AttachEffect* aeManager = AE->AEManager;
					switch (entity.AttachTo)
					{
					case CountTriggerWho::SOURCE:
						aeManager = GetAEManager<TechnoExt>(AE->pSource);
						break;
					case CountTriggerWho::COUNTER:
						aeManager = GetAEManager<TechnoExt>(counter->AE->pSource);
						break;
					}

					if (aeManager)
					{
						// From哪个
						TechnoClass* pSource = AE->pSource;
						HouseClass* pSourceHouse = AE->pSourceHouse;
						switch (entity.AttachFrom)
						{
						case CountTriggerWho::ME:
							pSource = nullptr;
							pSourceHouse = nullptr;
							break;
						case CountTriggerWho::COUNTER:
							pSource = counter->AE->pSource;
							pSourceHouse = counter->AE->pSourceHouse;
							break;
						}
						// 附加AE，按计数器数值执行N次，能否附加由AE自身判断
						for (int i = 0; i < times; i++)
						{
							aeManager->Attach(entity.AttachEffects, entity.AttachChances, false, pSource, pSourceHouse);
						}
					}
				}
				// 移除AE
				if (entity.Remove)
				{
					// Remove哪个
					AttachEffect* aeManager = AE->AEManager;
					switch (entity.RemoveWho)
					{
					case CountTriggerWho::SOURCE:
						aeManager = GetAEManager<TechnoExt>(AE->pSource);
						break;
					case CountTriggerWho::COUNTER:
						aeManager = GetAEManager<TechnoExt>(counter->AE->pSource);
						break;
					}

					if (aeManager)
					{
						// 移除AE，按计数器数值执行N次，能否移除由AE自身判断
						for (int i = 0; i < times; i++)
						{
							if (!entity.RemoveEffects.empty())
							{
								if (!entity.RemoveEffectsLevel.empty())
								{
									// 移除指定的层数
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
				// 移除计数器
				if (entity.RemoveCounter)
				{
					counter->RemoveCounter();
				}
				if (entity.TriggeredTimes > 0)
				{
					// 触发成功，计数
					_count[actionIdx]++;
					// 检查触发次数
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

}

void CountTriggerEffect::OnUpdate()
{
	if (!AE->OwnerIsDead())
	{
		Watch();
	}
}

void CountTriggerEffect::OnWarpUpdate()
{
	if (!AE->OwnerIsDead())
	{
		Watch();
	}
}
