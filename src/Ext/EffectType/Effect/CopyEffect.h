#pragma once
// ============================================================================
// CopyEffect — 效果器脚本层（纯主线程）
//
// 生命周期：AE 激活（_started=true）当帧 OnStart 立刻执行第 1 次；
// 之后每 Copy.Delay 帧执行 1 次（Delay<=0 按 1 帧）；累计 Copy.TriggeredTimes
// 次成功后整条 CopyAE 移除（Deactivate + AE->TimeToDie）。
// 单次执行主体 ExecuteOnce：收集观察源快照 -> Watch 过滤 -> Discard -> (Cut)
// -> AttachTo 解析发放 -> 逐条标准附加。附加动作直接调用现有 Attach 基建。
// ============================================================================

#include <string>
#include <vector>

#include <GeneralDefinitions.h> // CDTimerClass（YRpp/Timer.h）

#include "../EffectScript.h"
#include "CopyData.h"

/// @brief 效果器：观察指定单位身上的 AE，把命中观察名单的 AE 贴到选定目标
class CopyEffect : public EffectScript
{
public:
	EFFECT_SCRIPT(Copy);

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
		return const_cast<CopyEffect*>(this)->Serialize(stream);
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

	int _count = 0;          // 已成功执行次数（成功=清单非空且至少一个有效发放对象）
	CDTimerClass _cycleTimer{}; // 周期计时（第一次由 OnStart 启动）
};
