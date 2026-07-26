#include <exception>
#include <Windows.h>

#include <Extension.h>
#include <Utilities/Macro.h>

#include <Common/Components/Component.h>
#include <Common/Components/ComponentPool.h>
#include <Common/EventSystems/EventSystem.h>
#include <Common/INI/INI.h>
#include <Common/INI/INIConstant.h>

#include <Ext/Helper/MathEx.h>

#include <Ext/Common/FireSuperManager.h>
#include <Ext/Common/PaintballSyncManager.h>
#include <Ext/Common/PrintTextManager.h>

class GeneraHook
{
public:
	GeneraHook()
	{
		EventSystems::General.AddHandler(Events::CmdLineParse, Common::CmdLineParse);
		EventSystems::General.AddHandler(Events::ExeRun, Common::ExeRun);
		EventSystems::General.AddHandler(Events::ExeTerminate, Common::ExeTerminate);
		EventSystems::General.AddHandler(Events::ScenarioStartEvent, ComponentPool::Clear);
		EventSystems::General.AddHandler(Events::ScenarioStartEvent, INIConstant::SetGameModeName);
		EventSystems::General.AddHandler(Events::ScenarioStartEvent, FireSuperManager::Clear);
		EventSystems::General.AddHandler(Events::ScenarioStartEvent, PaintballSyncManager::Clear);
		EventSystems::General.AddHandler(Events::ScenarioStartEvent, PrintTextManager::Clear);
		EventSystems::General.AddHandler(Events::ScenarioClearClassesEvent, INI::ClearBuffer);
		EventSystems::General.AddHandler(Events::ScenarioClearClassesEvent, ExtTypeRegistryClear);
		EventSystems::Logic.AddHandler(Events::LogicUpdateEvent, FireSuperManager::Update);
	}
};

static GeneraHook _GeneraHook;

DEFINE_HOOK(0x52BA60, YR_Boot, 0x5)
{
	return 0;
}

DEFINE_HOOK(0x52F639, YR_CmdLineParse, 0x5)
{
	GET(char **, ppArgs, ESI);
	GET(int, nNumArgs, EDI);

	void *args[]{ppArgs, (void *)nNumArgs};
	EventSystems::General.Broadcast(Events::CmdLineParse, args);

	// Debug::LogDeferredFinalize();
	return 0;
}

DEFINE_HOOK(0x7CD810, ExeRun, 0x9)
{
	EventSystems::General.Broadcast(Events::ExeRun);

	return 0;
}

void NAKED _ExeTerminate()
{
	// Call WinMain
	SET_REG32(EAX, 0x6BB9A0);
	CALL(EAX);
	PUSH_REG(EAX);

	EventSystems::General.Broadcast(Events::ExeTerminate);

	// Jump back
	POP_REG(EAX);
	SET_REG32(EBX, 0x7CD8EF);
	__asm {jmp ebx}
	;
}
DEFINE_JUMP(LJMP, 0x7CD8EA, GET_OFFSET(_ExeTerminate));

DEFINE_HOOK(0x6851F0, Scenario_ClearClasses_Start, 0x6)
{
	Common::IsScenarioClear = true;
	return 0;
}


DEFINE_HOOK(0x685659, Scenario_ClearClasses_End, 0xA)
{
	EventSystems::General.Broadcast(Events::ScenarioClearClassesEvent);
	Common::IsScenarioClear = false;
	return 0;
}

// in progress: Initializing Tactical display
DEFINE_HOOK(0x6875F3, Scenario_Start1, 0x6)
{
	int seed = Game::Seed;
	Debug::Log("Scenario start, set random seed = %d\n", seed);

	// 打印ComponentFactory清单，用于Debug
	// ComponentFactory::GetInstance().PrintCreaterInfo();

	EventSystems::General.Broadcast(Events::ScenarioStartEvent);
	return 0;
}

DEFINE_HOOK(0x55AFB3, LogicClass_Update, 0x6)
{
	EventSystems::Logic.Broadcast(Events::LogicUpdateEvent);
	return 0;
}

DEFINE_HOOK(0x55B719, LogicClass_Update_Late, 0x5)
{
	EventSystems::Logic.Broadcast(Events::LogicUpdateEvent, EventArgsLate);
	return 0;
}
