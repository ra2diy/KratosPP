#pragma once

class TechnoClass;
class OverlayTypeClass;

/// @brief 碾压等级系统
///
/// 等级语义（与原版二态布尔等价）：
///   0  = 谁都能压（Crushable=yes 的单位默认落到这里）
///   5  = 普通碾压者 / "不可压"单位的默认等级
///   10 = 全压（OmniCrusher）/ 全免疫（OmniCrushResistant）
/// 判定：碾压者 CrushLevel > 被碾压者 CrushableLevel（严格大于）。
///
/// 最终等级 = 类型/全局基准值 + 箱子/AE 加成（加法聚合，见 Status.CrushLevel / Status.CrushableLevel）。
namespace Crush
{
	/// @brief 只按类型、全局规则与军衔计算的碾压者等级（不含箱子/AE 加成）
	int CalcCrushLevel(TechnoClass* pThis);

	/// @brief 只按类型、全局规则与军衔计算的被碾压等级（不含箱子/AE 加成）
	int CalcCrushableLevel(TechnoClass* pThis);

	/// @brief 最终碾压者等级：优先读 TechnoStatus 的每帧缓存，未就绪时现场计算
	int GetCrushLevel(TechnoClass* pThis);

	/// @brief 最终被碾压等级：优先读 TechnoStatus 的每帧缓存，未就绪时现场计算
	int GetCrushableLevel(TechnoClass* pThis);

	/// @brief overlay（墙、沙袋等）自己配置的被碾压等级（该 overlay 段落里的 CrushableLevel）。
	/// 未配置返回 -1，调用方沿用原版规则（看 Crushable 标记 / Wall + WallCrushableLevel）。
	int GetOverlayCrushableLevel(OverlayTypeClass* pOverlay);

	/// @brief 清空 overlay 等级缓存。换局时 overlay 类型会重建，缓存必须一起失效。
	void ClearOverlayLevelCache();
}
