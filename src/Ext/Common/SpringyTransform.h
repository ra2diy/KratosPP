#pragma once

#include <cmath>
#include <algorithm>

/// @brief 二维绘制变换：缩放 + 剪切（矩形 → 平行四边形）。
///
/// 矩阵按 **K·S** 复合（先缩放、后剪切）：
/// ```
/// [ A  B ]   [ sx   -kx*sy ]
/// [ C  D ] = [ ky*sx   sy   ]
/// ```
/// 约定（屏幕坐标，y 向下）：
///   - `kx > 0` = 顶部相对底部**右移**（水平推）；
///   - `ky > 0` = 右侧相对左侧**下移**（垂直推）；
///   - 枢轴是绘制基准线（VXL 的贴图基准点 / SHP 的底边中点），所以「脚不动、越高偏得越多」。
///
/// 为什么是先缩放后剪切：这样 `kx` 表达的是斜率（≈角度），不随缩放变化，调参直观。
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

	/// @brief 由缩放与剪切系数构造
	/// @param scaleX 横向倍率
	/// @param scaleY 纵向倍率
	/// @param shearX 水平推：每 1 个（缩放后）纵向像素位移多少横向像素；正 = 顶部右移
	/// @param shearY 垂直推：每 1 个（缩放后）横向像素位移多少纵向像素；正 = 右侧下移
	void Set(double scaleX, double scaleY, double shearX, double shearY)
	{
		A = scaleX;
		B = -shearX * scaleY;
		C = shearY * scaleX;
		D = scaleY;
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

	/// @brief 压成一个整数键，用于「这一帧的画面是否变了」的相等比较
	/// @note 1/256 量化、每项 16 位带偏移，取舍只在相等判断上：键相同就认为画面相同
	long long Key() const
	{
		return (Quantize(A) << 48) ^ (Quantize(B) << 32) ^ (Quantize(C) << 16) ^ Quantize(D);
	}

private:
	static long long Quantize(double value)
	{
		long long q = static_cast<long long>(std::lround(value * 256.0)) + 0x8000;
		return std::clamp<long long>(q, 0, 0xFFFF);
	}
};
