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

	/// @brief 忘掉体素剪切枢轴的记忆
	/// @note 枢轴是「所有部件里最低的那条边」，跨帧保持。效果撤除或某一帧不生效时必须
	///       忘掉，否则下次挂上会沿用旧形状（比如刚变身过）留下的地面线
	inline void ResetVoxelPivot(TechnoStatus* status)
	{
		if (status)
		{
			status->SpringyShearFrameValid = false;
			status->SpringyShearTop = 0.0;
			status->SpringyShearBottom = 0.0;
			status->SpringyShearLeft = 0.0;
			status->SpringyShearRight = 0.0;
		}
	}

	/// @brief 把快照换算成位图的缩放倍率
	/// @note 横向取 X 与 Y 的平均：两者在屏幕上都落成水平投影，这样 ScaleX=2/ScaleY=1
	///       与反过来效果相同。纵向取 Z
	inline void GetRenderScales(const SpringySnapshot& snap, double& scaleW, double& scaleH)
	{
		scaleW = (snap.ScaleX + snap.ScaleY) * 0.5;
		scaleH = snap.ScaleZ;
	}

	/// @brief 这一帧有没有需要重采样的变形（缩放或剪切）
	inline bool HasDeformation(const SpringySnapshot& snap)
	{
		if (!snap.Active)
		{
			return false;
		}
		if (snap.ShearX != 0.0 || snap.ShearY != 0.0)
		{
			return true;
		}
		return snap.ScaleX != 1.0 || snap.ScaleY != 1.0 || snap.ScaleZ != 1.0;
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
		double scaleW = 1.0;
		double scaleH = 1.0;
		GetRenderScales(snap, scaleW, scaleH);
		// 倍率按 1/128 量化；剪切是像素，按 1/4 像素量化（±8192 像素以内不撞键）
		const auto qScale = [](double v) -> long long {
			return std::clamp<long long>(static_cast<long long>(std::lround(v * 128.0)) + 0x8000, 0, 0xFFFF);
		};
		const auto qShear = [](double v) -> long long {
			return std::clamp<long long>(static_cast<long long>(std::lround(v * 4.0)) + 0x8000, 0, 0xFFFF);
		};
		return (qScale(scaleW) << 48) ^ (qScale(scaleH) << 32) ^ (qShear(snap.ShearX) << 16) ^ qShear(snap.ShearY);
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

	/// @brief 脏矩形按形变后的范围外扩，保证残影被擦掉
	/// @param rect 引擎报上来的渲染矩形
	/// @param scaleW,scaleH 本帧（或上一次绘制时）的位图缩放
	/// @param shearX,shearY 本帧的剪切像素位移
	/// @note 缩放横向绕中线、纵向绕底边（与两条绘制链路的枢轴一致）；
	///       剪切按方向补：顶边往右推就补右边、往左推就补左边，不能四周等量补，
	///       否则推出去的那一边会留下一圈像素
	inline RectangleStruct InflateDirtyRect(const RectangleStruct& rect,
		double scaleW, double scaleH, double shearX, double shearY)
	{
		const int w = std::max(rect.Width, 1);
		const int h = std::max(rect.Height, 1);

		const double overW = std::max(0.0, (w * std::max(scaleW, 1.0) - w) * 0.5);
		const double overH = std::max(0.0, h * std::max(scaleH, 1.0) - h);

		// 定值余量：像素取整、等轴投影下的泄漏、引擎自身的脏区粒度
		const int pad = 8;
		const int left = static_cast<int>(std::ceil(overW + std::max(0.0, -shearX))) + pad;
		const int right = static_cast<int>(std::ceil(overW + std::max(0.0, shearX))) + pad;
		const int top = static_cast<int>(std::ceil(overH + std::max(0.0, -shearY))) + pad;
		const int bottom = static_cast<int>(std::ceil(std::max(0.0, shearY))) + pad;

		return RectangleStruct{
			rect.X - left,
			rect.Y - top,
			w + left + right,
			h + top + bottom
		};
	}
}
