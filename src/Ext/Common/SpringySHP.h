#pragma once

#include <GeneralDefinitions.h>

struct SHPStruct;

/// @brief SHP 缩放帧的生成与缓存。
///
/// SHP 绘制本身没有缩放能力，所以变形要靠「把每一帧解码成像素、重采样成缩放版、
/// 再拼一份内存里的 SHP」然后把绘制用的指针换过去。调色板索引不做任何插值，
/// 只用最近邻取样，否则会引入透明像素与颜色混合，与调色板机制冲突。
namespace SpringySHP
{
	/// @brief 取一份指定缩放比例的单帧 SHP 副本（带缓存）
	/// @param pOriginal 原始 SHP
	/// @param scaleW 水平倍率
	/// @param scaleH 垂直倍率
	/// @param pivotX 缩放枢轴 X（SHP 帧坐标）；usePivot 为 false 时忽略
	/// @param pivotY 缩放枢轴 Y；usePivot 为 false 时忽略
	/// @param usePivot true = 绕给定枢轴缩放（贴身特效跟着单位走），
	///                 false = 自身底边对齐缩放（独立特效原地变大小）
	/// @return 缩放后的 SHP；倍率为 1 或构建失败时返回原 SHP
	SHPStruct* GetScaled(SHPStruct* pOriginal, double scaleW, double scaleH,
		int pivotX, int pivotY, bool usePivot);

	/// @brief 清空缓存并释放全部自造 SHP
	/// @note 只能在确认没有任何对象还持有这些指针时调用（场景开始 / 结束）
	void ClearCache();
}
