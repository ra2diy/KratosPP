#pragma once

#include <cmath>
#include <algorithm>

#include <GeneralStructures.h>
#include <TechnoClass.h>

#include <Extension/TechnoExt.h>
#include <Utilities/Debug.h>

#include <Ext/Helper/Scripts.h>
#include <Ext/StateType/State/SpringyData.h>
#include <Ext/TechnoType/TechnoStatus.h>


/// @brief Springy 各渲染落点共用的取值工具。
/// @note 波形公式照搬参考实现（A-small-C4/springy）：Steps=8 的固定正弦表，
///       以及「正半周取宽峰+矮谷、负半周取窄谷+高峰」的分半周规则。
///       这只是波形的采样形状，不是绘制尺寸：绘制倍率始终连续。
namespace SpringyHelper
{
	/// @brief 取一个单位的缩放快照
	inline const SpringySnapshot* GetSnapshot(TechnoClass* pTechno)
	{
		if (!pTechno)
		{
			return nullptr;
		}
		TechnoStatus* status = nullptr;
		if (!TryGetStatus<TechnoExt>(pTechno, status))
		{
			return nullptr;
		}
		return status->MySpringyData.Active ? &status->MySpringyData : nullptr;
	}

	/// @brief 按快照缩放一个 FLH 偏移
	/// @note FLH 与绘制矩阵的轴一一对应：F→X、L→Y、H→Z。
	///       依据是 GetFLHOffset 里直接 ToVector3D(flh)，按分量原样映射不做交换
	inline CoordStruct ScaleFLH(const CoordStruct& flh, const SpringySnapshot& snap)
	{
		return CoordStruct{
			static_cast<int>(std::lround(flh.X * snap.ScaleX)),
			static_cast<int>(std::lround(flh.Y * snap.ScaleY)),
			static_cast<int>(std::lround(flh.Z * snap.ScaleZ))
		};
	}

	/// @brief 把快照换算成 SHP 位图的放大倍率
	/// @note SHP 是二维位图，只有水平与垂直两个方向，而倍率有三个。横向取 X 与 Y 的平均：
	///       两者在屏幕上都落成水平投影，这样 ScaleX=2/ScaleY=1 与反过来效果相同。
	///       纵向取 Z
	inline void GetRenderScales(const SpringySnapshot& snap, double& scaleW, double& scaleH)
	{
		scaleW = (snap.ScaleX + snap.ScaleY) * 0.5;
		scaleH = snap.ScaleZ;
	}

	/// @brief 把快照压成一个可比较的整数键，用于判断「这一帧的画面是否变了」
	/// @note 只做相等比较，不参与任何计算
	inline long long RenderKey(const SpringySnapshot& snap)
	{
		if (!snap.Active)
		{
			return 0;
		}
		const long long x = static_cast<long long>(std::lround(snap.ScaleX * 1000.0));
		const long long y = static_cast<long long>(std::lround(snap.ScaleY * 1000.0));
		const long long z = static_cast<long long>(std::lround(snap.ScaleZ * 1000.0));
		// 三个千分位整数各占 21 位，互不重叠；倍率上限 10 倍 = 10000，远小于 2^21
		return (x << 42) ^ (y << 21) ^ z;
	}

	/// @brief 脏矩形按最坏幅度外扩，保证残影被擦掉
	/// @param rect 引擎报上来的渲染矩形
	/// @param widenPercent 横向最多超出原宽度的百分比（本效果按倍率算，可为 100 以上）
	/// @param stretchPercent 纵向最多超出原高度的百分比
	/// @note 公式照搬参考实现：横向每侧 = 宽度 × 变宽幅度的一半 + 8，
	///       纵向向上 = 高度 × 增高幅度 + 横向扩量（等轴投影下的泄漏）+ 8。
	///       只有下界钳位：参考实现的 90 上限是给「振幅参数」用的，
	///       这里传进来的是真实超出量，放大 3 倍就是 200%，截到 90 会留残影
	inline RectangleStruct InflateDirtyRect(const RectangleStruct& rect,
		int widenPercent, int stretchPercent)
	{
		const int w = std::max(rect.Width, 1);
		const int h = std::max(rect.Height, 1);
		const int widen = std::max(widenPercent, 0);
		const int stretch = std::max(stretchPercent, 0);

		const int mx = w * widen / 200 + 8;
		const int myTop = h * stretch / 100 + mx + 8;
		const int myBottom = mx;

		return RectangleStruct{
			rect.X - mx,
			rect.Y - myTop,
			w + mx * 2,
			h + myTop + myBottom
		};
	}
}

