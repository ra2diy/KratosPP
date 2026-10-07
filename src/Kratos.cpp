#include <Helpers/Macro.h>

#include <CCINIClass.h>
#include <Unsorted.h>
#include <Fundamentals.h>
#include <Drawing.h>
#include <HouseClass.h>
#include <MessageListClass.h>

#include <cstdio>
#include <exception>
#include <string>

#include <Utilities/Debug.h>

#include <Kratos.h>
#include <Common/KratosSession.h>
#include <Common/KratosUpdateUrl.h>

#include <Version.h>

#pragma comment(lib, "KratosPPLib.lib")

// /Gz（stdcall）：C 符号修饰为 _KL_Xxx@N。
#pragma comment(linker, "/include:_KL_Initialize@8")
#pragma comment(linker, "/include:_KL_AcquireSession@0")
#pragma comment(linker, "/include:_KL_GetEntitlements@8")
#pragma comment(linker, "/include:_KL_CheckIniName@12")
#pragma comment(linker, "/include:_KL_OpenContent@28")
#pragma comment(linker, "/include:_KL_GetTable@20")
#pragma comment(linker, "/include:_KL_Update@16")
#pragma comment(linker, "/include:_KL_UpdateAsync@8")
#pragma comment(linker, "/include:_KL_GetVersion@16")
#pragma comment(linker, "/include:_KL_GetDownloadedVersion@16")
#pragma comment(linker, "/include:_KL_Tick@12")
#pragma comment(linker, "/include:_KL_Attest@20")
#pragma comment(linker, "/include:_KL_Shutdown@8")

namespace
{
	// ---- 版本号：主.次.修订.补丁，各 8 位 ----
	constexpr uint32_t HostVersion = (uint32_t(VERSION_MAJOR) << 24) | (uint32_t(VERSION_MINOR) << 16)
		| (uint32_t(VERSION_REVISION) << 8) | uint32_t(VERSION_PATCH);

	const char* ReasonName(KL_Reason reason)
	{
		switch (reason)
		{
		case KL_Reason_None:           return "None";
		case KL_Reason_ConflictModule: return "ConflictModule";
		case KL_Reason_ConflictIni:    return "ConflictIni";
		case KL_Reason_EnvIncomplete:  return "EnvIncomplete";
		case KL_Reason_UpdateRequired: return "UpdateRequired";
		case KL_Reason_AbiMismatch:    return "AbiMismatch";
		default:                       return "Unknown";
		}
	}

	// 4 个数字逐位比（高位优先）。两侧都必须是 "1.2.3.4" 这类 4 段十进制串。
	int CompareVersions(const char* a, const char* b)
	{
		unsigned va[4]{};
		unsigned vb[4]{};
		if (std::sscanf(a, "%u.%u.%u.%u", &va[0], &va[1], &va[2], &va[3]) != 4
			|| std::sscanf(b, "%u.%u.%u.%u", &vb[0], &vb[1], &vb[2], &vb[3]) != 4)
		{
			return 0;
		}

		for (int i = 0; i < 4; ++i)
		{
			if (va[i] != vb[i])
			{
				return va[i] > vb[i] ? 1 : -1;
			}
		}
		return 0;
	}

	// 本次运行成功下载到的版本号是否比本机新。没下载到 / 无法解析 / 不更高 ⇒ 不提示。
	bool IsNewerVersionAvailable()
	{
		if (KratosLib::Session == 0)
		{
			return false;
		}

		const uint32_t needed = KL_GetDownloadedVersion(KratosLib::Session, nullptr, 0);
		if (needed == 0 || needed > 64u)
		{
			return false;
		}

		char downloaded[65]{};
		if (KL_GetDownloadedVersion(KratosLib::Session, downloaded, sizeof(downloaded)) != needed)
		{
			return false;
		}

		return CompareVersions(downloaded, FILE_VERSION_STR) > 0;
	}
}

void Kratos::ExeRun(EventSystem* sender, Event e, void* args)
{
	(void)sender;
	(void)e;
	(void)args;

	const char* const updateUrl = KratosUpdate::kUpdateUrl;
	if (*updateUrl == '\0')
	{
		Debug::Log("[Kratos] update url is empty, remote list download is off\n");
	}
	else
	{
		Debug::Log("[Kratos] update url: \"%s\"\n", updateUrl);
	}

	KL_HostInfo info{};
	info.abiVer = KL_ABI_VERSION;
	info.hostVer = HostVersion;
	info.modBase = Common::hInstance;
	info.accountToken = nullptr;
	info.updateUrl = updateUrl;

	// modSize：本模块映像大小，从自己的 PE 头读。
	if (Common::hInstance)
	{
		const auto* base = static_cast<const uint8_t*>(Common::hInstance);
		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
		if (dos->e_magic == IMAGE_DOS_SIGNATURE)
		{
			const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
			if (nt->Signature == IMAGE_NT_SIGNATURE)
			{
				info.modSize = nt->OptionalHeader.SizeOfImage;
			}
		}
	}

	KL_Reason reason = KL_Reason_None;
	if (KL_Initialize(&info, &reason) != KL_Verdict_Proceed)
	{
		Debug::Log("[Kratos] KL_Initialize refused (reason = %s)\n", ReasonName(reason));
		KL_Shutdown(0);
	}
	else
	{
		KratosLib::Session = KL_AcquireSession();
	}

	Debug::Log("[Kratos] host version = %ls, session = %llu\n",
		VERSION_PLAIN_WSTR, static_cast<unsigned long long>(KratosLib::Session));

	EventSystems::General.AddHandler(Events::ExeTerminate, Shutdown);

	// "Kratos 已激活"滚动提示每次运行只出一次
	_activeMessageShown = false;
	// "有更新版本"提示行：回到"等结论"状态（每次运行最多显示一次）
	_updateNotice = UpdateNotice{};
}

