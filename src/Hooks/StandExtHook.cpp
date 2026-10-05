#include <exception>
#include <Windows.h>

#include <GeneralStructures.h>
#include <TechnoClass.h>
#include <FootClass.h>
#include <AircraftClass.h>
#include <DriveLocomotionClass.h>
#include <JumpjetLocomotionClass.h>
#include <ShipLocomotionClass.h>

#include <Extension.h>
#include <Utilities/Macro.h>

#include <Extension/TechnoExt.h>
#include <Extension/WarheadTypeExt.h>

#include <Ext/Common/CommonStatus.h>
#include <Ext/TechnoType/TechnoStatus.h>
#include <Ext/ObjectType/AttachEffect.h>

DEFINE_HOOK(0x6FA2A2, TechnoClass_Update_DrawBehind, 0x6)
{
	GET(TechnoClass*, pTechno, ESI);
	if (AmIStand(pTechno))
	{
		// skip draw behind anim
		return 0x6FA2DA;
	}
	return 0;
}

// AI的替身在攻击友军时会强制取消目标
DEFINE_HOOK(0x6FA45D, TechnoClass_Update_NotHuman_ClearTarget_Stand, 0x6)
{
	GET(TechnoClass*, pTechno, ESI);
	if (CombatDamage::Data()->AllowAIAttackFriendlies || AmIStand(pTechno))
	{
		return 0x6FA472;
	}
	return 0;
}

DEFINE_HOOK(0x5F6B90, ObjectClass_InAir_SkipCheckIsOnMap, 0x5)
{
	GET(ObjectClass*, pObject, ECX);
	TechnoClass* pTechno = nullptr;
	if (CastToTechno(pObject, pTechno) && AmIStand(pTechno))
	{
		return 0x5F6B97;
	}
	return 0;
}

// Stand can't set destination
// BlackHole's victim can't set destination
// Jumping unit can't set destination
DEFINE_HOOK(0x4D94B0, FootClass_SetDestination_Stand, 0x5)
{
	GET(TechnoClass*, pTechno, ECX);
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTechno, status) && (status->AmIStand() || status->CaptureByBlackHole || status->Jumping || status->Freezing))
	{
		// 跳过目的地设置
		return 0x4D9711;
	}
	return 0;
}

// Stand support Radar
DEFINE_HOOK(0x508EC6, HouseClass_RadarUpdate_Stand, 0x5)
{
	GET(TechnoClass*, pTechno, EAX);
	if (pTechno->IsOnMap || AmIStand(pTechno))
	{
		return 0x508ECD;
	}
	return 0x508F08;
}

// Stand support SpySat
DEFINE_HOOK(0x508F9B, HouseClass_SpySatUpdate_Stand, 0x5)
{
	GET(TechnoClass*, pTechno, ECX);
	if (pTechno->IsOnMap || AmIStand(pTechno))
	{
		return 0x508FA2;
	}
	return 0x508FF6;
}

// 替身需要显示在上层时，修改了渲染的层，导致单位在试图攻击替身时，需要武器具备AA
DEFINE_HOOK(0x6FC749, TechnoClass_CanFire_WhichLayer_Stand, 0x5)
{
	GET(Layer, layer, EAX);
	enum { inAir = 0x6FC74E, onGround = 0x6FC762 };
	if (layer != Layer::Ground)
	{
		try
		{
			GET_STACK(AbstractClass*, pTarget, 0x20 - (-0x4));
			TechnoClass* pTechno = nullptr;
			if (CastToTechno(pTarget, pTechno) && AmIStand(pTechno))
			{
				if (pTechno->IsInAir())
				{
					return inAir;
				}
				return onGround;
			}
		}
		catch (const std::exception& e)
		{
			Debug::Log("TechnoClass_CanFire_WhichLayer_Stand throw exception: %s \n", e.what());
		}
		return inAir;
	}
	return onGround;
}

DEFINE_HOOK(0x6FF66C, TechnoClass_Fire_DecreaseAmmo, 0x6)
{
	GET(TechnoClass*, pTechno, ESI);
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTechno, status) && status->AmIStand()
		&& status->pMyMaster && status->MyStandData.UseMasterAmmo)
	{
		int ammo = status->pMyMaster->Ammo;
		if (ammo > 0)
		{
			status->pMyMaster->Ammo--;
			status->pMyMaster->StartReloading();
		}
	}
	return 0;
}


