#include "Gift.h"
#include "MathEx.h"
#include "StringEx.h"
#include "Scripts.h"

#include <Unsorted.h>
#include <JumpjetLocomotionClass.h>
#include <FootClass.h>

#include <Utilities/Debug.h>

#include <Ext/Helper/MathEx.h>
#include <Ext/Helper/Physics.h>

#include <Extension/TechnoExt.h>
#include <Ext/TechnoType/TechnoStatus.h>

#include <Ext/EffectType/Effect/CrateBuffData.h>

bool TryPutTechno(TechnoClass* pTechno, CoordStruct location, CellClass* pCell, bool virtualUnit)
{
	if (pCell || (pCell = MapClass::Instance->TryGetCellAt(location)) != nullptr)
	{
		auto occFlags = pCell->OccupationFlags;
		pTechno->OnBridge = pCell->ContainsBridge();
		CoordStruct xyz = pCell->GetCoordsWithBridge();
		if (pTechno->WhatAmI() == AbstractType::Infantry)
		{
			// 如果是步兵需要用步兵的位置，寻找最接近原位置的步兵位置，其余一律格子中心
			int x = xyz.X;
			int y = xyz.Y;
			int dX = location.X - x;
			int dY = location.Y - y;
			// 哪个更近
			if (dX > 0 && dY < 0)
			{
				// 左边更近
				xyz = CoordStruct{ x + 64, y - 64};
			}
			else if (dX < 0 && dY > 0)
			{
				// 右边更近
				xyz = CoordStruct{ x - 64, y + 64};
			}
			else if (dX > 0 && dY > 0)
			{
				// 下面更近
				xyz = CoordStruct{ x + 64, y + 64};
			}
		}
		++Unsorted::IKnowWhatImDoing;
		pTechno->Unlimbo(xyz, DirType::East);
		--Unsorted::IKnowWhatImDoing;
		if (virtualUnit)
		{
			pCell->OccupationFlags = occFlags;
		}
		bool dontMove = false;
		if (BuildingClass* pBuilding = abstract_cast<BuildingClass*, true>(pTechno))
		{
			if (!virtualUnit && pBuilding->Type->Foundation != Foundation::_0x0)
			{
				pBuilding->QueueMission(Mission::Construction, false);
				// pCell->AddContent(pBuilding, false); // 遇到ToTitle会导致崩溃
				dontMove = true;
			}
			else
			{
				dontMove = false;
			}
			pBuilding->DiscoveredBy(pTechno->Owner);
			pBuilding->IsReadyToCommence = true;
			pTechno->Owner->RecheckTechTree = true;
		}
		if (!dontMove)
		{
			// 单位移动到指定位置，此处必须放置在格子的中心，否则会导致单位位置产生偏移
			xyz.Z = location.Z;
			pTechno->SetLocation(xyz);
		}
		return true;
	}
	return false;
}

TechnoClass* CreateAndPutTechno(TechnoTypeClass* pType, HouseClass* pHouse, CoordStruct location, CellClass* pCell)
{
	TechnoClass* pTechno = abstract_cast<TechnoClass*, true>(pType->CreateObject(pHouse));
	if (TryPutTechno(pTechno, location, pCell))
	{
		return pTechno;
	}
	return nullptr;
}

TechnoClass* CreateAndPutTechno(std::string id, HouseClass* pHouse, CoordStruct location, CellClass* pCell)
{
	if (IsNotNone(id))
	{
		TechnoTypeClass* pType = TechnoTypeClass::Find(id.c_str());
		if (pType)
		{
			return CreateAndPutTechno(pType, pHouse, location, pCell);
		}
	}
	return nullptr;
}

