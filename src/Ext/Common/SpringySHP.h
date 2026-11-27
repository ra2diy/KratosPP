#pragma once

#include <GeneralDefinitions.h>

#include <Ext/Common/SpringyTransform.h>

struct SHPStruct;

/// @brief SHP 缩放/剪切的生成与缓存。
///
/// SHP 绘制本身没有缩放或剪切能力，所以变形要靠「把每一帧解码成像素、按形变重采样、
/// 再拼一份内存里的 SHP」然后把绘制用的指针换过去。调色板索引不做任何插值，
/// 只用最近邻取样，否则会引入透明像素与颜色混合，与调色板机制冲突。
namespace SpringySHP
{
	/// @brief 取一份按形变矩阵重采样的 SHP 副本（带缓存）
	/// @param pOriginal 原始 SHP
	/// @param scaleW,scaleH 位图缩放倍率
	/// @param shearX 顶边相对底边的水平位移（像素，正 = 右移）
	/// @param shearY 右边相对左边的垂直位移（像素或百分比，正 = 下移）
	/// @param shearMode 剪切单位：PX = 像素，PERCENT = 整图高度/宽度的百分比
	/// @param inPlace true = 保持原帧矩形（只换像素、超出裁掉）。建筑本体与建筑动画画进的是
	///        建筑自己的缓存画布，换更大的帧会越过缓存边界写坏邻行，必须用这个模式
	/// @param pivotX 形变枢轴 X（SHP 帧坐标）；usePivot 为 false 时忽略
	/// @param pivotY 形变枢轴 Y；usePivot 为 false 时忽略
	/// @param usePivot true = 绕给定枢轴变形（贴身特效跟着单位走），
	///                 false = 自身底边中点变形（独立特效原地变大小 / 倾斜）
	/// @return 变形后的 SHP；形变是恒等或构建失败时返回原 SHP
	SHPStruct* GetTransformed(SHPStruct* pOriginal,
		double scaleW, double scaleH, double shearX, double shearY, SpringyShearMode shearMode,
		int pivotX, int pivotY, bool usePivot, bool inPlace = false);

	/// @brief 清空缓存并释放全部自造 SHP
	/// @note 只能在确认没有任何对象还持有这些指针时调用（场景开始 / 结束）
	void ClearCache();
}
