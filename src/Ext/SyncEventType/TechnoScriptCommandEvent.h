#pragma once

#include <cstdint>

#include <TargetClass.h>
#include <TechnoClass.h>

#include <Ext/Common/SyncEventManager.h>

/// <summary>
/// Guard/Stop 的 Kratos 副作用走这条同步事件（消息 id = 2，载荷 6B ⇒ 小隧道 0x50）。
///
/// ★ 架构：
///   · **发端**（单端热键执行体 0x730DEB/0x730E56/0x730EEB，体内遍历 v_CurrentObjects）：
///     ①对**被下令对象自己**投递一条消息（`Raise(pTechno, cmd)`，默认 `engineAlreadySent = true`
///       —— 它在选中集里，引擎自己的原版命令已经/即将由引擎发出，本条只负责"通知脚本"）；
///     ②对它的每个替身调 `StandEffect::RaiseGuardCommand()` / `RaiseStopCommand()`
///       （`engineAlreadySent = false` —— 替身没被选中、引擎不会给它发原版命令）。
///   · **收端**（隧道 `Respond` → `DispatchToTechno`，两端各执行一次）：按 `Target` 重新取对象，
///     把命令派发给**该 techno 自己的** ITechnoScript；`TechnoStatus::On*Command`
///     → `On*Command_Stand`（替身在那里补发原版命令）。
///
/// ★ "原版命令恰好入队一条、且两端判定可复现"的机制：
///   `Respond` 在派发前把两个**瞬时值**设好（派发后恢复）：
///     · `IsCurrentInitiator()` = `GetEventHouse(pEvent) == HouseClass::CurrentPlayer`
///       —— 事件里的 house 是发起者（`Raise` 写入），所以**只有发起者那台为真**（各客户端
///       `CurrentPlayer` 不同）；它只决定"**由谁发送**"那条原版命令。
///       **对称性是构造性的**：这次补发被限定在发起端，而发起端正是**原版 Guard/Stop 热键会调用
///       这一句的那台客户端**（原版 0x730DEB/0x730E56 就在按键端的 `v_CurrentObjects` 循环里调
///       `TechnoClass::ClickedMission`(0x6FFBE0，slot 222) / `ObjectClass::GetCellAgain`(0x5F6A10，
///       slot 113)；Stop 同理走 `ClickedEvent`(0x6FFE00，slot 221)）；其余客户端只执行那条广播出去的
///       MegaMission 事件 ⇒ 每台机器的行为都分别等价于"原版按下这个热键时它该做的事" ⇒ 对称。
///       **该论证不依赖"这几个虚函数没有本地副作用"**：`ClickedMission` 在 planning 模式分支只构造+
///       入队，正常分支还会播语音并 `Queue_Mission` —— 按键端本来就该做这些事。
///     · `IsEngineAlreadySent()` = 载荷 bit7 —— 该对象当时在选中集里，引擎已发过原版命令 ⇒
///       `On*Command_Stand` 不再补发（这同时覆盖"替身自己被选中"的情形）。
///   ⇒ `On*Command_Stand` 只在 `!IsEngineAlreadySent() && IsCurrentInitiator()` 时补发，
///     每条命令的原版事件**恰好一条**。
///
/// 通道本身见 `Ext/Common/SyncEventManager.h`。
/// </summary>
namespace TechnoScriptCommandEvent
{
	/// <summary>本消息在隧道里的 id（登记在 `SyncEventManager.h` 的消息表里；只追加、永不改号）</summary>
	inline constexpr SyncEventManager::MsgId Id = SyncEventManager::MsgId::TechnoScriptCommand;

	enum class Command : uint8_t
	{
		Stop = 1,
		Guard = 2,
	};

	namespace Flags
	{
		/// <summary>载荷 `Command` 字节的低 4 位＝命令（Command 枚举）</summary>
		inline constexpr uint8_t CommandMask = 0x0F;
		/// <summary>
		/// 载荷 `Command` 字节的 bit7＝"该对象当时在选中集里，引擎已发过原版命令（Guard/Stop）"。
		/// 置位时收端**只通知脚本、不再补发原版命令**。
		/// </summary>
		inline constexpr uint8_t EngineSent = 0x80;
	}

	/// <summary>
	/// 事件载荷：只有一个跨端可重解析的引用（RTTI + ID）+ 一个命令字节
	/// （低 4 位命令 + bit7 `Flags::EngineSent`）。
	/// **6 字节 == SmallPayloadMax(6)** ⇒ `SyncEventManager::Raise` 里
	/// `DataSize &gt; SmallPayloadMax` 为假 ⇒ 自动走**小隧道 0x50**（不再占大隧道 0x51）。
	/// </summary>
#pragma pack(push, 1)
	struct EventData
	{
		TargetClass Target;  // 5B：被下令的 techno（主身或替身，逐对象一条）
		uint8_t Command;     // 1B：低 4 位 = Command；bit7 = Flags::EngineSent
	};
#pragma pack(pop)

	// 载荷布局属于线格式：两端必须逐字节一致，钉死它
	static_assert(sizeof(EventData) == 6, "payload layout is part of the wire format");
	static_assert(sizeof(EventData) <= SyncEventManager::SmallPayloadMax, "6B must fit the small tunnel (0x50)");

	/// <summary>
	/// 发端：为**单个**对象投递一条消息。
	/// `engineAlreadySent`（写入载荷 bit7）：true = 该对象在选中集里、引擎已发过原版命令，
	/// 收端只通知脚本；false = 替身（未被选中），收端需补发原版命令。
	/// house 用**发起者**（CurrentPlayer）写进事件（引擎按它记 ResponseTime），收端据此复现
	/// "由谁发送"的判定。
	/// </summary>
	void Raise(TechnoClass* pTechno, Command command, bool engineAlreadySent = true);

	/// <summary>收端：按事件里的 `Target` 重新解析对象后派发（注册给 SyncEventManager）。</summary>
	void Respond(SyncEventManager::Event* pEvent);

	/// <summary>
	/// 派发期间的瞬时值：事件里的 house 是否就是**本机**玩家（即"本机就是发起者"）。
	/// 只在 `Respond` → `DispatchToTechno` 期间有效（由 `Respond` 设置/恢复）。
	/// </summary>
	bool IsCurrentInitiator();

	/// <summary>派发期间的瞬时值：本条消息对应的对象当时是否已在选中集里（引擎已发过原版命令）。</summary>
	bool IsEngineAlreadySent();
}