bool RealReleaseGift(std::string id, HouseClass* pHouse,
	CoordStruct location, // 预订投放位置
	CellClass* pCell, // 预订投放位置的格子
	std::vector<CellStruct> cellOffsets, // 随机格子的范围
	bool emptyCell, // 是否空格子再投放
	TechnoTypeClass*& pGiftType, // 礼物的类型
	TechnoClass*& pGift, // 礼物本体
	CoordStruct& putLocation, // 礼物的投放位置
	CellClass*& pPutCell // 礼物投放的格子
)
{
	pGiftType = nullptr;
	pGift = nullptr;
	putLocation = location;
	pPutCell = pCell;
	CellStruct centerCellPos = pCell->MapCoords;
	if (IsNotNone(id) && (pGiftType = TechnoTypeClass::Find(id.c_str())) != nullptr)
	{
		int max = cellOffsets.size();
		for (int i = 0; i < max; i++)
		{
			int index = Random::RandomRanged(0, max - 1);
			CellStruct offset = cellOffsets[index];
			CellStruct targetCellPos = centerCellPos + offset;
			// 检查目标格子是否可用
			if (CellClass* pTargetCell = MapClass::Instance->TryGetCellAt(targetCellPos))
			{
				if (pTargetCell->IsClearToMove(pGiftType->SpeedType, pGiftType->MovementZone, !emptyCell, !emptyCell))
				{
					pPutCell = pTargetCell;
					putLocation = pTargetCell->GetCoordsWithBridge();
					break;
				}
			}
		}
		// 投送单位
		pGift = CreateAndPutTechno(pGiftType, pHouse, putLocation, pPutCell);
	}

	return pGift != nullptr;
}


BoxStateCache GiftBoxStateCache(GiftBoxData data)
{
	BoxStateCache boxState;
	// boxState.Location = pBullet->GetCoords();
	boxState.RandomRange = data.RandomRange;
	boxState.EmptyCell = data.EmptyCell;

	// boxState.CurrentMission = currentMission;
	boxState.ForceMission = data.ForceMission;
	// boxState.pTarget = pTarget;
	boxState.InheritTarget = data.InheritTarget;
	boxState.IsTransfrom = data.IsTransform;

	// boxState.pDest = pFocus;
	// boxState.pFocus = pFocus;
	boxState.Scatter = !data.Remove || data.ForceMission == Mission::Move;

	boxState.SameDir = data.IsTransform;
	// boxState.BodyDir = BodyDir;
	// boxState.TurretDir = TurretDir;

	// boxState.Group = Group;
	// boxState.IsSelected = IsSelected;
	// boxState.CrateBuff = CrateBuff;

	// boxState.pHouse = pSourceHouse;

	boxState.RemoveEffects = data.RemoveEffects;
	boxState.AttachEffects = data.AttachEffects;
	boxState.AttachChances = data.AttachChances;
	return boxState;
}

