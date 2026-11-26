#pragma once

#include <cstdint>

#include <Drawing.h>
#include <IndexClass.h>
#include <TechnoClass.h>

#include <Ext/Common/SpringyHelper.h>


/// @brief 体素缓存未命中那一帧的补画
///
/// @note 引擎的体素绘制（0x706640）只有两条路：
///       - 缓存命中：取出上次建好的精灵，调 0x707480 贴图；
///       - 未命中：调 0x706ED0 把体素**按原尺寸**光栅化到内部表面，再把它合成到
///         绘制表面（`0x707235` 处 `DSurface::Temp.CopyFromPart`），最后才把这张
///         原尺寸的图压成精灵存进缓存——**这一帧没有任何贴图动作**。
///       所以未命中的那一帧画出来的是基准尺寸。换朝向时 key 变了，必然未命中；
///       转向时朝向每帧都在变，于是整个转向过程都在按基准尺寸画，停下之后才恢复。
///
///       修法：未命中时先用同一组参数把原函数整个跑一遍。这一趟里
///       - 合成那一笔被掐掉（`0x707235`，画面不落笔），
///       - 精灵照常构建并写进引擎缓存，
///       然后外层这次调用继续按缓存命中走贴图，于是这一帧画的也是缩放后的精灵。
///       引擎的缓存表本身一个字节都不改，全部走它自己的构建与查询路径。
namespace SpringyVoxelCache
{
	/// @brief DrawVoxel 的十个栈参数（thiscall，`this` 在 ECX，`ret 0x28`）
	/// @note 顺序与 `0x706640` 的取用一致：key 在 `[esp+0xC]`、缓存表在 `[esp+0x10]`。
	///       `Key` 是 `VoxelIndexKey`（int，-1 = 不走缓存），不是 YRpp 里写的 `short Facing`
	struct DrawVoxelArgs
	{
		DWORD Voxel;      // VoxelStruct&
		DWORD Unk2;       // DWORD
		DWORD Key;        // VoxelIndexKey
		DWORD Cache;      // IndexClass<int, VoxelCacheStruct*>&
		DWORD Rect;       // RectangleStruct&
		DWORD Location;   // Point2D&
		DWORD Matrix;     // Matrix3D*（指针参数，不是结构体本身）
		DWORD Intensity;  // int
		DWORD Unk9;       // DWORD
		DWORD Unk10;      // DWORD
	};

	/// @brief 正在跑「预置」的那一趟
	/// @note 用状态位而不是函数参数：预置是通过**重入原函数**完成的，
	///       重入进来的那一次只能原样跑，不能再套一层
	inline bool& Prewarming()
	{
		static bool value = false;
		return value;
	}

	/// @brief 影子的光栅化那一趟要不要掐掉「合成到绘制表面」那一笔
	/// @note 影子与本体不一样：影子在缓存未命中时，引擎把原尺寸影子合成到绘制表面
	///       之后**会自己重入一遍**（`0x706EBF` 调 `0x706BD0`），这一遍命中了、走贴图，
	///       于是同一帧里先出现一张原尺寸的、再叠一张缩放后的。这里只在
	///       「紧随其后的那次光栅化」期间置位，由合成点消费掉，不跨调用残留
	inline bool& SkipShadowComposite()
	{
		static bool value = false;
		return value;
	}

	/// @brief 缓存里是否已经有一张键为 key 的精灵
	/// @param cache 引擎的缓存表
	/// @param key 缓存键
	/// @note 自己线性扫，不调 `IndexClass::IsPresent`：YRpp 那份实现会先 `Sort()`
	///       把引擎的表就地重排。正确性上无所谓，但那是对引擎私有结构的写操作，
	///       本效果器一直坚持不碰它，这里也一样
	inline bool HasSprite(const void* cache, int key)
	{
		const IndexClass<int, int>* pCache = static_cast<const IndexClass<int, int>*>(cache);
		if (!pCache || pCache->IndexCount <= 0 || !pCache->IndexTable)
		{
			return false;
		}

		if (pCache->Archive && pCache->Archive->ID == key)
		{
			return true;
		}

		for (int i = 0; i < pCache->IndexCount; ++i)
		{
			if (pCache->IndexTable[i].ID == key)
			{
				return true;
			}
		}

		return false;
	}

	/// @brief 这一次绘制要不要先补一趟预置
	/// @param pTechno 目标单位
	/// @param args 本次调用的参数
	inline bool Wanted(TechnoClass* pTechno, const DrawVoxelArgs& args)
	{
		if (Prewarming() || !pTechno)
		{
			return false;
		}

		// key 为 -1 = 引擎这一趟根本不碰缓存（DisableVoxelCache），精灵不会构建，
		// 预置也就无从谈起：这种单位只能维持基准尺寸
		if (args.Key == 0xFFFFFFFF || args.Cache == 0)
		{
			return false;
		}

		// 没有挂 Springy 的单位一次扩展都不查，避免给所有体素单位加开销
		if (!SpringyHelper::GetSnapshot(pTechno))
		{
			return false;
		}

		// 命中就不用管：本来就走贴图
		return !HasSprite(reinterpret_cast<const void*>(args.Cache), static_cast<int>(args.Key));
	}

	/// @brief 跑一趟预置：同参数重入原函数，让它在缓存里建好这张精灵
	/// @note 原函数是 thiscall 且 `ret 0x28`，用 `__fastcall` 顶掉前两个参数槽
	///       （ECX = this、EDX 无意义、其余十个参数走栈，与被调方自己清栈的约定一致）
	inline void Prewarm(TechnoClass* pTechno, const DrawVoxelArgs& args)
	{
		using DrawVoxelFn = void(__fastcall*)(void*, void*,
			DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);

		struct Guard
		{
			Guard() { Prewarming() = true; }
			~Guard() { Prewarming() = false; }
		} guard;

#ifdef DEBUG_AE
		Debug::Log("[Springy] VXL 换朝向，先补一拍缓存：key=0x%08X\n",
			static_cast<unsigned>(args.Key));
#endif // DEBUG_AE

		reinterpret_cast<DrawVoxelFn>(0x706640)(pTechno, nullptr,
			args.Voxel, args.Unk2, args.Key, args.Cache, args.Rect, args.Location,
			args.Matrix, args.Intensity, args.Unk9, args.Unk10);
	}
}
