#pragma once
#include <Windows.h>
#include <Version.h>

#include <string>
#include <GeneralStructures.h>
#include <Timer.h>
#include <VoxClass.h>
#include <VocClass.h>
#include <Common.h>
#include <Common/EventSystems/EventSystem.h>
#include <Common/KratosUpdateNotice.h>

class Kratos
{
public:
	struct VersionText
	{
		const wchar_t* text;
		Point2D pos;
		int color;
	};

	static VersionText _versionText;

	/// <summary>
	/// 版本号**上方一行**的"有更新版本"提示（纯 UI）。pos/color 与版本号同一套，
	/// 只是位置上移一行；text == nullptr 表示本次运行不显示该行。
	/// </summary>
	struct UpdateNoticeText
	{
		const wchar_t* text = nullptr;
		Point2D pos{};
		int color = 0;
	};

	static UpdateNoticeText _updateNoticeText;

	// 载入后初始化一次，并登记退出时的释放。
	static void ExeRun(EventSystem* sender, Event e, void* args);
	static void Shutdown(EventSystem* sender, Event e, void* args);

	static void SendActiveMessage(EventSystem* sender, Event e, void* args);
	static void DrawVersionText(EventSystem* sender, Event e, void* args);

private:
	// "Kratos 已激活"滚动提示每次运行只出一次
	inline static bool _activeMessageShown = false;

	// ---- "有更新版本"提示行的状态机 ----
	//
	// 显示时长：600 引擎帧（见 Common/KratosUpdateNotice.h），每次运行最多显示一次。
	struct UpdateNotice
	{
		enum class Phase
		{
			Pending, // 还在等"更新结论"
			Showing, // 正在显示，按帧数倒计时
			Done,    // 本行不会再出现
		};

		Phase phase = Phase::Pending;
		int shownFrame = 0;     // 开始显示时的引擎帧号
		int pendingFrame = 0;   // 进入等待时的引擎帧号（等待上限用）
		bool pendingArmed = false;
		uint64_t shownTick = 0; // 开始显示时的墙钟毫秒（上限用）
	};

	// 显示判据见 Common/KratosUpdateNotice.h：
	//   KratosNotice::kShowFrames（600 引擎帧）
	//   KratosNotice::kWallClockCapMs（120 秒墙钟上限）
	//   KratosNotice::ShouldShow / LineAboveY
	inline static UpdateNotice _updateNotice{};
};
