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
		// Force 模式：直接 SetLocation，彻底跳过引擎轨迹计算
		// 用 OnUpdate 快照的起始位置 + Vector 计算出的位移 = 引擎干预前的正确位置
		CoordStruct desiredPos;
		desiredPos.X = _vectorStartPos.X + _vectorResult.MoveDisp.X;
		desiredPos.Y = _vectorStartPos.Y + _vectorResult.MoveDisp.Y;
		desiredPos.Z = _vectorStartPos.Z + _vectorResult.MoveDisp.Z;
		pBullet->SetLocation(desiredPos);
		pBullet->SourceCoords = desiredPos;
		sourcePos = desiredPos;

		// SyncFacing=no：抛射体 Velocity 锁定到攻击目标方向（画弧时面朝目标）
		if (!_vectorResult.AllowRotateUnit)
		{
			CoordStruct targetCoords = pBullet->GetTargetCoords();
			pBullet->Velocity.X = targetCoords.X - desiredPos.X;
			pBullet->Velocity.Y = targetCoords.Y - desiredPos.Y;
			pBullet->Velocity.Z = targetCoords.Z - desiredPos.Z;
		}
	}
}
