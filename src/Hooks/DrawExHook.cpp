// 绘制相关的 hook 全在这里：
//   - 动画（AnimClass）：绘制入口置上下文 + 四个 blit 点换 SHP（Springy 动画、建筑覆盖动画）
//   - 体素（VXL）：绘制入口的预置补画、精灵贴图点换放大位图、SHP 主体替换、影子原尺寸合成抑制
//
// 这些落点以前散在 SpringyAnimHook.cpp / SpringyExtHook.cpp 里，按评审意见合并成一个
// 「绘制管线」文件：绘制侧新增的 hook 一律进这里，不再按效果器/类各开一个文件。
// 注意：一个地址只能挂一条 hook，合并时不要出现同址两条。

#include <exception>
#include <cstdint>

#include <GeneralStructures.h>
#include <SpecificStructures.h>
#include <ObjectClass.h>
#include <FootClass.h>
#include <TechnoClass.h>
#include <AnimClass.h>
#include <Ext/AnimType/SpringyAnim.h>
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
#include <Ext/Common/SpringyVoxelCache.h>
#include <Ext/Common/SpringyVoxelSprite.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/TechnoType/TechnoStatus.h>


// ==================== 动画（AnimClass）====================

// AnimClass::DrawIt 入口。ECX = 动画本体。
// 这里只做「判定归属 + 置绘制上下文」，不换 SHP；换 SHP 在下面四个 blit 点。
DEFINE_HOOK(0x422CA0, AnimClass_Draw_SpringyBegin, 0x5)
{
	GET(AnimClass*, pAnim, ECX);
	// ---- 临时探针：谁在画这个动画（R->Stack<DWORD>(0) = DrawIt 的返回地址）----
	// 每 60 帧记最多 12 行，用来确认建筑动画是走层链表还是被建筑绘制时一起贴出来
	{
		static int lastProbeFrame = -100000;
		static int probeBudget = 0;
		const int probeFrame = Unsorted::CurrentFrame.get();
		if (probeFrame - lastProbeFrame >= 60)
		{
			lastProbeFrame = probeFrame;
			probeBudget = 12;
		}
		if (probeBudget > 0)
		{
			--probeBudget;
			Debug::Log("[probe] anim=%p type=%s owner=%p isBldAnim=%d caller=%p\\n",
				pAnim,
				pAnim->Type ? pAnim->Type->ID : "?",
				pAnim->OwnerObject,
				pAnim->IsBuildingAnim ? 1 : 0,
				reinterpret_cast<void*>(R->Stack<DWORD>(0)));
		}
	}
	SpringyAnim::BeginDraw(pAnim);
	return 0;
}


// AnimClass::DrawIt 内部对共享 blitter(0x4AED70) 的四个调用点。
// 故意不挂 blitter 本身：那是所有 shape 绘制都会走的公共路径，
// 挂上去会波及与动画无关的绘制。
// blitter 的 shape 参数是最后 push 的，即 call 指令处 [esp+0]。
// REF_STACK 依赖 hook 体内的 R，不能抽成独立函数，所以直接写在 hook 体里。
DEFINE_HOOK_AGAIN(0x4233E9, AnimClass_Draw_BlitSHP_Springy, 0x5)
DEFINE_HOOK_AGAIN(0x4236F7, AnimClass_Draw_BlitSHP_Springy, 0x5)
DEFINE_HOOK_AGAIN(0x423827, AnimClass_Draw_BlitSHP_Springy, 0x5)
DEFINE_HOOK(0x42389E, AnimClass_Draw_BlitSHP_Springy, 0x5)
{
	REF_STACK(SHPStruct*, pSHP, 0x0);
	// 太小的值不是指针（原版会传一些常量标记），直接放过
	if (reinterpret_cast<uintptr_t>(pSHP) >= 0x10000)
	{
		if (SHPStruct* pScaled = SpringyAnim::SubstituteSHP(pSHP))
		{
			pSHP = pScaled;
		}
	}
	return 0;
}


// ==================== 体素（VXL）/ SHP ====================





