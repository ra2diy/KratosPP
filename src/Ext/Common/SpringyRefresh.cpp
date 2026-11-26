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
	/// @brief 把对象报上来的渲染矩形按最坏情况外扩
	/// @param rect 引擎给出的渲染矩形（与脏区同一坐标口径）
	/// @param snap 缩放快照
	/// @return 外扩后的矩形
	/// @note 只有放大才会把像素画到原矩形之外，缩小时不会，所以取相对 1.0 的正向增量
	RectangleStruct InflateDirtyRect(const RectangleStruct& rect, const SpringySnapshot& snap)
	{
		const double overW = std::max({ snap.ScaleX, snap.ScaleY, 1.0 }) - 1.0;
		const double overH = std::max(snap.ScaleZ, 1.0) - 1.0;
		return SpringyHelper::InflateDirtyRect(rect,
			static_cast<int>(std::lround(overW * 100.0)),
			static_cast<int>(std::lround(overH * 100.0)));
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

		// 覆盖范围取「这一次要画的」与「上一次覆盖的」较大值：
		// 倍率缩小时屏幕上还留着上一帧更大的那圈像素，只按当前尺寸算会擦不掉外圈
		SpringySnapshot dirty = snap;
		dirty.ScaleX = std::max(dirty.ScaleX, status->LastSpringyDirtySnap.ScaleX);
		dirty.ScaleY = std::max(dirty.ScaleY, status->LastSpringyDirtySnap.ScaleY);
		dirty.ScaleZ = std::max(dirty.ScaleZ, status->LastSpringyDirtySnap.ScaleZ);
		status->LastSpringyDirtySnap = dirty;

		if (markForRedraw)
		{
			pObject->MarkForRedraw();
		}
		AddDirtyArea(InflateDirtyRect(dims, dirty));
	}

	/// @brief 处理单个单位：倍率相对上次标脏时变了才重绘
	void UpdateOne(TechnoClass* pTechno, TechnoStatus* status)
	{
		const SpringySnapshot& snap = status->MySpringyData;
		const long long key = DirtyKey(pTechno, snap);
		if (key == status->LastSpringyDirtyKey)
		{
			// 画面与上次标脏时一致，不必重绘
			return;
		}
		status->LastSpringyDirtyKey = key;
		// 倍率撤除后画面要缩回原样，脏区得按撤除前那一份算，否则会留下一圈放大的残影
		MarkDirty(pTechno, status, snap.Active ? snap : status->LastSpringyDrawn, true);
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
	status->LastSpringyDirtyKey = DirtyKey(pTechno, status->MySpringyData);
	MarkDirty(pTechno, status,
		status->MySpringyData.Active ? status->MySpringyData : status->LastSpringyDrawn, true);
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
