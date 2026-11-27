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

	/// @brief 取本帧的画布形变（缩放 + 剪切复合后的矩阵）
	/// @note 三条绘制链路（VXL 贴图点 / SHP 重采样 / 依附动画）都读它，
	///       所以主体、炮塔、炮管、影子、动画的形变必然一致
	inline const SpringyTransform& GetTransform(const SpringySnapshot& snap)
	{
		return snap.Matrix;
	}

	/// @brief 把快照压成一个可比较的整数键，用于判断「这一帧的画面是否变了」
	/// @note 只做相等比较，不参与任何计算。键里必须含剪切：
	///       纯剪切（倍率都是 1）时如果只比倍率，脏区判定会认为画面没变、不重绘
	inline long long RenderKey(const SpringySnapshot& snap)
	{
		if (!snap.Active)
		{
			return 0;
		}
		return snap.Matrix.Key();
	}

	/// @brief 两个矩形取并集
	inline RectangleStruct UnionRect(const RectangleStruct& a, const RectangleStruct& b)
	{
		const int x0 = std::min(a.X, b.X);
		const int y0 = std::min(a.Y, b.Y);
		const int x1 = std::max(a.X + a.Width, b.X + b.Width);
		const int y1 = std::max(a.Y + a.Height, b.Y + b.Height);
		return RectangleStruct{ x0, y0, x1 - x0, y1 - y0 };
	}

	/// @brief 脏矩形按形变后的包围盒外扩，保证残影被擦掉
	/// @param rect 引擎报上来的渲染矩形
	/// @param tf 本帧（或上一次绘制时）的画布形变
	/// @note 形变后的图形是平行四边形，取它的外接矩形；四个方向各自补齐 ——
	///       剪切会把图形整体推到一侧（例如正 ShearX 时顶部右移、右边多出一块），
	///       只按「四周等量外扩」补会漏掉推出去的那一边
	inline RectangleStruct InflateDirtyRect(const RectangleStruct& rect, const SpringyTransform& tf)
	{
		const int w = std::max(rect.Width, 1);
		const int h = std::max(rect.Height, 1);

		double ox = 0.0;
		double oy = 0.0;
		double ow = 0.0;
		double oh = 0.0;
		tf.MapRect(0.0, 0.0, static_cast<double>(w), static_cast<double>(h), ox, oy, ow, oh);

		const int left = std::max(0, static_cast<int>(std::floor(-ox)));
		const int top = std::max(0, static_cast<int>(std::floor(-oy)));
		const int right = std::max(0, static_cast<int>(std::ceil(ox + ow - static_cast<double>(w))));
		const int bottom = std::max(0, static_cast<int>(std::ceil(oy + oh - static_cast<double>(h))));

		// 定值余量：像素取整、等轴投影下的泄漏、引擎自身的脏区粒度
		const int pad = 8;

		return RectangleStruct{
			rect.X - left - pad,
			rect.Y - top - pad,
			w + left + right + pad * 2,
			h + top + bottom + pad * 2
		};
	}
}

