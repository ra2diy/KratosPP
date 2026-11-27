#pragma once

#include <GeneralDefinitions.h>

#include <Ext/Common/SpringyTransform.h>

struct SHPStruct;
class AnimClass;
class TechnoClass;

/// @brief 依附动画的缩放与剪切。
///
/// 动画的绘制入口是 AnimClass::DrawIt，SHP 则在那条路径上的几个 blit 调用点使用。
/// 绘制入口处判定一次「这条动画属于谁、该用多大」，结果存进本命名空间的绘制上下文；
/// 后面的 blit 点直接用这份上下文换 SHP，不再重复判定。
namespace SpringyAnim
{
	/// @brief 吸附到当前正在绘制的动画上的形变参数
	struct DrawContext
	{
		bool Active = false;

		// 画布形变（缩放 + 剪切复合）。与主体共用同一个矩阵，
		// 所以单位一边胀缩一边推拉时，它身上的动画会跟着一起变形
		SpringyTransform Transform{};

		// 形变枢轴（SHP 帧坐标）。UsePivot 为 false 时绕自身帧底边中点变形
		bool UsePivot = false;
		int PivotX = 0;
		int PivotY = 0;

		// 依附单位画布尺寸，用于「画布不一致就退回自身底边」的判断
		short OwnerCanvasW = 0;
		short OwnerCanvasH = 0;
	};

	/// @brief 开始绘制一条动画：判定归属并算出本次要用的形变
	/// @param pAnim 正在绘制的动画
	/// @note 每条动画绘制前都会调用，内部先清空上一次的上下文
	void BeginDraw(AnimClass* pAnim);

	/// @brief 判定一条动画是否属于挂了 Springy 的单位
	/// @param pAnim 目标动画
	/// @param pOwner 输出的归属单位（可能为空）
	/// @return 有归属单位且该单位正在生效时返回 true
	/// @note 归属判定顺序：AE 动画的真实依附对象 pAttachOwner 优先，其次原版 OwnerObject。
	///       AE 动画的 OwnerObject 恒为空（创建时只设了所属方颜色），
	///       而建筑覆盖动画的 pAttachOwner 在创建时也被写成建筑，两者都能命中
	bool ResolveOwner(AnimClass* pAnim, TechnoClass*& pOwner);

	/// @brief 取一条动画当前应使用的形变 SHP
	/// @param pOriginal 原始 SHP
	/// @return 形变副本；上下文未激活或形变恒等时返回原 SHP
	/// @note 供各 blit 点直接调用，内部已含枢轴判定
	SHPStruct* SubstituteSHP(SHPStruct* pOriginal);
}