#pragma region Pick Stand as Target

DEFINE_HOOK(0x6FCDBE, TechnoClass_SetTarget_Stand, 0x6)
{
	GET(AbstractClass*, pTarget, EDI);
	if (pTarget)
	{
		TechnoClass* pTechno = nullptr;
		TechnoStatus* status = nullptr;
		if (CastToTechno(pTarget, pTechno) && TryGetStatus<TechnoExt>(pTechno, status) && status->AmIStand()
			&& (status->MyStandData.Immune || !pTechno->GetTechnoType() || pTechno->GetTechnoType()->Insignificant || IsImmune(pTechno))
			)
		{
			// 替身处于无敌状态，目标设置为替身使者
			R->EDI(status->pMyMaster);
		}
	}
	return 0;
}

//6F9256 6 AircraftType IsOnMap ? 6F925C : 6F9377
DEFINE_HOOK(0x4D9947, FootClass_Greatest_Threat_GetTarget, 0x6)
{
	GET(TechnoClass*, pTechno, ESI);
	GET(AbstractClass*, pTarget, EAX);
	if (!pTarget && CombatDamage::Data()->AllowAutoPickStandAsTarget)
	{
		// no target, extend find Stand
		// 搜索符合条件的替身
		// Logger.Log($"{Game.CurrentFrame}, {pTechno}自动搜索目标, 替身列表里有 {TechnoStatusScript.StandArray.Count()} 个记录");
		for (auto stand : TechnoExt::StandArray)
		{
			TechnoClass* pStand = stand.first;
			if (!stand.second.Immune && pStand->GetTechnoType() && !pStand->GetTechnoType()->Insignificant && !IsImmune(pStand))
			{
				// 是否可以攻击，并获得攻击使用的武器序号
				pTarget = pStand;
				int weaponIdx = pTechno->SelectWeapon(pTarget);
				bool canFire = CanAttack(pTechno, pTarget, true, weaponIdx);
				if (canFire)
				{
					// 检查所属
					int damage = pTechno->CombatDamage(weaponIdx);
					if (damage < 0)
					{
						// 维修武器
						canFire = pTechno->Owner->IsAlliedWith(pStand->Owner) && pStand->Health < pStand->GetTechnoType()->Strength;
					}
					else
					{
						// 只允许攻击敌人
						canFire = !pTechno->Owner->IsAlliedWith(pStand->Owner);
						if (canFire)
						{
							// 检查平民
							if (IsCivilian(pStand->Owner))
							{
								// Ares 的平民敌对目标
								canFire = INI::GetSection(INI::Rules, pStand->GetTechnoType()->ID)->Get("CivilianEnemy", false);
								// Ares 的反击平民
								TechnoClass* pStandTarget = nullptr;
								if (!canFire && AutoRepel(pTechno->Owner) && CastToTechno(pStand->Target, pStandTarget))
								{
									canFire = pTechno->Owner->IsAlliedWith(pStandTarget->Owner);
								}
							}
						}
					}
				}
				if (canFire)
				{
					// Pick this Stand as Target
					R->EAX(pStand);
					break;
				}
			}
		}

	}
	return 0;
}

#pragma endregion

