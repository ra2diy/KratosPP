#pragma once

#include <string>

#include <GeneralDefinitions.h>

#include "../EffectScript.h"
#include <Ext/StateType/State/SpringyData.h>

class SpringyState;

/// @brief 一条 Springy AE 的段。
///
/// 本段只负责自己：把配置换算成这一帧该用的倍率（含波形与过渡），
/// 并在生效期间把自己登记进所在单位的 SpringyState。
/// 多条段之间的合成由 SpringyState 与 TechnoStatus 负责，本段不感知别的段。
class SpringyEffect : public EffectScript
{
public:
	EFFECT_SCRIPT(Springy);

	virtual void Clean() override;

	virtual void OnStart() override;

	/// @brief 推进过渡进度；冻结期间不推进
	virtual void OnUpdate() override;

	/// @brief AE 因血量等条件暂停：本段留在汇总表里，倍率冻结在暂停那一刻，画面不闪
	virtual void OnPause() override;

	/// @brief AE 恢复：继续推进过渡
	virtual void OnRecover() override;

	/// @brief 宿主被 InheritAE 换成另一个对象：重新登记到新宿主的汇总表
	virtual void ExtChanged() override;

	/// @brief AE 结束：立刻从汇总表划掉，倍率当帧就回落到剩余各段
	/// @note 不做缩短过渡：AE 到期即复原，这正是「移除瞬间弹回」的实现点
	virtual void End(CoordStruct location) override;

	/// @brief 本段这一帧的最终倍率
	/// @param x 横向（FLH 的 F）倍率，输出
	/// @param y 横向（FLH 的 L）倍率，输出
	/// @param z 纵向（FLH 的 H）倍率，输出
	/// @note 已包含基准倍率、波形系数与过渡进度；未生效时三个输出都是 1.0
	void GetScale(double& x, double& y, double& z);

	/// @brief 本段这一帧的完整形变：三个轴倍率 + 两个方向剪切，一次算完
	/// @param shearX 水平推的斜率（正 = 顶部右移），输出
	/// @param shearY 垂直推的斜率（正 = 右侧下移），输出
	void GetTransform(double& x, double& y, double& z, double& shearX, double& shearY);

	/// @brief 本段的剪切参数单位（PX / PERCENT）
	SpringyShearMode GetShearMode();

	/// @brief 只取本段这一帧的两个剪切量
	void GetShear(double& shearX, double& shearY);

	/// @brief 本段配置的依附动画枢轴
	SpringyAnimPivot GetAnimPivot();

	/// @brief 本段的作用对象：MASTER 只给主身、STAND 只给替身、ALL 两边都给
	/// @note 由宿主的汇总表在取用时过滤，段本身不区分自己最终被谁读到
	AffectWho GetAffectWho();

	/// @brief 本段是否处于冻结（被暂停）状态
	bool IsFrozen() const { return _frozen; }

#pragma region Save/Load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->_progress)
			.Process(this->_frozen)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override;
	virtual bool Save(ExStreamWriter& stream) const override;
#pragma endregion
private:
	/// @brief 把本段登记进所在单位的汇总表（新建走 OnStart、读档走 Load、换宿主走 ExtChanged）
	void RegisterToState();

	/// @brief 把本段从汇总表划掉（组件回池时调用）
	void UnregisterFromState();

	/// @brief 过渡进度，0~1：0 = 完全原样，1 = 完全到位
	double Progress();

	int _progress = 0;   // 已走过的过渡帧数
	bool _frozen = false; // 被血量条件暂停时为 true，画面停在当前倍率

	SpringyState* _state = nullptr; // 本段登记所在的汇总表；失效时用它把自己划掉
};
