#include "PumpState.h"

#include <JumpjetLocomotionClass.h>

#include <Utilities/Debug.h>
#include <Ext/Helper/FLH.h>
#include <Ext/Helper/MathEx.h>
#include <Ext/Helper/Physics.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Weapon.h>

#include <Extension/WarheadTypeExt.h>

#include <Ext/TechnoType/TechnoStatus.h>
#include <Ext/BulletType/BulletStatus.h>

void PumpState::SetupPump()
{
	CoordStruct sourcePos = pTechno->GetCoords();
	// 计算初速度
	double gravity = Data.Gravity <= 0 ? RulesClass::Instance->Gravity : Data.Gravity;
	double straightDistance = 0;
	double realSpeed = 0;
	BulletVelocity velocity = BulletVelocity::Empty;
	CoordStruct targetPos = CoordStruct::Empty;
	CellClass* pTargetCell = nullptr;
	if (AEWarheadLocation.IsEmpty() || AEWarheadLocation == sourcePos)
	{
		targetPos = sourcePos;
		// 原地起跳，但不精确，随机落点，作为目的地，绘制抛物线
		if (Data.Inaccurate)
		{
			targetPos = sourcePos + GetInaccurateOffset(Data.ScatterMin, Data.ScatterMax);
		}
		if (sourcePos == targetPos)
		{
			// 随机过还是原地起跳，啧啧
			velocity = { 0, 0, gravity };
			// WWSB，计算滞空时间是用距离，如果直上直下，距离是跳跃高度
			straightDistance = Data.Range * Unsorted::LeptonsPerCell;
			realSpeed = std::sqrt(straightDistance * gravity * 1.2);
			if (Data.Lobber)
			{
				realSpeed = (int)(realSpeed * 0.5);
			}
		}
		else
		{
			velocity = GetBulletArcingVelocity(sourcePos, targetPos, 0, gravity, Data.Lobber, false, 0, 0, (int)gravity, straightDistance, realSpeed, pTargetCell);
		}
	}
	else
	{
		// 目的地是从发力点到当前位置的沿线距离后的一点
		int range = Data.Range * Unsorted::LeptonsPerCell; // 影响的范围
		int dist = (int)sourcePos.DistanceFrom(AEWarheadLocation); // 单位和爆心的距离
		int forward = Data.PowerBySelf ? range : range - dist;
		if (forward > 0)
		{
			// 单位在爆炸范围内
			targetPos = GetForwardCoords(AEWarheadLocation, sourcePos, dist + forward);
			int zOffset = (int)gravity;
			velocity = GetBulletArcingVelocity(sourcePos, targetPos, 0, gravity, Data.Lobber, Data.Inaccurate, Data.ScatterMin, Data.ScatterMax, zOffset, straightDistance, realSpeed, pTargetCell);
		}
	}
	// 步兵飞行姿态
	_flySequence = Data.InfSequence;
	// 生效后开始跳
	if (ActionJump(velocity, (int)gravity, straightDistance))
	{
		// 清空所有目标和任务
		ClearAllTarget(pTechno);
		pTechno->ForceMission(Mission::None);
	}
}

bool PumpState::Jump(CoordStruct targetPos, bool isLobber, Sequence flySequence, bool isHumanCannon)
{
	CoordStruct sourcePos = pTechno->GetCoords();
	int gravity = RulesClass::Instance->Gravity;
	// 计算初速度
	double straightDistance = 0;
	double realSpeed = 0;
	BulletVelocity velocity = GetBulletArcingVelocity(sourcePos, targetPos, 0, gravity, isLobber, gravity, straightDistance, realSpeed);
	// 跳
	if (straightDistance > Unsorted::LeptonsPerCell && ActionJump(velocity, gravity, straightDistance))
	{
		_isHumanCannon = isHumanCannon;
		if (_isHumanCannon)
		{
			// 计算飞行时间
			int frame = (int)(straightDistance / realSpeed);
			_flyTimer.Start(frame);
			// 记住目标落点：抛物线积分有误差（提前到期/过冲），
			// 收尾时把水平位置精确拉回落点，避免落到相邻的不可通行格摔死
			_jumpTarget = targetPos;
			// 诊断日志（排查飞行收尾时取消注释）：
			// Debug::Log("Pump Jump: frame=%d src{%d,%d} tgt{%d,%d}\n",
			// 	frame, sourcePos.X / 256, sourcePos.Y / 256, targetPos.X / 256, targetPos.Y / 256);
		}
		_flySequence = flySequence;
		// 从占据的格子中移除自己
		pTechno->UnmarkAllOccupationBits(sourcePos);
		FootClass* pFoot = abstract_cast<FootClass*, true>(pTechno);
		// 停止移动
		ForceStopMoving(pFoot);
		// 调整朝向飞行的方向
		if (sourcePos.X != targetPos.X || sourcePos.Y != targetPos.Y)
		{
			DirStruct facingDir = Point2Dir(sourcePos, targetPos);
			pTechno->PrimaryFacing.SetDesired(facingDir);
			if (JumpjetLocomotionClass* jjLoco = dynamic_cast<JumpjetLocomotionClass*>(pFoot->Locomotor.get()))
			{
				// JJ朝向是单独的Facing
				jjLoco->LocomotionFacing.SetDesired(facingDir);
			}
			else if (IsAircraft())
			{
				// 飞机使用的炮塔的Facing
				pTechno->SecondaryFacing.SetDesired(facingDir);
			}
		}
		return true;
	}
	return false;
}

