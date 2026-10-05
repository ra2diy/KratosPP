#include <exception>
#include <Windows.h>
#include <cstdio>
#include <cwchar>

#include <CommandClass.h>

#include <Utilities/Macro.h>

#include <Ext/SyncEventType/AttachEffectEvent.h>

// 创建命令类并加入到CommandClass::Array中
template <typename T>
T* MakeCommand()
{
	T* command = GameCreate<T>();
	CommandClass::Array->AddItem(command);
	return command;
}

// 按键附加AE命令类
template <int Group>
class AttachEffectCommandClass : public CommandClass
{
public:
	virtual const char* GetName() const override
	{
		static char name[32]{};
		sprintf_s(name, "AttachEffect%d", Group);
		return name;
	}

	virtual const wchar_t* GetUIName() const override
	{
		static wchar_t name[64]{};
		swprintf_s(name, L"Attach Effects %d", Group);
		return name;
	}

	virtual const wchar_t* GetUICategory() const override
	{
		return L"Kratos";
	}

	virtual const wchar_t* GetUIDescription() const override
	{
		return L"Attach effects to the selected units.";
	}

	virtual void Execute(WWKey eInput) const override
	{
		AttachEffectEvent::Raise(Group);
	}
};

DEFINE_HOOK(0x533066, CommandClassCallback_Register, 0x6)
{
	// 按键附加AE注册9个
	MakeCommand<AttachEffectCommandClass<1>>();
	MakeCommand<AttachEffectCommandClass<2>>();
	MakeCommand<AttachEffectCommandClass<3>>();
	MakeCommand<AttachEffectCommandClass<4>>();
	MakeCommand<AttachEffectCommandClass<5>>();
	MakeCommand<AttachEffectCommandClass<6>>();
	MakeCommand<AttachEffectCommandClass<7>>();
	MakeCommand<AttachEffectCommandClass<8>>();
	MakeCommand<AttachEffectCommandClass<9>>();
	return 0;
}
