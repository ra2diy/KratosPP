#include <Common.h>

#include <Drawing.h>

#include <Utilities/Debug.h>
#include <Utilities/Patch.h>
#include <Utilities/Macro.h>
#include <Utilities/SyncLogging.h>

#ifndef IS_RELEASE_VER
bool HideWarning = false;
#endif

bool InChinese = GetSystemDefaultLangID() == 0x0804;

HANDLE Common::hInstance = 0;

bool Common::IsLoadGame = false;
bool Common::IsScenarioClear = false;

bool Common::DebugAE = false;

char Common::readBuffer[Common::readLength];
wchar_t Common::wideBuffer[Common::readLength];
const char Common::readDelims[4] = ",";

//void Common::CmdLineParse(char** ppArgs, int nNumArgs)
void Common::CmdLineParse(EventSystem* sender, Event e, void* args)
{

	auto const& argsArray = reinterpret_cast<void**>(args);
	char** ppArgs = (char**)argsArray[0];
	int nNumArgs = (int)argsArray[1];

	// > 1 because the exe path itself counts as an argument, too!
	for (int i = 1; i < nNumArgs; i++)
	{
		const char* pArg = ppArgs[i];
		if (_stricmp(pArg, "-SYNCLOG") == 0)
		{
			SyncLogger::Enabled = true;
			Debug::Log("Sync logging enabled.\n");
		}
#ifdef DEBUG
		if (_stricmp(pArg, "-DebugAE") == 0)
		{
			DebugAE = true;
		}
#endif // DEBUG
	}

	if (InChinese)
	{
		Debug::Log("初始化完成 版本: " VERSION_SHORT_STR "\n");
	}
	else
	{
		Debug::Log("Initialized version: " VERSION_SHORT_STR "\n");
	}
}

void Common::ExeRun(EventSystem* sender, Event e, void* args)
{
	Patch::ApplyStatic();

#ifdef DEBUG
	if (InChinese)
	{
		if (Common::DetachFromDebugger())
		{
			MessageBoxW(NULL,
				L"你可以去附加调试器了。\n\n"

				L"按下 确定 继续运行尤里的复仇。",
				L"调试信息", MB_OK);
		}
		else
		{
			MessageBoxW(NULL,
				L"你可以去附加调试器了。\n\n"

				L"To attach a debugger find the YR process in Process Hacker "
				L"/ Visual Studio processes window and detach debuggers from it, "
				L"then you can attach your own debugger. After this you should "
				L"terminate Syringe.exe because it won't automatically exit when YR is closed.\n\n"

				L"按下 确定 继续运行尤里的复仇。",
				L"调试信息", MB_OK);
		}
	}
	else
	{
		if (Common::DetachFromDebugger())
		{
			MessageBoxW(NULL,
				L"You can now attach a debugger.\n\n"

				L"Press OK to continue YR execution.",
				L"Debugger Notice", MB_OK);
		}
		else
		{
			MessageBoxW(NULL,
				L"You can now attach a debugger.\n\n"

				L"To attach a debugger find the YR process in Process Hacker "
				L"/ Visual Studio processes window and detach debuggers from it, "
				L"then you can attach your own debugger. After this you should "
				L"terminate Syringe.exe because it won't automatically exit when YR is closed.\n\n"

				L"Press OK to continue YR execution.",
				L"Debugger Notice", MB_OK);
		}
	}

	if (!Console::Create())
	{
		if (InChinese)
		{
			MessageBoxW(NULL,
				L"控制台无法打开！",
				L"调试信息", MB_OK);
		}
		else
		{
			MessageBoxW(NULL,
				L"Failed to allocate the debug console!",
				L"Debug Console Notice", MB_OK);
		}
	}

#endif // DEBUG
}

void Common::ExeTerminate(EventSystem* sender, Event e, void* args)
{
	Console::Release();
}

#ifdef DEBUG

#pragma warning (disable : 4091)
#pragma warning (disable : 4245)

#include <Dbghelp.h>
#include <tlhelp32.h>

bool Common::DetachFromDebugger()
{
	auto GetDebuggerProcessId = [](DWORD dwSelfProcessId) -> DWORD
	{
		DWORD dwParentProcessId = -1;
		HANDLE hSnapshot = CreateToolhelp32Snapshot(2, 0);
		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32);
		Process32First(hSnapshot, &pe32);
		do
		{
			if (pe32.th32ProcessID == dwSelfProcessId)
			{
				dwParentProcessId = pe32.th32ParentProcessID;
				break;
			}
		} while (Process32Next(hSnapshot, &pe32));
		CloseHandle(hSnapshot);
		return dwParentProcessId;
	};

	HMODULE hModule = LoadLibrary("ntdll.dll");
	if (hModule != NULL)
	{
		auto const NtRemoveProcessDebug =
			(NTSTATUS(__stdcall*)(HANDLE, HANDLE))GetProcAddress(hModule, "NtRemoveProcessDebug");
		auto const NtSetInformationDebugObject =
			(NTSTATUS(__stdcall*)(HANDLE, ULONG, PVOID, ULONG, PULONG))GetProcAddress(hModule, "NtSetInformationDebugObject");
		auto const NtQueryInformationProcess =
			(NTSTATUS(__stdcall*)(HANDLE, ULONG, PVOID, ULONG, PULONG))GetProcAddress(hModule, "NtQueryInformationProcess");
		auto const NtClose =
			(NTSTATUS(__stdcall*)(HANDLE))GetProcAddress(hModule, "NtClose");

		HANDLE hDebug;
		HANDLE hCurrentProcess = GetCurrentProcess();
		NTSTATUS status = NtQueryInformationProcess(hCurrentProcess, 30, &hDebug, sizeof(HANDLE), 0);
		if (0 <= status)
		{
			ULONG killProcessOnExit = FALSE;
			status = NtSetInformationDebugObject(
				hDebug,
				1,
				&killProcessOnExit,
				sizeof(ULONG),
				NULL
			);
			if (0 <= status)
			{
				const auto pid = GetDebuggerProcessId(GetProcessId(hCurrentProcess));
				status = NtRemoveProcessDebug(hCurrentProcess, hDebug);
				if (0 <= status)
				{
					HANDLE hDbgProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
					if (INVALID_HANDLE_VALUE != hDbgProcess)
					{
						BOOL ret = TerminateProcess(hDbgProcess, EXIT_SUCCESS);
						CloseHandle(hDbgProcess);
						return ret;
					}
				}
			}
			NtClose(hDebug);
		}
		FreeLibrary(hModule);
	}

	return false;
}
#endif // DEBUG

bool __stdcall DllMain(HANDLE hInstance, DWORD dwReason, LPVOID v)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Common::hInstance = hInstance;
	}
	return true;
}