// 所有 Techno 类别体素绘制的入口。这里做两件事：
//   1. 建筑炮塔染色；
//   2. 缓存未命中时先补一趟预置（见下）。
// 体素放大本身不在这里，而在下面的贴图点
DEFINE_HOOK(0x706640, TechnoClass_DrawVX, 0x5)
{
	// 预置的那一趟就是从这个地址重入进来的：原样跑，别再套一层，
	// 也不重复做炮塔染色
	if (SpringyVoxelCache::Prewarming())
	{
		return 0;
	}

	GET(TechnoClass*, pTechno, ECX);
	TechnoStatus* status = nullptr;
	if (TryGetStatus<TechnoExt>(pTechno, status) && status->IsBuilding())
	{
		status->DrawVXL_Paintball(R, true);
	}

	// 引擎只有「缓存命中」那条路会调 0x707480 贴图；未命中的那一帧是把体素
	// 按原尺寸光栅化后直接合成到绘制表面，精灵只是顺手存进缓存、不当帧贴出。
	// 换朝向必然未命中，而转向时朝向每帧都在变，于是整个转向过程画的都是基准
	// 尺寸，停下之后才恢复——这就是「转向时跳回原大小」的来源。
	//
	// 这里在未命中时先用同一组参数把原函数整个跑一遍：这一趟里合成那一笔被
	// 掐掉（0x707235，画面不落笔），精灵照常建进缓存；外层这次调用随后按缓存
	// 命中走贴图，于是未命中的这一帧画的也是缩放后的精灵
	SpringyVoxelCache::DrawVoxelArgs args;
	args.Voxel = R->Stack<DWORD>(0x4);
	args.Unk2 = R->Stack<DWORD>(0x8);
	args.Key = R->Stack<DWORD>(0xC);
	args.Cache = R->Stack<DWORD>(0x10);
	args.Rect = R->Stack<DWORD>(0x14);
	args.Location = R->Stack<DWORD>(0x18);
	args.Matrix = R->Stack<DWORD>(0x1C);
	args.Intensity = R->Stack<DWORD>(0x20);
	args.Unk9 = R->Stack<DWORD>(0x24);
	args.Unk10 = R->Stack<DWORD>(0x28);

	if (SpringyVoxelCache::Wanted(pTechno, args))
	{
		SpringyVoxelCache::Prewarm(pTechno, args);
	}

	return 0;
}


// 0x706ED0 收尾处，把光栅化结果（VoxelSurface）合成到绘制表面（DSurface::Temp）
// 的那一条 call。预置那一趟不能让它落笔：否则屏幕上先出现一张基准尺寸的图，
// 后面贴上去的放大版只能盖住一部分，边缘留一圈原大小的残影
DEFINE_HOOK(0x707235, TechnoClass_DrawVoxel_PrewarmSkipComposite, 0x5)
{
	if (!SpringyVoxelCache::Prewarming())
	{
		return 0;
	}

	// 被跳过的是 `call 0x4AF2A0`，那条函数固定 `ret 0x2C`（11 个 dword 参数）。
	// 跳过它就得自己把这 11 个 dword 从栈上抹掉，再落到调用之后那条指令
	R->ESP(R->ESP() + 0x2C);
	return 0x70723A;
}


// 影子的缓存未命中路径与本体不一样：它把原尺寸影子合成到绘制表面（0x707432）之后，
// 还会建精灵、存缓存，然后**自己重入一遍**（0x706EBF 调 0x706BD0），那一遍命中、
// 走 0x707480 贴图。也就是说影子这一帧本来就会画出缩放后的那张，多出来的只有先落的
// 那一笔原尺寸——放大后它会在缩放版周围留一圈基准大小的残影。
//
// 所以影子不需要本体的「预置」：只要把那一笔掐掉就行。标记在光栅化调用之前打，
// 由合成点消费，不跨调用残留
DEFINE_HOOK(0x706D27, TechnoClass_DrawVoxelShadow_SpringySkipRaw, 0x5)
{
	GET(int, shadowKey, EBX);
	GET(TechnoClass*, pTechno, ESI);

	// key == -1（DisableVoxelCache / 类型上关了缓存）时引擎不建精灵、也不会重入，
	// 那一笔是这张影子唯一的绘制，不能掐
	SpringyVoxelCache::SkipShadowComposite() =
		shadowKey != -1 && SpringyHelper::GetSnapshot(pTechno) != nullptr;

	return 0;
}


