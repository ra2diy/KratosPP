#pragma once
// ============================================================================
// TraceEffect — 效果器脚本层（Copy 同源族）
//
// 生命周期与 Copy 一致：AE 激活当帧 OnStart 执行第 1 次 -> 每 Trace.Delay
// 帧一次（<=0 按 1 帧）-> 累计 Trace.TriggeredTimes 次成功后整条 AE 移除。
// 单次执行主体 ExecuteOnce：收集观察源快照 -> Watch 过滤得线索 -> 需要来源名单时
// 收线索来源去重 -> 逐轮（目标 + 来源 + 发放过滤）整串按名附加 AttachEffects。
// 观察只提供"分发给谁 / 来源记谁"的角色，附加内容始终是 INI 固定名单。
// ============================================================================

#include <string>
#include <vector>

#include <GeneralDefinitions.h> // CDTimerClass（YRpp/Timer.h）

#include "../EffectScript.h"
#include "TraceData.h"

/// @brief 效果器：观察指定单位身上的 AE，把固定名单按名附加到选定目标
class TraceEffect : public EffectScript
{
public:
	EFFECT_SCRIPT(Trace);

	virtual void Clean() override
	{
		EffectScript::Clean();

		_count = 0;
		_cycleTimer = {};
	}

	virtual void OnStart() override;
	virtual void OnUpdate() override;

#pragma region Save/Load
	template <typename T>
	bool Serialize(T& stream) {
		return stream
			.Process(this->_count)
			.Process(this->_cycleTimer)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		EffectScript::Load(stream, registerForChange);
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		EffectScript::Save(stream);
		return const_cast<TraceEffect*>(this)->Serialize(stream);
	}
#pragma endregion
private:

	/// @brief 单次执行主体。成功 = 有实际附加，计入 _count
	void ExecuteOnce();

	/// @brief 周期帧数：Delay>0 取 Delay，否则按 1 帧
	/// 不能声明 const：Data getter（EFFECT_SCRIPT 生成）是非 const 方法
	int GetDelayFrame()
	{
		return Data->Delay > 0 ? Data->Delay : 1;
	}

	int _count = 0;          // 已成功执行次数（成功=至少一个有效轮次完成附加）
	CDTimerClass _cycleTimer{}; // 周期计时（第一次由 OnStart 启动）
};
