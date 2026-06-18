#include "../BulletStatus.h"

void BulletStatus::OnUpdate_DestroySelf()
{
	if (DestroySelf->AmIDead())
	{
		// 啊我死了
		bool harmless = DestroySelf->Data.Peaceful;
		TakeDamage(0, true, harmless, false);
	}
}