void ReleaseGifts(std::vector<std::string> gifts, GiftBoxEntity data, BoxStateCache boxState, std::function<void(TechnoClass*, TechnoStatus*&, AttachEffect*&)> inheritFun)
{
	CoordStruct location = boxState.Location;

	// 获取投送单位的位置
	if (CellClass* pCell = MapClass::Instance->TryGetCellAt(location))
	{
		HouseClass* pHouse = boxState.pHouse;
		Mission currentMission = boxState.CurrentMission;
		Mission forceMission = boxState.ForceMission;
		// 获取目的地
		AbstractClass* pDest = boxState.pDest; // 载具当前的移动目的地
		AbstractClass* pFocus = boxState.pFocus; // 步兵当前的移动目的地

		AbstractClass* pTarget = boxState.pTarget;
		bool scatter = boxState.Scatter;
		// 随机投送位置
		CellStruct cellPos = pCell->MapCoords;
		std::vector<CellStruct> cellOffsets{};
		if (boxState.RandomRange > 0)
		{
			auto const range = static_cast<size_t>(boxState.RandomRange + 0.99);
			for (CellSpreadEnumerator it(boxState.RandomRange); it; ++it) {
				CellStruct pos = *it;
				cellOffsets.push_back(pos);
			}
		}
		// 开始投送单位，每生成一个单位就选择一次位置
		for (std::string id : gifts)
		{
			// 投送单位
			TechnoTypeClass* pGiftType = nullptr; // 礼物的类型
			TechnoClass* pGift = nullptr; // 礼物本体
			CoordStruct putLocation = location; // 礼物的投放位置
			CellClass* pPutCell = pCell; // 礼物投放的格子
			if (RealReleaseGift(id, pHouse,
				location, pCell, cellOffsets, boxState.EmptyCell,
				pGiftType, pGift,
				putLocation, pPutCell)
				)
			{
				TechnoStatus* pGiftStatus = GetStatus<TechnoExt, TechnoStatus>(pGift);
				if (boxState.SameDir)
				{
					// 同步朝向
					pGift->PrimaryFacing.SetCurrent(boxState.BodyDir);
					pGift->SecondaryFacing.SetCurrent(boxState.TurretDir);
					// JJ有单独的Facing
					if (pGiftStatus->IsJumpjet())
					{
						FootClass* pGiftFoot = abstract_cast<FootClass*, true>(pGift);
						JumpjetLocomotionClass* pLoco = dynamic_cast<JumpjetLocomotionClass*>(pGiftFoot->Locomotor.get());
						pLoco->LocomotionFacing.SetCurrent(boxState.BodyDir);
					}
					// 同步编队
					pGift->Group = boxState.Group;
					if (!boxState.BoxIsBullet) {
						// 同步箱子属性
						pGiftStatus->CrateBuff = boxState.CrateBuff;
					}
				}

				// 同步选中状态
				if (boxState.IsSelected)
				{
					pGiftStatus->DisableSelectVoice = true;
					pGift->Select();
					pGiftStatus->DisableSelectVoice = false;
				}

				AttachEffect* giftAEM = GetAEManager<TechnoExt>(pGift);
				// 调用继承
				inheritFun(pGift, pGiftStatus, giftAEM);

				// 调整AE
				if (giftAEM)
				{
					// 移除失效的AE
					giftAEM->DetachByName(boxState.RemoveEffects, false);
					// 附加新的AE
					giftAEM->Attach(boxState.AttachEffects, boxState.AttachChances, false, boxState.pOwner, pHouse);
				}

				// 强制任务
				if (forceMission != Mission::None && forceMission != Mission::Move)
				{
					// 强制任务
					pGift->QueueMission(forceMission, false);
				}
				else
				{
					if (pTarget && boxState.InheritTarget && CanAttack(pGift, pTarget))
					{
						// 同步目标
						pGift->SetTarget(pTarget);
						pGift->QueueMission(currentMission, false);
					}
					else if (!pGiftStatus->IsBuilding())
					{
						if (!pDest && !pFocus)
						{
							// 第一个傻站着，第二个之后的散开
							if (scatter || pGiftType->BalloonHover)
							{
								// 分散到所在的格子里
								CoordStruct scatterPos = CoordStruct::Empty;
								if (pPutCell)
								{
									scatterPos = pPutCell->GetCoordsWithBridge();
								}
								pGift->Scatter(scatterPos, true, false);
							}
							else if (boxState.IsTransfrom)
							{
								// 需要检查是否呆在原地不移动，比如原地变形，需要单独处理掉落/起飞
								if (pGift->IsInAir())
								{
									if (!pGiftType->ConsideredAircraft)
									{
										// 不会飞的掉地上
										FallingDown(pGift, 0, false);
									}
									else if (pGift->What_Am_I() == AbstractType::Aircraft)
									{
										// 会飞的动一下
										CoordStruct scatterPos = CoordStruct::Empty;
										if (pPutCell)
										{
											scatterPos = pPutCell->GetCoordsWithBridge();
										}
										pGift->Scatter(scatterPos, true, false);
									}
								}
							}
							scatter = true;
						}
						else
						{
							// 开往预定目的地
							if (!pGiftStatus->IsBuilding())
							{
								CoordStruct des = !pDest ? putLocation : pDest->GetCoords();
								if (pFocus)
								{
									pGift->SetFocus(pFocus);
									if (pGiftStatus->IsUnit())
									{
										des = pFocus->GetCoords();
									}
								}
								if (CellClass* pTargetCell = MapClass::Instance->TryGetCellAt(des))
								{
									pGift->SetDestination(pTargetCell, true);
									pGift->QueueMission(Mission::Move, true);
								}
								// 在天上，但不会飞
								if (pGift->IsInAir() && !pGiftType->ConsideredAircraft)
								{
									FallingDown(pGift, 0, false);
								}
							}
						}
					}
				}
			}
			else
			{
				Debug::Log("Warning: Gift box release gift failed, unknown TechnoType [%s]\n", id.c_str());
			}
		}
	}
}

