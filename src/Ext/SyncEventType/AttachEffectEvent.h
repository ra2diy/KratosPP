#pragma once

#include <cstdint>

#include <TargetClass.h>
#include <HouseClass.h>
#include <TechnoClass.h>

#include <Ext/Common/SyncEventManager.h>

/// <summary>
/// 发端把「目标 + 组号」作为一条同步事件投递，两端在 `Networking::RespondToEvent` 里各执行一次。
/// 载荷里**不放 AE 名字**：收端按目标的 TechnoType ID 重新解析 `[HotKeyX]`（全局 + 个体覆盖），
/// 两端读同一份 INI ⇒ 结果一致；成功率走引擎共享 RNG ⇒ 也一致。
/// 通道本身见 `Ext/Common/SyncEventManager.h`。
/// </summary>
namespace AttachEffectEvent
{
	/// <summary>本消息在隧道里的 id（登记在 `SyncEventManager.h` 的消息表里；只追加、永不改号）</summary>
	inline constexpr SyncEventManager::MsgId Id = SyncEventManager::MsgId::AttachEffectToTarget;

	/// <summary>
	/// 事件载荷：目标对象引用（RTTI + ID，可跨端序列化）+ 快捷键组号。
	/// </summary>
#pragma pack(push, 1)
	struct EventData
	{
		TargetClass Target;
		uint8_t Group;    // 1..9
	};
#pragma pack(pop)

	// 载荷布局属于线格式：两端必须逐字节一致，钉死它
	static_assert(sizeof(EventData) == 6, "payload layout is part of the wire format");

	/// <summary>发端：遍历**本地**选中集，逐个发起（供快捷键命令调用）。</summary>
	void Raise(int group);

	/// <summary>发端：对**单个目标**发起（供点击 / 图标等后续触发器复用）</summary>
	void RaiseToTarget(HouseClass* pHouse, TechnoClass* pTarget, int group);

	/// <summary>收端：按事件里的 house 与目标类型配置执行附加（注册给 SyncEventManager）。</summary>
	void Respond(SyncEventManager::Event* pEvent);
}