bool PumpState::ActionJump(BulletVelocity velocity, int gravity, double straightDistance)
{
	if (!velocity.IsEmpty())
	{
		_velocity = velocity;
		_gravity = gravity;
		TechnoStatus* status = dynamic_cast<TechnoStatus*>(_parent);
		status->Jumping = true;
		pTechno->IsFallingDown = false; // 强设为false
		return true;
	}
	return false;
}

void PumpState::CancelJump()
{
	TechnoStatus* status = dynamic_cast<TechnoStatus*>(_parent);
	status->Jumping = false;
	if (!status->CaptureByBlackHole && !IsDeadOrInvisible(pTechno))
	{
		// 人间大炮：先把水平位置精确修正到目标落点，再开始下落。
		// 不修的话，抛物线积分提前到期/过冲会让乘客从半路摔下，
		// 落到计算落点旁边的不可通行格（建筑/水面）直接摔死，
		// 死后预占被释放、Content 也清空，后续乘客又选中同一格 → 连锁堆叠。
		if (_isHumanCannon && !_jumpTarget.IsEmpty())
		{
			CoordStruct pos = pTechno->GetCoords();
			// 诊断日志（排查落地位置偏移时取消注释）：
			// Debug::Log("Pump CancelJump: pos{%d,%d}->jumpTarget{%d,%d}\n",
			// 	pos.X / 256, pos.Y / 256, _jumpTarget.X / 256, _jumpTarget.Y / 256);
			pos.X = _jumpTarget.X;
			pos.Y = _jumpTarget.Y;
			pTechno->UpdatePlacement(PlacementType::Remove);
			pTechno->SetLocation(pos);
			pTechno->UpdatePlacement(PlacementType::Put);
		}
		FallingExceptAircraft(pTechno, 0, _isHumanCannon);
	}
	End();
}

void PumpState::OnStart()
{
	TechnoStatus* status = dynamic_cast<TechnoStatus*>(_parent);
	AttachEffect* aem = nullptr;
	if (IsBuilding()
		|| !Data.Enable
		|| (status && status->AmIStand())
		|| !Data.CanAffectType(pTechno)
		|| !Data.CanAffectHouse(pAEHouse, pTechno->Owner)
		|| !TryGetAEManager<TechnoExt>(pTechno, aem) || !aem->IsOnMark(Data))
	{
		End();
		return;
	}
	_canJump = Data.Enable;
	if (_canJump)
	{
		SetupPump();
	}
}

void PumpState::OnEnd()
{
	_canJump = false;

	_gravity = RulesClass::Instance->Gravity;
	_velocity = BulletVelocity::Empty;

	_isHumanCannon = false;
	_flyTimer.Stop();

	// 解除预占用
	TechnoExt::HumanConnonPreOcc.ReleaseOccupancy(pTechno);
}

void PumpState::OnUpdate()
{
	if (!IsBuilding() && !pTechno->IsFallingDown)
	{
		if (_canJump && IfReset())
		{
			// 重新激活跳跃
			SetupPump();
		}
		// 正在跳跃，计算新的位置并移动
		TechnoStatus* status = dynamic_cast<TechnoStatus*>(_parent);
		if (status && status->Jumping)
		{
			if (status->CaptureByBlackHole || (_isHumanCannon && _flyTimer.Expired()))
			{
				CancelJump();
				return;
			}
			// 咱蹦高了
			CoordStruct sourcePos = pTechno->GetCoords();
			// 从占据的格子中移除自己
			pTechno->UnmarkAllOccupationBits(sourcePos);
			FootClass* pFoot = abstract_cast<FootClass*, true>(pTechno);
			// 停止移动
			ForceStopMoving(pFoot);
			// 初速度削减重力，下一个坐标位置
			_velocity.Z -= _gravity;
			CoordStruct nextPos = sourcePos + ToCoordStruct(_velocity);
			if (nextPos.IsEmpty())
			{
				// 没算出速度
				CancelJump();
				return;
			}
			CoordStruct nextCellPos = CoordStruct::Empty;
			bool onBridge = pTechno->OnBridge;
			PassError passError = CanMoveTo(sourcePos, nextPos, false, nextCellPos, onBridge);
			switch (passError)
			{
			case PassError::HITWALL:
			case PassError::HITBUILDING:
			case PassError::UPBRIDEG:
				// 反弹
				_velocity.X *= -1;
				_velocity.Y *= -1;
				nextPos = sourcePos + ToCoordStruct(_velocity);
				passError = CanMoveTo(sourcePos, nextPos, false, nextCellPos, onBridge);
				break;
			case PassError::UNDERGROUND:
			case PassError::DOWNBRIDGE:
				// 卡在地表
				nextPos = nextCellPos;
				break;
			}
			// 被黑洞吸走
			pTechno->UpdatePlacement(PlacementType::Remove);
			// 是否在桥上
			pTechno->OnBridge = onBridge;
			pTechno->SetLocation(nextPos);
			pTechno->UpdatePlacement(PlacementType::Put);
			// 移除黑幕
			MapClass::Instance->RevealArea2(&nextPos, pTechno->LastSightRange, pTechno->Owner, false, false, false, true, 0);
			MapClass::Instance->RevealArea2(&nextPos, pTechno->LastSightRange, pTechno->Owner, false, false, false, true, 1);
			if (IsInfantry())
			{
				abstract_cast<InfantryClass*, true>(pTechno)->PlayAnim(_flySequence);
			}
			switch (passError)
			{
			case PassError::UNDERGROUND:
			case PassError::HITWALL:
			case PassError::HITBUILDING:
			case PassError::DOWNBRIDGE:
				// 反弹后仍然触底或者撞悬崖
				// 掉落地面
				CancelJump();
				break;
			}
		}
	}
}