#pragma region Stand Drawing
// WWSB只排序在地面的layer，不排序在空中的layer
// 替身绘制顺序：让替身画在其 Master 之上（需求本身）——但**不再用"每帧排整层"实现**。
//
// 为什么必须换掉"每帧排序"（勿回退，详见 docs/联机失同步排查报告.md §12）：
//   · 引擎的绘制顺序 = 层数组 vec_ObjectsInLayers[Layer] 的**下标顺序**。
//     Tactical_Draw_All_6D8DB0 (0x6D8DB0) 对每一层（0..4）各做一遍升序 Items[0..Count-1]
//     遍历：第一遍画主体（vt+0x104）、第二遍画 vt+0x110 DrawExtras。
//     下标越大越后画 ⇒ 越在上。
//   · 但同一个数组也是**联机同步校验值**的输入：sub_64DAB0 (0x64DAB0) 按同一顺序做
//     顺序敏感哈希 sum = sum*2 + value。Air 层数组的消费者**只有**"绘制"和"校验值"这两个。
//   · 旧实现 0x55DBC3（Main_Loop 尾部对整层 Air 调 LayerClass::Sort()）同时改动了"画面"和
//     "校验值"；而它在帧内的位置是：绘制(0x55DBBE) → **[旧排序]** → Ground 排序(0x55DBC8)
//     → LogicClass_Update(0x55DC9E) → 校验值(0x55DE40)。也就是说它对**当前帧的画面已经毫无作用**，
//     却必然进入**同一帧的校验值** —— 是失同步的高危来源。
//     （下游 Phobos 的同类做法 0x4A9750 已因失同步被退役，见报告 §11.1 对照表。）
//
// 现行做法：不排序，改在**提交时刻把替身摆到其 Master 的正确一侧** —— 纯位置运算。
//
// 渲染模型（2026-10-04 复核）：
//   · 绘制顺序 = 层号升序 × 层内下标升序（Tactical_Draw_All_6D8DB0，0x6D8F39/0x6D941D）；
//     YR 没有 Z-buffer，"遮挡"就是"谁后画" ⇒ **同层内下标大者在上**。
//   · 引擎**只对 Ground(=2) 排序**：0x55DBC3 `mov ecx, offset 0x8A0390`（0x8A0390 = Ground
//     层对象）→ 0x55DBC8 `call LayerClass::Sort`(0x551A30，单趟相邻冒泡，按 vt[0xB8] GetYSort)。
//     且 Ground 的插入才带 sorted=true（Submit 内 0x4A9747 `cmp edi,2` / 0x4A974D `setz cl`）。
//     ⇒ **Air 层永不排序**：Air 层的顺序 = 插入顺序 = 本 Hook 说了算。
//   · 正因如此，Stand.ZOffset（经 ObjectClass_GetYSort 0x5F6BF7 → vt[0xB8]）对**空中替身**
//     原本完全不生效（它只借 Ground 的排序/有序插入生效）；地面替身不受本 Hook 影响，照旧有效。
//
namespace StandLayer
{
	enum EClass { Other = 0, Below = 1, Upper = 2 };

	// 把 Air 层数组里的对象相对某个 Master 分类：
	//   Other —— 与本 Master 无关（或根本不是 Techno 替身）
	//   Below —— 本 Master 的替身，且 Stand.ZOffset <  0（要求画在 Master 下层）
	//   Upper —— 本 Master 的替身，且 Stand.ZOffset >= 0（要求画在 Master 上层）
	// 纯只读：只做 ExtMap 查找与几个字段比较，无分配、无副作用。
	static EClass Classify(ObjectClass* const pObj, TechnoClass* const pMaster)
	{
		TechnoClass* pTechno = abstract_cast<TechnoClass*>(pObj);
		if (!pTechno)
			return Other;

		TechnoStatus* status = nullptr;
		if (!TryGetStatus<TechnoExt, TechnoStatus>(pTechno, status)
			|| !status->AmIStand() || status->pMyMaster != pMaster || status->MyStandData.IsTrain)
			return Other;

		return status->MyStandData.ZOffset < 0 ? Below : Upper;
	}

	static Layer GetLayer(TechnoClass* pTechno)
	{
		// 替身：跟随 Master 所在层（唯一必须介入的情形）
		TechnoStatus* status = nullptr;
		if (TryGetStatus<TechnoExt, TechnoStatus>(pTechno, status)
			&& status->AmIStand() && status->pMyMaster && !status->MyStandData.IsTrain)
		{
			Layer l = status->pMyMaster->LastLayer;
			if (l != Layer::None)
			{
				return l;
			}
		}

		// Kratos 原有语义，但**纯函数化**（无缓存、无历史依赖）
		return pTechno->IsInAir() && !pTechno->IsFallingDown ? Layer::Air : Layer::Ground;
	}
}

