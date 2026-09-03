#include "DamageText.h"

#include <Ext/Common/PrintTextManager.h>

#include <Ext/Helper/MathEx.h>

bool DamageText::SkipDrawDamageText(WarheadTypeClass* pWH, DamageTextData*& damageTextType)
{
	if (!SkipDamageText && pWH && !pTechno->InLimbo && !IsCloaked(pTechno))
	{
		damageTextType = INI::GetConfig<DamageTextData>(INI::Rules, pWH->ID)->Data;
		return damageTextType->Hidden;
	}
	return true;
}

void DamageText::OrderDamageText(std::wstring text, CoordStruct location, DamageTextEntity*& data)
{
	int x = VisualRandomRanged(data->XOffset.X, data->XOffset.Y);
	int y = VisualRandomRanged(data->YOffset.X, data->YOffset.Y) - 15; // 离地高度
	Point2D offset{ x, y };
	// 横向锚点修正
	int length = text.size() / 2;
	if (data->UseSHP)
	{
		offset.X -= data->ImageSize.X * length;
	}
	else
	{
		offset.X -= PrintTextManager::GetFontSize().X * length;
	}
	PrintTextManager::AddRollingText(text, location, offset, data->RollSpeed, data->Duration, *data);
}

void DamageText::Awake()
{
	DamageTextData* data = INI::GetConfig<DamageTextData>(INI::Rules, INI::SectionAudioVisual)->Data;
	if (data->Hidden)
	{
		Disable();
	}
}

void DamageText::OnUpdate()
{
	CoordStruct location = pTechno->GetCoords();
	int frame = Unsorted::CurrentFrame;
	// 伤害数字
	for (auto it = _damageCache.begin(); it != _damageCache.end();)
	{
		if (frame - it->second.StartFrame >= it->first->Rate)
		{
			std::wstring text = L"-" + std::to_wstring(it->second.Value);
			DamageTextEntity* data = it->first;
			OrderDamageText(text, location, data);
			it = _damageCache.erase(it);
		}
		else
		{
			it++;
		}
	}
	// 治疗数字
	for (auto it = _repairCache.begin(); it != _repairCache.end();)
	{
		if (frame - it->second.StartFrame >= it->first->Rate)
		{
			std::wstring text = L"+" + std::to_wstring(it->second.Value);
			DamageTextEntity* data = it->first;
			OrderDamageText(text, location, data);
			it = _repairCache.erase(it);
		}
		else
		{
			it++;
		}
	}
}

void DamageText::OnReceiveDamageEnd(int* pRealDamage, WarheadTypeClass* pWH, DamageState damageState, TechnoClass* pAttacker, HouseClass* pAttackingHouse)
{
	DamageTextData* whDamageTextType = nullptr;
	if (SkipDrawDamageText(pWH, whDamageTextType))
	{
		return;
	}
	else
	{
		SkipDamageText = false;
	}
	std::wstring text{};
	DamageTextEntity* data = nullptr;
	int damage = *pRealDamage;
	int damageValue = 0;
	int repairValue = 0;
	if (damage > 0)
	{
		data = &whDamageTextType->Damage;
		if (!data->Hidden)
		{
			damageValue += damage;
			text = L"-" + std::to_wstring(damage);
		}
	}
	else if (damage < 0)
	{
		data = &whDamageTextType->Repair;
		if (!data->Hidden)
		{
			repairValue -= damage;
			text = L"+" + std::to_wstring(-damage);
		}
	}
	if (text.empty() || !data || data->Hidden)
	{
		return;
	}
	if (data->Detail || damageState == DamageState::NowDead)
	{
		// 直接下单
		CoordStruct location = pTechno->GetCoords();
		OrderDamageText(text, location, data);
	}
	else
	{
		// 写入缓存
		if (damageValue > 0)
		{
			auto it = _damageCache.find(data);
			if (it != _damageCache.end())
			{
				_damageCache[data].Add(damageValue);
			}
			else
			{
				DamageTextCache cache{ damageValue };
				_damageCache[data] = cache;
			}
		}
		else if (repairValue > 0)
		{
			auto it = _repairCache.find(data);
			if (it != _repairCache.end())
			{
				_repairCache[data].Add(repairValue);
			}
			else
			{
				DamageTextCache cache{ repairValue };
				_repairCache[data] = cache;
			}
		}
	}
}
