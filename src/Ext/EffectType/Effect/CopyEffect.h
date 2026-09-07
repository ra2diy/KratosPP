#pragma once
// ============================================================================
// CopyEffect — 效果器脚本层
//
// 生命周期：AE 激活（_started=true）当帧 OnStart 立刻执行第 1 次拷贝；
// 之后每 Copy.Delay 帧执行 1 次（Delay<=0 按 1 帧）；累计 Copy.TriggeredTimes
// 次成功后整条 CopyAE 移除（Deactivate + AE->TimeToDie）。
//
// 单次执行主体 ExecuteOnce 的执行顺序：
//   收集宿主全部生效 AE -> 过滤 -> 来源死活判定 -> (Cut) -> AttachTo 解析发放 -> 逐条 Attach
// 每个动作都直接调用现有函数，无一处复制式重写（复用铁律）。
// ============================================================================

#include <string>
#include <vector>

#include <GeneralDefinitions.h> // CDTimerClass（YRpp/Timer.h）

#include "../EffectScript.h"
#include "CopyData.h"

/// @brief 效果器：克隆宿主身上正在生效的 AE 的配置到指定对象
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

	/// @brief 单次拷贝执行主体（主清单通道 + Additional 通道编排）
	/// 成功 = 主清单或 Additional 至少一个通道有实际附加，计入 _count
	void ExecuteOnce();

	/// @brief Additional 附加通道：把 AdditionalAttachEffects 名单按
	/// AdditionalAttachTo/AdditionalAttachFrom 的迭代表逐轮按名附加
	/// @return 是否有实际附加（用于成功计数）
	/// @param fromAEM 复制源（Copy.From 对象）的 AE 管理器（收集来源名单用）
	bool ExecuteAdditional(TechnoClass* host, TechnoClass* copySource, AttachEffect* fromAEM);

	/// @brief AttachFrom 来源解析 + 死亡回退链（只决定"来源记谁"，与贴判定无关）
	/// @param initialSource 被复制 AE 自己的来源（InitialSource 位）
	/// @param copySource CopyAE 的来源（Source 位）
	/// @param host 宿主（Target 位，恒活）
	TechnoClass* ResolveSource(TechnoClass* initialSource, TechnoClass* copySource, TechnoClass* host);

	/// @brief 周期帧数：Delay>0 取 Delay，否则按 1 帧（拍板）
	/// 不能声明 const：Data getter（EFFECT_SCRIPT 生成）是非 const 方法
	int GetDelayFrame()
	{
		return Data->Delay > 0 ? Data->Delay : 1;
	}

	int _count = 0;          // 已成功执行次数（成功=最终清单非空且至少一个有效粘贴对象）
	CDTimerClass _cycleTimer{}; // 周期计时（第一次由 OnStart 启动）
};
