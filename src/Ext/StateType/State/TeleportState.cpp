#include "TeleportState.h"

#include <AircraftTrackerClass.h>
#include <JumpjetLocomotionClass.h>

#include <Extension/TechnoTypeExt.h>
#include <Extension/WarheadTypeExt.h>

#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Physics.h>
#include <Ext/Helper/DrawEx.h>

#include <Ext/TechnoType/TechnoStatus.h>
#include <Ext/ObjectType/AttachEffect.h>

#include <Ext/Common/PrintTextManager.h>

bool TeleportState::Teleport(CoordStruct* pLocation, WarheadTypeClass* pWH)
{
	if (IsAlive() && _step == TeleportStep::READY)
	{
		CoordStruct targetPos = *pLocation;
		if (!targetPos.IsEmpty())
		{
			WarheadTypeExt::TypeData* typeData = GetTypeData<WarheadTypeExt, WarheadTypeExt::TypeData>(pWH);
			if (typeData && typeData->Teleporter)
			{
				_warpTo = targetPos;
				return true;
			}
		}
	}
	return false;
}

bool TeleportState::IsFreezing()
{
	bool freeze = false;
	switch (_step)
	{
	case TeleportStep::TELEPORTED:
	case TeleportStep::FREEZING:
		freeze = true;
		break;
	}
	return freeze;
}

bool TeleportState::IsReadyToMoveWarp()
{
	return IsReady() && _step == TeleportStep::READY && Data.Mode != TeleportMode::WARHEAD;
}

void TeleportState::Reload()
{
	if (_delay > 0)
	{
		_delayTimer.Start(_delay);
	}
	_count++;
}

bool TeleportState::IsReady()
{
	// WarpingOut 为 true 说明上一跳的冻结/传送序列尚未结束，禁止再次起跳
	return !IsDone() && Timeup() && _canWarp && !pTechno->WarpingOut;
}

bool TeleportState::Timeup()
{
	return _delay <= 0 || _delayTimer.Expired();
}

bool TeleportState::IsDone()
{
	return Data.TriggeredTimes > 0 && _count >= Data.TriggeredTimes;
}

void TeleportState::OnStart()
{
	isJumpJet = pTechno->GetTechnoType()->Locomotor == LocomotionClass::CLSIDs::Jumpjet;
	isAircraft = !isJumpJet && IsAircraft();
	isInfantry = !isAircraft && IsInfantry();

	_count = 0;
	_delay = Data.Delay;
	TechnoStatus* status = dynamic_cast<TechnoStatus*>(_parent);
	if (status && status->AmIStand())
	{
		End();
		return;
	}
	_canWarp = Data.Enable;
}

void TeleportState::OnEnd()
{
	_canWarp = false;
}

void TeleportState::OnWarpUpdate()
{
	// WarpingOut=true 期间，步兵/飞行器的 Update 会进入原版 warp 分支并提前返回，
	// 跳过 FootClass::AI，导致本状态机的 OnUpdate 不再被调用而永久停摆。
	// 在这里继续驱动状态机，保证解冻逻辑始终执行。
	// OnUpdate();
}

CoordStruct TeleportState::GetAndMarkDestination(CoordStruct location)
{
	CoordStruct targetPos = CoordStruct::Empty;

	FootClass* pFoot = abstract_cast<FootClass*, true>(pTechno);
	// 没有被磁电抬起
	if (!pFoot->IsAttackedByLocomotor)
	{
		// 移动到自身相对位置
		if (!Data.MoveTo.IsEmpty())
		{
			targetPos = GetFLHAbsoluteCoords(pTechno, Data.MoveTo, Data.IsOnTurret);
		}
		else
		{
			// 是否正在移动, Aircraft pFoot->GetCurrentSpeed always is Zero
			// targetPos = pFoot->GetDestination();
			pFoot->Locomotor->Destination(&targetPos);
			// 子机导弹不一定具有移动目的地，有目标时，亦可使用目标位置作为跳跃点位置
			if (targetPos.IsEmpty()
				&& pTechno->WhatAmI() == AbstractType::Aircraft
				&& pTechno->GetTechnoType()->MissileSpawn
				&& pTechno->Target)
			{
				targetPos = pTechno->Target->GetCoords();
			}
		}
		// 目的地和本体位置在同一格内就不跳
		CellStruct s = CellClass::Coord2Cell(location);
		CellStruct t = CellClass::Coord2Cell(targetPos);
		if (s.X == t.X && s.Y == t.Y)
		{
			// Same cell, don't move
			targetPos = CoordStruct::Empty;
		}
		if (!targetPos.IsEmpty())
		{
			// 记录下目的地
			pDest = pFoot->Destination;
			pFocus = pTechno->Focus;
		}
	}
	return targetPos;
}

