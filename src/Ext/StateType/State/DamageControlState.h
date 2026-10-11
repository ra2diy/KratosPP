#pragma once

#include <vector>

#include <GeneralDefinitions.h>
#include <HouseClass.h>

#include "../StateScript.h"
#include <Ext/EffectType/Effect/DamageControlData.h>

class DamageControlEffect;

/// @brief 单位的伤害控制管线宿主。
///
/// 本状态默认处于休眠（未激活）状态：没有配伤害控制的单位挨打时，引擎的组件遍历会整个跳过本组件，
/// 不留任何开销。每一段伤害控制 AE 生效时把自己登记进来、失效时注销，表空则重新休眠。
///
/// 表里存的是段本身（AE）而不是合并后的数值，结算时按相位逐个问段 [这一次响应吗]：
/// 概率、冷却、弹头匹配这些都取决于 [这一发打过来的是什么]，无法预先合并成几个数。
/// 顺序与淘汰规则在段增删时一次算好，平时挨打不再排序。
class DamageControlState : public StateScript<DamageControlData>
{
public:
	STATE_SCRIPT(DamageControl);

	virtual void Clean() override;

	/// @brief 不读 [TechnoType] 上的配置：本状态完全由 AE 段的增删驱动，
	/// 不提供 [直接在单位类型里开启] 这个入口，所以这里什么都不做
	virtual void OnInitState(bool replace) override {}

	virtual void OnReceiveDamage(args_ReceiveDamage* args) override;

	/// @brief 一段开始生效：登记进来并唤醒状态
	/// @param segment 这条段（AE 效果器），生命周期由 AE 自己管，本状态不负责销毁它
	void AddSegment(DamageControlEffect* segment);

	/// @brief 一段不再生效：从表里划掉；表空了就休眠
	void RemoveSegment(DamageControlEffect* segment);

private:
	/// @brief 依据段列表重算三张分表，并据此决定唤醒还是休眠
	void Rebuild();

	// 全部生效中的段，按附着顺序保存；三张分表由 Rebuild 从它算出来
	std::vector<DamageControlEffect*> _segments{};

	std::vector<DamageControlEffect*> _evasions{}; // 闪避相位
	std::vector<DamageControlEffect*> _modifiers{}; // 刚毅 / 减免相位
	std::vector<DamageControlEffect*> _prevents{}; // 免死相位
};
