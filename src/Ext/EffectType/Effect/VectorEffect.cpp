#include "VectorEffect.h"

#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>
#include <Ext/Helper/Physics.h>
#include <Ext/Helper/Weapon.h>
#include <Ext/Helper/FLH.h>

#include <Ext/ObjectType/AttachEffect.h>
#include <Ext/EffectType/AttachEffectScript.h>
#include <Ext/BulletType/BulletStatus.h>


// Vector: Freeze / Circle / MoveTo / Speed / ReachTarget
// 基于 V1 VectorEffect.cpp 精简

void VectorEffect::OnStart()
{
	if (pTechno && pTechno->WhatAmI() == AbstractType::Building)
	{
		Deactivate();
		return;
	}

	_active = true;
	_elapsedFrames = 0;
	_moveFrame = 0;
	_currentAngle = 0.0;
	_prevCirclePos = pObject->GetCoords();
	_effectiveTimeStep = Data->TimeStep;

	_initialLocation = pObject->GetCoords();
	_totalDuration = AE->GetDuration();

	_randomTargetOffset.X = Random::RandomRanged(Data->TargetOffsetFMin, Data->TargetOffsetFMax);
	_randomTargetOffset.Y = Random::RandomRanged(Data->TargetOffsetLMin, Data->TargetOffsetLMax);
	_randomTargetOffset.Z = Random::RandomRanged(Data->TargetOffsetHMin, Data->TargetOffsetHMax);

	// --- 初始速度 ---
	_currentSpeed = 0.0;
	if (Data->InitialSpeed >= 0)
	{
		_currentSpeed = static_cast<double>(Data->InitialSpeed);
	}
	else if (pTechno)
	{
		TechnoTypeClass* pType = pTechno->GetTechnoType();
		if (GetLocoType(pTechno) == LocoType::Jumpjet)
			_currentSpeed = pType->JumpjetSpeed;
		else
			_currentSpeed = pType->Speed;
	}
	else if (pBullet)
	{
		_currentSpeed = pBullet->Speed;
	}
	// Speed 模式随机速度
	if (Data->RandomSpeedMax > Data->RandomSpeedMin)
	{
		_currentSpeed = Random::RandomRanged(Data->RandomSpeedMin, Data->RandomSpeedMax);
	}

	// --- Origin 初始化 ---
	switch (Data->Origin)
	{
	case VectorData::VectorOrigin::Target:
		if (Data->OriginNoUpdate)
		{
			if (pTechno && pTechno->Target)
				_initialOriginPos = pTechno->Target->GetCoords();
			else if (pBullet)
				_initialOriginPos = pBullet->TargetCoords;
		}
		break;

	case VectorData::VectorOrigin::Launcher:
		if (Data->OriginNoUpdate)
		{
			if (pBullet && pBullet->Owner)
				_initialOriginPos = pBullet->Owner->GetCoords();
			else if (pTechno)
				_initialOriginPos = pTechno->GetCoords();
		}
		if (pBullet)
			_pLauncher = pBullet->Owner;
		else if (pTechno)
			_pLauncher = pTechno;
		break;

	case VectorData::VectorOrigin::Source:
		if (Data->OriginNoUpdate)
		{
			if (AE && AE->pSource)
				_initialOriginPos = AE->pSource->GetCoords();
		}
		if (AE && AE->pSource)
			_pSource = AE->pSource;
		break;

	case VectorData::VectorOrigin::FLH:
		if (pTechno)
		{
			CoordStruct unitPos = pTechno->GetCoords();
			DirStruct unitFacing = pTechno->TurretFacing().Current();
			_initialOriginPos = GetFLHAbsoluteCoords(unitPos, Data->OriginFLH, unitFacing);
		}
		else if (pBullet && pBullet->Owner)
		{
			CoordStruct unitPos = pBullet->Owner->GetCoords();
			DirStruct unitFacing = pBullet->Owner->TurretFacing().Current();
			_initialOriginPos = GetFLHAbsoluteCoords(unitPos, Data->OriginFLH, unitFacing);
		}
		break;
	}

	// --- 锁定 FLH 旋转朝向（OnStart 时固定，避免炮塔追踪导致目标漂移） ---
	switch (Data->Origin)
	{
	case VectorData::VectorOrigin::Launcher:
	{
		// F 轴 = Launcher → 抛射体/单位 连线方向，Launcher 为原点
		if (pBullet && _pLauncher)
		{
			CoordStruct launcherPos = _pLauncher->GetCoords();
			CoordStruct objPos = pBullet->GetCoords();
			_facingRad = std::atan2(objPos.Y - launcherPos.Y, objPos.X - launcherPos.X);
		}
		else if (pTechno && _pLauncher)
		{
			CoordStruct launcherPos = _pLauncher->GetCoords();
			CoordStruct objPos = pTechno->GetCoords();
			_facingRad = std::atan2(objPos.Y - launcherPos.Y, objPos.X - launcherPos.X);
		}
		break;
	}

	case VectorData::VectorOrigin::Target:
	{
		// F 轴 = Target → 抛射体/单位 连线方向，Target 为原点
		if (pTechno && pTechno->Target)
		{
			CoordStruct targetPos = pTechno->Target->GetCoords();
			CoordStruct selfPos = pTechno->GetCoords();
			_facingRad = std::atan2(selfPos.Y - targetPos.Y, selfPos.X - targetPos.X);
		}
		else if (pBullet)
		{
			CoordStruct bulletPos = pBullet->GetCoords();
			CoordStruct targetPos = pBullet->TargetCoords;
			_facingRad = std::atan2(bulletPos.Y - targetPos.Y, bulletPos.X - targetPos.X);
		}
		break;
	}

	case VectorData::VectorOrigin::Source:
	{
		// F 轴 = Source → 抛射体/单位当前位置（仅 XY，不纳入 Z 以避免倾斜坐标系）
		if (pBullet && AE && AE->pSource)
		{
			CoordStruct srcPos = AE->pSource->GetCoords();
			CoordStruct objPos = pBullet->GetCoords();
			_facingRad = std::atan2(objPos.Y - srcPos.Y, objPos.X - srcPos.X);
		}
		else if (pTechno && AE && AE->pSource)
		{
			CoordStruct srcPos = AE->pSource->GetCoords();
			CoordStruct objPos = pTechno->GetCoords();
			_facingRad = std::atan2(objPos.Y - srcPos.Y, objPos.X - srcPos.X);
		}
		break;
	}

	default:
	{
		DirStruct facing;
		if (pTechno)
			facing = pTechno->TurretFacing().Current();
		else if (pBullet && pBullet->Owner)
			facing = pBullet->Owner->TurretFacing().Current();
		_facingRad = facing.GetRadian();
		break;
	}
	}
}