DEFINE_HOOK(0x4A9768, DisplayClass_Submit_KeepStandAboveMaster, 0x5)
{
	GET(ObjectClass*, pObject, ESI);
	if (!pObject)
		return 0;          // Submit 有 esi==null 的早退分支同样汇合到本出口

	GET(Layer, layer, EDI);

	// 只处理 Air 层：Ground 层由引擎自己的有序插入（sortable == true）负责，不动它。
	if (layer != Layer::Air)
		return 0;

	TechnoClass* pTechno = abstract_cast<TechnoClass*>(pObject);
	if (!pTechno)
		return 0;          // 动画 / 子弹 / 碎片等非 Techno：本 Hook 不负责（Air 层替身只会是 Techno）
	                       // 注：AE 动画（AnimClass）相对主身的顺序由 AnimStatus::OnUpdate 的
	                       //     层监视 + DisplayClass::Instance->Submit（AnimStatus.cpp）负责，
	                       //     经由本函数同一引擎出口时在此早退，两套机制互不重叠，勿在此叠加动画逻辑。

	TechnoStatus* selfStatus = nullptr;
	const bool selfIsStand =
		TryGetStatus<TechnoExt, TechnoStatus>(pTechno, selfStatus)
		&& selfStatus->AmIStand() && selfStatus->pMyMaster && !selfStatus->MyStandData.IsTrain;

	LayerClass* pLayer = MapClass::GetLayer(Layer::Air);
	if (!pLayer || !pLayer->Items)
		return 0;

	const int count = pLayer->Count;
	// 防御：Count 与 Capacity 不一致说明数组已被外部破坏，此时不做任何写入。
	if (count < 2 || count > pLayer->Capacity)
		return 0;

	ObjectClass** const items = pLayer->Items;

	// Air 层是"追加"，刚插入的对象必然在队尾；否则不做任何假设。
	if (items[count - 1] != pObject)
		return 0;

	// ---- 情形 R1：刚入队的是替身自己 ----
	if (selfIsStand)
	{
		// ZOffset >= 0（"正值显示在 JOJO 上层"）：队尾天然高于 Master，无需动作。
		if (selfStatus->MyStandData.ZOffset >= 0)
			return 0;

		// ZOffset < 0（"负值在下层"）：把替身移到其 Master 之前。
		// Air 层从不被引擎排序，这一步只有我们能做。
		TechnoClass* const pSelfMaster = selfStatus->pMyMaster;
		for (int m = 0; m < count - 1; ++m)
		{
			if (items[m] != pSelfMaster)
				continue;

			// 把队尾的替身左移到 m（[m, count-2] 整体右移一位）。
			for (int k = count - 1; k > m; --k)
				items[k] = items[k - 1];
			items[m] = pObject;
			break;                 // 找不到 Master 则什么都不做（保持队尾）
		}
		return 0;
	}

	// ---- 情形 R2：刚入队的是 Master，且层内已有它的替身 ----
	// 目标布局：[ 其它对象..., ZOffset<0 的替身..., Master, ZOffset>=0 的替身... ]
	// 做法：对 [0, count-1) 做稳定三分类（两趟「顺序扫描 + 单元素右旋」），
	//       再把队尾的 Master 移到"第一件上位替身"之前。额外空间 O(1)、无状态。
	int upperStart = 0;
	for (int i = 0; i < count - 1; ++i)
	{
		if (StandLayer::Classify(items[i], pTechno) == StandLayer::Upper)
			continue;                      // 上位替身：留给后面的右旋，保持原序
		if (upperStart != i)
		{
			ObjectClass* const pMoved = items[i];
			for (int k = i; k > upperStart; --k)
				items[k] = items[k - 1];
			items[upperStart] = pMoved;
		}
		++upperStart;
	}
	// 此时 [0, upperStart) = 其它对象 ∪ 下沉替身（原序），[upperStart, count-1) = 上位替身（原序）
	int belowStart = 0;
	for (int i = 0; i < upperStart; ++i)
	{
		if (StandLayer::Classify(items[i], pTechno) == StandLayer::Below)
			continue;                      // 下沉替身：留给后面的右旋，保持原序
		if (belowStart != i)
		{
			ObjectClass* const pMoved = items[i];
			for (int k = i; k > belowStart; --k)
				items[k] = items[k - 1];
			items[belowStart] = pMoved;
		}
		++belowStart;
	}
	// 此时 [0, belowStart) = 其它对象，[belowStart, upperStart) = 下沉替身，其后 = 上位替身

	// 没有上位替身 ⇒ 所有替身都要求在下层，当前顺序已正确。
	if (upperStart == count - 1)
		return 0;

	// 把队尾的 Master 左移到 upperStart（第一件上位替身之前）。
	for (int k = count - 1; k > upperStart; --k)
		items[k] = items[k - 1];
	items[upperStart] = pObject;

	return 0;              // 重放 `test al, al`，继续 Submit 原逻辑
}