void TeleportState::OnUpdate()
{
	if (!IsBuilding() && !IsDeadOrInvisible(pTechno))
	{
		CoordStruct location = pTechno->GetCoords();
		switch (_step)
		{
		case TeleportStep::READY:
		{
			if (IsDone())
			{
				End();
				return;
			}
			if (IsReady())
			{
				// 不断尝试获得目的地位置，并判断是否可以传送
				CoordStruct targetPos = GetAndMarkDestination(location);
				switch (Data.Mode)
				{
				case TeleportMode::MOVE:
					break;
				case TeleportMode::WARHEAD:
					targetPos = _warpTo;
					_warpTo = CoordStruct::Empty;
					break;
				case TeleportMode::BOTH:
					if (!_warpTo.IsEmpty())
					{
						targetPos = _warpTo;
						_warpTo = CoordStruct::Empty;
					}
					break;
				default:
					return;
				}
				if (!targetPos.IsEmpty())
				{
					// 传送距离检查
					double distance = targetPos.DistanceFrom(location);
					if (distance > Data.RangeMin * Unsorted::LeptonsPerCell && (Data.RangeMax < 0 ? true : distance < Data.RangeMax * Unsorted::LeptonsPerCell))
					{
						// 在可以传送的范围内
						double dist = Data.Distance * Unsorted::LeptonsPerCell;
						if (dist > 0 && distance > dist)
						{
							// 有限距离的传送，重新计算目标位置
							targetPos = GetForwardCoords(location, targetPos, dist, distance);
						}
					}
					else
					{
						// 不可以传送
						targetPos = CoordStruct::Empty;
					}
				}
				if (!targetPos.IsEmpty())
				{
					bool isInAir = pTechno->IsInAir();
					// 跳跃位置偏移
					if (!Data.Offset.IsEmpty())
					{
						DirStruct facing{};
						AbstractClass* pTarget = pTechno->Target;
						TechnoClass* pTargetTechno = nullptr;
						if (Data.IsOnTarget && CastToTechno(pTarget, pTargetTechno))
						{
							if (pTargetTechno->WhatAmI() == AbstractType::Aircraft || (Data.IsOnTurret && pTargetTechno->HasTurret()))
							{
								facing = pTargetTechno->SecondaryFacing.Current();
							}
							else if (pTargetTechno->GetTechnoType()->Locomotor == LocomotionClass::CLSIDs::Jumpjet)
							{
								FootClass* pTargetFoot = abstract_cast<FootClass*, true>(pTargetTechno);
								if (JumpjetLocomotionClass* jjLoco = dynamic_cast<JumpjetLocomotionClass*>(pTargetFoot->Locomotor.get()))
								{
									facing = jjLoco->LocomotionFacing.Current();
								}
							}
							else
							{
								facing = pTargetTechno->PrimaryFacing.Current();
							}
						}
						else
						{
							facing = Point2Dir(location, targetPos);
						}
						targetPos = GetFLHAbsoluteCoords(targetPos, Data.Offset, facing);
					}
					// 检查目的地是否可以着陆
					CellClass* pTargetCell = nullptr;
					if (CellClass* pCell = MapClass::Instance->TryGetCellAt(targetPos))
					{
						if (isInAir)
						{
							pTargetCell = pCell;
						}
						else
						{
							TechnoTypeClass* pType = pTechno->GetTechnoType();

							int times = 0;
							do
							{
								bool canEnterCell = false;
								Move move = pTechno->IsCellOccupied(pCell, -1, -1, nullptr, false);
								switch (move)
								{
								case Move::OK:
									// case Move::MovingBlock:
									canEnterCell = true;
									break;
								}
								if (isJumpJet)
								{
									canEnterCell = pCell->Jumpjet == nullptr;
								}
								bool canMoveTo = pCell->IsClearToMove(pType->SpeedType, pType->MovementZone, true, true) && canEnterCell;
								if (canMoveTo)
								{
									pTargetCell = pCell;
									break;
								}
								CellStruct curretCell = pCell->MapCoords;
								int zone = MapClass::Instance->GetMovementZoneType(curretCell, pType->MovementZone, pTechno->IsOnBridge());
								bool alt = (bool)(pCell->Flags & CellFlags::CenterRevealed);
								CellStruct nextCell = MapClass::Instance->NearByLocation(curretCell, pType->SpeedType, zone, pType->MovementZone, alt, 1, 1, 0, true, false, true, curretCell, false, false);
								pCell = MapClass::Instance->TryGetCellAt(nextCell);
							} while (pCell && times++ < 9);
						}
					}
					// 可以跳，一轮跳跃开始
					if (pTargetCell)
					{
						// 记录跳跃位置，落地位置
						_jumpTo = pTargetCell->GetCoordsWithBridge();
						// 清除记录的目标
						_teleportTimer.Stop();
						if (Data.ClearTarget)
						{
							ClearAllTarget(pTechno);
							pTarget = nullptr;
						}
						else
						{
							pTarget = pTechno->Target;
						}
						// 开跳
						FootClass* pFoot = abstract_cast<FootClass*, true>(pTechno);
						if (isInAir || isAircraft || !Data.Super)
						{
							// Warp，TeleportLocomotionClass会有一帧的延迟，因此不使用Loco切换，而是全部采用自定义跳
							// 根据类型判断，落点
							// 地面步兵落地使用引擎的子格选择（1/3格），会避开已被占用的子格，多单位不会堆叠
							if (isInfantry && !isInAir && !isJumpJet)
							{
								CoordStruct subcellPos = CoordStruct::Empty;
								if (MapClass::PickInfantrySublocation(subcellPos, _jumpTo, false) && !subcellPos.IsEmpty())
								{
									_jumpTo = subcellPos;
								}
							}
							else if (isAircraft || isInAir || isJumpJet)
							{
								// 空中跳
								int height = pTechno->GetHeight() + Data.Offset.Z;
								if (CellClass* pSourceCell = MapClass::Instance->TryGetCellAt(location))
								{
									if (pSourceCell->ContainsBridge())
									{
										height -= pSourceCell->BridgeHeight;
									}
								}
								_jumpTo.Z += height;
							}

							// 清除占位
							ForceStopMoving(pFoot);
							pFoot->Locomotor->Force_Track(-1, location);
							pFoot->FrozenStill = true;
							pFoot->SendToEachLink(RadioCommand::NotifyUnlink);
							// 释放原格占用位对所有地面单位都需要（含 Jumpjet，避免原格留下幽灵占用位）
							if (!isInAir && !isAircraft)
							{
								// 释放原格子的占用位（本体虚表 +0xF4），UpdatePlacement 不写占用位，
								// 详见 Ext/Helper/Physics.h 的备忘
								ReleaseCell(pTechno, location);
							}
							// 移动位置
							pTechno->UpdatePlacement(PlacementType::Remove);
							pTechno->SetLocation(_jumpTo);
							pTechno->UpdatePlacement(PlacementType::Put);
							// Jumpjet 不写落点占用位：Jumpjet 自己的降落逻辑（JJ_Descending）会用
							// sub_481130 检查子格占用位，提前写位会让它认为落不了地，
							// 飞回起飞点并把目的地改回原格，导致反复跳
							if (!isInAir && !isAircraft && !isJumpJet)
							{
								// 占住落点格子（本体虚表 +0xF0）：步兵写子格位，载具写 0x40 位，
								// 让 PickInfantrySublocation / IsClearToMove 能感知到，多单位按子格分散
								OccupyCell(pTechno, _jumpTo);
							}
							if (isJumpJet)
							{
								pFoot->Jumpjet_OccupyCell(pTargetCell->MapCoords);
								// 已到达原移动目标时：重置 Jumpjet 移动状态并停住，不再下达移动指令。
								// 若不重置，loco 会带着旧目标继续升空/巡航（日志表现 st1/st3、dd 变回上一格），
								// 引擎随后把目的地改回上一格，导致来回跳和爬升
								// if (JumpjetLocomotionClass* jjLoco = dynamic_cast<JumpjetLocomotionClass*>(pFoot->Locomotor.get()))
								// {
								// 	jjLoco->State = JumpjetLocomotionClass::State::Hovering;
								// }
								pFoot->SetDestination(pTargetCell, true);
							}
							// 设置面向
							pTechno->PrimaryFacing.SetCurrent(pTechno->PrimaryFacing.Current());
							pTechno->SecondaryFacing.SetCurrent(pTechno->SecondaryFacing.Current());
							// 移除黑幕
							MapClass::Instance->RevealArea2(&_jumpTo, pTechno->LastSightRange, pTechno->Owner, false, false, false, true, 0);
							MapClass::Instance->RevealArea2(&_jumpTo, pTechno->LastSightRange, pTechno->Owner, false, false, false, true, 1);
							// 播放自定义传送动画
							TechnoTypeExt::TypeData* typeData = GetTypeData<TechnoTypeExt, TechnoTypeExt::TypeData>(pTechno->GetTechnoType());
							AnimTypeClass* pWarpOut = nullptr;
							AnimTypeClass* pWarpIn = nullptr;
							if (IsNotNone(typeData->WarpOut))
							{
								pWarpOut = AnimTypeClass::Find(typeData->WarpOut.c_str());
							}
							else
							{
								pWarpOut = RulesClass::Instance->WarpOut;
							}
							if (pWarpOut)
							{
								AnimClass* pAnimOut = GameCreate<AnimClass>(pWarpOut, location);
								SetAnimOwner(pAnimOut, pTechno);
							}
							if (IsNotNone(typeData->WarpIn))
							{
								pWarpIn = AnimTypeClass::Find(typeData->WarpIn.c_str());
							}
							else
							{
								pWarpIn = RulesClass::Instance->WarpIn;
							}
							if (pWarpIn)
							{
								AnimClass* pAnimIn = GameCreate<AnimClass>(pWarpIn, _jumpTo);
								SetAnimOwner(pAnimIn, pTechno);
							}
							// 播放声音
							int outSound = pTechno->GetTechnoType()->ChronoOutSound;
							if (outSound >= 0 || (outSound = RulesClass::Instance->ChronoOutSound) >= 0)
							{
								VocClass::PlayAt(outSound, location);
							}
							int inSound = pTechno->GetTechnoType()->ChronoInSound;
							if (inSound >= 0 || (inSound = RulesClass::Instance->ChronoInSound) >= 0)
							{
								VocClass::PlayAt(inSound, _jumpTo);
							}
							// 传送冷冻
							if (!isInAir || Data.FreezingInAir)
							{
								int delay = typeData->ChronoMinimumDelay;
								if (typeData->ChronoTrigger && delay > 0)
								{
									// 根据传送距离计算时间
									double distance = _jumpTo.DistanceFrom(location);
									if (distance > typeData->ChronoRangeMinimum)
									{
										int factor = std::max(typeData->ChronoDistanceFactor, 1);
										delay = (int)(distance / factor);
									}
								}
								if (delay > 0)
								{
									pTechno->WarpingOut = true;
									_teleportTimer.Start(delay);
								}
								else
								{
									pTechno->WarpingOut = false;
									_teleportTimer.Stop();
								}
							}
						}
						else
						{
							// 使用超武跳
							pFoot->ChronoWarpTo(_jumpTo);
						}
						// 通知AE管理器进行了跳跃
						if (AttachEffect* aeManager = GetAEManager<TechnoExt>(pTechno))
						{
							aeManager->ClearLocationMarks();
						}
						// 进入下一阶段
						Reload();
						_step = TeleportStep::TELEPORTED;
					}
				}
			}
			break;
		}
		case TeleportStep::TELEPORTED:
		{
			if (Data.Super && !pTechno->IsInAir())
			{
				// 超武跳，不用冷冻计时器
				if (!pTechno->WarpingOut)
				{
					_step = TeleportStep::MOVEFORWARD;
				}
			}
			else
			{
				if (_teleportTimer.Expired())
				{
					// 解冻，进入下一个阶段
					pTechno->WarpingOut = false;
					_step = TeleportStep::MOVEFORWARD;
				}
				else
				{
					_step = TeleportStep::FREEZING;
				}
			}
			if (isJumpJet)
			{
				// 已经到达原移动目标，移动指令已完成，必须清除移动目标。
				// Jumpjet 的 loco 已在跳跃时重置（见 isJumpJet 分支），
				// jumpjet会在任务2、4时将自身原位置设置为移动目标，造成反复横跳。
				CellStruct jumpToCell = CellClass::Coord2Cell(_jumpTo);
				CellStruct curCell = CellClass::Coord2Cell(location);
				if (curCell.X == jumpToCell.X && curCell.Y == jumpToCell.Y)
				{
					pTechno->SetDestination(nullptr, true);
				}
			}
			break;
		}
		case TeleportStep::FREEZING:
		{
			if (_teleportTimer.Expired() || !pTechno->WarpingOut)
			{
				// 解冻，进入下一个阶段
				pTechno->WarpingOut = false;
				_step = TeleportStep::MOVEFORWARD;
			}
			break;
		}
		case TeleportStep::MOVEFORWARD:
		{
			_step = TeleportStep::READY;

			CellClass* pCell = MapClass::Instance->TryGetCellAt(location);
			// 空中单位需要更新在空中的追踪位置，关系到防空武器的命中判定
			if (pTechno->IsInAir() && pCell)
			{
				FootClass* pFoot = abstract_cast<FootClass*, true>(pTechno);
				AircraftTrackerClass::Instance->Update_Entry(pTechno, pFoot->LastJumpjetMapCoords, pCell->MapCoords);
			}
			// 继续其他任务指令
			if (!pTechno->Target)
			{
				// 跳跃后冷冻会清除攻击目标，如果有记录，设回去
				if (pTarget)
				{
					pTechno->SetTarget(pTarget);
					pTechno->QueueMission(Mission::Attack, true);
				}
				else if (Data.MoveForward)
				{
					// 把移动目的地，设回去
					pTechno->SetFocus(pFocus);
					pTechno->SetDestination(pDest, true);
					pTechno->QueueMission(Mission::Move, true);
				}
				else if (pCell && !isJumpJet)
				{
					// 停留在原地，把当前位置设置为新的移动目的地
					pTechno->SetDestination(pCell, true);
					pTechno->QueueMission(Mission::Move, true);
				}
			}
			else
			{
				// 跳跃后没有清除攻击目标，继续攻击目标
				pTechno->QueueMission(Mission::Attack, true);
			}
			// 一轮跳跃结束
			_jumpTo = CoordStruct::Empty;
			pFocus = nullptr;
			pDest = nullptr;
			pTarget = nullptr;
			break;
		}
		}
	}
}
