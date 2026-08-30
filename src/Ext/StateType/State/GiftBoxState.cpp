#include "GiftBoxState.h"

#include <Ext/Helper/MathEx.h>
#include <Ext/Helper/Gift.h>

bool GiftBoxState::CanOpen()
{
	return IsAlive() && !IsOpen && Timeup() && GetGiftData().Enable;
}


void GiftBoxState::ResetGiftBox()
{
	GiftBoxEntity data = GetGiftData();
	IsOpen = false;
	_delay = GetRandomValue(data.RandomDelay, data.Delay);
	if (_delay > 0)
	{
		_delayTimer.Start(_delay);
	}
}

void GiftBoxState::OnStart()
{
	// Dynamic 模式：自动读取被附加对象的类型名填入 Types
	if ((Data.Data.Dynamic || Data.Data.DynamicFromSource) && !_dynamicFilled)
	{
		_dynamicFilled = true;
		std::vector<std::string> types;
		if (Data.Data.DynamicFromSource)
		{
			if (pAESource)
			{
				types.push_back(pAESource->GetTechnoType()->ID);
			}
		}
		if (Data.Data.Dynamic)
		{
			if (pTechno)
			{
				types.push_back(pTechno->GetTechnoType()->ID);
			}
			else if (pBullet && pBullet->Type)
			{
				types.push_back(pBullet->Type->ID);
			}
		}
		if (!types.empty())
		{
			std::sort(types.begin(), types.end());
			types.erase(std::unique(types.begin(), types.end()), types.end());
			Data.Data.Gifts = types;
			Data.EliteData.Gifts = Data.Data.Gifts;
		}
	}
	ResetGiftBox();
}

void GiftBoxState::OnUpdate()
{
#ifdef DEBUG
	StateScript<GiftBoxData>::OnUpdate();
#endif // DEBUG
	bool isElite = false;
	if (pTechno)
	{
		isElite = pTechno->Veterancy.IsElite();
	}
	else if (pBullet && pBullet->Owner)
	{
		isElite = pBullet->Owner->Veterancy.IsElite();
	}
	if (_isElite != isElite && IsAlive() && _delayTimer.Expired())
	{
		ResetGiftBox();
	}
	_isElite = isElite;
}