// ③ 精灵的屏幕纵向偏移（`FootClass::GetZAdjustment` `0x4DB091`）：替身从 Master 取 ZAdjust。
//    `pSkip / pStand / zAdjust` 是"跨调用记忆"缓存 —— 报告 §12.6(3) 判定：
//    渲染路径、不进校验值，风险等级低，**待单独评估**（本轮先不动）。
namespace StandZAdjust
{
	TechnoClass* pSkip = nullptr;
	TechnoClass* pStand = nullptr;
	int zAdjust = -1;
}

DEFINE_HOOK(0x4DB091, FootClass_GetZAdjustment, 0x6)
{
	GET(TechnoClass*, pTechno, ESI);
	if (pTechno == StandZAdjust::pStand)
	{
		R->EDI(0);
		R->Stack(0x8, 0);
		R->EAX(StandZAdjust::zAdjust);
	}
	else if (pTechno == StandZAdjust::pSkip)
	{
		// 什么都不做
	}
	else
	{
		TechnoStatus* status = nullptr;
		if (TryGetStatus<TechnoExt>(pTechno, status) && status->AmIStand() && status->pMyMaster && !status->MyStandData.IsTrain)
		{
			// 从JOJO身上获取ZAdjust
			TechnoClass* pMaster = status->pMyMaster;
			int z = pMaster->GetZAdjustment();
			if (BuildingClass* pBuilding = abstract_cast<BuildingClass*, true>(pMaster))
			{
				if (status->MyStandData.ZOffset < 0)
				{
					z += 3;
				}
				else
				{
					// 加上自身的zAdjust
					int zz = pTechno->GetZ();
					int zzz = -TacticalClass::Instance->AdjustForZ(zz);
					z += zzz;
					// 加上建筑动画的zAdjust
					BuildingTypeClass* pType = pBuilding->Type;
					int bZ = 0;
					for (int i = 0; i < 0x15; i++)
					{
						int zAdjust = pType->BuildingAnim[i].ZAdjust;
						if (bZ > zAdjust)
						{
							bZ = zAdjust;
						}
					}
					// 再加上一层，每一层高度是15
					z += bZ - 14;
				}
			}

			StandZAdjust::pSkip = nullptr;
			StandZAdjust::pStand = pTechno;
			StandZAdjust::zAdjust = z;
			R->EDI(0);
			R->Stack(0x8, 0);
			R->EAX(z);
		}
		else
		{
			StandZAdjust::pSkip = pTechno;
			StandZAdjust::pStand = nullptr;
			StandZAdjust::zAdjust = -1;
		}
	}
	return 0;
}

// Whether to render in the upper or lower layers is controlled by sorting DesplayClass::LayerClass[Layer].
// When Layer==Ground, use the YSort function for sorting,
// and the return value is the X + Y of the rendering coordinates.
//
// ② Ground 层的**排序键**（`ObjectClass::GetYSort` `0x5F6BF7`）—— 引擎每帧唯一排序的就是 Ground 层。
DEFINE_HOOK(0x5F6BF7, ObjectClass_GetYSort, 0x5)
{
	GET(ObjectClass*, pObject, ESI);
	GET(int*, x, EAX);
	int* y = (int*)(R->EDI() + 4);

	if (TechnoClass* pTechno = abstract_cast<TechnoClass*>(pObject))
	{
		TechnoStatus* status = nullptr;
		if (TryGetStatus<TechnoExt, TechnoStatus>(pTechno, status) && status->AmIStand() && status->pMyMaster && !status->MyStandData.IsTrain)
		{
			// 替身从Master身上获取渲染坐标
			CoordStruct r = status->pMyMaster->GetRenderCoords();
			int offset = status->MyStandData.ZOffset;
			if (offset >= 0)
			{
				if (BuildingClass* pBuilding = abstract_cast<BuildingClass*>(status->pMyMaster))
				{
					BuildingTypeClass* pType = pBuilding->Type;
					int bY = 0;
					for (int i = 0; i < 0x15; i++)
					{
						int ySort = pType->BuildingAnim[i].YSort;
						if (bY < ySort)
						{
							bY = ySort;
						}
					}
					offset += bY + 16;
				}
			}
				// 修改返回值
			*x = r.X + offset;
			*y = r.Y + offset;
		}
	}
	return 0;
}

