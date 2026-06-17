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

	// --- 基础参考系（始终赋值，供 OriginOrigin 等使用） ---
	if (pBullet)
	{
		_pLauncher = pBullet->Owner;
		if (AE && AE->pSource) _pSource = AE->pSource;
	}
	else if (pTechno)
	{
		_pLauncher = pTechno;
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
			else
				_initialOriginPos = pObject->GetCoords();
		}
		break;

	case VectorData::VectorOrigin::Launcher:
		if (Data->OriginNoUpdate)
		{
			if (pBullet && pBullet->Owner)
				_initialOriginPos = pBullet->Owner->GetCoords();
			else if (pTechno)
				_initialOriginPos = pTechno->GetCoords();
			else
				_initialOriginPos = pObject->GetCoords();
		}
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

	case VectorData::VectorOrigin::Self:
		if (pBullet)
		{
			double bulletRad = std::atan2(pBullet->Velocity.X, pBullet->Velocity.Y);
			DirStruct bulletFacing;
			bulletFacing.SetValue(static_cast<short>(bulletRad * 32768.0 / M_PI));
			_initialOriginPos = GetFLHAbsoluteCoords(pBullet->GetCoords(), Data->OriginFLH, bulletFacing);
		}
		else if (pTechno)
		{
			CoordStruct unitPos = pTechno->GetCoords();
			DirStruct unitFacing = pTechno->TurretFacing().Current();
			_initialOriginPos = GetFLHAbsoluteCoords(unitPos, Data->OriginFLH, unitFacing);
		}
		break;
	}

	// NormalVector 设为后，F 轴完全由向量决定，Origin 只控制原点位置
	bool hasNormal = !Data->NormalVector.IsEmpty()
		|| Data->NormalRandomF.Y > Data->NormalRandomF.X
		|| Data->NormalRandomL.Y > Data->NormalRandomL.X
		|| Data->NormalRandomH.Y > Data->NormalRandomH.X;

	// --- 锁定 FLH 旋转朝向（OnStart 时固定） ---
	// NormalVector 使用 FLH 坐标系：F=南北(X→世界Y)，L=东西(Y→世界X)，H=Z
	if (hasNormal)
	{
		double fwY = static_cast<double>(Data->NormalVector.X);  // F → 世界 Y（北）
		double fwX = static_cast<double>(Data->NormalVector.Y);  // L → 世界 X（东）
		double fwZ = static_cast<double>(Data->NormalVector.Z);  // H → Z

		// 随机分量
		if (Data->NormalRandomF.Y > Data->NormalRandomF.X)
			fwY = Random::RandomRanged(Data->NormalRandomF.X, Data->NormalRandomF.Y);
		if (Data->NormalRandomL.Y > Data->NormalRandomL.X)
			fwX = Random::RandomRanged(Data->NormalRandomL.X, Data->NormalRandomL.Y);
		if (Data->NormalRandomH.Y > Data->NormalRandomH.X)
			fwZ = Random::RandomRanged(Data->NormalRandomH.X, Data->NormalRandomH.Y);

		double lenXY = std::sqrt(fwX * fwX + fwY * fwY);
		_facingRad = lenXY > 1e-6 ? std::atan2(fwY, fwX) : 0.0;
		_tiltRad = lenXY > 1e-6 ? std::atan2(fwZ, lenXY) : (fwZ > 0 ? M_PI / 2.0 : -M_PI / 2.0);
	}
	else
	{
		_tiltRad = 0.0;
	}

	// 法线旋转角速度解析（常数优先，否则随机）
	auto resolveAngleStep = [](double perStep, double m1, double M1, double m2, double M2) {
		if (perStep != 0.0) return perStep;
		if (M1 <= m1 && M2 <= m2) return 0.0;
		if (M2 > m2 && Random::RandomRanged(0, 1))
			return m2 + (M2 - m2) * Random::RandomDouble();
		return M1 > m1 ? m1 + (M1 - m1) * Random::RandomDouble() : 0.0;
	};
	_normalStepF = resolveAngleStep(Data->NormalFAnglePerStep, Data->NormalFAngleRMin, Data->NormalFAngleRMax, Data->NormalFAngleRMin2, Data->NormalFAngleRMax2);
	_normalStepL = resolveAngleStep(Data->NormalLAnglePerStep, Data->NormalLAngleRMin, Data->NormalLAngleRMax, Data->NormalLAngleRMin2, Data->NormalLAngleRMax2);
	_normalStepH = resolveAngleStep(Data->NormalHAnglePerStep, Data->NormalHAngleRMin, Data->NormalHAngleRMax, Data->NormalHAngleRMin2, Data->NormalHAngleRMax2);

	switch (Data->Origin)
	{
	case VectorData::VectorOrigin::Launcher:
	{
		if (!hasNormal)
		{
			TechnoClass* pLauncherTechno = abstract_cast<TechnoClass*>(_pLauncher);
			if (pLauncherTechno)
				_facingRad = pLauncherTechno->TurretFacing().Current().GetRadian();
		}
		break;
	}

	case VectorData::VectorOrigin::Target:
	{
		if (!hasNormal)
		{
			double dx = 0, dy = 0, dz = 0;
			if (pTechno && pTechno->Target)
			{
				CoordStruct targetPos = pTechno->Target->GetCoords();
				CoordStruct selfPos = pTechno->GetCoords();
				dx = selfPos.X - targetPos.X;
				dy = selfPos.Y - targetPos.Y;
				dz = selfPos.Z - targetPos.Z;
			}
			else if (pBullet)
			{
				CoordStruct bulletPos = pBullet->GetCoords();
				CoordStruct targetPos = pBullet->TargetCoords;
				dx = bulletPos.X - targetPos.X;
				dy = bulletPos.Y - targetPos.Y;
				dz = bulletPos.Z - targetPos.Z;
			}
			_facingRad = std::atan2(dy, dx);
			double lenXY = std::sqrt(dx * dx + dy * dy);
			_tiltRad = (lenXY > 1e-6 && Data->AllowedTilt) ? std::atan2(dz, lenXY) : 0.0;
		}
		break;
	}

	case VectorData::VectorOrigin::Source:
	{
		if (!hasNormal)
		{
			double dx = 0, dy = 0, dz = 0;
			if (pBullet && AE && AE->pSource)
			{
				CoordStruct srcPos = AE->pSource->GetCoords();
				CoordStruct objPos = pBullet->GetCoords();
				dx = objPos.X - srcPos.X; dy = objPos.Y - srcPos.Y; dz = objPos.Z - srcPos.Z;
			}
			else if (pTechno && AE && AE->pSource)
			{
				CoordStruct srcPos = AE->pSource->GetCoords();
				CoordStruct objPos = pTechno->GetCoords();
				dx = objPos.X - srcPos.X; dy = objPos.Y - srcPos.Y; dz = objPos.Z - srcPos.Z;
			}
			_facingRad = std::atan2(dy, dx);
			double lenXY = std::sqrt(dx * dx + dy * dy);
			_tiltRad = (lenXY > 1e-6 && Data->AllowedTilt) ? std::atan2(dz, lenXY) : 0.0;
		}
		break;
	}

	default: // FLH：抛射体自身朝向
	{
		if (pBullet)
			_facingRad = std::atan2(pBullet->Velocity.X, pBullet->Velocity.Y);
		else if (pTechno)
			_facingRad = pTechno->TurretFacing().Current().GetRadian();
		break;
	}
	}
}