std::vector<std::string> GetGiftList(GiftBoxEntity data)
{
	std::vector<std::string> gifts{};
	if (data.Enable)
	{
		int giftCount = data.Gifts.size();
		int numsCount = data.Nums.size();
		if (data.RandomType)
		{
			// 随机类型，将所有的数量限制加起来，总和为礼物数量
			int times = 1;
			if (numsCount > 0)
			{
				times = 0;
				for (int num : data.Nums)
				{
					times += num;
				}
			}
			// 计算权重
			int maxValue = 0;
			std::map<Point2D, int> targetPad = MakeTargetPad(data.RandomWeights, giftCount, maxValue);
			// 算出随机值，确认位置，取得序号，选出单位
			for (int i = 0; i < times; i++)
			{
				int index = Hit(targetPad, maxValue);
				// 计算概率
				if (Bingo(data.Chances, index))
				{
					std::string id = data.Gifts[index];
					if (IsNotNone(id))
					{
						gifts.emplace_back(id);
					}
				}
			}
		}
		else
		{
			// 指定类型，遍历礼物类型，取指定数量个
			for (int index = 0; index < giftCount; index++)
			{
				std::string id = data.Gifts[index];
				if (IsNotNone(id))
				{
					int times = 1;
					if (numsCount > 0 && index < numsCount)
					{
						times = data.Nums[index];
					}
					// 取指定数量个
					for (int i = 0; i < times; i++)
					{
						// 计算概率
						if (Bingo(data.Chances, index))
						{
							gifts.push_back(id); // 存copy
						}
					}
				}
			}
		}
	}
	return gifts;
}

AttachEffect* InheritAE(TechnoStatus* pBoxStatus, TechnoStatus*& pGiftStatus)
{
	// 复制除了giftBox之外的状态机
	pBoxStatus->InheritedStatsTo(pGiftStatus);
	// 获取根组件
	Component* giftGO = pGiftStatus->GetParent();
	Component* boxGO = pBoxStatus->GetParent();
	// 交换AE管理器
	AttachEffect* boxAEM = boxGO->GetComponent<AttachEffect>();
	AttachEffect* giftAEM = giftGO->GetComponent<AttachEffect>();
	// 第一遍：标记变形/替换时不保留的AE（DiscardOnTransform=true）、GiftBox/Transform型AE、
	// 以及目标类型不适用的AE（CanAffectType），统一等待清理
	boxAEM->ForeachChild([&](Component* c) {
		if (auto ae = dynamic_cast<AttachEffectScript*>(c))
		{
			if (ae->AEData.DiscardOnTransform || ae->AEData.GiftBox.Enable || ae->AEData.Transform.Enable
				|| !ae->AEData.CanAffectType(pGiftStatus->pTechno))
			{
				ae->TimeToDie();
			}
		}
		});
	boxAEM->CheckDurationAndDisable(true);
	boxAEM->ClearDisableComponent();
	// 第二遍：幸存者重绑到新单位。来源为源单位的AE，pSource和新单位的所属一起修正；
	// DeploysInto 源与目标必然同属，GiftBox 可能由异所属的AE附加，因此统一采用新单位的实际Owner
	boxAEM->ForeachChild([&](Component* c) {
		if (auto ae = dynamic_cast<AttachEffectScript*>(c))
		{
			if (ae->pSource == pBoxStatus->pTechno)
			{
				ae->InheritedTo(pGiftStatus->pTechno, pGiftStatus->pTechno->Owner);
			}
		}
		});
	// 移动AE管理器（组件级原子操作，内部自动脱离旧父并递归重绑新父的_extData）
	// 先记录两个管理器在原GO中的位置，交换时保持各自的顺序位置
	int boxIndex = boxAEM->GetIndexInParent();
	int giftIndex = giftAEM->GetIndexInParent();
	if (!boxAEM->MoveTo(giftGO, boxIndex))
	{
		Debug::Log("Error: InheritAE failed to move box AE manager [%s]%p to gift GO.\n", boxAEM->thisName.c_str(), boxAEM);
		return nullptr;
	}
	if (!giftAEM->MoveTo(boxGO, giftIndex))
	{
		Debug::Log("Error: InheritAE failed to move gift AE manager [%s]%p to box GO, rollback box AE manager.\n", giftAEM->thisName.c_str(), giftAEM);
		// 回滚第一个移动，保证两个管理器都留在原父下
		boxAEM->MoveTo(boxGO, boxIndex);
		return nullptr;
	}
	// 通知被搬移的AE子树：管理器清TypeData缓存并触发DetachWhenTransform，
	// 子Effect（Animation/Stand等）据此把自身资源重绑到新单位。
	// 不再向整个GO广播ExtChanged，避免TechnoStatus等无关组件被重复InitExt
	boxAEM->Foreach([](Component* c)
	{
		c->ExtChanged();
	});

	return boxAEM;
}

