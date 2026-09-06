#include "ECMState.h"

#include <Ext/Helper/Finder.h>
#include <Ext/Helper/FLH.h>
#include <Ext/Helper/MathEx.h>
#include <Ext/Helper/Scripts.h>

#include <Ext/BulletType/BulletStatus.h>

bool ECMState::TryGetSourceLocation(CoordStruct& location)
{
	if (AEFromWarhead)
	{
		location = AEWarheadLocation;
	}
	else if (pAESource)
	{
		location = pAESource->GetCoords();
	}
	return !location.IsEmpty();
}

void ECMState::Reload()
{
	if (_delay > 0)
	{
		_delayTimer.Start(_delay);
	}
	_count++;
}

bool ECMState::IsReady()
{
	return !IsDone() && Timeup();
}

bool ECMState::Timeup()
{
	return _delay <= 0 || _delayTimer.Expired();
}

bool ECMState::IsDone()
{
	return Data.TriggeredTimes > 0 && _count >= Data.TriggeredTimes;
}

BulletStatus* ECMState::GetBulletStatus()
{
	if (!_status)
	{
		_status = GetStatus<BulletExt, BulletStatus>(pBullet);
	}
	return _status;
}

void ECMState::OnStart()
{
	_count = 0;
	_delay = Data.Rate;
	// BulletStatus* status = GetBulletStatus();
	// if (status && !status->IsMissile())
	// {
	// 	End();
	// }
}

