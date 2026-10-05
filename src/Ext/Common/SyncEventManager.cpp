#include <cstring>
#include <map>

#include <EventClass.h>
#include <HouseClass.h>
#include <Unsorted.h>

#include <Utilities/Debug.h>

#include <Ext/Common/SyncEventManager.h>

namespace
{
	struct Entry
	{
		size_t DataSize;
		SyncEventManager::Handler Handler;
	};

	/// <summary>
	/// 消息 id 登记表。用函数内静态（Meyers）保证**先于任何 Registrar**构造，
	/// 避免跨编译单元的静态初始化顺序问题（各功能的 Registrar 在 DLL 加载期执行）。
	/// </summary>
	std::map<SyncEventManager::MsgId, Entry>& Messages()
	{
		static std::map<SyncEventManager::MsgId, Entry> messages{};
		return messages;
	}
}

bool SyncEventManager::Register(MsgId id, size_t dataSize, Handler handler)
{
	if ((uint16_t)id == 0)
	{
		Debug::Log("Warning: SyncEvent msg id 0 is reserved (empty), refused.\n");
		return false;
	}
	if (dataSize == 0 || dataSize > LargePayloadMax)
	{
		Debug::Log("Warning: SyncEvent msg id %u payload %d bytes is out of range (1..%d), refused.\n",
			(uint16_t)id, (int)dataSize, (int)LargePayloadMax);
		return false;
	}
	if (IsRegistered(id))
	{
		Debug::Log("Warning: SyncEvent msg id %u was already registered, overwritten.\n", (uint16_t)id);
	}
	Messages()[id] = Entry{ dataSize, handler };
	return true;
}

bool SyncEventManager::IsRegistered(MsgId id)
{
	return Messages().find(id) != Messages().end();
}

bool SyncEventManager::IsTunnel(uint8_t engineType)
{
	return engineType == SmallTunnel || engineType == LargeTunnel;
}

size_t SyncEventManager::GetTunnelSize(uint8_t engineType)
{
	switch (engineType)
	{
	case SmallTunnel:
		return SmallTunnelSize;
	case LargeTunnel:
		return LargeTunnelSize;
	default:
		break;
	}
	return 0;
}

bool SyncEventManager::Dispatch(Event* pEvent)
{
	if (!pEvent)
	{
		return false;
	}

	// ① 只接管自己的隧道号（其余号一律交回链上：Phobos / Ares / vanilla）
	const size_t tunnelSize = GetTunnelSize(pEvent->Type);
	if (tunnelSize == 0)
	{
		return false;
	}

	// ② 查消息 id（未登记 ⇒ 忽略）
	const Envelope* envelope = reinterpret_cast<const Envelope*>(pEvent->DataBuffer);
	auto it = Messages().find(envelope->Id);
	if (it == Messages().end())
	{
		Debug::Log("Warning: SyncEvent msg id %u is not registered, ignored.\n", (uint16_t)envelope->Id);
		return false;
	}

	if (!it->second.Handler)
	{
		return false;
	}
	it->second.Handler(pEvent);
	return true;
}

bool SyncEventManager::Event::Add() const
{
	// 与 Phobos 同款：QueueClass::Add（含 128 条上限判定 + Timings 记录 + 环形推进）
	return EventClass::OutList->Add(*reinterpret_cast<const EventClass*>(this));
}

bool SyncEventManager::Raise(MsgId id, HouseClass* pHouse, const void* data, size_t dataSize)
{
	if (!pHouse || !data)
	{
		return false;
	}

	auto it = Messages().find(id);
	if (it == Messages().end())
	{
		Debug::Log("Warning: SyncEvent msg id %u is not registered, raise refused.\n", (uint16_t)id);
		return false;
	}
	if (dataSize == 0 || dataSize > it->second.DataSize)
	{
		Debug::Log("Warning: SyncEvent msg id %u payload %d bytes != registered %d, raise refused.\n",
			(uint16_t)id, (int)dataSize, (int)it->second.DataSize);
		return false;
	}

	// 按登记大小选隧道（两端据此得到同样的定长，收发两侧的帧长判定一致）
	const bool large = it->second.DataSize > SmallPayloadMax;
	const size_t tunnelSize = large ? LargeTunnelSize : SmallTunnelSize;
	if (it->second.DataSize > tunnelSize - EnvelopeSize)
	{
		Debug::Log("Warning: SyncEvent msg id %u registered size %d exceeds tunnel %d, raise refused.\n",
			(uint16_t)id, (int)it->second.DataSize, (int)(tunnelSize - EnvelopeSize));
		return false;
	}

	Event event{};
	event.Type = large ? LargeTunnel : SmallTunnel;
	event.IsExecuted = false;
	event.HouseIndex = (char)pHouse->ArrayIndex;
	event.Frame = Unsorted::CurrentFrame;

	Envelope* envelope = reinterpret_cast<Envelope*>(event.DataBuffer);
	envelope->Id = id;

	std::memcpy(event.DataBuffer + EnvelopeSize, data, dataSize);
	return event.Add();
}

HouseClass* SyncEventManager::GetEventHouse(const Event* pEvent)
{
	if (!pEvent)
	{
		return nullptr;
	}
	const int index = (int)pEvent->HouseIndex;
	if (index < 0 || index >= HouseClass::Array->Count)
	{
		return nullptr;
	}
	return HouseClass::Array->GetItem(index);
}
