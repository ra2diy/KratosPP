#include <exception>
#include <cstdint>

#include <GeneralStructures.h>
#include <SpecificStructures.h>
#include <ObjectClass.h>
#include <FootClass.h>
#include <TechnoClass.h>
#include <Drawing.h>
#include <Surface.h>

#include <Extension.h>
#include <Utilities/Macro.h>
#include <Utilities/Debug.h>

#include <FileFormats/SHP.h>

#include <Extension/TechnoExt.h>

#include <Ext/Common/SpringyHelper.h>
#include <Ext/Common/SpringyRefresh.h>
#include <Ext/Common/SpringySHP.h>
#include <Ext/Common/SpringyVoxelSprite.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/TechnoType/TechnoStatus.h>




// 所有 Techno 类别体素绘制的入口。这里只做建筑炮塔染色；
// 本效果器的体素放大不在这里，而在下面的贴图点
DEFINE_HOOK(0x706640, TechnoClass_DrawVX, 0x5)
{
	GET(TechnoClass*, pTechno, ECX);
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTechno, status) && status->IsBuilding())
	{
		status->DrawVXL_Paintball(R, true);
	}

	return 0;
}


// 体素精灵的贴图点。全游戏只有两个调用者：0x706866（主体 / 炮塔 / 炮管）、
// 0x706CC0（影子）。栈布局（函数入口）：
//   [esp+4] = VoxelCacheStruct*（这次要贴的精灵）、[esp+8] = 绘制基准点、
//   ECX = this（Techno）
//
// 引擎在这里拿到的是按原尺寸光栅化好的成品位图，而放大必须在光栅化之后做：
// 光栅化器按绘制矩阵的步长在屏幕行列上取样，把放大尺寸直接交给它，相邻采样之间
// 就留 倍率−1 个空位，画面上是一片离散像素被网格状缝隙切开。成品位图本身是完整的，
// 对它重采样不会凭空产生缝。
//
// 采样点唯一，所以主体、炮塔、炮管、影子全都走这一条，不需要各自处理；
// 精灵不写进引擎缓存，只在贴图这一刻替换，引擎的缓存逻辑一点不动。
DEFINE_HOOK(0x707480, TechnoClass_BlitVoxelSprite_Springy, 0x5)
{
	GET(TechnoClass*, pTechno, ECX);

	const SpringySnapshot* snap = SpringyHelper::GetSnapshot(pTechno);

	double scaleW = 1.0;
	double scaleH = 1.0;
	if (snap)
	{
		SpringyHelper::GetRenderScales(*snap, scaleW, scaleH);
	}

	if (!snap)
	{
		return 0;
	}

	if (scaleW == 1.0 && scaleH == 1.0)
	{
		return 0;
	}

	GET_STACK(VoxelCacheStruct*, pSprite, 0x4);

	// 第二个参数是绘制基准点：精灵画到「基准点 + (X, Y)」上。目标是 Temp 这张表面
	// （0x707730 处把它的指针装进 ECX 再调贴图），它的尺寸就是这次贴图的硬边界：
	// 主体、炮塔、炮管画进去的是 256×256 的临时表面，基准点恒为它的中心，放大到超出它
	// 就会被写成环绕到相邻行，所以放大函数要拿到这个矩形才能裁齐
	GET_STACK(int*, pBase, 0x8);

	SpringyVoxelSprite::TargetBox box;
	DSurface* pTarget = DSurface::Temp;
	if (pBase && pTarget)
	{
		box.BaseX = pBase[0];
		box.BaseY = pBase[1];
		box.Width = pTarget->GetWidth();
		box.Height = pTarget->GetHeight();
	}

	VoxelCacheStruct* pScaled = SpringyVoxelSprite::Scale(pSprite, scaleW, scaleH, box);
	if (!pScaled)
	{
		// 精灵格式与预期不符时放弃放大、按原精灵画：画面退化成不缩放，但不能出错
		return 0;
	}

	R->Stack<VoxelCacheStruct*>(0x4, pScaled);


	// 放大后的精灵比原尺寸大，会画出引擎预计算的矩形之外，周围留残影
	SpringyRefresh::AddDirtyOnly(pTechno);

	return 0;
}


// TechnoClass::DrawObject（0x705E00）：ECX = 单位，[esp+4] = 要绘制的 SHP。
// 把 SHP 换成缩放副本；帧号与坐标都不用改，缩放副本的帧已按底边或枢轴排好。
DEFINE_HOOK(0x705E00, TechnoClass_DrawObject_Springy, 0x7)
{
	GET(TechnoClass*, pTechno, ECX);
	GET_STACK(SHPStruct*, pSHP, 0x4);

	if (!pSHP || !pTechno)
	{
		return 0;
	}

	// 这个绘制入口只服务这四类；先判类型再碰扩展状态，避免按 Techno 布局解读别的对象
	const AbstractType what = pTechno->WhatAmI();
	if (what != AbstractType::Unit && what != AbstractType::Aircraft
		&& what != AbstractType::Building && what != AbstractType::Infantry)
	{
		return 0;
	}

	TechnoStatus* status = nullptr;
	if (!TryGetStatus<TechnoExt>(pTechno, status) || !status->MySpringyData.Active)
	{
		return 0;
	}

	double scaleW = 1.0;
	double scaleH = 1.0;
	SpringyHelper::GetRenderScales(status->MySpringyData, scaleW, scaleH);

	if (SHPStruct* pScaled = SpringySHP::GetScaled(pSHP, scaleW, scaleH, 0, 0, false))
	{
		R->Stack<SHPStruct*>(0x4, pScaled);
	}

	// 非建筑每帧都在重绘，但放大后的像素会画出引擎预计算的矩形之外，周围留残影。
	// 建筑不在这里刷，由主循环统一处理
	if (what != AbstractType::Building)
	{
		SpringyRefresh::AddDirtyOnly(pTechno);
	}

	return 0;
}
