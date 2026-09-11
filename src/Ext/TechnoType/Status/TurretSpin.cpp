#include "../TechnoStatus.h"

#include <Ext/Helper/Scripts.h>
#include <Ext/EffectType/Effect/TurretSpinData.h>

void TechnoStatus::OnUpdate_TurretSpin()
{
	if (!pTechno || IsDeadOrInvisible(pTechno))
	{
		_turretSpinActive = false;
		return;
	}

	AttachEffect* aem = AEManager();
	if (!aem)
	{
		_turretSpinActive = false;
		return;
	}

	TurretSpinData* pData = nullptr;
	aem->ForeachChild([&pData](Component* c) {
		auto ae = dynamic_cast<AttachEffectScript*>(c);
		if (ae && ae->IsAlive() && !ae->IsPaused() && ae->AEData.TurretSpin.Enable)
		{
			pData = &ae->AEData.TurretSpin;
		}
	});
	if (!pData)
	{
		_turretSpinActive = false;
		return;
	}
	// 激活标志供 Rotation_AI 的 hook 读取：跳过引擎对炮塔朝向的瞄准/回正覆写
	_turretSpinActive = true;

	int speed = pData->Speed;
	int period = pData->SweepPeriod;

	DirStruct dir = pTechno->SecondaryFacing.Current();
	double rad = dir.GetValue();

	if (period <= 0)
	{
		rad += speed;
		while (rad >= 32768) rad -= 65536;
		while (rad < -32768) rad += 65536;
	}
	else
	{
		rad += _turretTime * speed;
		while (rad >= 32768) rad -= 65536;
		while (rad < -32768) rad += 65536;

		if (_turretFlip)
		{
			if (++_turretTime >= period)
				_turretFlip = false;
		}
		else
		{
			if (--_turretTime <= -period)
				_turretFlip = true;
		}
	}

	dir.SetValue(static_cast<short>(rad));
	pTechno->SecondaryFacing.SetCurrent(dir);
}