void Kratos::Shutdown(EventSystem* sender, Event e, void* args)
{
	(void)sender;
	(void)e;
	(void)args;

	if (KratosLib::Session != 0)
	{
		KL_Shutdown(KratosLib::Session);
		KratosLib::Session = 0;
	}
}

void Kratos::SendActiveMessage(EventSystem* sender, Event e, void* args)
{
	if (!_activeMessageShown && args)
	{
		const wchar_t* message = L"Kratos " VERSION_SHORT_WSTR L" is active, have fun.";
		if (InChinese)
		{
			message = L"奎秃斯 " VERSION_SHORT_WSTR L" 已经激活，愉快的玩耍吧。";
		}

		MessageListClass::Instance->PrintMessage(L"[" PRODUCT_NAME L"]", message, 150, HouseClass::CurrentPlayer->ColorSchemeIndex, true);

		_activeMessageShown = true;
		sender->RemoveHandler(e, SendActiveMessage);
	}
}

Kratos::VersionText Kratos::_versionText{};
Kratos::UpdateNoticeText Kratos::_updateNoticeText{};

// 版本号显示 + 其正上方一行的"有更新版本"提示。
//
// 每次渲染问一次 KL_GetDownloadedVersion 并与本机版本比较；有更新就记下当前引擎帧号开始
// 显示，每次运行最多显示一次（无更新 / 没下载到 / 无法比较 / 已提示过 ⇒ 不画这一行）。
//
// 持续时间：600 引擎帧（Unsorted::CurrentFrame），另有 120 秒墙钟上限，两者取先到者；
// 判据在 Common/KratosUpdateNotice.h（KratosNotice::ShouldShow / LineAboveY）。
void Kratos::DrawVersionText(EventSystem* sender, Event e, void* args)
{
	if (!_versionText.text)
	{
		_versionText.text = L"Kratos " VERSION_SHORT_WSTR;
		RectangleStruct textRect = Drawing::GetTextDimensions(_versionText.text, { 0, 0 }, 0, 2, 0);
		RectangleStruct sidebarRect = DSurface::Sidebar->GetRect();
		int x = sidebarRect.Width / 2 - textRect.Width / 2;
		int y = sidebarRect.Height - textRect.Height - textRect.Height / 4;
		_versionText.pos = { x, y };
		_versionText.color = Drawing::RGB_To_Int(Drawing::TooltipColor);

		// 提示行：同一水平居中轴、同一颜色/字体/图层，位置上移一行字高
		_updateNoticeText.text = L"有更新版本";
		RectangleStruct noticeRect = Drawing::GetTextDimensions(_updateNoticeText.text, { 0, 0 }, 0, 2, 0);
		_updateNoticeText.pos = { sidebarRect.Width / 2 - noticeRect.Width / 2,
			KratosNotice::LineAboveY(y, textRect.Height) };
		_updateNoticeText.color = _versionText.color;
	}

	DSurface::Sidebar->DrawText(_versionText.text, &_versionText.pos, _versionText.color);

	const int frame = Unsorted::CurrentFrame.get();

	// ---- 等结论（一次性）----
	if (_updateNotice.phase == UpdateNotice::Phase::Pending)
	{
		if (!_updateNotice.pendingArmed)
		{
			_updateNotice.pendingArmed = true;
			_updateNotice.pendingFrame = frame;
		}

		if (IsNewerVersionAvailable())
		{
			_updateNotice.phase = UpdateNotice::Phase::Showing;
			_updateNotice.shownFrame = frame;
			_updateNotice.shownTick = GetTickCount64();
			Debug::Log("[Kratos] update notice shown at frame %d (will hide after %d frames / %llu ms)\n",
				frame, KratosNotice::kShowFrames,
				static_cast<unsigned long long>(KratosNotice::kWallClockCapMs));
		}
		else if (frame - _updateNotice.pendingFrame >= KratosNotice::kWaitConclusionFrames)
		{
			// 超过等待上限 ⇒ 放弃（不再每帧轮询）。
			_updateNotice.phase = UpdateNotice::Phase::Done;
			Debug::Log("[Kratos] update notice: no newer version within %d frames -> this line is not drawn\n",
				KratosNotice::kWaitConclusionFrames);
		}
	}

	// ---- 显示中：600 帧 + 120 秒墙钟上限 ----
	if (_updateNotice.phase == UpdateNotice::Phase::Showing)
	{
		const uint64_t now = GetTickCount64();
		if (!KratosNotice::ShouldShow(frame, _updateNotice.shownFrame, now, _updateNotice.shownTick))
		{
			_updateNotice.phase = UpdateNotice::Phase::Done;
			Debug::Log("[Kratos] update notice hidden: %d frames elapsed (limit %d), %llu ms elapsed (limit %llu)\n",
				frame - _updateNotice.shownFrame, KratosNotice::kShowFrames,
				static_cast<unsigned long long>(now - _updateNotice.shownTick),
				static_cast<unsigned long long>(KratosNotice::kWallClockCapMs));
		}
		else
		{
			DSurface::Sidebar->DrawText(_updateNoticeText.text, &_updateNoticeText.pos, _updateNoticeText.color);
		}
	}
}
