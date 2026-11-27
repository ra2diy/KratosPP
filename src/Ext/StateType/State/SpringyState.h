#pragma once

#include <vector>

#include <GeneralDefinitions.h>
#include <HouseClass.h>

#include "../StateScript.h"
#include "SpringyData.h"

class SpringyEffect;

/// @brief 单位身上所有 Springy 段的汇总入口。
///
/// 与 Paintball 那类状态不同，本状态不保存「一份配置」：一个单位可以同时挂多条 Springy AE，
/// 每条 AE 是一个段，各自算自己的倍率，最后由本状态逐轴相乘得出一个结果。
/// 段生效时把自己登记进来、失效时注销，表空则休眠。
///
/// 本状态不直接写宿主快照：快照由 TechnoStatus::OnUpdate_Springy 刷新，
/// 因为替身要额外纳入母体上 AffectWho 允许的那些段，而休眠的状态不会被更新。
class SpringyState : public StateScript<SpringyData>
{
public:
	STATE_SCRIPT(Springy);

	virtual void Clean() override;

	/// @brief 不读 [TechnoType] 上的配置：本状态完全由 AE 段的增删驱动，
	/// 不提供「在单位类型里直接开启」这个入口，所以这里什么都不做
	virtual void OnInitState(bool replace) override {}

	/// @brief 一段开始生效：登记进来
	/// @param segment 这条段（AE 效果器），生命周期由 AE 自己管，本状态不负责销毁它
	/// @note 段一律登记在自己宿主的状态上，不跨对象。AffectWho 的取舍在取用时过滤，
	/// 这样替身先死也不会有指向别的对象的悬空指针
	void AddSegment(SpringyEffect* segment);

	/// @brief 一段不再生效：从表里划掉；表空了就休眠
	void RemoveSegment(SpringyEffect* segment);

	bool HasSegment() const { return !_segments.empty(); }

	/// @brief 把表里允许作用于 viewer 的段合成这一帧的倍率与剪切
	/// @param x 横向（对应 FLH 的 F）倍率，输出
	/// @param y 横向（对应 FLH 的 L）倍率，输出
	/// @param z 纵向（对应 FLH 的 H）倍率，输出
	/// @param shearX 顶边相对底边的水平位移（像素），输出；多条段相加
	/// @param shearY 右边相对左边的垂直位移（像素），输出
	/// @param viewer 从谁的视角取：MASTER 计入 AffectWho 为 MASTER/ALL 的段，
	///               STAND 计入 AffectWho 为 STAND/ALL 的段
	/// @return 是否至少计入了一个段
	/// @note 没有符合条件的段时倍率都是 1.0、剪切都是 0
	bool GetGeometry(double& x, double& y, double& z, double& shearX, double& shearY, AffectWho viewer);
	/// @brief 依附动画的缩放枢轴设置：取最后挂上的那条段
	/// @return 表空时返回 ANIM（动画自身底边）
	SpringyAnimPivot GetAnimPivot();

private:
	// 生效中的段，按附着顺序保存；last 即最后挂上的那条
	std::vector<SpringyEffect*> _segments{};
};
