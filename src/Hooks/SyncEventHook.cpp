#include <Utilities/Macro.h>

#include <Ext/Common/SyncEventManager.h>

/// 已分配：
///   0x50 —— 快捷键附加 AE
///   0x51 —— 预留
DEFINE_HOOK(0x4C6CC8, Networking_RespondToEvent_SyncEvent, 0x5)
{
	GET(SyncEventManager::Event*, pEvent, ESI);

	SyncEventManager::Dispatch(pEvent);
	return 0;
}

DEFINE_HOOK(0x64B6FE, sub_64B660_GetEventSize_SyncEvent, 0x6)
{
	const size_t eventSize = SyncEventManager::GetTunnelSize(static_cast<uint8_t>(R->EDI() & 0xFF));

	if (eventSize > 0)
	{
		R->EDX(eventSize);
		R->EBP(eventSize);
		return 0x64B71D;
	}
	return 0;
}

DEFINE_HOOK(0x64BE7D, sub_64BDD0_GetEventSize1_SyncEvent, 0x6)
{
	const size_t eventSize = SyncEventManager::GetTunnelSize(static_cast<uint8_t>(R->EDI() & 0xFF));

	if (eventSize > 0)
	{
		REF_STACK(size_t, eventSizeInStack, STACK_OFFSET(0xAC, -0x8C));
		eventSizeInStack = eventSize;
		R->ECX(eventSize);
		R->EBP(eventSize);
		return 0x64BE97;
	}
	return 0;
}

DEFINE_HOOK(0x64C30E, sub_64BDD0_GetEventSize2_SyncEvent, 0x6)
{
	const size_t eventSize = SyncEventManager::GetTunnelSize(static_cast<uint8_t>(R->ESI() & 0xFF));

	if (eventSize > 0)
	{
		R->ECX(eventSize);
		R->EBP(eventSize);
		return 0x64C321;
	}
	return 0;
}
