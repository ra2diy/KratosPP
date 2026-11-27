#include "SpringyRefresh.h"

#include <algorithm>
#include <cmath>

#include <BuildingClass.h>
#include <AnimClass.h>
#include <TacticalClass.h>

#include <Ext/Common/SpringyHelper.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>


namespace
{
	/// @brief 把对象报上来的渲染矩形按形变后的范围外扩
	/// @param rect 引擎给出的渲染矩形（与脏区同一坐标口径）
	/// @param snap 本帧（或上一次绘制时）的快照
	/// @return 外扩后的矩形
	/// @note 缩放与剪切都可能把像素画到原矩形之外，且剪切会把顶边整体推到一侧，
	///       所以交给 SpringyHelper 按四个方向各自补齐，而不是四周等量外扩
	RectangleStruct InflateDirtyRect(const RectangleStruct& rect, const SpringySnapshot& snap)
	{
		double scaleW = 1.0;
		double scaleH = 1.0;
		SpringyHelper::GetRenderScales(snap, scaleW, scaleH);
		return SpringyHelper::InflateDirtyRect(rect, scaleW, scaleH, snap.ShearX, snap.ShearY, snap.ShearMode);
	}
	/// @brief 建筑自己会画的东西：本体帧号与所有覆盖动画的当前帧
	/// @param pObject 目标对象
	/// @return 一个可比较的帧状态键；非建筑恒为 0
	/// @note 建筑静止时引擎不会主动重绘它，所以除了倍率变化，
	///       本体换帧、覆盖动画（活动/待机/损毁火焰）换帧同样要把画面重刷一遍，
	///       否则缩放后的覆盖动画会停在上一帧的位置与尺寸上
	long long VisualKey(ObjectClass* pObject)
	{
		// 已死或已进 limbo 的对象不再参与：它的覆盖动画指针可能已经被清掉，
		// 再去读 Anims[] 里那些指针就是读已释放的内存
		if (IsDeadOrInvisible(pObject) || pObject->WhatAmI() != AbstractType::Building)
		{
			return 0;
		}

		BuildingClass* pBuilding = static_cast<BuildingClass*>(pObject);
		long long key = static_cast<long long>(pBuilding->GetCurrentFrame()) + 1;

		for (int i = 0; i < 0x15; i++)
		{
			if (AnimClass* pAnim = pBuilding->Anims[i])
			{
				key = key * 31 + pAnim->Animation.Value + 1;
			}
		}

		for (int i = 0; i < 0x8; i++)
		{
			if (AnimClass* pAnim = pBuilding->DamageFireAnims[i])
			{
				key = key * 31 + pAnim->Animation.Value + 1;
			}
		}

		return key;
	}

	/// @brief 判断「这一帧的画面是否与上次补脏时一样」的键
	/// @param pTechno 目标单位
	/// @param snap 当前缩放快照
	/// @note 倍率与建筑自身的帧状态一起参与：任一变化都要重刷
	long long DirtyKey(TechnoClass* pTechno, const SpringySnapshot& snap)
	{
		return SpringyHelper::RenderKey(snap) * 31 + VisualKey(pTechno);
	}

	/// @brief 把一块矩形交给引擎的脏区列表，让它从暂存表面重绘
	/// @param rect 矩形（与引擎脏区同一坐标口径）
	void AddDirtyArea(const RectangleStruct& rect)
	{
		if (rect.Width <= 0 || rect.Height <= 0)
		{
			return;
		}
		TacticalClass::Instance->RegisterDirtyArea(rect, false);
	}

	/// @brief 给一个对象补一次重绘与脏矩形
	/// @param pObject 目标对象
	/// @param status 该对象的扩展状态（记录上次标脏覆盖过的尺寸）
	/// @param snap 该对象当前的（或撤除前最后一份）缩放快照
	/// @param markForRedraw 是否同时标记对象重绘：本来每帧都在重绘的对象不需要
	/// @note 矩形直接用 GetRenderDimensions 报上来的那份，引擎的脏区与它同一坐标口径；
	///       自己再从世界坐标换算一遍会差一个视口偏移，补出来的脏区落不到对象身上
	void MarkDirty(ObjectClass* pObject, TechnoStatus* status,
		const SpringySnapshot& snap, bool markForRedraw)
	{
		// 已死或已进 limbo 的对象问不到渲染矩形：图像数据随死亡一起释放，
		// 引擎的 GetRenderDimensions 在取图像那一步拿到空指针就直接崩（0x5F48A3）。
		// 而且这种对象不会再被绘制，放大版最后一次贴图时的脏区已经在那一次的
		// AddDirtyOnly 里补过，这里没有需要补的东西
		if (IsDeadOrInvisible(pObject))
		{
			return;
		}

		RectangleStruct dims = {};
		pObject->GetRenderDimensions(&dims);
		if (dims.Width <= 0 || dims.Height <= 0)
		{
			return;
		}

		// 覆盖范围取「这一次要画的」与「上一次覆盖的」之并：
		// 缩小时屏幕上还留着上一帧更大的那圈像素，只按当前形变算会擦不掉外圈；
		// 剪切改变的是形状与偏移，不能拿两份矩阵逐项取 max（那不是一个合法形变）
		RectangleStruct now = InflateDirtyRect(dims, snap);
		RectangleStruct before = InflateDirtyRect(dims, status->LastSpringyDirtySnap);

		// 建筑是增量重绘：屏幕上的旧像素只能靠我们登记的脏区擦掉。
		// 形变每帧都在变，只按当前形状外扩会留下一圈旧帧的像素（表现为横条纹），
		// 所以建筑再额外四周留一圈与剪切量同量级的余量
		if (pObject->WhatAmI() == AbstractType::Building)
		{
			const int extra = static_cast<int>(std::ceil(std::max(64.0,
				std::abs(snap.ShearX) + std::abs(snap.ShearY)
				+ std::abs(status->LastSpringyDirtySnap.ShearX) + std::abs(status->LastSpringyDirtySnap.ShearY))));
			const auto grow = [extra](const RectangleStruct& r) -> RectangleStruct {
				return RectangleStruct{ r.X - extra, r.Y - extra, r.Width + extra * 2, r.Height + extra * 2 };
			};
			now = grow(now);
			before = grow(before);
		}
		status->LastSpringyDirtySnap = snap;

		if (markForRedraw)
		{
			pObject->MarkForRedraw();
		}
		AddDirtyArea(SpringyHelper::UnionRect(now, before));
	}

