#include "VectorEffect.h"

#include <Utilities/Debug.h>

#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>
#include <Ext/Helper/Physics.h>
#include <Ext/Helper/Weapon.h>
#include <Ext/Helper/FLH.h>

#include <Ext/ObjectType/AttachEffect.h>
#include <Ext/EffectType/AttachEffectScript.h>
#include <Ext/BulletType/BulletStatus.h>

// 跨 AE 链路目标坐标缓存
std::map<TechnoClass*, CoordStruct> VectorEffect::_lastTargetCache;


// Vector: Freeze / Circle / MoveTo / Speed / ReachTarget
// 基于 V1 VectorEffect.cpp 精简

void VectorEffect::OnStart()
{
	if (pTechno && pTechno->WhatAmI() == AbstractType::Building)
	{
		Deactivate();
		return;
	}

	_elapsedFrames = 0;
	_moveFrame = 0;
	_movementFrames = 0;
	_currentAngle = 0.0;
	_effectiveTimeStep = Data->TimeStep;
	// _prevCircleCenter 不在此初始化：圆心追踪依赖 Origin 移动系统首帧的 skipOriginUpdate 赋值

	_initialLocation = pObject->GetCoords();
	_vectorAcquireZ = _initialLocation.Z;  // Circle 圆心高度基准：获取 Vector 时的 Z
	_totalDuration = AE->AEData.GetDuration() / _effectiveTimeStep;

	_randomTargetOffset.X = Random::RandomRanged(Data->TargetOffsetFMin, Data->TargetOffsetFMax);
	_randomTargetOffset.Y = Random::RandomRanged(Data->TargetOffsetLMin, Data->TargetOffsetLMax);
	_randomTargetOffset.Z = Random::RandomRanged(Data->TargetOffsetHMin, Data->TargetOffsetHMax);

	// 弧面旋转角
	_arcRotation = Data->ArcRotation;
	if (Data->ArcRandomRotationMax > Data->ArcRandomRotationMin)
		_arcRotation = Data->ArcRandomRotationMin + (Data->ArcRandomRotationMax - Data->ArcRandomRotationMin) * Random::RandomDouble();

	_arcHeight = Data->ArcHeight;
	if (Data->ArcRandomHeightMax > Data->ArcRandomHeightMin)
		_arcHeight = Random::RandomRanged(Data->ArcRandomHeightMin, Data->ArcRandomHeightMax);

	_arcPeakPercent = Data->ArcPeakPercent / 100.0;
	if (Data->ArcPeakRandomPercent.X < Data->ArcPeakRandomPercent.Y)
		_arcPeakPercent = Random::RandomRanged(Data->ArcPeakRandomPercent.X, Data->ArcPeakRandomPercent.Y) / 100.0;
	if (_arcPeakPercent <= 0.0) _arcPeakPercent = 0.5;
	if (_arcPeakPercent >= 1.0) _arcPeakPercent = 0.5;

	// Origin 弧参数（镜像小圆）
	_originArcRotation = Data->OriginArcRotation;
	if (Data->OriginArcRandomRotationMax > Data->OriginArcRandomRotationMin)
		_originArcRotation = Data->OriginArcRandomRotationMin + (Data->OriginArcRandomRotationMax - Data->OriginArcRandomRotationMin) * Random::RandomDouble();

	_originArcHeight = Data->OriginArcHeight;
	if (Data->OriginArcRandomHeightMax > Data->OriginArcRandomHeightMin)
		_originArcHeight = Random::RandomRanged(Data->OriginArcRandomHeightMin, Data->OriginArcRandomHeightMax);

	_originArcPeakPercent = Data->OriginArcPeakPercent / 100.0;
	if (Data->OriginArcPeakRandomPercent.X < Data->OriginArcPeakRandomPercent.Y)
		_originArcPeakPercent = Random::RandomRanged(Data->OriginArcPeakRandomPercent.X, Data->OriginArcPeakRandomPercent.Y) / 100.0;
	if (_originArcPeakPercent <= 0.0) _originArcPeakPercent = 0.5;
	if (_originArcPeakPercent >= 1.0) _originArcPeakPercent = 0.5;

	// 影子坐标（Speed 模式弧高进度基准，不受弧高 Z 偏移污染）
	_shadowPosX = _initialLocation.X;
	_shadowPosY = _initialLocation.Y;
	_shadowPosZ = _initialLocation.Z;
	_shadowTraveled = 0.0;

	// --- 初始速度 ---
	_currentSpeed = 0.0;
	if (Data->LinearSpeed >= 0)
	{
		_currentSpeed = static_cast<double>(Data->LinearSpeed);
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
		_pLauncher = (AE && AE->pSource) ? AE->pSource : pTechno; // 单位侧用攻击者作为Launcher
	}

	// 缓存：只要单位有攻击目标就记录，不限Origin类型，供AE链路后续使用
	if (pTechno && pTechno->Target)
		_lastTargetCache[pTechno] = pTechno->Target->GetCoords();

	// --- Origin 初始化 ---
	switch (Data->Origin)
	{
	case VectorData::VectorOrigin::Target:
		if (Data->OriginNoUpdate)
		{
			// 对标 AutoWeapon IsOnTarget: 锚点是目标单位自身，不是 pTechno->Target
			if (pTechno)
			{
				_initialOriginPos = pTechno->GetCoords();
				if (pTechno->Target)
					_lastTargetCache[pTechno] = pTechno->Target->GetCoords();
			}
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
			_facingDir = bulletFacing; // 锁定初始朝向
			_facingRad = _facingDir.GetRadian();
		}
		else if (pTechno)
		{
			CoordStruct unitPos = pTechno->GetCoords();
			DirStruct unitFacing = pTechno->TurretFacing().Current();
			_initialOriginPos = GetFLHAbsoluteCoords(unitPos, Data->OriginFLH, unitFacing);
			_facingDir = unitFacing; // 锁定初始朝向
			_facingRad = _facingDir.GetRadian();
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
	_lissajousStep = Data->Lissajous;

	// 初始化 3D 法向量（从球坐标 _facingRad/_tiltRad 转换）
	// 球坐标→笛卡尔：X=cos(tilt)cos(facing), Y=cos(tilt)sin(facing), Z=sin(tilt)
	{
		double ct = std::cos(_tiltRad), st = std::sin(_tiltRad);
		double cf = std::cos(_facingRad), sf = std::sin(_facingRad);
		_normalX = ct * cf;
		_normalY = ct * sf;
		_normalZ = st;
	}

	switch (Data->Origin)
	{
	case VectorData::VectorOrigin::Launcher:
	{
		if (!hasNormal)
		{
			TechnoClass* pLauncherTechno = abstract_cast<TechnoClass*>(_pLauncher);
			if (pLauncherTechno && !IsDeadOrInvisible(pLauncherTechno))
			{
				_facingDir = pLauncherTechno->TurretFacing().Current(); // 直接存 DirStruct，不经 GetRadian 往返
				_facingRad = _facingDir.GetRadian();
			}
		}
		break;
	}

	case VectorData::VectorOrigin::Target:
	{
		if (!hasNormal)
		{
			// 照搬 AutoWeapon GetSourcePosOnTarget:
			// facingDir = Point2Dir(sourcePos, targetPos)  → attacker→target (RA2)
			// 锚点 = targetPos（目标单位自身坐标）
			if (pTechno && AE && AE->pSource)
			{
				CoordStruct targetPos = pTechno->GetCoords();
				CoordStruct sourcePos = AE->pSource->GetCoords();
				_facingDir = Point2Dir(targetPos, sourcePos); // 官方API，不得修改
				_facingRad = _facingDir.GetRadian();
			}
			else if (pBullet)
			{
				CoordStruct bulletPos = pBullet->GetCoords();
				CoordStruct targetPos = pBullet->TargetCoords;
				_facingDir = Point2Dir(targetPos, bulletPos); // 官方API，不得修改
				_facingRad = _facingDir.GetRadian();
			}
		}
		break;
	}

	case VectorData::VectorOrigin::Source:
	{
		if (!hasNormal)
		{
			if (pBullet && AE && AE->pSource)
			{
				_facingDir = Point2Dir(pBullet->GetCoords(), AE->pSource->GetCoords()); // 官方API
				_facingRad = _facingDir.GetRadian();
			}
			else if (pTechno && AE && AE->pSource)
			{
				_facingDir = Point2Dir(pTechno->GetCoords(), AE->pSource->GetCoords()); // 官方API
				_facingRad = _facingDir.GetRadian();
			}
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
	result.AllowRotateUnit = Data->SyncFacing; // 成熟机制：单位端同步朝向，删改前确认

	// Circle 预初始化：在 DisabledFrames 冻结前完成，保证首帧后参数可用
	if (_elapsedFrames == 0)
	{
		_currentCircleSpeed = static_cast<double>(Data->CircleSpeed);
		if (_currentCircleSpeed <= 0.0)
		{
			if (pBullet)
				_currentCircleSpeed = pBullet->Speed;
			else if (pTechno)
				_currentCircleSpeed = pTechno->GetTechnoType()->Speed;
		}
		_currentCircleAngle = Data->CircleAnglePerStep;
		if (Data->CircleRandomAngleMax > Data->CircleRandomAngleMin)
		{
			if (Data->CircleRandomAngleMax2 > Data->CircleRandomAngleMin2 && Random::RandomRanged(0, 1))
				_currentCircleAngle = Data->CircleRandomAngleMin2 + (Data->CircleRandomAngleMax2 - Data->CircleRandomAngleMin2) * Random::RandomDouble();
			else
				_currentCircleAngle = Data->CircleRandomAngleMin + (Data->CircleRandomAngleMax - Data->CircleRandomAngleMin) * Random::RandomDouble();
		}
		_currentCircleRadius = static_cast<double>(Data->CircleRadius);
		if (Data->CircleRandomRadiusMax > Data->CircleRandomRadiusMin)
			_currentCircleRadius = Random::RandomRanged(Data->CircleRandomRadiusMin, Data->CircleRandomRadiusMax);
	}

	// DisabledFrames：首帧快照后冻结，不阻塞其他 AE，不计入运动时间
	if (_elapsedFrames < Data->DisabledFrames)
	{
		result.MoveDisp = { 0, 0, 0 };
		AdvanceFrame();
		return result;
	}


	// ========================================================================
	// Freeze — 成熟机制，别乱动
	// ========================================================================
	if (Data->Freeze)
	{
		result.Freeze = true;
		result.Force = true;  // 抛射体 Freeze 必须 Force，否则引擎检测"卡住"自爆
		if (result.FrozenPos.IsEmpty())
			result.FrozenPos = _initialLocation;
		CoordStruct currentPos = pObject->GetCoords();
		result.MoveDisp.X = result.FrozenPos.X - currentPos.X;
		result.MoveDisp.Y = result.FrozenPos.Y - currentPos.Y;
		result.MoveDisp.Z = result.FrozenPos.Z - currentPos.Z;
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

	// 每帧更新缓存：即使OnStart时Target为空，后续帧获取后也能传递
	if (pTechno)
	{
		CoordStruct cached{};
		bool got = false;
		if (pTechno->Target) { cached = pTechno->Target->GetCoords(); got = true; }
		else {
			FootClass* pf = abstract_cast<FootClass*>(pTechno);
			if (pf && pf->Destination) { cached = pf->Destination->GetCoords(); got = true; }
			else if (pTechno->Focus) { cached = pTechno->Focus->GetCoords(); got = true; }
		}
		if (got)
			_lastTargetCache[pTechno] = cached;
	}

	_normalRotF += _lissajousStep;
	// 3D 法向量增量旋转（绕世界 F=Y / L=X / H=Z 轴，正速度=顺时针）
		if (_normalStepF != 0.0 || _normalStepL != 0.0 || _normalStepH != 0.0)
		{
			if (_normalStepF != 0.0)
			{
				double rad = Math::deg2rad(_normalStepF), c = std::cos(rad), s = std::sin(rad);
				double nx = _normalX, nz = _normalZ;
				_normalX = nx * c - nz * s;
				_normalZ = nx * s + nz * c;
			}
			if (_normalStepL != 0.0)
			{
				double rad = Math::deg2rad(_normalStepL), c = std::cos(rad), s = std::sin(rad);
				double ny = _normalY, nz = _normalZ;
				_normalY = ny * c + nz * s;
				_normalZ = -ny * s + nz * c;
			}
			if (_normalStepH != 0.0)
			{
				double rad = Math::deg2rad(_normalStepH), c = std::cos(rad), s = std::sin(rad);
				double nx = _normalX, ny = _normalY;
				_normalX = nx * c + ny * s;
				_normalY = -nx * s + ny * c;
			}
		}

	CoordStruct currentPos = pObject->GetCoords();

	// ========================================================================
	// 动态 F 轴：非 NoUpdate 时每帧根据当前坐标重新计算 FLH 朝向
	// ========================================================================

	// AllowOriginTilt：从 Origin 单位获取倾斜，注入 _facingRad/_tiltRad（同 NormalVector 机制）
	// 仅 Circle 模式生效，避免非 Circle 模式覆盖 OnStart 的方向
	double originTerrainTilt = 0.0;
	bool hasCircleForTilt = Data->CircleRadius > 0 || Data->CircleAnglePerStep > 0.0;
	if ((Data->AllowOriginTilt || Data->OriginAllowOriginTilt) && hasCircleForTilt && !Data->OriginIsOnWorld)
	{
		TechnoClass* pOriginTechno = nullptr;
		switch (Data->Origin)
		{
		case VectorData::VectorOrigin::Target:
			if (pBullet && pBullet->Target)
				pOriginTechno = abstract_cast<TechnoClass*>(pBullet->Target);
			else if (pTechno && pTechno->Target)
				pOriginTechno = abstract_cast<TechnoClass*>(pTechno->Target);
			break;
		case VectorData::VectorOrigin::Source:
			pOriginTechno = abstract_cast<TechnoClass*>(_pSource);
			break;
		case VectorData::VectorOrigin::Launcher:
			pOriginTechno = abstract_cast<TechnoClass*>(_pLauncher);
			break;
		case VectorData::VectorOrigin::Self:
			pOriginTechno = pTechno;
			break;
		}
		if (pOriginTechno && !IsDeadOrInvisible(pOriginTechno))
		{
			// 优先用引擎动态倾斜（Rocker等），为 0 时从地形采样
			originTerrainTilt = pOriginTechno->AngleRotatedForwards;
			if (std::abs(originTerrainTilt) < 1e-6)
			{
				CoordStruct originCoord = pOriginTechno->GetCoords();
				double unitFacing = pOriginTechno->PrimaryFacing.Current().GetRadian();
				double cosF = std::cos(unitFacing), sinF = std::sin(unitFacing);
				Point2D frontPt = { originCoord.X + static_cast<int>(128.0 * cosF), originCoord.Y + static_cast<int>(128.0 * sinF) };
				Point2D backPt  = { originCoord.X - static_cast<int>(128.0 * cosF), originCoord.Y - static_cast<int>(128.0 * sinF) };
				int hFront = MapClass::Instance->GetCellFloorHeight({frontPt.X, frontPt.Y, 0});
				int hBack  = MapClass::Instance->GetCellFloorHeight({backPt.X, backPt.Y, 0});
				double dz = static_cast<double>(hFront - hBack);
				double dxy = 256.0;
				originTerrainTilt = (dxy > 1e-6) ? std::atan2(dz, dxy) : 0.0;
			}

			// 注入 _facingRad/_tiltRad（同 NormalVector 机制）
			// 单位倾斜 = 默认法向量 (0,0,1) 绕单位 L 轴旋转 originTerrainTilt
			// L 轴 = (-sinF, cosF, 0)，绕 L 轴旋转后法向量 = (-sinT*sinF, sinT*cosF, cosT)
			double unitFacing2 = pOriginTechno->PrimaryFacing.Current().GetRadian();
			double sinT = std::sin(originTerrainTilt), cosT = std::cos(originTerrainTilt);
			double nx = -sinT * std::sin(unitFacing2);
			double ny = sinT * std::cos(unitFacing2);
			double nz = cosT;
			double nLenXY = std::sqrt(nx * nx + ny * ny);
			_facingRad = nLenXY > 1e-6 ? std::atan2(ny, nx) : 0.0;
			_tiltRad = nLenXY > 1e-6 ? std::atan2(nz, nLenXY) : (nz > 0 ? M_PI / 2.0 : -M_PI / 2.0);
		}
	}

	double effectiveFacing = _facingRad;
	double effectiveTilt = _tiltRad;
	DirStruct mainFacingDir = Radians2Dir(effectiveFacing); // 官方API，不得修改：引擎弧度→DirStruct转换
	// OriginNoUpdate + Target/Source/Launcher/Self：直接用 OnStart 存的 DirStruct，不经 GetRadian→Radians2Dir 往返
	if (Data->OriginNoUpdate && (Data->Origin == VectorData::VectorOrigin::Target || Data->Origin == VectorData::VectorOrigin::Source || Data->Origin == VectorData::VectorOrigin::Launcher || Data->Origin == VectorData::VectorOrigin::Self))
	{
		mainFacingDir = _facingDir;
		effectiveFacing = _facingDir.GetRadian();
	}

	// OriginIsOnWorld：锁定世界坐标系（朝北），覆盖 Origin 朝向
	if (Data->OriginIsOnWorld)
	{
		mainFacingDir = DirStruct{};
		effectiveFacing = 0.0;
		effectiveTilt = 0.0;
	}

	bool hasNormal = !Data->NormalVector.IsEmpty()
		|| Data->NormalRandomF.Y > Data->NormalRandomF.X
		|| Data->NormalRandomL.Y > Data->NormalRandomL.X
		|| Data->NormalRandomH.Y > Data->NormalRandomH.X;
	if (!Data->OriginNoUpdate && !hasNormal && !Data->AllowOriginTilt && !Data->OriginIsOnWorld)
	{
		switch (Data->Origin)
		{
		case VectorData::VectorOrigin::Source:
			if (_pSource && !IsDeadOrInvisible(_pSource))
			{
				mainFacingDir = Point2Dir(_pSource->GetCoords(), currentPos); // 官方API，不得修改
				effectiveFacing = mainFacingDir.GetRadian();
				double dx = currentPos.X - _pSource->GetCoords().X;
				double dy = currentPos.Y - _pSource->GetCoords().Y;
				double dz = currentPos.Z - _pSource->GetCoords().Z;
				double lenXY = std::sqrt(dx*dx + dy*dy);
				effectiveTilt = (lenXY > 1e-6 && Data->AllowCircleTilt) ? std::atan2(dz, lenXY) : 0.0;
			}
			break;
		case VectorData::VectorOrigin::Target:
		{
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
			else if (pTechno && AE && AE->pSource)
				targetPos = AE->pSource->GetCoords(); // 对标 AutoWeapon: 攻击者位置
			else
				break;
			mainFacingDir = Point2Dir(targetPos, currentPos); // 官方API，不得修改
			effectiveFacing = mainFacingDir.GetRadian();
			double dx = currentPos.X - targetPos.X, dy = currentPos.Y - targetPos.Y, dz = currentPos.Z - targetPos.Z;
			double lenXY = std::sqrt(dx*dx + dy*dy);
			effectiveTilt = (lenXY > 1e-6 && Data->AllowCircleTilt) ? std::atan2(dz, lenXY) : 0.0;
		}
			break;

		case VectorData::VectorOrigin::Self:
			if (pTechno)
			{
				mainFacingDir = Data->OriginIsOnBody
					? pTechno->PrimaryFacing.Current()     // 官方API，不得修改
					: pTechno->TurretFacing().Current();   // 官方API，不得修改
				effectiveFacing = mainFacingDir.GetRadian();
			}
			else if (pBullet)
			{
				mainFacingDir = Facing(pBullet, currentPos); // 官方API，不得修改
				effectiveFacing = mainFacingDir.GetRadian();
			}
			break;

		case VectorData::VectorOrigin::Launcher:
			if (_pLauncher && !IsDeadOrInvisible(_pLauncher))
			{
				TechnoClass* pLauncherTechno = abstract_cast<TechnoClass*>(_pLauncher);
				if (pLauncherTechno)
				{
					mainFacingDir = Data->OriginIsOnBody
						? pLauncherTechno->PrimaryFacing.Current()     // 官方API，不得修改
						: pLauncherTechno->TurretFacing().Current();   // 官方API，不得修改
					effectiveFacing = mainFacingDir.GetRadian();
				}
			}
			break;
		}
	}

	// 3D 法向量旋转覆盖：当 NormalF/L/HAnglePerStep 设定时，无视基座变化
	if (_normalStepF != 0.0 || _normalStepL != 0.0 || _normalStepH != 0.0)
	{
		double lenXY = std::sqrt(_normalX * _normalX + _normalY * _normalY);
		effectiveFacing = lenXY > 1e-6 ? std::atan2(_normalY, _normalX) : 0.0;
		effectiveTilt = lenXY > 1e-6 ? std::atan2(_normalZ, lenXY) : (_normalZ > 0 ? M_PI / 2.0 : -M_PI / 2.0);
		mainFacingDir = Radians2Dir(effectiveFacing); // 官方API，不得修改
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
		else if (pTechno)
			originPos = pTechno->GetCoords();           // 对标 AutoWeapon: 目标单位自身
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
	// AllowOriginTilt 时跳过二维 GetFLHAbsoluteCoords，后续用三维旋转处理
	if (!Data->AllowOriginTilt)
	{
		if (!Data->OriginFLH.IsEmpty() && Data->Origin != VectorData::VectorOrigin::Self)
			originPos = GetFLHAbsoluteCoords(originPos, Data->OriginFLH, mainFacingDir);
	}

	// ========================================================================
	// 成熟机制，别乱动 — 模式 C: Circle（独立圆周，圆心=Origin，三选二参数）
	// ========================================================================
	bool hasCircle = Data->CircleRadius > 0 || Data->CircleAnglePerStep > 0.0
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

		// 动态线速：每帧叠加加速度（初始值已在 DisabledFrames 前预初始化）
		_currentCircleSpeed += Data->CircleSpeedAcceleration;
		if (Data->CircleMaxSpeed != 0 && _currentCircleSpeed > Data->CircleMaxSpeed)
			_currentCircleSpeed = static_cast<double>(Data->CircleMaxSpeed);
		if (Data->CircleMinSpeed != 0 && _currentCircleSpeed < Data->CircleMinSpeed)
			_currentCircleSpeed = static_cast<double>(Data->CircleMinSpeed);

		// 角速度动态：每帧叠加加速度（初始值已在 DisabledFrames 前预初始化）
		_currentCircleAngle += Data->CircleAngleAcceleration;
		if (Data->CircleMaxAngle != 0.0 && _currentCircleAngle > Data->CircleMaxAngle)
			_currentCircleAngle = Data->CircleMaxAngle;
		if (Data->CircleMinAngle != 0.0 && _currentCircleAngle < Data->CircleMinAngle)
			_currentCircleAngle = Data->CircleMinAngle;

		double speed = _currentCircleSpeed;
		double angleStep = _currentCircleAngle;

		// 三选二：半径 + 角速度优先，两者都有时速率由角速度推算（忽略显式 CircleSpeed）
		if (angleStep > 0.0)
			speed = calcRadius * Math::deg2rad(angleStep);
		else if (speed > 0.0)
			angleStep = Math::rad2deg(speed / calcRadius);

	// 圆心 = Origin + CircleOrigin 偏移（世界坐标系）
	// CircleOrigin Z 高度规则：CircleOrigin 非空 → 绝对覆写 OriginFLH.Z+CircleOrigin.Z；
	// 仅 OriginFLH 非空 → 相对偏移 _vectorAcquireZ+OriginFLH.Z
	// 修改 CircleOrigin 的 Z 后再走 GetFLHAbsoluteCoords，tilt 对 F/L 分量的 Z 投影自动保留
	CoordStruct adjustedCircleOrigin = Data->CircleOrigin;
	if (!Data->CircleOrigin.IsEmpty())
		adjustedCircleOrigin.Z = Data->OriginFLH.Z + Data->CircleOrigin.Z;

	CoordStruct circleCenter = originPos;

	// AllowOriginTilt：用三维 FLH 旋转替代二维 GetFLHAbsoluteCoords
	if (Data->AllowOriginTilt && !Data->OriginFLH.IsEmpty() && Data->Origin != VectorData::VectorOrigin::Self)
	{
		int f = Data->OriginFLH.X, l = Data->OriginFLH.Y, h = Data->OriginFLH.Z;
		double sinT = std::sin(originTerrainTilt), cosT = std::cos(originTerrainTilt);
		// FLH 旋转用单位自身 facing（_facingRad 是法向量方向，不适用于 FLH 的 F/L 分量）
		TechnoClass* pOriginTechno = nullptr;
		switch (Data->Origin)
		{
		case VectorData::VectorOrigin::Target:
			if (pBullet && pBullet->Target)
				pOriginTechno = abstract_cast<TechnoClass*>(pBullet->Target);
			else if (pTechno && pTechno->Target)
				pOriginTechno = abstract_cast<TechnoClass*>(pTechno->Target);
			break;
		case VectorData::VectorOrigin::Source:
			pOriginTechno = abstract_cast<TechnoClass*>(_pSource);
			break;
		case VectorData::VectorOrigin::Launcher:
			pOriginTechno = abstract_cast<TechnoClass*>(_pLauncher);
			break;
		case VectorData::VectorOrigin::Self:
			pOriginTechno = pTechno;
			break;
		}
		double unitF = (pOriginTechno && !IsDeadOrInvisible(pOriginTechno))
			? pOriginTechno->PrimaryFacing.Current().GetRadian() : 0.0;
		double cosF = std::cos(unitF), sinF = std::sin(unitF);
		// 先绕 L 轴（左右轴）转 tilt（俯仰），再绕 Z 轴转 facing
		// tilt>0=头低屁股高，头部在 +F 方向偏下
		// FLH=(f,l,h) 先 tilt: f'=f*cosT-h*sinT, l'=l, h'=f*sinT+h*cosT
		// 再 facing: X=cosF*f'-sinF*l', Y=sinF*f'+cosF*l', Z=h'
		double fTilt = f * cosT - h * sinT;
		double hTilt = f * sinT + h * cosT;
		circleCenter.X += static_cast<int>(fTilt * cosF - l * sinF);
		circleCenter.Y += static_cast<int>(fTilt * sinF + l * cosF);
		circleCenter.Z += static_cast<int>(hTilt);
	}

	if (!Data->CircleOrigin.IsEmpty())
	{
		if (Data->AllowOriginTilt)
		{
			// FLH→世界坐标（facing + terrainTilt 三维旋转），替换 GetFLHAbsoluteCoords（仅二维）
			double cosF = std::cos(effectiveFacing), sinF = std::sin(effectiveFacing);
			double cosT = std::cos(originTerrainTilt), sinT = std::sin(originTerrainTilt);
			double f = static_cast<double>(adjustedCircleOrigin.X);
			double l = static_cast<double>(adjustedCircleOrigin.Y);
			double h = static_cast<double>(adjustedCircleOrigin.Z);
			// 先绕 L 轴（左右轴）转 tilt（俯仰），再绕 Z 轴转 facing
			double fTilt = f * cosT - h * sinT;
			double hTilt = f * sinT + h * cosT;
			circleCenter.X += static_cast<int>(fTilt * cosF - l * sinF);
			circleCenter.Y += static_cast<int>(fTilt * sinF + l * cosF);
			circleCenter.Z += static_cast<int>(hTilt);
		}
		else
		{
			circleCenter = originPos + adjustedCircleOrigin;
		}
	}
	else if (!Data->OriginFLH.IsEmpty())
	{
		// 仅 OriginFLH：CircleOrigin 为空时不走 FLH 转换，手动设 Z
		circleCenter.Z = _vectorAcquireZ + Data->OriginFLH.Z;
	}

		// 圆心移动：Vector.Origin.* 系统
		if (!Data->OriginMoveTo.IsEmpty() || Data->OriginReachTarget || Data->OriginLinearSpeed >= 0 || !Data->OriginTargetFLH.IsEmpty()
			|| Data->OriginCircleRadius >= 0 || Data->OriginCircleSpeed != 0 || Data->OriginCircleAnglePerStep != 0)
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

			// OriginNoUpdate：首帧快照基座，后续帧冻结
			if (_elapsedFrames == 0)
				_initialBaseCenter = baseCenter;
			else if (Data->OriginOriginNoUpdate)
				baseCenter = _initialBaseCenter;

			if (_elapsedFrames == 0)
			{
				// 初始偏移 = 圆心相对于基座的向量
				_originOffset = circleCenter - baseCenter;
				// Circle 初始化
				_originCircleRadius = Data->OriginCircleRadius;
				_originCircleSpeed = Data->OriginCircleSpeed;
				_originCircleAngle = 0.0; // 初始相位
				// 未显式设半径：取当前偏移的水平距离
				if (_originCircleRadius < 0)
					_originCircleRadius = (int)std::sqrt(
						(double)_originOffset.X * _originOffset.X +
						(double)_originOffset.Y * _originOffset.Y +
						(double)_originOffset.Z * _originOffset.Z);
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
			_originLissajousStep = Data->OriginLissajous;
			// 无 NormalVector 时：默认水平圆面（法向量朝上）
				if (Data->OriginNormalVector.IsEmpty())
				{
					_originFacing = 0;
					_originTilt = M_PI / 2.0;
					// OriginAllowCircleTilt: 大圆面跟随目标倾斜（Origin=Target 时有效）
					if (Data->OriginAllowCircleTilt && Data->OriginOrigin == VectorData::VectorOrigin::Target)
					{
						CoordStruct targetPos {};
						bool hasTargetPos = false;
						if (pBullet) { targetPos = pBullet->TargetCoords; hasTargetPos = true; }
						else if (pTechno && pTechno->Target) { targetPos = pTechno->Target->GetCoords(); hasTargetPos = true; }
						if (hasTargetPos)
						{
							double dx = circleCenter.X - targetPos.X;
							double dy = circleCenter.Y - targetPos.Y;
							double dz = circleCenter.Z - targetPos.Z;
							double lenXY = std::sqrt(dx * dx + dy * dy);
							_originTilt = (lenXY > 1e-6) ? std::atan2(dz, lenXY) : M_PI / 2.0;
						}
					}
				}
				else
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
				// 初始化大圆 3D 法向量
				{
					double ct = std::cos(_originTilt), st = std::sin(_originTilt);
					double cf = std::cos(_originFacing), sf = std::sin(_originFacing);
					_originNormalX = ct * cf;
					_originNormalY = ct * sf;
					_originNormalZ = st;
				}
			}
		// 每帧累加 Lissajous + 3D 法向量增量旋转
		_originNormalRotF += _originLissajousStep;
		if (_originNormalStepF != 0.0 || _originNormalStepL != 0.0 || _originNormalStepH != 0.0)
		{
			if (_originNormalStepF != 0.0)
			{
				double rad = Math::deg2rad(_originNormalStepF), c = std::cos(rad), s = std::sin(rad);
				double nx = _originNormalX, nz = _originNormalZ;
				_originNormalX = nx * c - nz * s;
				_originNormalZ = nx * s + nz * c;
			}
			if (_originNormalStepL != 0.0)
			{
				double rad = Math::deg2rad(_originNormalStepL), c = std::cos(rad), s = std::sin(rad);
				double ny = _originNormalY, nz = _originNormalZ;
				_originNormalY = ny * c + nz * s;
				_originNormalZ = -ny * s + nz * c;
			}
			if (_originNormalStepH != 0.0)
			{
				double rad = Math::deg2rad(_originNormalStepH), c = std::cos(rad), s = std::sin(rad);
				double nx = _originNormalX, ny = _originNormalY;
				_originNormalX = nx * c + ny * s;
				_originNormalY = -nx * s + ny * c;
			}
		}

			// OriginAllowCircleTilt：每帧从目标 Z 差更新大圆面倾斜
			if (Data->OriginAllowCircleTilt && Data->OriginOrigin == VectorData::VectorOrigin::Target)
			{
				CoordStruct oc = baseCenter + _originOffset;
				CoordStruct targetPos {};
				bool hasTargetPos = false;
				if (pBullet) { targetPos = pBullet->TargetCoords; hasTargetPos = true; }
				else if (pTechno && pTechno->Target) { targetPos = pTechno->Target->GetCoords(); hasTargetPos = true; }
				if (hasTargetPos)
				{
					double dx = oc.X - targetPos.X, dy = oc.Y - targetPos.Y, dz = oc.Z - targetPos.Z;
					double lenXY = std::sqrt(dx * dx + dy * dy);
					_originTilt = (lenXY > 1e-6) ? std::atan2(dz, lenXY) : M_PI / 2.0;
				}
			}

			double oFacing = _originFacing;
			double oTilt = _originTilt + (Data->OriginAllowOriginTilt ? originTerrainTilt : 0.0);
			// 3D 法向量旋转覆盖
			if (_originNormalStepF != 0.0 || _originNormalStepL != 0.0 || _originNormalStepH != 0.0)
			{
				double lenXY = std::sqrt(_originNormalX * _originNormalX + _originNormalY * _originNormalY);
				oFacing = lenXY > 1e-6 ? std::atan2(_originNormalY, _originNormalX) : 0.0;
				oTilt = lenXY > 1e-6 ? std::atan2(_originNormalZ, lenXY) : (_originNormalZ > 0 ? M_PI / 2.0 : -M_PI / 2.0);
			}
			DirStruct oFacingDir = Radians2Dir(oFacing); // 官方API，不得修改：弧度→DirStruct

			// 当前圆心绝对位置 = 基座 + 偏移
			CoordStruct originCenter = baseCenter + _originOffset;

			CoordStruct disp;
			if (!Data->OriginMoveTo.IsEmpty())
			{
				// MoveTo 模式：GrowRate 随帧数线性增长
				_originAngle += Data->OriginAnglePerStep;
				CoordStruct growOffset;
				growOffset = Data->OriginGrowRate * _originElapsed;
				disp = GetFLHAbsoluteOffset(Data->OriginMoveTo + growOffset, Radians2Dir(oFacing + Math::deg2rad(_originAngle))); // 官方API，不得修改
			}
			else if (Data->OriginReachTarget || Data->OriginLinearSpeed >= 0 || !Data->OriginTargetFLH.IsEmpty())
			{
				// Speed / ReachTarget
				if (_originElapsed == 0)
				{
					_originSpeed = Data->OriginLinearSpeed >= 0 ? Data->OriginLinearSpeed : (pTechno ? pTechno->GetTechnoType()->Speed : 40.0);
					_originArcStartCenter = originCenter;
				}

				CoordStruct targetWorld = GetFLHAbsoluteCoords(baseCenter, Data->OriginTargetFLH + _originTargetOffset, oFacingDir); // 官方API，不得修改
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
					if (_originArcHeight != 0)
					{
						double t = static_cast<double>(_movementFrames) / effectiveSteps;
						double arcOffset = CalcArcOffsetAt(_originArcHeight, _originArcPeakPercent, t);
						double baseX = _originArcStartCenter.X + (targetWorld.X - _originArcStartCenter.X) * t;
						double baseY = _originArcStartCenter.Y + (targetWorld.Y - _originArcStartCenter.Y) * t;
						double baseZ = _originArcStartCenter.Z + (targetWorld.Z - _originArcStartCenter.Z) * t;
						if (_originArcRotation == 0.0)
						{
							disp.Z = static_cast<int>(baseZ + arcOffset) - originCenter.Z;
						}
						else
						{
							double dx = targetWorld.X - _originArcStartCenter.X;
							double dy = targetWorld.Y - _originArcStartCenter.Y;
							double dz = targetWorld.Z - _originArcStartCenter.Z;
							double dLen = std::sqrt(dx*dx + dy*dy + dz*dz);
							if (dLen > 1e-6)
							{
								double dnx = dx/dLen, dny = dy/dLen, dnz = dz/dLen;
								double upDotD = dnz;
								double px = -dnx*upDotD, py = -dny*upDotD, pz = 1.0 - dnz*upDotD;
								double pLen = std::sqrt(px*px + py*py + pz*pz);
								if (pLen < 1e-6) { px = 1.0-dnx*dnx; py = -dny*dnx; pz = -dnz*dnx; pLen = std::sqrt(px*px+py*py+pz*pz); }
								double pnx = px/pLen, pny = py/pLen, pnz = pz/pLen;
								double rad = Math::deg2rad(_originArcRotation);
								double c = std::cos(rad), s = std::sin(rad);
								double rx = pnx*c + (dny*pnz - dnz*pny)*s;
								double ry = pny*c + (dnz*pnx - dnx*pnz)*s;
								double rz = pnz*c + (dnx*pny - dny*pnx)*s;
								disp.X = static_cast<int>(baseX + rx*arcOffset) - originCenter.X;
								disp.Y = static_cast<int>(baseY + ry*arcOffset) - originCenter.Y;
								disp.Z = static_cast<int>(baseZ + rz*arcOffset) - originCenter.Z;
							}
							else
							{
								disp.Z = static_cast<int>(baseZ + arcOffset) - originCenter.Z;
							}
						}
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
					else if (Data->OriginSpeedEndOnReach && _originSpeed >= dist)
					{
						disp.X = dx; disp.Y = dy; disp.Z = dz;
						Deactivate();
					}
					else { double s = _originSpeed / dist; disp.X = (int)(dx*s); disp.Y = (int)(dy*s); disp.Z = (int)(dz*s); }

					// 弧高增量叠加（与 OriginReachTarget 一致，支持 ArcPeakPercent / ArcRotation）
					if (_originArcHeight != 0 && dist >= 1.0)
					{
						if (_originArcTotalDist < 0.0)
							_originArcTotalDist = dist;
						double t = (_originArcTotalDist > 1e-6) ? 1.0 - dist / _originArcTotalDist : 0.0;
						if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0;
						double arcThis = CalcArcOffsetAt(_originArcHeight, _originArcPeakPercent, t);
						double arcDelta = arcThis - _originPrevArcOffset;
						_originPrevArcOffset = arcThis;
						if (_originArcRotation == 0.0)
						{
							disp.Z += static_cast<int>(arcDelta);
						}
						else
						{
							double dx0 = targetWorld.X - _originArcStartCenter.X;
							double dy0 = targetWorld.Y - _originArcStartCenter.Y;
							double dz0 = targetWorld.Z - _originArcStartCenter.Z;
							double dLen = std::sqrt(dx0*dx0 + dy0*dy0 + dz0*dz0);
							if (dLen > 1e-6)
							{
								double dnx = dx0/dLen, dny = dy0/dLen, dnz = dz0/dLen;
								double upDotD = dnz;
								double px = -dnx*upDotD, py = -dny*upDotD, pz = 1.0 - dnz*upDotD;
								double pLen = std::sqrt(px*px + py*py + pz*pz);
								if (pLen < 1e-6) { px = 1.0-dnx*dnx; py = -dny*dnx; pz = -dnz*dnx; pLen = std::sqrt(px*px+py*py+pz*pz); }
								double pnx = px/pLen, pny = py/pLen, pnz = pz/pLen;
								double rad = Math::deg2rad(_originArcRotation);
								double c = std::cos(rad), s = std::sin(rad);
								double rx = pnx*c + (dny*pnz - dnz*pny)*s;
								double ry = pny*c + (dnz*pnx - dnx*pnz)*s;
								double rz = pnz*c + (dnx*pny - dny*pnx)*s;
								disp.X += static_cast<int>(rx * arcDelta);
								disp.Y += static_cast<int>(ry * arcDelta);
								disp.Z += static_cast<int>(rz * arcDelta);
							}
							else
							{
								disp.Z += static_cast<int>(arcDelta);
							}
						}
					}
				}
			}
			else // Circle 模式
			{
				_originCircleRadius += Data->OriginCircleRadiusGrow;
				double tr = _originCircleRadius;
				if (Data->OriginCircleMaxRadius > 0 && tr > Data->OriginCircleMaxRadius) tr = Data->OriginCircleMaxRadius;
				if (Data->OriginCircleMinRadius > 0 && tr < Data->OriginCircleMinRadius) tr = Data->OriginCircleMinRadius;
				// 角步长：优先线速度/半径推算，否则用固定角速度
				double originAngleStep = Data->OriginCircleAnglePerStep;
				if (Data->OriginCircleSpeed != 0 && tr > 0)
					originAngleStep = Math::rad2deg(Data->OriginCircleSpeed / tr);
			// Lissajous>0: 累积大角旋转（增减边震荡），==0: 每帧仅增量旋转（平滑行星）
			_originCircleAngle += originAngleStep;
			double r = Data->OriginLissajous > 0.0 ? Math::deg2rad(_originCircleAngle + _originNormalRotF) : Math::deg2rad(originAngleStep + _originNormalRotF);
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
	CoordStruct centerDelta{ 0, 0, 0 };  // 初始化避免 C4701 警告
	bool useCenterTracking = false;
	if (_prevCircleCenter.X || _prevCircleCenter.Y || _prevCircleCenter.Z)
	{
		centerDelta = circleCenter - _prevCircleCenter;
		if (Data->OriginLissajous <= 0.0 && (Data->OriginCircleRadius >= 0 || Data->OriginCircleSpeed != 0 || Data->OriginLinearSpeed >= 0 || Data->OriginCircleAnglePerStep != 0.0))
			useCenterTracking = true;
	}
	_prevCircleCenter = circleCenter;

	// 圆上目标基于内部跟踪位置（非 currentPos），避免与 MoveTo 等 AE 的位移打架
	if (_circlePos.IsEmpty())
		_circlePos = currentPos;
	CoordStruct trackPos = _circlePos;
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
		bool useTiltPlane = hasNormal || (Data->AllowCircleTilt && effectiveTilt != 0.0);
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

		double rad = Math::deg2rad(angleStep + _normalRotF);
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
			result.MoveDisp.X = circleCenter.X + static_cast<int>(rL * (-sinF) + rH * (-cosF * sinT)) - _circlePos.X;
			result.MoveDisp.Y = circleCenter.Y + static_cast<int>(rL * cosF + rH * (-sinF * sinT)) - _circlePos.Y;
			result.MoveDisp.Z = circleCenter.Z + static_cast<int>(rH * cosT) - _circlePos.Z;
		}
		else
		{
			// 传统 2D 圆面（XY 平面）
			double ndx = (dx / currentDist * targetRadius);
			double ndy = (dy / currentDist * targetRadius);
			double rx = ndx * cosA - ndy * sinA;
			double ry = ndx * sinA + ndy * cosA;
			result.MoveDisp.X = circleCenter.X + static_cast<int>(rx) - _circlePos.X;
			result.MoveDisp.Y = circleCenter.Y + static_cast<int>(ry) - _circlePos.Y;
			result.MoveDisp.Z = Data->CircleOrigin.IsEmpty() && Data->OriginFLH.IsEmpty()
				? 0 : circleCenter.Z - _circlePos.Z;  // 有显式高度指定时拉 Z，否则维持抛射体自身高度
		}
		_circlePos.X += result.MoveDisp.X;
		_circlePos.Y += result.MoveDisp.Y;
		_circlePos.Z += result.MoveDisp.Z;
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
	// 成熟机制，别乱动 — 模式 1: MoveTo（纯 FLH 位移 + GrowRate）
	// ========================================================================
	if (!Data->MoveTo.IsEmpty())
	{
		DirStruct moveDir = mainFacingDir;
		double useCosT = 1.0, useSinT = 0.0;
		bool hasTilt = (effectiveTilt != 0.0);

		// Origin=Target：F 轴应以 抛射体→目标 连线为准（含 Z 落差），而非全局的 target→抛射体
		if (Data->Origin == VectorData::VectorOrigin::Target)
		{
			CoordStruct tgt {};
			bool hasTgt = false;
			if (pBullet && pBullet->Target)
				{ tgt = pBullet->Target->GetCoords(); hasTgt = true; }
			else if (pBullet)
				{ tgt = pBullet->TargetCoords; hasTgt = true; }
			else if (pTechno && pTechno->Target)
				{ tgt = pTechno->Target->GetCoords(); hasTgt = true; }
			if (hasTgt)
			{
				double tdx = static_cast<double>(tgt.X - currentPos.X);
				double tdy = static_cast<double>(tgt.Y - currentPos.Y);
				double tdz = static_cast<double>(tgt.Z - currentPos.Z);
				double tLenXY = std::sqrt(tdx * tdx + tdy * tdy);
				if (tLenXY > 1e-6)
				{
					moveDir = Radians2Dir(std::atan2(tdy, tdx)); // 官方API，不得修改
					if (Data->AllowCircleTilt)
					{
						double tLen3D = std::sqrt(tdx * tdx + tdy * tdy + tdz * tdz);
						useCosT = tLenXY / tLen3D;
						useSinT = tdz / tLen3D;
						hasTilt = true;
					}
				}
			}
		}

		if (Data->AnglePerStep != 0.0)
		{
			if (_elapsedFrames == 0)
				_currentAngle = 0.0;
			_currentAngle += Data->AnglePerStep;
			moveDir = Radians2Dir(moveDir.GetRadian() + Math::deg2rad(_currentAngle)); // 官方API，不得修改
		}

		CoordStruct grow = { static_cast<int>(Data->GrowRate.X * _movementFrames),
			static_cast<int>(Data->GrowRate.Y * _movementFrames),
			static_cast<int>(Data->GrowRate.Z * _movementFrames) };
		CoordStruct moveFlh = Data->MoveTo + grow;

		if (hasTilt)
		{
			double mf = moveDir.GetRadian();
			double cosF = std::cos(mf), sinF = std::sin(mf);
			double cosT = (Data->Origin == VectorData::VectorOrigin::Target && Data->AllowCircleTilt)
				? useCosT : std::cos(effectiveTilt);
			double sinT = (Data->Origin == VectorData::VectorOrigin::Target && Data->AllowCircleTilt)
				? useSinT : std::sin(effectiveTilt);
			double F = static_cast<double>(moveFlh.X);
			double L = static_cast<double>(moveFlh.Y);
			double H = static_cast<double>(moveFlh.Z);
			result.MoveDisp.X = static_cast<int>(F * cosF * cosT + L * (-sinF) + H * (-cosF * sinT));
			result.MoveDisp.Y = static_cast<int>(F * sinF * cosT + L * cosF + H * (-sinF * sinT));
			result.MoveDisp.Z = static_cast<int>(F * sinT + H * cosT);
		}
		else
		{
			result.MoveDisp = GetFLHAbsoluteOffset(moveFlh, moveDir); // 官方API，不得修改
		}

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

	// TargetFLH → 世界坐标：照搬 AutoWeapon/AttachFire 的成熟管线
	// 官方API，不得修改：Techno 路径走 Locomotor 矩阵，非Techno 路径走 Point2Dir+GetFLHAbsoluteCoords
	CoordStruct frameTarget;
	bool isOnTurret = !Data->OriginIsOnBody;
	switch (Data->Origin)
	{
	case VectorData::VectorOrigin::Launcher:
		{
			if (Data->OriginNoUpdate)
				frameTarget = GetFLHAbsoluteCoords(originPos, frameTargetFlh, mainFacingDir); // 锁定原点
			else
			{
				TechnoClass* pLT = abstract_cast<TechnoClass*>(_pLauncher);
				if (pLT && !IsDeadOrInvisible(pLT))
					frameTarget = GetFLHAbsoluteCoords(pLT, frameTargetFlh, isOnTurret); // 官方API，不得修改
				else
					frameTarget = GetFLHAbsoluteCoords(originPos, frameTargetFlh, mainFacingDir); // 官方API，不得修改
			}
		}
		break;
	case VectorData::VectorOrigin::Self:
		if (pTechno)
		{
			if (Data->OriginIsOnWorld)
				frameTarget = GetFLHAbsoluteCoords(originPos, frameTargetFlh, DirStruct{}); // 世界坐标系，无视倾斜
			else if (Data->OriginNoUpdate)
				frameTarget = GetFLHAbsoluteCoords(originPos, frameTargetFlh, mainFacingDir); // 锁定原点
			else
				frameTarget = GetFLHAbsoluteCoords(pTechno, frameTargetFlh, isOnTurret); // 官方API，不得修改
		}
		else if (pBullet)
		{
			if (Data->OriginIsOnWorld)
				frameTarget = GetFLHAbsoluteCoords(originPos, frameTargetFlh, DirStruct{}); // 世界坐标系，无视倾斜
			else if (Data->OriginNoUpdate)
				frameTarget = GetFLHAbsoluteCoords(originPos, frameTargetFlh, mainFacingDir); // 锁定原点
			else
				frameTarget = GetFLHAbsoluteCoords(currentPos, frameTargetFlh, Facing(pBullet, currentPos)); // 官方API，不得修改
		}
		else
			frameTarget = GetFLHAbsoluteCoords(originPos, frameTargetFlh, mainFacingDir); // 官方API，不得修改
		break;
	default: // Target / Source
		frameTarget = GetFLHAbsoluteCoords(originPos, frameTargetFlh, mainFacingDir); // 官方API，不得修改
		break;
	}

	CoordStruct dirVec;
	dirVec.X = frameTarget.X - currentPos.X;
	dirVec.Y = frameTarget.Y - currentPos.Y;
	dirVec.Z = frameTarget.Z - currentPos.Z;
	double dirLen = std::sqrt(static_cast<double>(dirVec.X * dirVec.X + dirVec.Y * dirVec.Y + dirVec.Z * dirVec.Z));

	CoordStruct resultDisp{ 0, 0, 0 };

	// ========================================================================
	// 成熟机制，别乱动 — 模式 2: ReachTarget（剩余帧数强制到达）
	// ========================================================================
	if (Data->ReachTarget && _totalDuration > 0)
	{
			int effectiveDuration = _totalDuration - Data->DisabledFrames;
		if (effectiveDuration < 1) effectiveDuration = 1;
		int remainingFrames = effectiveDuration - _movementFrames + 1;
		if (Data->ReachTargetEarlyEnd > 0 && Data->ReachTargetEarlyEnd < effectiveDuration
			&& remainingFrames <= Data->ReachTargetEarlyEnd)
		{
			Deactivate();
			AdvanceFrame();
			return result;
		}
		if (remainingFrames <= 0)
		{
			// 已超时：瞬移到目标，引擎正常到达检测会自然引爆
			if (Data->Force && pBullet)
			{
				pBullet->SetLocation(frameTarget);
				result.MoveDisp = { 0, 0, 0 };
				result.Force = false;
				Deactivate();
				AdvanceFrame();
				return result;
			}
			// Force=no 或无 pBullet：不设位移，自然结束
		}
		else if (dirLen > 1e-6)
		{
			double adjustedSpeed = dirLen / remainingFrames;
			resultDisp.X = static_cast<int>(dirVec.X / dirLen * adjustedSpeed);
			resultDisp.Y = static_cast<int>(dirVec.Y / dirLen * adjustedSpeed);
			resultDisp.Z = static_cast<int>(dirVec.Z / dirLen * adjustedSpeed);

			// 抛物线弧高（基于弧线起点，支持 ArcPeakPercent / ArcRotation）
			if (_arcHeight != 0)
			{
				if (_arcStartLocation.IsEmpty())
					_arcStartLocation = currentPos; // 首次弧线执行时抓取当前位置作为起点
				double t = static_cast<double>(_movementFrames) / effectiveDuration;
				double arcOffset = CalcArcOffsetAt(t);
				double baseX = _arcStartLocation.X + (frameTarget.X - _arcStartLocation.X) * t;
				double baseY = _arcStartLocation.Y + (frameTarget.Y - _arcStartLocation.Y) * t;
				double baseZ = _arcStartLocation.Z + (frameTarget.Z - _arcStartLocation.Z) * t;

				if (_arcRotation == 0.0)
				{
					resultDisp.X = static_cast<int>(baseX - currentPos.X);
					resultDisp.Y = static_cast<int>(baseY - currentPos.Y);
					resultDisp.Z = static_cast<int>(baseZ + arcOffset - currentPos.Z);
				}
				else
				{
					double dx = frameTarget.X - _arcStartLocation.X;
					double dy = frameTarget.Y - _arcStartLocation.Y;
					double dz = frameTarget.Z - _arcStartLocation.Z;
					double dLen = std::sqrt(dx * dx + dy * dy + dz * dz);
					if (dLen > 1e-6)
					{
						double dnx = dx / dLen, dny = dy / dLen, dnz = dz / dLen;
						double upDotD = dnz;
						double px = -dnx * upDotD, py = -dny * upDotD, pz = 1.0 - dnz * upDotD;
						double pLen = std::sqrt(px * px + py * py + pz * pz);
						if (pLen < 1e-6)
						{
							px = 1.0 - dnx * dnx; py = -dny * dnx; pz = -dnz * dnx;
							pLen = std::sqrt(px * px + py * py + pz * pz);
						}
						double pnx = px / pLen, pny = py / pLen, pnz = pz / pLen;
						double rad = Math::deg2rad(_arcRotation);
						double c = std::cos(rad), s = std::sin(rad);
						double rx = pnx * c + (dny * pnz - dnz * pny) * s;
						double ry = pny * c + (dnz * pnx - dnx * pnz) * s;
						double rz = pnz * c + (dnx * pny - dny * pnx) * s;
						resultDisp.X = static_cast<int>(baseX + rx * arcOffset - currentPos.X);
						resultDisp.Y = static_cast<int>(baseY + ry * arcOffset - currentPos.Y);
						resultDisp.Z = static_cast<int>(baseZ + rz * arcOffset - currentPos.Z);
					}
					else
					{
						resultDisp.X = static_cast<int>(baseX - currentPos.X);
						resultDisp.Y = static_cast<int>(baseY - currentPos.Y);
						resultDisp.Z = static_cast<int>(baseZ + arcOffset - currentPos.Z);
					}
				}
			}
		}
		result.MoveDisp = resultDisp;
		AdvanceFrame();
		return result;
	}

	// ========================================================================
	// 模式5: Speed（直线追踪 + 加速度 + 影子坐标弧高）
	// 影子沿 _shadowPos→frameTarget 方向推进，不受弧高Z偏移污染
	// SpeedEndOnReach 和 t 均基于影子距离判定
	// ========================================================================
	if (Data->LinearSpeed >= 0)
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

		// SpeedEndOnReach=no：不走影子系统，全程旧版 dirVec/dirLen 直追（自然产生抽搐跟随）
		if (!Data->SpeedEndOnReach)
		{
			if (dirLen > 1e-6)
			{
				resultDisp.X = static_cast<int>(dirVec.X / dirLen * speed);
				resultDisp.Y = static_cast<int>(dirVec.Y / dirLen * speed);
				resultDisp.Z = static_cast<int>(dirVec.Z / dirLen * speed);
			}
			result.MoveDisp = resultDisp;
			AdvanceFrame();
			return result;
		}

		// 以下：SpeedEndOnReach=yes，走影子系统
		// 有弧线：影子追踪完整 3D，弧高增量叠加
		double sdx = frameTarget.X - _shadowPosX;
		double sdy = frameTarget.Y - _shadowPosY;
		double sdz, shadowDist;
		if (_arcHeight != 0)
		{
			sdz = frameTarget.Z - _shadowPosZ;
			shadowDist = std::sqrt(sdx * sdx + sdy * sdy + sdz * sdz);
		}
		else
		{
			// 无弧线：仅 XY 距离，避免影子 Z 追踪目标导致 shadowDist 虚小
			sdz = 0.0;
			shadowDist = std::sqrt(sdx * sdx + sdy * sdy);
		}

		// SpeedEndOnReach：瞬移到目标位置，引擎正常到达检测会自然引爆（不手动Detonate，避免重复伤害）
		if (Data->SpeedEndOnReach && shadowDist <= speed)
		{
			if (pBullet)
			{
				pBullet->SetLocation(frameTarget);
			}
			// 零位移：位置已由 SetLocation 设定，不再让 Vector.cpp 回退
			result.MoveDisp = { 0, 0, 0 };
			result.Force = false;
			Deactivate();
			AdvanceFrame();
			return result;
		}

		if (shadowDist > 1e-6)
		{
			// 影子沿 shadow→target 方向推进
			double sInv = 1.0 / shadowDist;
			double shadowStepX = sdx * sInv * speed;
			double shadowStepY = sdy * sInv * speed;
			double shadowStepZ = 0.0;
			_shadowPosX += shadowStepX;
			_shadowPosY += shadowStepY;
			if (_arcHeight != 0)
			{
				shadowStepZ = sdz * sInv * speed;
				_shadowPosZ += shadowStepZ;
			}
			// 无弧线：_shadowPosZ 不变（始终 = _initialLocation.Z），Z 由 t 插值
			_shadowTraveled += speed;

			// 重新计算影子距离（影子已移动）
			sdx = frameTarget.X - _shadowPosX;
			sdy = frameTarget.Y - _shadowPosY;
			if (_arcHeight != 0)
			{
				sdz = frameTarget.Z - _shadowPosZ;
				shadowDist = std::sqrt(sdx * sdx + sdy * sdy + sdz * sdz);
			}
			else
			{
				shadowDist = std::sqrt(sdx * sdx + sdy * sdy);
			}

			// t = 已走路程 / 总路程（动态更新，目标移动时自动调整）
			double total = _shadowTraveled + shadowDist;
			double t = (total > 1e-6) ? _shadowTraveled / total : 0.0;
			if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0;

			// 实际位移：影子步长（XY）
			resultDisp.X = static_cast<int>(shadowStepX);
			resultDisp.Y = static_cast<int>(shadowStepY);

			if (_arcHeight != 0)
			{
				// 有弧线：Z 用影子增量 + 弧高增量叠加
				resultDisp.Z = static_cast<int>(shadowStepZ);
			}
			else
			{
				// 无弧线：Z 从抛射体起始高度 lerp 到目标高度
				// _shadowPosZ 在无弧线时冻结为抛射体起始 Z，_initialLocation.Z 可能是目标 Z（Origin=Target 时不同）
				double targetZ = _shadowPosZ + (frameTarget.Z - _shadowPosZ) * t;
				resultDisp.Z = static_cast<int>(targetZ - currentPos.Z);
			}

			if (_arcHeight != 0)
			{
				double arcOffset = CalcArcOffsetAt(t);
				double arcDelta = arcOffset - _prevArcOffset;
				_prevArcOffset = arcOffset;

				if (_arcRotation == 0.0)
				{
					// 无旋转：弧高纯 Z，增量叠加在影子 Z 之上
					resultDisp.Z += static_cast<int>(arcDelta);
				}
				else
				{
					// 旋转弧面：弧高分解到 XYZ（增量叠加，螺旋路径）
					double totalDx = frameTarget.X - _initialLocation.X;
					double totalDy = frameTarget.Y - _initialLocation.Y;
					double totalDz = frameTarget.Z - _initialLocation.Z;
					double totalDLen = std::sqrt(totalDx * totalDx + totalDy * totalDy + totalDz * totalDz);
					if (totalDLen > 1e-6)
					{
						double dnx = totalDx / totalDLen, dny = totalDy / totalDLen, dnz = totalDz / totalDLen;
						double upDotD = dnz;
						double px = -dnx * upDotD, py = -dny * upDotD, pz = 1.0 - dnz * upDotD;
						double pLen = std::sqrt(px * px + py * py + pz * pz);
						if (pLen < 1e-6)
						{
							px = 1.0 - dnx * dnx; py = -dny * dnx; pz = -dnz * dnx;
							pLen = std::sqrt(px * px + py * py + pz * pz);
						}
						double pnx = px / pLen, pny = py / pLen, pnz = pz / pLen;
						double rad = Math::deg2rad(_arcRotation);
						double c = std::cos(rad), s = std::sin(rad);
						double rx = pnx * c + (dny * pnz - dnz * pny) * s;
						double ry = pny * c + (dnz * pnx - dnx * pnz) * s;
						double rz = pnz * c + (dnx * pny - dny * pnx) * s;
						resultDisp.X += static_cast<int>(rx * arcDelta);
						resultDisp.Y += static_cast<int>(ry * arcDelta);
						resultDisp.Z += static_cast<int>(rz * arcDelta);
					}
					else
					{
						resultDisp.Z += static_cast<int>(arcDelta);
					}
				}
			}
		}
		result.MoveDisp = resultDisp;
		AdvanceFrame();
		return result;
	}

	// 没命中任何模式，返回空
	AdvanceFrame();
	return result;
}