// 0x707280 收尾处把光栅化结果合成到绘制表面的那一条 call（与本体同一条函数）
DEFINE_HOOK(0x707432, TechnoClass_DrawVoxelShadow_SpringySkipComposite, 0x5)
{
	if (!SpringyVoxelCache::SkipShadowComposite())
	{
		return 0;
	}

	// 只消费一次：紧跟着的重入那一趟走的是贴图，不会再到 0x707280
	SpringyVoxelCache::SkipShadowComposite() = false;

	// 同 0x707235：`call 0x4AF2A0` 固定 `ret 0x2C`，跳过它就把 11 个 dword 参数抹掉
	R->ESP(R->ESP() + 0x2C);
	return 0x707437;
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

	TechnoStatus* status = nullptr;
	if (!TryGetStatus<TechnoExt>(pTechno, status) || !status->MySpringyData.Active)
	{
		SpringyHelper::ResetVoxelPivot(status);
		return 0;
	}

	// 本帧的画布形变：缩放 + 剪切复合后的一张 2×2 矩阵。
	// 恒等就说明这一帧没变形（倍率恰为 1、剪切为 0），直接按原精灵画
	if (!SpringyHelper::HasDeformation(status->MySpringyData))
	{
		return 0;
	}

	// 剪切单位是像素：写 300 就是顶边相对底边位移 300 像素
	double scaleW = 1.0;
	double scaleH = 1.0;
	SpringyHelper::GetRenderScales(status->MySpringyData, scaleW, scaleH);

	GET_STACK(VoxelCacheStruct*, pSprite, 0x4);
	if (!pSprite)
	{
		return 0;
	}

	// 剪切的共享参照框：这个单位所有部件的最低/最高/最左/最右。
	// 主体、炮塔、炮管是三次独立贴图，包围盒大小差很多 —— 斜率必须按整个单位的跨度算，
	// 各部件自己算的话，同样的剪切值会让炮塔、炮管斜得比车体陡（表现就是各偏各的）。
	// 跨帧保持：某帧最先画的部件也能用到完整参照框
	const double partLeft = static_cast<double>(pSprite->X);
	const double partTop = static_cast<double>(pSprite->Y);
	const double partRight = partLeft + static_cast<double>(pSprite->Width);
	const double partBottom = partTop + static_cast<double>(pSprite->Height);
	if (!status->SpringyShearFrameValid)
	{
		status->SpringyShearLeft = partLeft;
		status->SpringyShearTop = partTop;
		status->SpringyShearRight = partRight;
		status->SpringyShearBottom = partBottom;
		status->SpringyShearFrameValid = true;
	}
	else
	{
		status->SpringyShearLeft = std::min(status->SpringyShearLeft, partLeft);
		status->SpringyShearTop = std::min(status->SpringyShearTop, partTop);
		status->SpringyShearRight = std::max(status->SpringyShearRight, partRight);
		status->SpringyShearBottom = std::max(status->SpringyShearBottom, partBottom);
	}

	// 底行像素中心 = 枢轴；跨度取「顶行中心到底行中心」/「最左列中心到最右列中心」
	const double pivotY = status->SpringyShearBottom - 0.5;
	const double spanRows = (status->SpringyShearBottom - 0.5) - (status->SpringyShearTop + 0.5);
	const double spanCols = (status->SpringyShearRight - 0.5) - (status->SpringyShearLeft + 0.5);

	// 第二个参数是绘制基准点：精灵画到「基准点 + (X, Y)」上。目标是 Temp 这张表面
	// （0x707730 处把它的指针装进 ECX 再调贴图），它的尺寸就是这次贴图的硬边界：
	// 主体、炮塔、炮管画进去的是 256×256 的临时表面，基准点恒为它的中心，变形到超出它
	// 就会被写成环绕到相邻行，所以变形函数要拿到这个矩形才能裁齐
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

	VoxelCacheStruct* pScaled = SpringyVoxelSprite::Transform(pSprite, scaleW, scaleH,
		status->MySpringyData.ShearX, status->MySpringyData.ShearY, status->MySpringyData.ShearMode,
		spanRows, spanCols, pivotY, box);
	if (!pScaled)
	{
		// 精灵格式与预期不符时放弃变形、按原精灵画：画面退化成没有效果，但不能出错
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

	if (!SpringyHelper::HasDeformation(status->MySpringyData))
	{
		return 0;
	}

	// 剪切单位是像素：写 300 就是顶边相对底边位移 300 像素
	double scaleW = 1.0;
	double scaleH = 1.0;
	SpringyHelper::GetRenderScales(status->MySpringyData, scaleW, scaleH);

	// 画布按形变后包围盒动态变化：锁死原帧矩形会让推出去的部分被切掉
	const bool inPlace = false;
	// ---- 临时探针：建筑本体绘制 ----
	{
		static int lastProbeFrame = -100000;
		static int probeBudget = 0;
		const int probeFrame = Unsorted::CurrentFrame.get();
		if (probeFrame - lastProbeFrame >= 60)
		{
			lastProbeFrame = probeFrame;
			probeBudget = 12;
		}
		if (probeBudget > 0 && what == AbstractType::Building)
		{
			--probeBudget;
			Debug::Log("[probe] body bld=%p type=%s caller=%p\\n",
				pTechno,
				pTechno->GetTechnoType() ? pTechno->GetTechnoType()->ID : "?",
				reinterpret_cast<void*>(R->Stack<DWORD>(0)));
		}
	}
	if (SHPStruct* pScaled = SpringySHP::GetTransformed(pSHP, scaleW, scaleH,
		status->MySpringyData.ShearX, status->MySpringyData.ShearY, status->MySpringyData.ShearMode, 0, 0, false, inPlace))
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