static double DegToRad(double deg) { return deg * M_PI / 180.0; }
static double RadToDeg(double rad) { return rad * 180.0 / M_PI; }

// FLH → 世界坐标旋转（3D，支持倾斜坐标系）
// facingRad=XY 方位角，tiltRad=俯仰角（0=水平，正=向上）
// 世界旋转（无 FLH 坐标系的 Y 镜像），用于 TargetFLH 类标签
static CoordStruct WorldRotate(const CoordStruct& v, double facingRad, double tiltRad = 0.0)
{
	double cF = std::cos(facingRad), sF = std::sin(facingRad);
	double cT = std::cos(tiltRad), sT = std::sin(tiltRad);
	// F = (cF*cT, sF*cT, sT)   — 前向
	// L = (-sF, cF, 0)         — 左向
	// H = (-cF*sT, -sF*sT, cT) — 上向（F×L）
	// 注意：Y 不加负号，保持世界坐标系方向
	return CoordStruct{
		static_cast<int>(v.X * cF * cT + v.Y * (-sF) + v.Z * (-cF * sT)),
		static_cast<int>(v.X * sF * cT + v.Y * cF + v.Z * (-sF * sT)),
		static_cast<int>(v.X * sT + v.Z * cT)
	};
}

static CoordStruct RotateFLH(const CoordStruct& flh, double facingRad, double tiltRad = 0.0)
{
	double cosF = std::cos(facingRad), sinF = std::sin(facingRad);
	double cosT = std::cos(tiltRad), sinT = std::sin(tiltRad);
	// F = (cosF*cosT, sinF*cosT, sinT)   — 前向
	// L = (-sinF, cosF, 0)               — 左向（严格垂直 XY 面）
	// H = (-cosF*sinT, -sinF*sinT, cosT) — 上向（F×L）
	double fx = cosF * cosT, fy = sinF * cosT, fz = sinT;
	double lx = -sinF, ly = cosF;
	double hx = -cosF * sinT, hy = -sinF * sinT, hz = cosT;
	return CoordStruct{
		static_cast<int>(flh.X * fx + flh.Y * lx + flh.Z * hx),
		static_cast<int>(-(flh.X * fy + flh.Y * ly + flh.Z * hy)),
		static_cast<int>(flh.X * fz + flh.Z * hz)
	};
}

