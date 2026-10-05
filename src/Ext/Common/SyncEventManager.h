#pragma once

#include <cstdint>
#include <cstddef>

#include <EventClass.h>
#include <HouseClass.h>

/// <summary>
/// 同步事件管理器（**隧道模型**）
///
/// 机制：`EventClass::OutList.Add(一条事件)` → 引擎随同步包广播 →
/// 两端都在 `Networking::RespondToEvent`(0x4C6CC8) 各执行一次处理器。
///
/// ★ 为什么用隧道：引擎的**载荷长度表是按"事件号"索引的**——三处 `GetEventSize` 钩子只拿得到事件号，
///   看不到载荷内容（已用 IDA 复核）。所以"一个动作一个号"会被功能数量吃掉号段。
///   于是引擎号段只留**两条定长隧道**，真正的语义用**我们自己的消息 id**在隧道内多路复用：
///
///     引擎号（稀缺，需跨平台协商）           消息 id（自家维护，只追加）
///     ────────────────────────────         ────────────────────────────────
///     0x50 小隧道，定长 SmallTunnelSize     1 = AttachEffectToTarget（给目标附加 AE）
///     0x51 大隧道，定长 LargeTunnelSize     … 后续动作继续追加，**不占新号**
///
///   隧道内层信封（定长隧道里再自带长度）：
///     { MsgId Id; uint8_t Data[...] }   ← 2 字节头 + 载荷（消息长度由登记表决定，不写在帧里）
///
/// 事件号分段（沿用 Phobos 源码注释给出的既成约定，并经 IDA 复核）：
///   vanilla 0x00-0x2F ／ CnCNet 0x30-0x3F ／ Phobos 0x40-0x41 ／ Ares 0x60-0x61
/// ⇒ **Kratos 只占 0x50-0x51**，不向外铺开，避免与其它平台/分支的既有号段交叉。
///
/// ⚠️ 纪律：
///   · **唯一需要小心的是"别撞号"**：0x50-0x51 不能与同进程里其它 DLL 占的号重合
///     （Phobos 0x40-0x41、Ares 0x60-0x61、CnCNet 0x30-0x3F）—— 那是"第三方共存"问题，
///     与"两端版本必须相同"无关；
///   · **内部 `MsgId` 与隧道长度随版本自由变更**：两端跑同一份 DLL 是前提，
///     而这条前提本来就必须成立 ⇒ 不存在"另一端还留着旧值"的情形；
///   · id 未登记 / 载荷超出登记大小 ⇒ **一律忽略并记日志**：定位是**防手滑**（typo、写坏结构），不是正确性依赖；
///   · 处理器只能依赖**同步状态**；引用对象必须用 `TargetClass`（RTTI+ID）重新解析，
///     **绝不能用发起端的本地指针**（本地选中集之类的状态一律不可信）；
///   · "参照玩家"必须用 `GetEventHouse()`（事件里的 house），**不能用本地 `CurrentPlayer`**；
///     唯一例外是 `TechnoScriptCommandEvent::IsCurrentInitiator()`：它读本地 `CurrentPlayer`
///     只为判定"原版命令由**哪一台**补发"（发起端＝按下热键的那台，各机不同正是设计要求），
///     不参与任何模拟量计算；见 `Ext/SyncEventType/TechnoScriptCommandEvent.h`；
///   · 随机数只能用引擎共享 RNG（`ScenarioClass::Instance->Random`）。
/// </summary>
#pragma pack(push, 1)
class SyncEventManager
{
public:
	// ---------------- 隧道（引擎号，定长） ----------------

	inline static constexpr uint8_t SmallTunnel = 0x50;
	inline static constexpr uint8_t LargeTunnel = 0x51;

	/// <summary>小隧道总长（2 字节信封头 + 6 字节载荷，正好装下“一个目标 + 一个标量”）</summary>
	inline static constexpr size_t SmallTunnelSize = 8;

	/// <summary>大隧道总长（2 字节信封头 + 102 字节载荷，正好用满 EventClass::Data）</summary>
	inline static constexpr size_t LargeTunnelSize = 104;

	/// <summary>`EventClass::Data` 的容量</summary>
	inline static constexpr size_t MaxDataSize = 104;

	/// <summary>信封头长度</summary>
	inline static constexpr size_t EnvelopeSize = 2;

	/// <summary>小隧道能装的载荷上限</summary>
	inline static constexpr size_t SmallPayloadMax = SmallTunnelSize - EnvelopeSize;

	/// <summary>大隧道能装的载荷上限</summary>
	inline static constexpr size_t LargePayloadMax = LargeTunnelSize - EnvelopeSize;

