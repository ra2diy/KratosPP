#include "../BulletStatus.h"

#include <Ext/Helper/FLH.h>
#include <Ext/Helper/Physics.h>
#include <Ext/Helper/Weapon.h>
#include <Ext/Helper/Scripts.h>

void BulletStatus::OnUpdate_Vector()
{
	// 被黑洞捕获时，停止 Vector 执行
	if (CaptureByBlackHole)
		return;

	// 记录起始位置（Force 模式用）
	_vectorStartPos = pBullet->GetCoords();

	_vectorResult = AEManager()->MarginVectorOffset();
	VectorForced = _vectorResult.Force;

	// Force 模式：不设 Velocity，位置由 OnUpdateEnd 的 SetLocation 接管
	// 无论 ROT=1 还是 ROT>1，引擎轨迹计算都会被 SetLocation 覆盖
}

void BulletStatus::OnUpdateEnd_Vector(CoordStruct& sourcePos)
{
	// 被黑洞捕获时，停止 Vector 执行
	if (CaptureByBlackHole)
		return;

	// Freeze 优先：暴力锁定位置（不需要 VectorForced）
	if (_vectorResult.Freeze && !_vectorResult.FrozenPos.IsEmpty())
	{
		pBullet->SetLocation(_vectorResult.FrozenPos);
		pBullet->SourceCoords = _vectorResult.FrozenPos;
		sourcePos = _vectorResult.FrozenPos;
		return;
	}

	if (VectorForced)
	{
		// Force=yes：暴力挪移，纯 Vector 控制位置
		CoordStruct desiredPos = _vectorStartPos + _vectorResult.MoveDisp;
		pBullet->SetLocation(desiredPos);
		pBullet->SourceCoords = desiredPos;
		sourcePos = desiredPos;

		// 成熟机制，别乱动：弹体 Force 挪移纯靠 SetLocation
		// SyncFacing=yes：黑洞同款 GetBulletVelocity，弹体面朝移动方向
		// SyncFacing=no：不碰 Velocity，引擎默认指向攻击目标
		if (_vectorResult.AllowRotateUnit && !_vectorResult.MoveDisp.IsEmpty())
		{
			pBullet->Velocity = GetBulletVelocity(_vectorStartPos, desiredPos);
		}
	}
	else if (!_vectorResult.MoveDisp.IsEmpty())
	{
		// Force=no：引擎轨迹 + Vector 位移叠加，原版引擎也介入
		CoordStruct enginePos = pBullet->GetCoords();
		enginePos.X += _vectorResult.MoveDisp.X;
		enginePos.Y += _vectorResult.MoveDisp.Y;
		enginePos.Z += _vectorResult.MoveDisp.Z;
		pBullet->SetLocation(enginePos);
		pBullet->SourceCoords = enginePos;
		sourcePos = enginePos;
	}
}
