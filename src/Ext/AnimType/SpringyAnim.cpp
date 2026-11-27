#include "SpringyAnim.h"

#include <cmath>

#include <ObjectClass.h>
#include <FootClass.h>
#include <TechnoClass.h>
#include <AnimClass.h>
#include <AnimTypeClass.h>
#include <BuildingClass.h>
#include <FileFormats/SHP.h>
#include <TechnoTypeClass.h>

#include <Ext/AnimType/AnimStatus.h>
#include <Ext/Common/SpringyHelper.h>
#include <Ext/Common/SpringySHP.h>
#include <Ext/Helper/CastEx.h>
#include <Ext/Helper/Scripts.h>


namespace
{
	SpringyAnim::DrawContext Context{};

	/// @brief 取一份 SHP 的画布尺寸（引用类型跟随到实际文件）
	bool GetCanvasDims(SHPStruct* pSHP, short& w, short& h)
	{
		if (!pSHP)
		{
			return false;
		}
		if (pSHP->Width > 0 && pSHP->Height > 0)
		{
			w = pSHP->Width;
			h = pSHP->Height;
			return true;
		}
		if (pSHP->IsReference())
		{
			if (const auto pFile = pSHP->GetData())
			{
				w = pFile->Width;
				h = pFile->Height;
				return w > 0 && h > 0;
			}
		}
		return false;
	}

	/// @brief 取建筑本体帧的底边中点，作为其覆盖动画的缩放枢轴
	/// @param pBuilding 依附的建筑
	/// @param pivotX,pivotY 输出的枢轴（SHP 帧坐标）
	/// @param canvasW,canvasH 输出的建筑画布尺寸，用于校验动画画布是否同源
	/// @return 是否取到
	bool GetBuildingPivot(BuildingClass* pBuilding, int& pivotX, int& pivotY, short& canvasW, short& canvasH)
	{
		TechnoTypeClass* pType = pBuilding->GetTechnoType();
		if (!pType)
		{
			return false;
		}
		SHPStruct* pImg = pType->GetImage();
		if (!pImg || !GetCanvasDims(pImg, canvasW, canvasH))
		{
			return false;
		}

		const int frame = pBuilding->GetCurrentFrame();
		if (frame < 0 || frame >= pImg->Frames)
		{
			return false;
		}

		RectangleStruct bounds = {};
		pImg->GetFrameBounds(bounds, frame);
		pivotX = bounds.X + bounds.Width / 2;
		pivotY = bounds.Y + bounds.Height;
		return true;
	}
}


bool SpringyAnim::ResolveOwner(AnimClass* pAnim, TechnoClass*& pOwner)
{
	pOwner = nullptr;
	if (!pAnim)
	{
		return false;
	}

	// AE 动画的 OwnerObject 恒为空：创建时只设了所属方颜色（SetAnimOwner），
	// 真实依附对象记在 AnimStatus::pAttachOwner 上。
	// 建筑覆盖动画创建时也把 pAttachOwner 写成了建筑，所以这一条能同时覆盖两者
	ObjectClass* pAttach = nullptr;
	AnimStatus* status = nullptr;
	if (TryGetStatus<AnimExt>(pAnim, status) && status->pAttachOwner)
	{
		pAttach = status->pAttachOwner;
	}
	if (!pAttach)
	{
		pAttach = pAnim->OwnerObject;
	}

	pOwner = abstract_cast<TechnoClass*>(pAttach);
	return pOwner != nullptr;
}

void SpringyAnim::BeginDraw(AnimClass* pAnim)
{
	// 每条动画绘制前先清空，避免沿用上一条的判定结果
	Context = {};

	TechnoClass* pOwner = nullptr;
	if (!ResolveOwner(pAnim, pOwner))
	{
		return;
	}

	const SpringySnapshot* snap = SpringyHelper::GetSnapshot(pOwner);
	if (!snap)
	{
		return;
	}

	Context.Active = true;
	// 直接用单位的画布形变矩阵：主体怎么缩放 / 剪切，它身上的动画就怎么变
	Context.Transform = snap->Matrix;

	if (snap->AnimPivot == SpringyAnimPivot::OWNER)
	{
		// 只有建筑与它的覆盖动画共用同一张画布，用建筑体型的枢轴才有意义；
		// 其它单位类型的画布与动画画布不同源，硬套会把动画甩出去，这里保持自身底边
		if (BuildingClass* pBuilding = abstract_cast<BuildingClass*>(pOwner))
		{
			int pivotX = 0;
			int pivotY = 0;
			short ownerW = 0;
			short ownerH = 0;
			if (GetBuildingPivot(pBuilding, pivotX, pivotY, ownerW, ownerH))
			{
				Context.OwnerCanvasW = ownerW;
				Context.OwnerCanvasH = ownerH;
				Context.UsePivot = true;
				Context.PivotX = pivotX;
				Context.PivotY = pivotY;
			}
		}
	}
}

SHPStruct* SpringyAnim::SubstituteSHP(SHPStruct* pOriginal)
{
	if (!Context.Active || !pOriginal)
	{
		return pOriginal;
	}
	// 画布尺寸与依附单位不一致的动画（例如复用通用美术的损毁火焰）套单位枢轴会跑偏，
	// 这类退回自身帧底边
	bool usePivot = Context.UsePivot;
	if (usePivot && Context.OwnerCanvasW > 0 && Context.OwnerCanvasH > 0)
	{
		short w = 0;
		short h = 0;
		if (GetCanvasDims(pOriginal, w, h)
			&& (w != Context.OwnerCanvasW || h != Context.OwnerCanvasH))
		{
			usePivot = false;
		}
	}
	return SpringySHP::GetTransformed(pOriginal, Context.Transform,
		Context.PivotX, Context.PivotY, usePivot);
}
