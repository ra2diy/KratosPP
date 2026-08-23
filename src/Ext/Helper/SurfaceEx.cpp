#include "SurfaceEx.h"

#include "MathEx.h"


bool InRect(Point2D point, RectangleStruct bound)
{
	return point.X >= bound.X && point.X <= bound.X + bound.Width && point.Y >= bound.Y && point.Y <= bound.Y + bound.Height;
}

bool InFog(CoordStruct location)
{
	if (CellClass* pCell = MapClass::Instance->TryGetCellAt(location))
	{
		if (pCell->Flags & CellFlags::Revealed)
		{
			return false;
		}
	}
	return true;
}

/**
 * @brief 处理四角越界并绘制虚线
 *
 * @param pSurface 面板
 * @param point1 起始点
 * @param point2 终止点
 * @param dwColor 颜色
 * @param bound 区域
 * @param blink 闪烁
 * @return true 成功绘制
 * @return false 绘制失败
 */
bool DrawDashedLine(DSurface* pSurface,
	Point2D point1, Point2D point2, int dwColor, RectangleStruct bound,
	bool blink)
{
	if (bound.IsEmpty())
	{
		bound = pSurface->GetRect();
	}
	Surface::ClipLine(point1, point2, bound);
	// point in rect then draw
	if (InRect(point1, bound) && InRect(point2, bound))
	{
		int offset = 0;
		if (blink)
		{
			offset = 7 * Unsorted::CurrentFrame % 16;
		}
		return pSurface->DrawDashedLine(&point1, &point2, dwColor, offset);
	}
	return false;
}

/**
 * @brief 处理四角越界并绘制虚线
 *
 * @param pSurface 面板
 * @param point1 起始点
 * @param point2 终止点
 * @param color 颜色
 * @param bound 区域
 * @param blink 闪烁
 * @return true 成功绘制
 * @return false 绘制失败
 */
bool DrawDashedLine(DSurface* pSurface,
	Point2D point1, Point2D point2, ColorStruct color, RectangleStruct bound,
	bool blink)
{
	return DrawDashedLine(pSurface, point1, point2, Drawing::RGB_To_Int(color), bound, blink);
}

/**
 * @brief 处理四角越界并绘制线条
 *
 * @param pSurface 面板
 * @param point1 起始点
 * @param point2 终止点
 * @param dwColor 颜色
 * @param bound 区域
 * @return true 成功绘制
 * @return false 绘制失败
 */
bool DrawLine(DSurface* pSurface,
	Point2D point1, Point2D point2, int dwColor, RectangleStruct bound)
{
	if (bound.IsEmpty())
	{
		bound = pSurface->GetRect();
	}
	Surface::ClipLine(point1, point2, bound);
	// point in rect then draw
	if (InRect(point1, bound) && InRect(point2, bound))
	{
		return pSurface->DrawLine(&point1, &point2, dwColor);
	}
	return false;
}

/**
 * @brief 处理四角越界并绘制线条
 *
 * @param pSurface 面板
 * @param point1 起始点
 * @param point2 终止点
 * @param color 颜色
 * @param bound 区域
 * @return true 成功绘制
 * @return false 绘制失败
 */
bool DrawLine(DSurface* pSurface,
	Point2D point1, Point2D point2, ColorStruct color, RectangleStruct bound)
{
	return DrawLine(pSurface, point1, point2, Drawing::RGB_To_Int(color), bound);
}

void DrawCrosshair(DSurface* pSurface,
	CoordStruct sourcePos, int length, ColorStruct color, RectangleStruct bounds,
	bool dashed, bool blink)
{
	CoordStruct p1 = sourcePos + CoordStruct{ length, 0, 0 };
	CoordStruct p2 = sourcePos + CoordStruct{ -length, 0, 0 };
	CoordStruct p3 = sourcePos + CoordStruct{ 0, length, 0 };
	CoordStruct p4 = sourcePos + CoordStruct{ 0, -length, 0 };
	if (dashed)
	{
		DrawDashedLine(pSurface, ToClientPos(p1), ToClientPos(p2), color, bounds, blink);
		DrawDashedLine(pSurface, ToClientPos(p3), ToClientPos(p4), color, bounds, blink);
	}
	else
	{
		DrawLine(pSurface, ToClientPos(p1), ToClientPos(p2), color, bounds);
		DrawLine(pSurface, ToClientPos(p3), ToClientPos(p4), color, bounds);
	}
}

void DrawCell(DSurface* pSurface,
	CoordStruct sourcePos, ColorStruct color, RectangleStruct bounds,
	bool dashed, bool blink, int length)
{
	CoordStruct p1 = sourcePos + CoordStruct{ length, length, 0 };
	CoordStruct p2 = sourcePos + CoordStruct{ -length, length, 0 };
	CoordStruct p3 = sourcePos + CoordStruct{ -length, -length, 0 };
	CoordStruct p4 = sourcePos + CoordStruct{ length, -length, 0 };
	if (dashed)
	{
		DrawDashedLine(pSurface, ToClientPos(p1), ToClientPos(p2), color, bounds);
		DrawDashedLine(pSurface, ToClientPos(p2), ToClientPos(p3), color, bounds);
		DrawDashedLine(pSurface, ToClientPos(p3), ToClientPos(p4), color, bounds);
		DrawDashedLine(pSurface, ToClientPos(p4), ToClientPos(p1), color, bounds);
	}
	else
	{
		DrawLine(pSurface, ToClientPos(p1), ToClientPos(p2), color, bounds);
		DrawLine(pSurface, ToClientPos(p2), ToClientPos(p3), color, bounds);
		DrawLine(pSurface, ToClientPos(p3), ToClientPos(p4), color, bounds);
		DrawLine(pSurface, ToClientPos(p4), ToClientPos(p1), color, bounds);
	}
}