VectorResult VectorEffect::GetVectorResult()
{
	VectorResult result;

	// 首帧快照（仅一次，供弧高计算等）
	if (_elapsedFrames == 0)
		_initialLocation = pObject->GetCoords();

	// InitialDelay 期间 AE 存在但未启动，不施加任何位移
	if (!_started)
	{
		AdvanceFrame();
		return result;
	}

	// Force 必须在闸门之前设，确保非运动帧也走 SetLocation（Freeze 等效）
	result.Force = Data->Force;
	result.AllowFallingDestroy = Data->AllowFallingDestroy;
	result.FallingDestroyHeight = Data->FallingDestroyHeight;
	result.AllowRotateUnit = Data->SyncFacing;

	// DisabledFrames：首帧快照后冻结，不阻塞其他 AE，不计入运动时间
	if (_elapsedFrames < Data->DisabledFrames)
	{
		result.MoveDisp = { 0, 0, 0 };
		AdvanceFrame();
		return result;
	}

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
	_movementFrames++;

	CoordStruct currentPos = pObject->GetCoords();

	// ========================================================================
	// 动态 F 轴：非 NoUpdate 时每帧根据当前坐标重新计算 FLH 朝向
	// ========================================================================
	double effectiveFacing = _facingRad;
	double effectiveTilt = _tiltRad;

	// 法线旋转：每运动帧累加角速度（H=转向，L=俯仰）
	if (ShouldMoveThisFrame())
	{
		_normalRotH += _normalStepH;
		_normalRotL += _normalStepL;
	}
	effectiveFacing += DegToRad(_normalRotH);
	effectiveTilt += DegToRad(_normalRotL);
	bool hasNormal = !Data->NormalVector.IsEmpty()
		|| Data->NormalRandomF.Y > Data->NormalRandomF.X
		|| Data->NormalRandomL.Y > Data->NormalRandomL.X
		|| Data->NormalRandomH.Y > Data->NormalRandomH.X;
	if (!Data->OriginNoUpdate && !hasNormal)
	{
		switch (Data->Origin)
		{
		case VectorData::VectorOrigin::Source:
			if (_pSource && !IsDeadOrInvisible(_pSource))
			{
				CoordStruct srcPos = _pSource->GetCoords();
				double dx = currentPos.X - srcPos.X, dy = currentPos.Y - srcPos.Y, dz = currentPos.Z - srcPos.Z;
				effectiveFacing = std::atan2(dy, dx);
				double lenXY = std::sqrt(dx*dx + dy*dy);
				effectiveTilt = (lenXY > 1e-6 && Data->AllowedTilt) ? std::atan2(dz, lenXY) : 0.0;
			}
			break;
		case VectorData::VectorOrigin::Target:
		{
			// 目标有自身 facing（Techno）= 动态跟踪；无（地面/Cell）= 首帧锁定
			bool isGround = (pBullet && !abstract_cast<TechnoClass*>(pBullet->Target));
			if (isGround && _movementFrames > 1)
				break;
			CoordStruct targetPos;
			if (pBullet && pBullet->Target)
				targetPos = pBullet->Target->GetCoords();
			else if (pBullet)
				targetPos = pBullet->TargetCoords;
			else if (pTechno && pTechno->Target)
				targetPos = pTechno->Target->GetCoords();
			else
				break;
			double dx = currentPos.X - targetPos.X, dy = currentPos.Y - targetPos.Y, dz = currentPos.Z - targetPos.Z;
			effectiveFacing = std::atan2(dy, dx);
			double lenXY = std::sqrt(dx*dx + dy*dy);
			effectiveTilt = (lenXY > 1e-6 && Data->AllowedTilt) ? std::atan2(dz, lenXY) : 0.0;
		}
			break;

		case VectorData::VectorOrigin::Self:
			if (pTechno)
				effectiveFacing = (Data->OriginIsOnBody
					? pTechno->PrimaryFacing.Current().GetRadian()
					: pTechno->TurretFacing().Current().GetRadian());
			else if (pBullet)
				effectiveFacing = std::atan2(pBullet->Velocity.X, pBullet->Velocity.Y);
			break;

		case VectorData::VectorOrigin::Launcher:
			if (_pLauncher && !IsDeadOrInvisible(_pLauncher))
			{
				TechnoClass* pLauncherTechno = abstract_cast<TechnoClass*>(_pLauncher);
				if (pLauncherTechno)
					effectiveFacing = (Data->OriginIsOnBody
						? pLauncherTechno->PrimaryFacing.Current().GetRadian()
						: pLauncherTechno->TurretFacing().Current().GetRadian());
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
			originPos = _initialOriginPos;
		else if (pBullet && pBullet->Target)
			originPos = pBullet->Target->GetCoords(); // 单位：实时跟踪
		else if (pBullet)
			originPos = pBullet->TargetCoords;         // 地面：自动锁定
		else if (pTechno && pTechno->Target)
			originPos = pTechno->Target->GetCoords();
		else
			originPos = currentPos;
		break;
	case VectorData::VectorOrigin::Launcher:
		originPos = Data->OriginNoUpdate ? _initialOriginPos :
			(_pLauncher && !IsDeadOrInvisible(_pLauncher) ? _pLauncher->GetCoords() : currentPos);
		break;
	case VectorData::VectorOrigin::Source:
		originPos = Data->OriginNoUpdate ? _initialOriginPos :
			(_pSource && !IsDeadOrInvisible(_pSource) ? _pSource->GetCoords() : currentPos);
		break;
	case VectorData::VectorOrigin::Self:
		originPos = Data->OriginNoUpdate ? _initialOriginPos : currentPos;
		break;
	}

	// OriginFLH 偏移：对非 Self 模式生效
	// OriginIsOnWorld=yes 时用世界 FLH（朝北），否则用单位/弹体朝向
	{
		double flhFacing = Data->OriginIsOnWorld ? 0.0 : effectiveFacing;
		double flhTilt = Data->OriginIsOnWorld ? 0.0 : effectiveTilt;
		if (!Data->OriginFLH.IsEmpty() && Data->Origin != VectorData::VectorOrigin::Self)
			originPos = originPos + RotateFLH(Data->OriginFLH, flhFacing, flhTilt);
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
		{
			if (Data->CircleRandomAngleMax2 > Data->CircleRandomAngleMin2 && Random::RandomRanged(0, 1))
				_currentCircleAngle = Data->CircleRandomAngleMin2 + (Data->CircleRandomAngleMax2 - Data->CircleRandomAngleMin2) * Random::RandomDouble();
			else
				_currentCircleAngle = Data->CircleRandomAngleMin + (Data->CircleRandomAngleMax - Data->CircleRandomAngleMin) * Random::RandomDouble();
		}
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

		// 圆心 = Origin + CircleOrigin 偏移（世界坐标系）
		CoordStruct circleCenter = originPos;
		if (!Data->CircleOrigin.IsEmpty())
		{
			if (Data->AllowOriginTilt)
				circleCenter = originPos + RotateFLH(Data->CircleOrigin, effectiveFacing, effectiveTilt);
			else
				circleCenter = originPos + Data->CircleOrigin;
		}

		// 圆心移动：Vector.Origin.* 系统
		if (!Data->OriginMoveTo.IsEmpty() || !Data->OriginTargetFLH.IsEmpty()
			|| Data->OriginCircleRadius >= 0 || Data->OriginCircleSpeed > 0 || Data->OriginCircleAnglePerStep != 0)
		{
			// 基座：默认 originPos，OriginOrigin 可替换为独立参考系
			CoordStruct baseCenter = originPos;
			if (Data->OriginOrigin != VectorData::VectorOrigin::Self)
			{
				switch (Data->OriginOrigin)
				{
				case VectorData::VectorOrigin::Launcher:
					if (_pLauncher && !IsDeadOrInvisible(_pLauncher))
						baseCenter = _pLauncher->GetCoords();
					break;
				case VectorData::VectorOrigin::Target:
					if (pTechno && pTechno->Target)
						baseCenter = pTechno->Target->GetCoords();
					else if (pTechno)
					{
						FootClass* pFoot = abstract_cast<FootClass*>(pTechno);
						if (pFoot && pFoot->Destination)
							baseCenter = pFoot->Destination->GetCoords();
					}
					else if (pBullet && pBullet->Target)
						baseCenter = pBullet->Target->GetCoords();
					else if (pBullet && pBullet->Owner && pBullet->Owner->Target)
						baseCenter = pBullet->Owner->Target->GetCoords();
					else if (pBullet)
						baseCenter = pBullet->TargetCoords;
					break;
				case VectorData::VectorOrigin::Source:
					if (_pSource && !IsDeadOrInvisible(_pSource))
						baseCenter = _pSource->GetCoords();
					break;
				}
			}
			else if (!Data->OriginOriginFLH.IsEmpty())
			{
				baseCenter.X += Data->OriginOriginFLH.X;
				baseCenter.Y += Data->OriginOriginFLH.Y;
				baseCenter.Z += Data->OriginOriginFLH.Z;
			}

			// Origin.CircleOffset 世界偏移
			if (!Data->OriginCircleOffset.IsEmpty())
				baseCenter = baseCenter + Data->OriginCircleOffset;

			if (_elapsedFrames == 0)
			{
				// 初始偏移 = 圆心相对于基座的向量
				_originOffset = circleCenter - baseCenter;
				// Circle 初始化
				_originCircleRadius = Data->OriginCircleRadius;
				_originCircleSpeed = Data->OriginCircleSpeed;
				_originCircleAngle = 0.0; // 初始相位
				// 随机
				if (Data->OriginCircleRandomRadiusMax > Data->OriginCircleRandomRadiusMin)
					_originCircleRadius = Random::RandomRanged(Data->OriginCircleRandomRadiusMin, Data->OriginCircleRandomRadiusMax);
				if (Data->OriginCircleRandomAngleMax > Data->OriginCircleRandomAngleMin)
					_originCircleAngle = Data->OriginCircleRandomAngleMin + (Data->OriginCircleRandomAngleMax - Data->OriginCircleRandomAngleMin) * Random::RandomDouble();
				// Target 随机偏移
				_originTargetOffset.X = Random::RandomRanged(Data->OriginTargetOffsetFMin, Data->OriginTargetOffsetFMax);
				_originTargetOffset.Y = Random::RandomRanged(Data->OriginTargetOffsetLMin, Data->OriginTargetOffsetLMax);
				_originTargetOffset.Z = Random::RandomRanged(Data->OriginTargetOffsetHMin, Data->OriginTargetOffsetHMax);
				// Normal 初始化
				if (!Data->OriginNormalVector.IsEmpty())
				{
					double fy = Data->OriginNormalVector.X, fx = Data->OriginNormalVector.Y, fz = Data->OriginNormalVector.Z;
					if (Data->OriginNormalRandomF.Y > Data->OriginNormalRandomF.X) fy = Random::RandomRanged(Data->OriginNormalRandomF.X, Data->OriginNormalRandomF.Y);
					if (Data->OriginNormalRandomL.Y > Data->OriginNormalRandomL.X) fx = Random::RandomRanged(Data->OriginNormalRandomL.X, Data->OriginNormalRandomL.Y);
					if (Data->OriginNormalRandomH.Y > Data->OriginNormalRandomH.X) fz = Random::RandomRanged(Data->OriginNormalRandomH.X, Data->OriginNormalRandomH.Y);
					double len = std::sqrt(fx*fx+fy*fy);
					_originFacing = len>1e-6 ? std::atan2(fy,fx) : 0;
					_originTilt = len>1e-6 ? std::atan2(fz,len) : (fz>0?M_PI/2.0:-M_PI/2.0);
				}
				// Normal 角速度
				auto res = [](double ps, double m1,double M1,double m2,double M2){if(ps)return ps;if(M2>m2&&Random::RandomRanged(0,1))return m2+(M2-m2)*Random::RandomDouble();return M1>m1?m1+(M1-m1)*Random::RandomDouble():0.0;};
				_originNormalStepF = res(Data->OriginNormalFAnglePerStep, Data->OriginNormalFAngleRMin, Data->OriginNormalFAngleRMax, Data->OriginNormalFAngleRMin2, Data->OriginNormalFAngleRMax2);
				_originNormalStepL = res(Data->OriginNormalLAnglePerStep, Data->OriginNormalLAngleRMin, Data->OriginNormalLAngleRMax, Data->OriginNormalLAngleRMin2, Data->OriginNormalLAngleRMax2);
				_originNormalStepH = res(Data->OriginNormalHAnglePerStep, Data->OriginNormalHAngleRMin, Data->OriginNormalHAngleRMax, Data->OriginNormalHAngleRMin2, Data->OriginNormalHAngleRMax2);
				if (!Data->OriginNormalVector.IsEmpty() && !_originCircleRadius)
					_originCircleRadius = effectiveFacing > -1 ? 0 : -1;
				// 无 NormalVector 时从 Origin 参考系取 facing
				if (Data->OriginNormalVector.IsEmpty())
				{
					switch (Data->OriginOrigin)
					{
					case VectorData::VectorOrigin::Launcher:
						if (_pLauncher && !IsDeadOrInvisible(_pLauncher))
							_originFacing = abstract_cast<TechnoClass*>(_pLauncher)->TurretFacing().Current().GetRadian();
						else if (pTechno) _originFacing = pTechno->TurretFacing().Current().GetRadian();
						break;
					case VectorData::VectorOrigin::Target:
						if (pBullet) { auto tp = pBullet->TargetCoords; auto bp = pBullet->GetCoords(); _originFacing = std::atan2(bp.Y-tp.Y, bp.X-tp.X); }
						else if (pTechno && pTechno->Target) { auto tp = pTechno->Target->GetCoords(); auto sp = pTechno->GetCoords(); _originFacing = std::atan2(sp.Y-tp.Y, sp.X-tp.X); }
						break;
					case VectorData::VectorOrigin::Source:
						if (AE && AE->pSource) { auto sp = AE->pSource->GetCoords(); auto bp = pObject->GetCoords(); _originFacing = std::atan2(bp.Y-sp.Y, bp.X-sp.X); }
						break;
					default: // FLH
						if (!Data->OriginOriginFLH.IsEmpty())
						{
							double fy = Data->OriginOriginFLH.X, fx = Data->OriginOriginFLH.Y;
							_originFacing = std::atan2(fy, fx);
						}
						else if (pBullet) _originFacing = pBullet->Velocity.Magnitude() > 0 ? std::atan2(pBullet->Velocity.X, pBullet->Velocity.Y) : 0.0;
						else if (pTechno) _originFacing = pTechno->TurretFacing().Current().GetRadian();
						break;
					}
				}
			}

			// 每帧累加 Normal 旋转
			_originNormalRotF += _originNormalStepF;
			_originNormalRotL += _originNormalStepL;
			_originNormalRotH += _originNormalStepH;

			double oFacing = _originFacing + DegToRad(_originNormalRotH);
			double oTilt = _originTilt + DegToRad(_originNormalRotL);

			// 当前圆心绝对位置 = 基座 + 偏移
			CoordStruct originCenter = baseCenter + _originOffset;

			CoordStruct disp;
			if (!Data->OriginMoveTo.IsEmpty())
			{
				// MoveTo 模式：GrowRate 随帧数线性增长
				_originAngle += Data->OriginAnglePerStep;
				CoordStruct growOffset;
				growOffset = Data->OriginGrowRate * _originElapsed;
				disp = RotateFLH(Data->OriginMoveTo + growOffset, oFacing + DegToRad(_originAngle), oTilt);
			}
			else if (!Data->OriginTargetFLH.IsEmpty())
			{
				// Speed / ReachTarget
				if (_originElapsed == 0)
					_originSpeed = Data->OriginInitialSpeed >= 0 ? Data->OriginInitialSpeed : (pTechno ? pTechno->GetTechnoType()->Speed : 40.0);

				CoordStruct targetWorld = baseCenter + WorldRotate(Data->OriginTargetFLH + _originTargetOffset, oFacing, oTilt);
				if (Data->OriginReachTarget)
				{
					int effectiveSteps = (_totalDuration - Data->DisabledFrames) / _effectiveTimeStep;
					if (effectiveSteps < 1) effectiveSteps = 1;
					int rem = effectiveSteps - _movementFrames;
					if (rem <= 0)
					{
						disp.X = targetWorld.X - originCenter.X;
						disp.Y = targetWorld.Y - originCenter.Y;
						disp.Z = targetWorld.Z - originCenter.Z;
						_originOffset.X += disp.X; _originOffset.Y += disp.Y; _originOffset.Z += disp.Z;
						circleCenter = baseCenter + _originOffset;
						_prevCircleCenter = circleCenter;
						Deactivate();
						goto skipOriginUpdate;
					}
					disp.X = (targetWorld.X - originCenter.X) / rem;
					disp.Y = (targetWorld.Y - originCenter.Y) / rem;
					disp.Z = (targetWorld.Z - originCenter.Z) / rem;
					if (Data->OriginArcHeight)
					{
						int total = _totalDuration / _effectiveTimeStep;
						double t = total>0 ? (double)_originElapsed/total : 0, t0 = total>0 ? (double)(_originElapsed-1)/total : 0;
						disp.Z += (int)(4.0*Data->OriginArcHeight*(t*(1-t) - t0*(1-t0)));
					}
				}
				else
				{
					_originSpeed += Data->OriginAcceleration;
					if (Data->OriginMaxSpeed >= 0 && _originSpeed > Data->OriginMaxSpeed) _originSpeed = Data->OriginMaxSpeed;
					if (Data->OriginMinSpeed >= 0 && _originSpeed < Data->OriginMinSpeed) _originSpeed = Data->OriginMinSpeed;
					int dx = targetWorld.X - originCenter.X, dy = targetWorld.Y - originCenter.Y, dz = targetWorld.Z - originCenter.Z;
					double dist = std::sqrt((double)dx*dx + dy*dy + dz*dz);
					if (dist < 1.0) disp = {};
					else { double s = _originSpeed / dist; disp.X = (int)(dx*s); disp.Y = (int)(dy*s); disp.Z = (int)(dz*s); }
				}
			}
			else // Circle 模式
			{
				_originCircleRadius += Data->OriginCircleRadiusGrow;
				double tr = _originCircleRadius;
				if (Data->OriginCircleMaxRadius > 0 && tr > Data->OriginCircleMaxRadius) tr = Data->OriginCircleMaxRadius;
				if (Data->OriginCircleMinRadius > 0 && tr < Data->OriginCircleMinRadius) tr = Data->OriginCircleMinRadius;
				// 角步长：优先线速度/半径推算，否则用固定角速度
				double angleStep = Data->OriginCircleAnglePerStep;
				if (Data->OriginCircleSpeed > 0 && tr > 0)
					angleStep = RadToDeg(Data->OriginCircleSpeed / tr);
				// Lissajous=yes: 累积大角旋转（增减边震荡），no: 每帧仅增量旋转（平滑行星）
				_originCircleAngle += angleStep;
				double r = Data->OriginLissajous ? DegToRad(_originCircleAngle) : DegToRad(angleStep);
				double ca = std::cos(r), sa = std::sin(r);
				// 当前圆心相对基座的偏移在 LH 平面投影
				double dx = (double)_originOffset.X, dy = (double)_originOffset.Y, dz = (double)_originOffset.Z;
				double cf = std::cos(oFacing), sf = std::sin(oFacing), ct = std::cos(oTilt), st = std::sin(oTilt);
				double dL = dx*(-sf) + dy*cf;
				double dH = dx*(-cf*st) + dy*(-sf*st) + dz*ct;
				double cd = std::sqrt(dL*dL + dH*dH);
				// 圆心在基座上（偏移≈0），初始化到半径位置
				if (cd < 1.0 && tr > 0)
				{
					dL = tr; dH = 0; cd = tr;
				}
				else if (cd < 1.0) cd = 1.0;
				double rL = (dL/cd*tr*ca - dH/cd*tr*sa), rH = (dL/cd*tr*sa + dH/cd*tr*ca);
				// 新偏移（世界坐标）
				CoordStruct newOffset;
				newOffset.X = (int)(rL*(-sf) + rH*(-cf*st));
				newOffset.Y = (int)(rL*cf + rH*(-sf*st));
				newOffset.Z = (int)(rH*ct);
				disp.X = newOffset.X - _originOffset.X;
				disp.Y = newOffset.Y - _originOffset.Y;
				disp.Z = newOffset.Z - _originOffset.Z;
			}
		skipOriginUpdate:
			_originOffset.X += disp.X; _originOffset.Y += disp.Y; _originOffset.Z += disp.Z;
			circleCenter = baseCenter + _originOffset;
			_originElapsed++;
		}

	// 圆心位移叠加：Circle 模式追踪圆心→调整 currentPos
	CoordStruct centerDelta;
	bool useCenterTracking = false;
	if (_prevCircleCenter.X || _prevCircleCenter.Y || _prevCircleCenter.Z)
	{
		centerDelta = circleCenter - _prevCircleCenter;
		if (!Data->OriginLissajous && (Data->OriginCircleRadius >= 0 || Data->OriginCircleSpeed > 0 || Data->OriginCircleAnglePerStep != 0.0))
			useCenterTracking = true;
	}
	_prevCircleCenter = circleCenter;

	// 旋转当前方向向量并按目标半径缩放（追踪模式下调整 currentPos）
	CoordStruct trackPos = currentPos;
	if (useCenterTracking)
	{
		trackPos.X += centerDelta.X;
		trackPos.Y += centerDelta.Y;
		trackPos.Z += centerDelta.Z;
	}
	double dx = static_cast<double>(trackPos.X - circleCenter.X);
	double dy = static_cast<double>(trackPos.Y - circleCenter.Y);
	double dz = static_cast<double>(trackPos.Z - circleCenter.Z);
		double currentDist;
		bool useTiltPlane = hasNormal || (Data->AllowedTilt && effectiveTilt != 0.0);
		if (useTiltPlane)
		{
			// 倾斜圆面：投影到 LH 平面计算当前距离
			double cosF = std::cos(effectiveFacing), sinF = std::sin(effectiveFacing);
			double cosT = std::cos(effectiveTilt), sinT = std::sin(effectiveTilt);
			double dL = dx * (-sinF) + dy * cosF;
			double dH = dx * (-cosF * sinT) + dy * (-sinF * sinT) + dz * cosT;
			currentDist = std::sqrt(dL * dL + dH * dH);
		}
		else
		{
			currentDist = std::sqrt(dx * dx + dy * dy);
		}
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

		if (useTiltPlane)
		{
			// 倾斜圆面：在 LH 平面内旋转
			double cosF = std::cos(effectiveFacing), sinF = std::sin(effectiveFacing);
			double cosT = std::cos(effectiveTilt), sinT = std::sin(effectiveTilt);
			double dL = dx * (-sinF) + dy * cosF;
			double dH = dx * (-cosF * sinT) + dy * (-sinF * sinT) + dz * cosT;
			double curDist = std::sqrt(dL * dL + dH * dH);
			if (curDist < 1.0) curDist = 1.0;
			double ndL = (dL / curDist * targetRadius);
			double ndH = (dH / curDist * targetRadius);
			double rL = ndL * cosA - ndH * sinA;
			double rH = ndL * sinA + ndH * cosA;
			result.MoveDisp.X = circleCenter.X + static_cast<int>(rL * (-sinF) + rH * (-cosF * sinT)) - currentPos.X;
			result.MoveDisp.Y = circleCenter.Y + static_cast<int>(rL * cosF + rH * (-sinF * sinT)) - currentPos.Y;
			result.MoveDisp.Z = circleCenter.Z + static_cast<int>(rH * cosT) - currentPos.Z;
		}
		else
		{
			// 传统 2D 圆面（XY 平面）
			double ndx = (dx / currentDist * targetRadius);
			double ndy = (dy / currentDist * targetRadius);
			double rx = ndx * cosA - ndy * sinA;
			double ry = ndx * sinA + ndy * cosA;
			result.MoveDisp.X = circleCenter.X + static_cast<int>(rx) - currentPos.X;
			result.MoveDisp.Y = circleCenter.Y + static_cast<int>(ry) - currentPos.Y;
			result.MoveDisp.Z = circleCenter.Z - currentPos.Z;
		}
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

	CoordStruct frameTargetDisp = WorldRotate(frameTargetFlh, effectiveFacing, effectiveTilt);
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
			int effectiveDuration = _totalDuration - Data->DisabledFrames;
		if (effectiveDuration < 1) effectiveDuration = 1;
		int remainingFrames = effectiveDuration - _movementFrames + 1;
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
				double t = static_cast<double>(_movementFrames - 1) / effectiveDuration;
				double tNext = static_cast<double>(_movementFrames) / effectiveDuration;
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

		resultDisp.X = static_cast<int>(dirVec.X / dirLen * speed);
		resultDisp.Y = static_cast<int>(dirVec.Y / dirLen * speed);
		resultDisp.Z = static_cast<int>(dirVec.Z / dirLen * speed);
	}

	result.MoveDisp = resultDisp;

	AdvanceFrame();
	return result;
}
