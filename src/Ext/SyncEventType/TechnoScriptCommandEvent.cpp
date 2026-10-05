#include <ObjectClass.h>
#include <TechnoClass.h>
#include <HouseClass.h>

#include <Extension/TechnoExt.h>

#include <Common/Components/Scriptable.h>
#include <Ext/Common/SyncEventManager.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>
#include <Ext/SyncEventType/TechnoScriptCommandEvent.h>

namespace
{
	/// <summary>
	/// `Respond` 派发期间的瞬时上下文（模块级）。两端都会各自求值，但结论只对**发起者**为真
	/// —— 详见头文件"恰好一条"的说明。派发结束即恢复（保存/恢复，防嵌套派发串味）。
	/// </summary>
	bool _isCurrentInitiator = false;
	bool _isEngineAlreadySent = false;

	/// <summary>
	/// 收端唯一动作：把命令派发给某个 techno 的全部脚本。
	/// `TechnoStatus::On*Command` 会转到 `On*Command_Stand`（替身在那里补发原版命令）。
	/// 不做归属/结构性校验（理由见头文件：TargetClass 两端一致是锁步前提，
	/// 在事件处理器里"防御"只会掩盖问题）。
	/// </summary>
	void DispatchToTechno(TechnoClass* pTechno, TechnoScriptCommandEvent::Command command)
	{
		if (!pTechno || IsDeadOrInvisible(pTechno) || pTechno->InLimbo)
		{
			return;
		}
		if (auto pExt = TechnoExt::ExtMap.Find(pTechno))
		{
			pExt->_GameObject->Foreach([&](Component* c)
			{
				if (auto cc = dynamic_cast<ITechnoScript*>(c))
				{
					if (command == TechnoScriptCommandEvent::Command::Guard)
					{
						cc->OnGuardCommand();
					}
					else
					{
						cc->OnStopCommand();
					}
				}
			});
		}
	}

	/// <summary>发端：为**单个**对象投递一条事件（Target = 它自己）。</summary>
	void RaiseToTechno(TechnoClass* pTechno, HouseClass* pInitiator, uint8_t commandByte)
	{
		TechnoScriptCommandEvent::EventData data{ TargetClass(pTechno), commandByte };
		SyncEventManager::Raise(TechnoScriptCommandEvent::Id, pInitiator, data);
	}
}

namespace TechnoScriptCommandEvent
{
	bool IsCurrentInitiator()
	{
		return _isCurrentInitiator;
	}

	bool IsEngineAlreadySent()
	{
		return _isEngineAlreadySent;
	}

	void Respond(SyncEventManager::Event* pEvent)
	{
		EventData* data = SyncEventManager::GetPayload<EventData>(pEvent);

		// 用 RTTI + ID 重新取对象，绝不使用发起端的本地指针。
		// 空/已死/InLimbo 是**合法状态**早退（对象可能在执行帧之前就没了、或上了运输机），
		// 两端判定一致 ⇒ 这不是"防脏数据"。
		TechnoClass* pTarget = data->Target.As_Techno();
		if (!pTarget)
		{
			return;
		}

		// 载荷字节：低 4 位＝命令；bit7＝"该对象当时在选中集里，引擎已发过原版命令"。
		const uint8_t commandByte = data->Command;
		const auto command = (Command)(commandByte & Flags::CommandMask);
		const bool engineSent = (commandByte & Flags::EngineSent) != 0;

		// "由我发送"的判据：事件里的 house 就是本机玩家（发起者）。
		// 事件 house 由 `Raise` 写入＝发起端的 CurrentPlayer ⇒ **只有发起端为真**；
		// 发起端就是原版热键所在的那台客户端 ⇒ 补发＝"原版按下 G 时它该做的事"，
		// 对称性是构造性的（不依赖 `ClickedMission` 有无本地副作用）⇒ 恰好一条、两端对称。
		HouseClass* pEventHouse = SyncEventManager::GetEventHouse(pEvent);
		const bool isInitiator = pEventHouse && pEventHouse == HouseClass::CurrentPlayer;

		// 瞬时上下文：派发期间有效（保存/恢复，防嵌套派发时串味）
		const bool prevInitiator = _isCurrentInitiator;
		const bool prevEngineSent = _isEngineAlreadySent;
		_isCurrentInitiator = isInitiator;
		_isEngineAlreadySent = engineSent;

		DispatchToTechno(pTarget, command);

		_isCurrentInitiator = prevInitiator;
		_isEngineAlreadySent = prevEngineSent;
	}

	void Raise(TechnoClass* pTechno, Command command, bool engineAlreadySent)
	{
		if (!pTechno)
		{
			return;
		}
		// 事件里的 house = **发起者**（按下热键的那个玩家）：引擎按它记 ResponseTime，
		// 收端也据此复现"由谁发送原版命令"的判定（只有发起端为真）。
		HouseClass* pInitiator = HouseClass::CurrentPlayer;
		if (!pInitiator)
		{
			return;
		}

		const uint8_t commandByte = (uint8_t)command
			| (engineAlreadySent ? Flags::EngineSent : 0);
		RaiseToTechno(pTechno, pInitiator, commandByte);
	}
}

// 注册给同步事件通道（消息 id = 2，载荷 6 字节 ⇒ 走小隧道 0x50）
namespace
{
	const SyncEventManager::Registrar _technoScriptCommandReg(
		TechnoScriptCommandEvent::Id, sizeof(TechnoScriptCommandEvent::EventData), &TechnoScriptCommandEvent::Respond);
}