	/// @brief 缩放撤除后把「残影账」结清
	/// @param status 该对象的扩展状态
	/// @note 补完最后一次脏区之后这两份旧快照就没有用了。不复位的话，之后每次动画换帧
	///       都会按撤除前的外扩量再登记一遍脏矩形 —— 对一栋早就没有本效果器的建筑来说，
	///       那是纯浪费，而且脏区还会一直偏大
	void FinishDirty(TechnoStatus* status)
	{
		status->LastSpringyDrawn.Reset();
		status->LastSpringyDirtySnap.Reset();
	}

	/// @brief 处理单个单位：倍率相对上次标脏时变了才重绘
	void UpdateOne(TechnoClass* pTechno, TechnoStatus* status)
	{
		const SpringySnapshot& snap = status->MySpringyData;
		const bool active = snap.Active;

		// 没在缩放、也没有上一帧留下的残影要擦 —— 这个对象本效果器一次都不该碰。
		// 主循环每帧遍历全部建筑，没这一句的话，一栋从没挂过效果的建筑只要覆盖动画换帧，
		// 就要算一遍 VisualKey（本体帧 + 0x15 个 Anims + 8 个 DamageFireAnims），
		// 再 MarkForRedraw + 登记一块外扩过的脏矩形，纯属替引擎做无用功
		if (!active && !status->LastSpringyDrawn.Active)
		{
			return;
		}

		const long long key = DirtyKey(pTechno, snap);
		if (key != status->LastSpringyDirtyKey)
		{
			status->LastSpringyDirtyKey = key;
			// 倍率撤除后画面要缩回原样，脏区得按撤除前那一份算，否则会留下一圈放大的残影
			MarkDirty(pTechno, status, active ? snap : status->LastSpringyDrawn, true);
		}

		if (!active)
		{
			// 这一帧（或 End 里那次 RefreshNow）补的就是最后一笔，结清之后不再参与本效果器
			FinishDirty(status);
		}
	}
}


void SpringyRefresh::OnFrame()
{
	// 建筑不会自己重绘：位置不动、动画帧不变时引擎根本不碰它，
	// 所以要在这里逐帧检查它们的倍率是否变了，变了才标脏
	for (BuildingClass* pBuilding : *BuildingClass::Array)
	{
		if (!pBuilding)
		{
			continue;
		}
		TechnoStatus* status = nullptr;
		if (!TryGetStatus<TechnoExt>(pBuilding, status))
		{
			continue;
		}
		UpdateOne(pBuilding, status);
	}
}

void SpringyRefresh::RefreshNow(TechnoClass* pTechno)
{
	if (!pTechno)
	{
		return;
	}
	TechnoStatus* status = nullptr;
	if (!TryGetStatus<TechnoExt>(pTechno, status))
	{
		return;
	}
	// 强制当帧重绘：AE 刚撤除、倍率要弹回原样，不等下一帧的键比较
	const bool active = status->MySpringyData.Active;
	status->LastSpringyDirtyKey = DirtyKey(pTechno, status->MySpringyData);
	MarkDirty(pTechno, status,
		active ? status->MySpringyData : status->LastSpringyDrawn, true);

	// AE 撤除走的就是这一条：补完这一笔残影账，主循环那边就再也不必碰这个对象了
	if (!active)
	{
		FinishDirty(status);
	}
}

void SpringyRefresh::AddDirtyOnly(TechnoClass* pTechno)
{
	if (!pTechno)
	{
		return;
	}
	TechnoStatus* status = nullptr;
	if (!TryGetStatus<TechnoExt>(pTechno, status))
	{
		return;
	}
	if (!status->MySpringyData.Active)
	{
		return;
	}
	// 只在本帧画面与上次标脏时不同才补，避免每帧重复压一堆同样的矩形进脏区列表
	const long long key = DirtyKey(pTechno, status->MySpringyData);
	if (key == status->LastSpringyDirtyKey)
	{
		return;
	}
	status->LastSpringyDirtyKey = key;

	// 这个入口只补脏区：载具与步兵本来每帧就在重绘，不需要再标一次
	MarkDirty(pTechno, status, status->MySpringyData, false);
}