void DrawTargetLaserPoint(DSurface* pSurface,
	CoordStruct location, ColorStruct color, RectangleStruct bound)
{
	Point2D targetPos = ToClientPos(location);
	// 颜色的随机偏移
	ColorStruct c = color;
	// 渲染路径，使用本地随机数，避免联机分叉
	BYTE rand = (BYTE)VisualRandomRanged(0, 14);
	if (c.R > rand)
	{
		c.R -= rand;
	}
	if (c.G > rand)
	{
		c.G -= rand;
	}
	if (c.B > rand)
	{
		c.B -= rand;
	}
	int dwColor = Drawing::RGB_To_Int(c);
	DrawTargetLaserPoint(pSurface, targetPos, dwColor, bound);
}

void DrawTargetLaserPoint(DSurface* pSurface,
	Point2D targetPos, int dwColor, RectangleStruct bound)
{
	// 在目标位置绘制光点 #
	Point2D a1 = targetPos, a2 = targetPos;
	a1.Y -= 1; a2.Y += 2;
	Point2D b1 = a1, b2 = a2;
	b1.X += 1; b2.X += 1;
	Point2D c1 = targetPos, c2 = targetPos;
	c1.X -= 1; c2.X += 2;
	Point2D d1 = c1, d2 = c2;
	d1.Y += 1; d2.Y += 1;
	DrawLine(pSurface, a1, a2, dwColor, bound);
	DrawLine(pSurface, b1, b2, dwColor, bound);
	DrawLine(pSurface, c1, c2, dwColor, bound);
	DrawLine(pSurface, d1, d2, dwColor, bound);
}

/**
 *@brief 绘制激光照射
 *
 * @param pSurface
 * @param start
 * @param end
 * @param color
 * @param bound
 */
void DrawTargetLaser(DSurface* pSurface,
	CoordStruct startLocation, CoordStruct endLocation, ColorStruct color, RectangleStruct bound,
	bool drawPoint)
{
	Point2D point1 = ToClientPos(startLocation);
	Point2D point2 = ToClientPos(endLocation);
	// 颜色的随机偏移
	ColorStruct c = color;
	// 渲染路径，使用本地随机数，避免联机分叉
	BYTE rand = (BYTE)VisualRandomRanged(0, 64);
	if (c.R > rand)
	{
		c.R -= rand;
	}
	if (c.G > rand)
	{
		c.G -= rand;
	}
	if (c.B > rand)
	{
		c.B -= rand;
	}
	int dwColor = Drawing::RGB_To_Int(c);
	if (drawPoint)
	{
		// 在目标位置绘制光点 #
		DrawTargetLaserPoint(pSurface, point2, dwColor, bound);
	}
	// 绘制线条
	int startZ = -32 - TacticalClass::AdjustForZ(startLocation.Z);
	int endZ = -32 - TacticalClass::AdjustForZ(endLocation.Z);
	int deltaX = point2.X - point1.X;
	int deltaY = point2.Y - point1.Y;
	int deltaZ = endZ - startZ;
	int dX = abs(deltaX);
	int dY = abs(deltaY);
	int times = 0;
	int length = 100;
	Point2D startPos = point1;
	Point2D endPos = startPos;
	int deltaX2 = deltaX;
	int deltaY2 = deltaY;
	int deltaZ2 = deltaZ;
	int endZ2 = endZ;
	do
	{
		endPos.X = point1.X + deltaX2 / 4;
		endPos.Y = point1.Y + deltaY2 / 4;
		endZ2 = startZ + deltaZ2 / 4;
		// 中心
		pSurface->DrawLineBlit(&bound, &startPos, &endPos, &c, times / 4 + 255, startZ, endZ2);
		// 两侧
		Point2D start = startPos;
		Point2D end = endPos;
		if (dX <= dY)
		{
			start.X += 1;
			end.X += 1;
			pSurface->DrawLineBlit(&bound, &start, &end, &c, length, startZ, endZ2);
			start.X -= 2;
			end.X -= 2;
		}
		else
		{
			start.Y += 1;
			end.Y += 1;
			pSurface->DrawLineBlit(&bound, &start, &end, &c, length, startZ, endZ2);
			start.Y -= 2;
			end.Y -= 2;
		}
		// 两侧
		pSurface->DrawLineBlit(&bound, &start, &end, &c, length, startZ, endZ2);
		// 位移绘制下一节
		startPos = endPos;
		startZ = endZ2;
		deltaX2 += deltaX;
		deltaY2 += deltaY;
		deltaZ2 += deltaZ;
		length -= 25;
		times -= 255;
	} while (times >= -765);
}