static double DegToRad(double deg) { return deg * M_PI / 180.0; }
static double RadToDeg(double rad) { return rad * 180.0 / M_PI; }

// FLH → 世界坐标旋转（Y 轴镜像，使用 OnStart 时锁定的朝向）
static CoordStruct RotateFLH(const CoordStruct& flh, double facingRad)
{
	double cosA = std::cos(facingRad);
	double sinA = std::sin(facingRad);
	return CoordStruct{
		static_cast<int>(flh.X * cosA - flh.Y * sinA),
		static_cast<int>(-(flh.X * sinA + flh.Y * cosA)),
		flh.Z
	};
}

VectorResult VectorEffect::GetVectorResult()
{
	VectorResult result;

	// Force 必须在闸门之前设，确保非运动帧也走 SetLocation（Freeze 等效）
	result.Force = Data->Force;
	result.AllowFallingDestroy = Data->AllowFallingDestroy;
	result.FallingDestroyHeight = Data->FallingDestroyHeight;

	// ========================================================================
	// Freeze
	// ========================================================================
	if (Data->Freeze)
	{
		result.Freeze = true;
		result.Force = true;  // 抛射体 Freeze 必须 Force，否则引擎检测"卡住"自爆
		if (result.FrozenPos.IsEmpty())
			result.FrozenPos = _initialLocation;
		return result;
	}

	// ========================================================================
	// TimeStep 闸门
	// ========================================================================
	if (!ShouldMoveThisFrame())
	{
		_moveFrame++;
		return result;
	}

	CoordStruct currentPos = pObject->GetCoords();

	// ========================================================================
	// 动态 F 轴：非 NoUpdate 时每帧根据当前坐标重新计算 FLH 朝向
	// ========================================================================
	double effectiveFacing = _facingRad;
	if (!Data->OriginNoUpdate)
	{
		switch (Data->Origin)
		{
		case VectorData::VectorOrigin::Source:
			if (_pSource && !IsDeadOrInvisible(_pSource))
			{
				CoordStruct srcPos = _pSource->GetCoords();
				effectiveFacing = std::atan2(currentPos.Y - srcPos.Y, currentPos.X - srcPos.X);
			}
			break;
		case VectorData::VectorOrigin::Target:
			if (pTechno && pTechno->Target)
			{
				CoordStruct targetPos = pTechno->Target->GetCoords();
				effectiveFacing = std::atan2(currentPos.Y - targetPos.Y, currentPos.X - targetPos.X);
			}
			else if (pBullet)
			{
				CoordStruct targetPos = pBullet->TargetCoords;
				effectiveFacing = std::atan2(currentPos.Y - targetPos.Y, currentPos.X - targetPos.X);
			}
			break;

		case VectorData::VectorOrigin::Launcher:
			if (_pLauncher && !IsDeadOrInvisible(_pLauncher))
			{
				CoordStruct launcherPos = _pLauncher->GetCoords();
				effectiveFacing = std::atan2(currentPos.Y - launcherPos.Y, currentPos.X - launcherPos.X);
			}
			break;
		}
	}

	// ========================================================================
	// Origin 坐标
	// ========================================================================
	CoordStruct originPos = currentPos;

	switch (Data->Origin)
	{
	case VectorData::VectorOrigin::Target:
		if (Data->OriginNoUpdate)
		{
			originPos = _initialOriginPos;
		}
		else if (pTechno && pTechno->Target)
		{
			originPos = pTechno->Target->GetCoords();
		}
		else if (pBullet)
		{
			// 优先取抛射体自身的实时目标（寻的弹会更新）
			if (pBullet->Target)
				originPos = pBullet->Target->GetCoords();
			// 其次取发射者的当前目标
			else if (pBullet->Owner && pBullet->Owner->Target)
				originPos = pBullet->Owner->Target->GetCoords();
			// 最后回退到自身位置（避免圆心锁死在初始坐标）
			else
				originPos = currentPos;
		}
		break;
	case VectorData::VectorOrigin::Launcher:
		originPos = Data->OriginNoUpdate ? _initialOriginPos :
			(_pLauncher && !IsDeadOrInvisible(_pLauncher) ? _pLauncher->GetCoords() : currentPos);
		break;
	case VectorData::VectorOrigin::Source:
		originPos = Data->OriginNoUpdate ? _initialOriginPos :
			(_pSource && !IsDeadOrInvisible(_pSource) ? _pSource->GetCoords() : currentPos);
		break;
	case VectorData::VectorOrigin::FLH:
		originPos = _initialOriginPos;
		break;
	}

	// ========================================================================
	// 模式 C: Circle（独立圆周，圆心=Origin，三选二参数）
	// ========================================================================
	bool hasCircle = Data->CircleRadius > 0 || Data->CircleSpeed > 0 || Data->CircleAnglePerStep > 0.0
		|| (Data->CircleRandomRadiusMax > Data->CircleRandomRadiusMin)
		|| (Data->CircleRandomAngleMax > Data->CircleRandomAngleMin);
	if (hasCircle)
	{
		// 三选二：缺半径用当前XY距离，缺速度用半径×角速度，缺角速度用速度/半径
		double calcRadius = static_cast<double>(Data->CircleRadius);
		if (calcRadius <= 0.0)
		{
			double tdx = currentPos.X - originPos.X;
			double tdy = currentPos.Y - originPos.Y;
			calcRadius = std::sqrt(tdx * tdx + tdy * tdy);
		}

		// 动态线速：首帧初始化，每帧叠加加速度
		if (_elapsedFrames == 0)
			_currentCircleSpeed = static_cast<double>(Data->CircleSpeed);
		_currentCircleSpeed += Data->CircleSpeedAcceleration;
		if (Data->CircleMaxSpeed != 0 && _currentCircleSpeed > Data->CircleMaxSpeed)
			_currentCircleSpeed = static_cast<double>(Data->CircleMaxSpeed);
		if (Data->CircleMinSpeed != 0 && _currentCircleSpeed < Data->CircleMinSpeed)
			_currentCircleSpeed = static_cast<double>(Data->CircleMinSpeed);

		// 角速度动态：首帧初始化，每帧叠加加速度
		if (_elapsedFrames == 0)
		{
			_currentCircleAngle = Data->CircleAnglePerStep;
			if (Data->CircleRandomAngleMax > Data->CircleRandomAngleMin)
				_currentCircleAngle = Data->CircleRandomAngleMin + (Data->CircleRandomAngleMax - Data->CircleRandomAngleMin) * Random::RandomDouble();
		}
		_currentCircleAngle += Data->CircleAngleAcceleration;
		if (Data->CircleMaxAngle != 0.0 && _currentCircleAngle > Data->CircleMaxAngle)
			_currentCircleAngle = Data->CircleMaxAngle;
		if (Data->CircleMinAngle != 0.0 && _currentCircleAngle < Data->CircleMinAngle)
			_currentCircleAngle = Data->CircleMinAngle;

		double speed = _currentCircleSpeed;
		double angleStep = _currentCircleAngle;

		if (speed <= 0.0 && angleStep > 0.0)
			speed = calcRadius * DegToRad(angleStep);
		else if (angleStep <= 0.0 && speed > 0.0)
			angleStep = RadToDeg(speed / calcRadius);

		// 旋转当前方向向量并按目标半径缩放
		double dx = static_cast<double>(currentPos.X - originPos.X);
		double dy = static_cast<double>(currentPos.Y - originPos.Y);
		double currentDist = std::sqrt(dx * dx + dy * dy);
		if (currentDist < 1.0) currentDist = 1.0;

		// 动态半径：首帧初始化，每帧叠加增长率
		if (_elapsedFrames == 0)
		{
			_currentCircleRadius = static_cast<double>(Data->CircleRadius);
			if (_currentCircleRadius <= 0.0)
				_currentCircleRadius = currentDist;
			if (Data->CircleRandomRadiusMax > Data->CircleRandomRadiusMin)
				_currentCircleRadius = Random::RandomRanged(Data->CircleRandomRadiusMin, Data->CircleRandomRadiusMax);
		}
		_currentCircleRadius += Data->CircleRadiusGrow;

		double targetRadius = _currentCircleRadius;
		// 钳位
		if (Data->CircleMaxRadius > 0 && targetRadius > Data->CircleMaxRadius)
			targetRadius = static_cast<double>(Data->CircleMaxRadius);
		if (Data->CircleMinRadius > 0 && targetRadius < Data->CircleMinRadius)
			targetRadius = static_cast<double>(Data->CircleMinRadius);

		double rad = DegToRad(angleStep);
		double cosA = std::cos(rad), sinA = std::sin(rad);

		// 当前方向归一化 × 目标半径 → 旋转 → 新世界坐标
		double ndx = (dx / currentDist * targetRadius);
		double ndy = (dy / currentDist * targetRadius);
		double rx = ndx * cosA - ndy * sinA;
		double ry = ndx * sinA + ndy * cosA;

		result.MoveDisp.X = originPos.X + static_cast<int>(rx) - currentPos.X;
		result.MoveDisp.Y = originPos.Y + static_cast<int>(ry) - currentPos.Y;
		result.MoveDisp.Z = 0;
		result.Force = true;

		// 到达边界时结束 AE
		if (Data->CircleEndOnMaxRadius && Data->CircleMaxRadius > 0
			&& _currentCircleRadius >= Data->CircleMaxRadius)
		{
			Deactivate();
		}
		if (Data->CircleEndOnMinRadius && Data->CircleMinRadius > 0
			&& _currentCircleRadius <= Data->CircleMinRadius)
		{
			Deactivate();
		}

		AdvanceFrame();
		return result;
	}

	// ========================================================================
	// 模式 1: MoveTo（纯 FLH 位移 + GrowRate，每帧动态 FLH 朝向）
	// ========================================================================
	if (!Data->MoveTo.IsEmpty())
	{
		double moveFacing;
		if (Data->AnglePerStep != 0.0)
		{
			if (_elapsedFrames == 0)
				_currentAngle = 0.0;
			_currentAngle += Data->AnglePerStep;
			moveFacing = effectiveFacing + DegToRad(_currentAngle);
		}
		else if (pTechno)
			moveFacing = pTechno->TurretFacing().Current().GetRadian();
		else if (pBullet)
			moveFacing = std::atan2(pBullet->Velocity.X, pBullet->Velocity.Y);
		else
			moveFacing = _facingRad;

		CoordStruct moveFlh;
		moveFlh.X = Data->MoveTo.X + static_cast<int>(Data->GrowRate.X * _elapsedFrames);
		moveFlh.Y = Data->MoveTo.Y + static_cast<int>(Data->GrowRate.Y * _elapsedFrames);
		moveFlh.Z = Data->MoveTo.Z + static_cast<int>(Data->GrowRate.Z * _elapsedFrames);

		result.MoveDisp = RotateFLH(moveFlh, moveFacing);
		result.Force = true;

		AdvanceFrame();
		return result;
	}

	// ========================================================================
	// 以下为 TargetFLH 相关模式
	// ========================================================================

	// --- 目标世界坐标 ---
	CoordStruct frameTargetFlh;
	frameTargetFlh.X = Data->TargetFLH.X + _randomTargetOffset.X;
	frameTargetFlh.Y = Data->TargetFLH.Y + _randomTargetOffset.Y;
	frameTargetFlh.Z = Data->TargetFLH.Z + _randomTargetOffset.Z;

	CoordStruct frameTargetDisp = RotateFLH(frameTargetFlh, effectiveFacing);
	CoordStruct frameTarget;
	frameTarget.X = originPos.X + frameTargetDisp.X;
	frameTarget.Y = originPos.Y + frameTargetDisp.Y;
	frameTarget.Z = originPos.Z + frameTargetDisp.Z;

	// --- 方向向量 ---
	CoordStruct dirVec;
	dirVec.X = frameTarget.X - currentPos.X;
	dirVec.Y = frameTarget.Y - currentPos.Y;
	dirVec.Z = frameTarget.Z - currentPos.Z;
	double dirLen = std::sqrt(static_cast<double>(dirVec.X * dirVec.X + dirVec.Y * dirVec.Y + dirVec.Z * dirVec.Z));

	CoordStruct resultDisp{ 0, 0, 0 };

	// ========================================================================
	// 模式 2: ReachTarget（剩余帧数强制到达，每帧重算以支持目标移动）
	// ========================================================================
	if (Data->ReachTarget && _totalDuration > 0)
	{
		int remainingFrames = _totalDuration - _elapsedFrames;
		if (remainingFrames <= 0)
		{
			// 已超时，直接到达目标
			resultDisp.X = frameTarget.X - currentPos.X;
			resultDisp.Y = frameTarget.Y - currentPos.Y;
			resultDisp.Z = frameTarget.Z - currentPos.Z;
		}
		else if (dirLen > 1e-6)
		{
			double adjustedSpeed = dirLen / remainingFrames;
			resultDisp.X = static_cast<int>(dirVec.X / dirLen * adjustedSpeed);
			resultDisp.Y = static_cast<int>(dirVec.Y / dirLen * adjustedSpeed);
			resultDisp.Z = static_cast<int>(dirVec.Z / dirLen * adjustedSpeed);

			// 抛物线弧高修正 Z
			if (Data->ArcHeight != 0)
			{
				double t = static_cast<double>(_elapsedFrames) / _totalDuration;
				double tNext = static_cast<double>(_elapsedFrames + 1) / _totalDuration;
				double z0 = _initialLocation.Z;
				double z1 = frameTarget.Z;
				double h = Data->ArcHeight;
				double zCur = z0 + (z1 - z0) * t + 4.0 * h * t * (1.0 - t);
				double zNext = z0 + (z1 - z0) * tNext + 4.0 * h * tNext * (1.0 - tNext);
				resultDisp.Z = static_cast<int>(zNext - zCur);
			}
		}
	}
	// ========================================================================
	// 模式 3: Speed（直线追踪 + 加速度）
	// ========================================================================
	else if (dirLen > 1e-6)
	{
		double speed = _currentSpeed;

		// 加速度
		if (Data->Acceleration != 0)
		{
			speed += Data->Acceleration * _elapsedFrames;
		}

		// 钳位
		if (Data->MinSpeed >= 0 && speed < Data->MinSpeed)
			speed = static_cast<double>(Data->MinSpeed);
		if (Data->MaxSpeed >= 0 && speed > Data->MaxSpeed)
			speed = static_cast<double>(Data->MaxSpeed);

		if (speed <= 0.0) speed = 1.0;

		resultDisp.X = static_cast<int>(dirVec.X / dirLen * speed);
		resultDisp.Y = static_cast<int>(dirVec.Y / dirLen * speed);
		resultDisp.Z = static_cast<int>(dirVec.Z / dirLen * speed);
	}

	result.MoveDisp = resultDisp;

	AdvanceFrame();
	return result;
}