DEFINE_HOOK(0x75C7E0, WalkLocomotionClass_In_Which_Layer, 0x5) // WalkLoco
{
	GET(TechnoClass*, pTechno, ESI);
	Layer layer = StandLayer::GetLayer(pTechno);
	R->EAX(layer);
	return 0x75C7E5;
}

DEFINE_HOOK(0x6A3E50, ShipLocomotionClass_In_Which_Layer, 0x5) // ShipLoco
{
	GET(TechnoClass*, pTechno, ESI);
	Layer layer = StandLayer::GetLayer(pTechno);
	R->EAX(layer);
	return 0x6A3E55;
}

DEFINE_HOOK(0x5B19D0, MechLocomotionClass_In_Which_Layer, 0x5) // MechLoco
{
	GET(TechnoClass*, pTechno, ESI);
	Layer layer = StandLayer::GetLayer(pTechno);
	R->EAX(layer);
	return 0x5B19D5;
}

DEFINE_HOOK(0x517100, HoverLocomotionClass_In_Which_Layer, 0x5) // HoverLoco
{
	GET(TechnoClass*, pTechno, ESI);
	Layer layer = StandLayer::GetLayer(pTechno);
	R->EAX(layer);
	return 0x517105;
}

DEFINE_HOOK(0x4B4820, DriveLocomotionClass_In_Which_Layer, 0x5) // DriveLoco
{
	GET(TechnoClass*, pTechno, ESI);
	Layer layer = StandLayer::GetLayer(pTechno);
	R->EAX(layer);
	return 0x4B4825;
}

// 不允许JJ类型，layer设置为top
DEFINE_HOOK(0x54B8E9, JumpjetLocomotionClass_In_Which_Layer_Deviation, 0x6)
{
	GET(TechnoClass*, pTechno, EAX);

	if (pTechno->IsInAir()) {
		// Override JumpjetHeight / CruiseHeight check so it always results in 3 / Layer::Air.
		R->EDX(INT32_MAX);
		return 0x54B96B;
	}
	return 0;
}
#pragma endregion

#pragma region Amin ChronoSparkle

DEFINE_HOOK(0x414C27, AircraftClass_Update_SkipCreateChronoSparkleAnimOnStand, 0x5)
{
	GET(TechnoClass*, pTechno, ESI);

	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTechno, status) && status->AmIStand() && status->MyStandData.Immune)
	{
		return 0x414C78;
	}
	return 0;
}

DEFINE_HOOK(0x440406, BuildingClass_Update_SkipCreateChronoSparkleAnimOnStand1, 0x5)
{
	GET(TechnoClass*, pTechno, ESI);

	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTechno, status) && status->AmIStand() && status->MyStandData.Immune)
	{
		return 0x44055D;
	}
	return 0;
}

DEFINE_HOOK(0x44050C, BuildingClass_Update_SkipCreateChronoSparkleAnimOnStand2, 0x5)
{
	GET(TechnoClass*, pTechno, ESI);

	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTechno, status) && status->AmIStand() && status->MyStandData.Immune)
	{
		return 0x44055D;
	}
	return 0;
}

DEFINE_HOOK(0x51BB17, InfantryClass_Update_SkipCreateChronoSparkleAnimOnStand, 0x5)
{
	GET(TechnoClass*, pTechno, ESI);

	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTechno, status) && status->AmIStand() && status->MyStandData.Immune)
	{
		return 0x51BB6E;
	}
	return 0;
}

DEFINE_HOOK(0x736250, UnitClass_Update_SkipCreateChronoSparkleAnimOnStand, 0x5)
{
	GET(TechnoClass*, pTechno, ESI);

	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTechno, status) && status->AmIStand() && status->MyStandData.Immune)
	{
		return 0x7362A7;
	}
	return 0;
}

#pragma endregion