void ECMState::OnUpdate()
{
	if (IsBullet() && !IsDeadOrInvisible(pBullet))
	{
		if (IsDone())
		{
			End();
			return;
		}
		if (IsReady() && _lockTimer.Expired())
		{
			if (Bingo(Data.Chance))
			{
				BulletStatus* status = GetBulletStatus();
				if (Data.Feedback)
				{
					// 忽悠炮，艹他
					TechnoClass* pOwner = status->pSource;
					if (Data.FeedbackToSourceTarget && pOwner)
					{
						AbstractClass* pOwnerTarget = pOwner->Target;
						TechnoClass* pOwnerTargetTechno = nullptr;
						CastToTechno(pOwnerTarget, pOwnerTargetTechno);
						if (pOwnerTargetTechno && !IsDeadOrInvisible(pOwnerTargetTechno))
						{
							pBullet->SetTarget(pOwnerTargetTechno);
							pBullet->TargetCoords = pOwnerTargetTechno->GetCoords();
						}
						else if (pOwnerTarget)
						{
							pBullet->SetTarget(pOwnerTarget);
							pBullet->TargetCoords = pOwnerTarget->GetCoords();
						}
						else
						{
							pBullet->TargetCoords = pBullet->SourceCoords;
						}
					}
					else
					{
						pBullet->SetTarget(pOwner);
						if (pOwner)
						{
							pBullet->TargetCoords = pOwner->GetCoords();
						}
						else
						{
							pBullet->TargetCoords = pBullet->SourceCoords;
						}
					}
					// FeedbackToSourceTarget=yes：弹体改为打发射者的目标，仍在为发射者效力——
					// 保留 Owner（Vector Origin=Launcher 锚依赖 pBullet->Owner，2026-09-06 修复：
					// 无条件清空导致链式 Vector 二次附加时大圆锚点丢失，圆心钉死在错误快照）；
					// no：弹体被打回发射者自己，剥夺主从关系才清空。
					if (!Data.FeedbackToSourceTarget)
					{
						pBullet->Owner = nullptr;
					}
				}
				else
				{
					AbstractClass* pTarget = pBullet->Target;
					// 确定搜索的圆心
					CoordStruct location = CoordStruct::Empty;
					bool pickFormAround = false;
					// 根据设置取得圆心
					switch (Data.Around)
					{
					case ECMAround::Source:
					{
						// 获取来源，如果没有就跳过
						pickFormAround = TryGetSourceLocation(location);
						break;
					}
					case ECMAround::Target:
					{
						// 获取当前目标的位置，没有就跳过
						if (pTarget)
						{
							location = pTarget->GetCoords();
							pickFormAround = true;
						}
						break;
					}
					case ECMAround::Self:
					{
						// 获取抛射体自身的位置，肯定有
						location = pBullet->GetCoords();
						pickFormAround = true;
						break;
					}
					case ECMAround::Shooter:
					{
						// 获取发射者的位置，没有就跳过
						if (status->pSource)
						{
							location = status->pSource->GetCoords();
							pickFormAround = true;
						}
						break;
					}
					}
					if (!pickFormAround)
					{
						// 没有从Arounds设置里获得，以自身为目标搜索
						location = pBullet->GetCoords();
					}
					// 开始搜索新目标
					bool pickTechno = Bingo(Data.ToTechnoChance);
					bool forceToGround = false;
					// 一定概率选择一个单位做目标，而不是地面
					if (pickTechno)
					{
						ECMData data = Data;
						std::vector<TechnoClass*> targetList;
						// 强制重置，那么排除掉原目标
						AbstractClass* pExclude = Data.ForceRetarget ? pTarget : nullptr;
						// 是否搜索空中单位由抛射体AA决定
						data.AffectInAir = pBullet->Type->AA;
						// 搜索范围内的一个单位作为新目标
						FindTechnoOnMark([&](TechnoClass* pTarget, AttachEffect* aeManager)
							{
								if (pTarget != pExclude)
								{
									targetList.push_back(pTarget);
								}
								return false;
							}, location, data.RangeMax, data.RangeMin, data.FullAirspace, status->pSourceHouse, data, pObject);
						int count = targetList.size();
						if (count > 0)
						{
							AbstractClass* pNewTarget = nullptr;
							bool found = false;
							int times = 0;
							if (times < count)
							{
								// 循环一轮，随机查找目标
								for (int i = 0; i < count; i++)
								{
									// 至少会找到一个，可能是原目标，取一个随机数，就它了
									int index = Random::RandomRanged(0, count - 1);
									// 检查选择的新目标能不能打
									TechnoClass* pTryTarget = targetList[index];
									if (CanAttack(pBullet, pTryTarget))
									{
										// 就是你了
										pNewTarget = pTryTarget;
										found = true;
										break;
									}
								}
								if (!found)
								{
									// 纳尼？随机居然没有找到，按顺序一个个找
									for (int j = 0; j < count; j++)
									{
										// 检查选择的新目标能不能打
										TechnoClass* pTryTarget = targetList[j];
										if (CanAttack(pBullet, pTryTarget))
										{
											// 就是你了
											pNewTarget = pTryTarget;
											found = true;
											break;
										}
									}
								}
							}
							if (found)
							{
								status->ResetTarget(pNewTarget, CoordStruct::Empty);
								if (data.NoOwner)
								{
									pBullet->Owner = nullptr;
								}
							}
							else
							{
								// 一个目标都没找到
								forceToGround = true;
							}
						}
						else
						{
							// 一个目标都没找到
							forceToGround = true;
						}
					}
					// 没有找到新的单位作为目标，则挑一个地板
					if (forceToGround || !pickTechno)
					{
						// 随机搜索附近的一个地面作为新目标
						CoordStruct targetPos = location;
						targetPos += GetRandomOffset(Data.RangeMax, Data.RangeMin);
						// 将偏移后的坐标对应的格子，设置为新的目标
						if (CellClass* pCell = MapClass::Instance->TryGetCellAt(targetPos))
						{
							status->ResetTarget(pCell, pCell->GetCoordsWithBridge());
							if (Data.NoOwner)
							{
								pBullet->Owner = nullptr;
							}
						}
					}
				}
				// 进入锁定
				if (Data.LockDuration > 0)
				{
					_lockTimer.Start(Data.LockDuration);
				}
			}
			Reload();
		}
	}
}
