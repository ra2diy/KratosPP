#pragma once

// "有更新版本"提示行的时间与位置规则（纯 UI，无引擎依赖）。

#include <cstdint>

namespace KratosNotice
{
	// 显示时长（引擎帧）：600 帧 ≈ 标准 15 逻辑帧/秒下约 40 秒
	inline constexpr int kShowFrames = 600;

	// 墙钟上限（毫秒）：防止帧号停滞（暂停/最小化/读档中）导致提示永不消失
	inline constexpr uint64_t kWallClockCapMs = 120000ull;

	// 等待"更新结论"的上限（引擎帧）：900 帧 ≈ 60 秒。
	inline constexpr int kWaitConclusionFrames = 900;

	// 提示行此刻是否应当绘制。
	// 帧数达上限**或**墙钟达上限 ⇒ 不再绘制（先到者为准）。
	inline bool ShouldShow(int frameNow, int shownFrame, uint64_t tickNow, uint64_t shownTick) noexcept
	{
		if (frameNow - shownFrame >= kShowFrames)
		{
			return false;
		}
		return (tickNow - shownTick) < kWallClockCapMs;
	}

	// 提示行的 y 坐标：版本号 y 上移一行字高。
	inline int LineAboveY(int versionY, int lineHeight) noexcept
	{
		return versionY - lineHeight;
	}
}
