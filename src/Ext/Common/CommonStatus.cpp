#include "CommonStatus.h"

#include <Common/EventSystems/EventSystem.h>

GeneralData* General::_data = nullptr;

CombatDamageData* CombatDamage::_data = nullptr;

AudioVisualData* AudioVisual::_data = nullptr;

AIConfigData* AIConfig::_data = nullptr;

// 挂到 Events::ScenarioClearClassesEvent（必须排在 INI::ClearBuffer 之后）：
// INI::ClearBuffer 会 GameDelete 掉这些 INIConfig，缓存指针必须同时失效，
// 否则新一局里 UAF。见 CommonStatus.h:General::ClearCache。
void ClearCommonStatusCache(EventSystem* sender, Event e, void* args)
{
	General::ClearCache();
	CombatDamage::ClearCache();
	AudioVisual::ClearCache();
	AIConfig::ClearCache();
}
