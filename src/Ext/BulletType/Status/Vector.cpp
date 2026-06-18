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
		CoordStruct desiredPos = _vectorStartPos + _vectorResult.MoveDisp;
		pBullet->SetLocation(desiredPos);
		pBullet->SourceCoords = desiredPos;
		sourcePos = desiredPos;

		// SyncFacing=yes（默认）：Velocity 设为运动方向，弹体面朝移动方向
		// SyncFacing=no：不碰 Velocity，引擎开火时已指向目标
		if (_vectorResult.AllowRotateUnit && !_vectorResult.MoveDisp.IsEmpty())
		{
			double dx = static_cast<double>(_vectorResult.MoveDisp.X);
			double dy = static_cast<double>(_vectorResult.MoveDisp.Y);
			double dz = static_cast<double>(_vectorResult.MoveDisp.Z);
			double moveLen = std::sqrt(dx * dx + dy * dy + dz * dz);
			if (moveLen > 1e-6)
			{
				double speed = static_cast<double>(pBullet->Speed);
				if (speed <= 0.0)
					speed = static_cast<double>(pBullet->Velocity.Magnitude());
				if (speed > 0.0)
				{
					double scale = speed / moveLen;
					pBullet->Velocity.X = static_cast<int>(dx * scale);
					pBullet->Velocity.Y = static_cast<int>(dy * scale);
					pBullet->Velocity.Z = static_cast<int>(dz * scale);
				}
			}
		}
	}
}