	// ---------------- 消息 id ----------------
	enum class MsgId : uint16_t
	{
		/// <summary>给目标附加 AE（触发方式：快捷键）</summary>
		AttachEffectToTarget = 1,

		/// <summary>把"停止 / 警戒"派发给某个 techno（必要时含它的替身）的脚本。
		/// 载荷 = TechnoScriptCommandEvent::EventData = 6 字节（TargetClass 5B + 命令字节 1B）
		/// ⇒ 不超过 SmallPayloadMax(6) ⇒ 自动走**小隧道 0x50**（0x51 大隧道仍空闲）</summary>
		TechnoScriptCommand = 2,
	};

	/// <summary>隧道内层信封头</summary>
	struct Envelope
	{
		MsgId Id;
	};

	/// <summary>
	/// 与 `EventClass` **逐字节同构**（111 字节；Data 从偏移 7 开始。
	/// IDA 实证：`InitList_1132` 按 111 字节 × 128 清零，见 `ida_work/probe_event_queue.py`）。
	/// 用独立 struct 是因为 `EventClass` 没有默认构造；入队时按字节重新解释。
	/// </summary>
	struct Event
	{
		uint8_t Type;
		bool IsExecuted;
		char HouseIndex;      // 发起方所属 house（-1 = 无效）
		uint32_t Frame;       // 期望执行帧（与引擎自身事件一致：Unsorted::CurrentFrame）
		char DataBuffer[MaxDataSize];

		/// <summary>入队（走引擎 `QueueClass<EventClass,128>::Add`，含上限判定与 Timings 记录）。</summary>
		bool Add() const;
	};

	using Handler = void (*)(Event* pEvent);

	/// <summary>
	/// 静态注册器：功能在自己的 .cpp 里放一个
	/// `namespace { const SyncEventManager::Registrar _reg(Id, sizeof(载荷), &Handler); }` 即可。
	/// </summary>
	struct Registrar
	{
		Registrar(MsgId id, size_t dataSize, Handler handler)
		{
			Register(id, dataSize, handler);
		}
	};

	// ---------------- 登记 ----------------

	/// <summary>登记一个消息 id（同 id 重复登记会覆盖并告警）。</summary>
	static bool Register(MsgId id, size_t dataSize, Handler handler);

	/// <summary>该消息 id 是否已登记。</summary>
	static bool IsRegistered(MsgId id);

	// ---------------- 发起端 ----------------

	/// <summary>发起：按登记大小选隧道、装信封、入队。</summary>
	static bool Raise(MsgId id, HouseClass* pHouse, const void* data, size_t dataSize);

	/// <summary>发起（载荷按引用，最常用）。</summary>
	template <typename TPayload>
	static bool Raise(MsgId id, HouseClass* pHouse, const TPayload& payload)
	{
		static_assert(sizeof(TPayload) <= LargePayloadMax, "payload must fit the tunnel");
		return Raise(id, pHouse, &payload, sizeof(TPayload));
	}

	// ---------------- 收端 ----------------

	/// <summary>收端入口：验信封 → 查 id → 校验长度 → 调处理器。返回是否已处理。</summary>
	static bool Dispatch(Event* pEvent);

	/// <summary>
	/// 收端取载荷（自动跳过信封头）：
	/// `auto* data = SyncEventManager::GetPayload<MyPayload>(pEvent);`
	/// </summary>
	template <typename TPayload>
	static TPayload* GetPayload(Event* pEvent)
	{
		static_assert(sizeof(TPayload) <= LargePayloadMax, "payload must fit the tunnel");
		return reinterpret_cast<TPayload*>(pEvent->DataBuffer + EnvelopeSize);
	}

	/// <summary>按事件里的 HouseIndex 安全取回 HouseClass（越界/失效返回 nullptr）。</summary>
	static HouseClass* GetEventHouse(const Event* pEvent);

	// ---------------- 引擎钩子用 ----------------

	/// <summary>是否为本管理器的隧道号。</summary>
	static bool IsTunnel(uint8_t engineType);

	/// <summary>隧道号的定长；非隧道返回 0（钩子据此决定是否接管）。</summary>
	static size_t GetTunnelSize(uint8_t engineType);
};
#pragma pack(pop)

static_assert(sizeof(SyncEventManager::Event) == sizeof(EventClass),
	"SyncEventManager::Event must be byte-identical to EventClass");
static_assert(offsetof(SyncEventManager::Event, DataBuffer) == 7,
	"DataBuffer must sit at offset 7");
static_assert(sizeof(SyncEventManager::Envelope) == SyncEventManager::EnvelopeSize,
	"Envelope header must be exactly EnvelopeSize bytes");
