#pragma once

#include <GeneralDefinitions.h>

class TechnoClass;

/// @brief 缩放后的画面刷新。
///
/// 引擎只重绘它认为「脏」的对象，而缩放会把像素画出对象原本的渲染矩形：
/// 放大时新像素落在原矩形之外，缩小时原矩形内会留下旧像素。所以这里做两件事：
///   1. 倍率变化时把对象标记重绘；
///   2. 把对象的渲染矩形按最坏情况外扩后交回引擎，让它真正重画那片区域。
namespace SpringyRefresh
{
	/// @brief 每帧调用一次：让画面正在变化的单位重新绘制
	/// @note 只在倍率真的变了时动手，静止的建筑不会每帧重绘
	void OnFrame();

	/// @brief 给一个单位补一次重绘与脏矩形，用于「AE 刚失效、画面要弹回原样」的当帧
	/// @param pTechno 目标单位
	void RefreshNow(TechnoClass* pTechno);

	/// @brief 只补脏矩形，不标重绘
	/// @param pTechno 目标单位
	/// @note 给本来就在每帧重绘的对象用（载具、步兵）：它们的像素会刷新，
	///       但缩放后画到的面积可能超出引擎预计算的矩形，周围会留下残影
	void AddDirtyOnly(TechnoClass* pTechno);
}
