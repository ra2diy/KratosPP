#pragma once

#include <cmath>
#include <algorithm>

/// @brief 剪切参数的单位
/// @note PX = 像素：写 300 就是顶边整条右移 300 像素（与图片大小无关）；
///       PERCENT = 百分比：写 100 就是顶边右移到「整单位高度」那么远（约 45°，同样与图片大小无关）
enum class SpringyShearMode : int
{
	PX = 0,
	PERCENT = 1
};


/// @brief 二维绘制变换：缩放 + 剪切（矩形 → 平行四边形）。
///
/// 剪切的**参数是像素**：`shearX` = 顶边相对底边的水平位移（缩放后的像素），
/// `shearY` = 右边相对左边的垂直位移。变换到矩阵里时要除以**源精灵的尺寸**：
/// ```
/// [ A  B ]   [ sx      -shearX / h ]
/// [ C  D ] = [ shearY / w    sy     ]
/// ```
/// 这样任意一行相对底边的位移就是 `shearX × (这一行离底部的行数 / 总行数)`：
/// 底行不动、顶行正好位移 `shearX` 像素，中间按行数线性递增 —— 就是「矩形推成平行四边形」。
/// 除的是源尺寸而不是最终尺寸，所以缩放不会把这个像素数放大：写 300 就是 300 像素。
///
/// 约定（屏幕坐标，y 向下）：
///   - `shearX > 0` = 顶边右移；`shearY > 0` = 右边下移；
///   - 枢轴是底边（VXL 取该单位所有部件里最低的那条边，SHP 取帧底边中点）。
///
/// 三条绘制链路（VXL 贴图点、SHP 重采样、依附动画）都走这一个结构，
/// 保证主体 / 炮塔 / 炮管 / 影子 / 动画的形变完全一致。
struct SpringyTransform
{
	double A = 1.0; // x' = A*x + B*y
	double B = 0.0;
	double C = 0.0; // y' = C*x + D*y
	double D = 1.0;

	void Reset()
	{
		A = 1.0;
		B = 0.0;
		C = 0.0;
		D = 1.0;
	}

	bool IsIdentity() const
	{
		return A == 1.0 && B == 0.0 && C == 0.0 && D == 1.0;
	}

	/// @brief 由缩放与**像素**位移构造（一张图自身就是整体的场合：SHP / 动画）
	/// @param scaleX 横向倍率
	/// @param scaleY 纵向倍率
	/// @param shearX 顶边相对底边的水平位移（像素，正 = 右移）
	/// @param shearY 右边相对左边的垂直位移（像素，正 = 下移）
	/// @param w,h 源精灵的尺寸（像素）。为 0 或 1 时该方向的剪切退化为 0
	void SetFromPixels(double scaleX, double scaleY, double shearX, double shearY,
		double w, double h)
	{
		SetFromSpan(scaleX, scaleY, shearX, shearY, h - 1.0, w - 1.0);
	}

	/// @brief 用**外部给定的跨度**构造
	/// @param spanRows 纵向跨度（顶行中心到底行中心的距离），X 剪切的除数
	/// @param spanCols 横向跨度（最左列中心到最右列中心的距离），Y 剪切的除数
	/// @note 体素单位的主体、炮塔、炮管是三次独立贴图，各自的包围盒大小差很多。
	///       若各自拿自己的高度当除数，同样的 shearX 会让炮塔、炮管斜得比车体陡，
	///       表现为三个部件各偏各的。所以它们必须共用「整个单位」的跨度
	void SetFromSpan(double scaleX, double scaleY, double shearX, double shearY,
		double spanRows, double spanCols)
	{
		A = scaleX;
		D = scaleY;
		// 除以跨度：顶行正好位移 shearX、底行位移恒为 0，中间按行数线性过渡
		B = spanRows > 0.5 ? -shearX / spanRows : 0.0;
		C = spanCols > 0.5 ? shearY / spanCols : 0.0;
	}

	/// @brief 复合：this = outer · this，即「先做自己，再做 outer」
	/// @note 多条段按附着顺序依次复合，最后挂上的那条在最外层
	void Compose(const SpringyTransform& outer)
	{
		const double a = A;
		const double b = B;
		const double c = C;
		const double d = D;
		A = outer.A * a + outer.B * c;
		B = outer.A * b + outer.B * d;
		C = outer.C * a + outer.D * c;
		D = outer.C * b + outer.D * d;
	}

	void Map(double x, double y, double& ox, double& oy) const
	{
		ox = A * x + B * y;
		oy = C * x + D * y;
	}

	/// @brief 求逆矩阵
	/// @param ia,ib,ic,id 输出的四个系数
	/// @return 矩阵可逆；行列式接近 0（极端剪切）时返回 false，调用方按原样绘制
	bool Invert(double& ia, double& ib, double& ic, double& id) const
	{
		const double det = A * D - B * C;
		if (std::abs(det) < 1e-6)
		{
			return false;
		}
		const double inv = 1.0 / det;
		ia = D * inv;
		ib = -B * inv;
		ic = -C * inv;
		id = A * inv;
		return true;
	}

	/// @brief 一个矩形变换后的**外接矩形**（平行四边形的 AABB）
	/// @param x,y 矩形左上角（相对枢轴）
	/// @param w,h 矩形尺寸
	/// @note 输出同样是相对枢轴；剪切后四个角会落到这个包围盒之外，那些区域是透明的
	void MapRect(double x, double y, double w, double h,
		double& ox, double& oy, double& ow, double& oh) const
	{
		double xs[4];
		double ys[4];
		Map(x, y, xs[0], ys[0]);
		Map(x + w, y, xs[1], ys[1]);
		Map(x, y + h, xs[2], ys[2]);
		Map(x + w, y + h, xs[3], ys[3]);

		const double minX = std::min({ xs[0], xs[1], xs[2], xs[3] });
		const double maxX = std::max({ xs[0], xs[1], xs[2], xs[3] });
		const double minY = std::min({ ys[0], ys[1], ys[2], ys[3] });
		const double maxY = std::max({ ys[0], ys[1], ys[2], ys[3] });

		ox = minX;
		oy = minY;
		ow = maxX - minX;
		oh = maxY - minY;
	}
};
