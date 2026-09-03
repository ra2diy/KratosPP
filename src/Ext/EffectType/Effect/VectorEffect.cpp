#include "VectorEffect.h"

#include <Utilities/Debug.h>

#include <Kamikaze.h>
#include <SpawnManagerClass.h>
#include <RocketLocomotionClass.h>

#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>
#include <Ext/Helper/Physics.h>
#include <Ext/Helper/Weapon.h>
#include <Ext/Helper/FLH.h>

#include <Ext/ObjectType/AttachEffect.h>
#include <Ext/EffectType/AttachEffectScript.h>
#include <Ext/TechnoType/TechnoStatus.h>
#include <Extension/TechnoExt.h>

#include <Ext/BulletType/BulletStatus.h>

// ============================================================================
// 目标有效性辅助（SpawnMissile 场景专用）——照搬旧版
// ============================================================================

// 从引擎 Kamikaze 控制器读导弹的目标点（现有基建，不自己造轮子）：
// KamikazeControl->Cell 存目标格子——Homing 开启时每帧更新（目标活着跟随、死亡冻结），
// 不 Homing 时由引擎写入目标 Cell。这是引擎眼中导弹真正要飞向的位置
static bool TryGetKamikazeTarget(TechnoClass* pTechno, CoordStruct& out)
{
	AircraftClass* pAircraft = pTechno ? abstract_cast<AircraftClass*>(pTechno) : nullptr;
	if (!pAircraft)
		return false;
	auto& nodes = Kamikaze::Instance->Nodes;
	for (int i = 0; i < nodes.Count; i++)
	{
		Kamikaze::KamikazeControl* pControl = nodes[i];
		if (pControl && pControl->Item == pAircraft && pControl->Cell)
		{
			out = pControl->Cell->GetCoords();
			return true;
		}
	}
	return false;
}

// 从 spawn 发射者的管理器读目标（Kamikaze Cell 的源头）：
// KamikazeTrackerClass_Add 就是用 SpawnManager->Destination 设置 Cell 的。
// 导弹未全部发射（未进 Kamikaze 容器）期间容器为空，直接读源头补全这一阶段
static bool TryGetSpawnManagerTarget(TechnoClass* pTechno, CoordStruct& out)
{
	if (pTechno && pTechno->SpawnOwner && !IsDeadOrInvisible(pTechno->SpawnOwner))
	{
		SpawnManagerClass* pSM = pTechno->SpawnOwner->SpawnManager;
		if (pSM)
		{
			if (pSM->Target)
			{
				out = pSM->Target->GetCoords();
				return true;
			}
			if (pSM->Destination)
			{
				out = pSM->Destination->GetCoords();
				return true;
			}
		}
	}
	return false;
}

// ============================================================================
// 生命周期
// ============================================================================

// 参照 MissileHoming 先例：注册 UnInit 事件，launcher/source 被删除时置空指针防悬垂
void VectorEffect::Awake()
{
	EventSystems::General.AddHandler(Events::ObjectUnInitEvent, this, &VectorEffect::OnTechnoDelete);
}

void VectorEffect::Destroy()
{
	EventSystems::General.RemoveHandler(Events::ObjectUnInitEvent, this, &VectorEffect::OnTechnoDelete);
}

void VectorEffect::Clean()
{
	EffectScript::Clean();

	_elapsedFrames = 0;
	_moveFrame = 0;
	_movementFrames = 0;
	_effectiveTimeStep = 1;
	_totalDuration = 0;

	_initialLocation = {};
	_initialOriginPos = {};
	_initialBaseCenter = {};
	_lockedTarget = {};
	_vectorAcquireZ = 0;
	_pLauncher = nullptr;
	_pSource = nullptr;

	_facingRad = 0.0;
	_facingDir = DirStruct(0);
	_tiltRad = 0.0;

	_originFacing = 0.0;
	_originTilt = 0.0;
	_baseOriginFacing = 0.0;
	_baseOriginTilt = M_PI / 2.0;

	_randomTargetOffset = {};
	_originTargetOffset = {};

	_originOffset = {};
	_prevCircleCenter = {};
	_circlePos = {};

	_motion = MotionState{};
	_originMotion = MotionState{};
}

// ============================================================================
// 公共数学函数（行为等价：逐字照搬旧版公式）
// ============================================================================

// 弧面旋转：arcDelta 按 Rodrigues 正交基分解到 XYZ。
// D = 总位移向量，rotDeg = 弧面旋转角（0=纯 Z 朝上），arcDelta = 本帧弧高增量
VectorEffect::ArcDelta3D VectorEffect::RotateArcDelta(const CoordStruct& D, double rotDeg, double arcDelta)
{
	ArcDelta3D out{ 0.0, 0.0, 0.0 };
	if (rotDeg == 0.0)
	{
		out.z = arcDelta;
		return out;
	}
	double dx = static_cast<double>(D.X);
	double dy = static_cast<double>(D.Y);
	double dz = static_cast<double>(D.Z);
	double dLen = std::sqrt(dx * dx + dy * dy + dz * dz);
	if (dLen <= 1e-6)
	{
		out.z = arcDelta;
		return out;
	}
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
	double rad = Math::deg2rad(rotDeg);
	double c = std::cos(rad), s = std::sin(rad);
	out.x = (pnx * c + (dny * pnz - dnz * pny) * s) * arcDelta;
	out.y = (pny * c + (dnz * pnx - dnx * pnz) * s) * arcDelta;
	out.z = (pnz * c + (dnx * pny - dny * pnx) * s) * arcDelta;
	return out;
}

// 3D 法向量增量旋转（绕世界 F=Y / L=X / H=Z 轴，正速度=顺时针）
void VectorEffect::RotateNormal3D(double& nx, double& ny, double& nz,
	double stepF, double stepL, double stepH)
{
	if (stepF != 0.0)
	{
		double rad = Math::deg2rad(stepF), c = std::cos(rad), s = std::sin(rad);
		double x = nx, z = nz;
		nx = x * c - z * s;
		nz = x * s + z * c;
	}
	if (stepL != 0.0)
	{
		double rad = Math::deg2rad(stepL), c = std::cos(rad), s = std::sin(rad);
		double y = ny, z = nz;
		ny = y * c + z * s;
		nz = -y * s + z * c;
	}
	if (stepH != 0.0)
	{
		double rad = Math::deg2rad(stepH), c = std::cos(rad), s = std::sin(rad);
		double x = nx, y = ny;
		nx = x * c + y * s;
		ny = -x * s + y * c;
	}
}

// 法线角速度解析：常数优先 → 区间2 50% 随机 → 区间1 随机 → 0
double VectorEffect::ResolveAngleStep(double perStep, double m1, double M1, double m2, double M2)
{
	if (perStep != 0.0) return perStep;
	if (M1 <= m1 && M2 <= m2) return 0.0;
	if (M2 > m2 && Random::RandomRanged(0, 1))
		return m2 + (M2 - m2) * Random::RandomDouble();
	return M1 > m1 ? m1 + (M1 - m1) * Random::RandomDouble() : 0.0;
}

// 三态跟踪：NoUpdate=yes → 冻结 last；no + 单位存活 → 每帧快照 last；死亡 → 冻结 last
// 主 Origin（_initialOriginPos）与大圆基座（_initialBaseCenter）共用
CoordStruct VectorEffect::TrackOriginCoord(ObjectClass* pUnit, bool noUpdate, CoordStruct& last)
{
	if (!noUpdate && pUnit && !IsDeadOrInvisible(pUnit))
		last = pUnit->GetCoords();
	return last;
}

// ========================================================================
// "取基准点"管线辅助实现（OriginFLH 偏移完整化）
// 时序统一：取基准单位 → 定坐标系(facing+tilt) → 定偏移量(FLH) → 算完整基准点。
// 挂载快照（LockFacing/补读）与每帧刷新（GetVectorResult）共用同一套逻辑；
// 其他散落取点机制（frameTarget/大圆基座等）可并入本管线。
// ========================================================================

// Origin=Target/Source/Launcher/Self 对应的单位对象（打格子/无单位返回 nullptr）
TechnoClass* VectorEffect::FindOriginTechno()
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
	return pOriginTechno;
}

// 单位倾斜角：动态倾斜（Rocker 等）优先，为 0 时从地形采样（前后 128 lepton 高度差）
double VectorEffect::SampleOriginTilt(TechnoClass* pUnit)
{
	if (!pUnit || IsDeadOrInvisible(pUnit))
		return 0.0;
	double tilt = pUnit->AngleRotatedForwards; // 官方API，不得修改
	if (std::abs(tilt) < 1e-6)
	{
		CoordStruct originCoord = pUnit->GetCoords();
		double unitFacing = pUnit->PrimaryFacing.Current().GetRadian(); // 官方API，不得修改
		double cosF = std::cos(unitFacing), sinF = std::sin(unitFacing);
		Point2D frontPt = { originCoord.X + static_cast<int>(128.0 * cosF), originCoord.Y + static_cast<int>(128.0 * sinF) };
		Point2D backPt  = { originCoord.X - static_cast<int>(128.0 * cosF), originCoord.Y - static_cast<int>(128.0 * sinF) };
		int hFront = MapClass::Instance->GetCellFloorHeight({frontPt.X, frontPt.Y, 0});
		int hBack  = MapClass::Instance->GetCellFloorHeight({backPt.X, backPt.Y, 0});
		double dz = static_cast<double>(hFront - hBack);
		double dxy = 256.0;
		tilt = (dxy > 1e-6) ? std::atan2(dz, dxy) : 0.0;
	}
	return tilt;
}

// 把偏移量按坐标系转成世界偏移加到基准坐标 → 完整基准点。
// 统一公式（无二维/三维分叉，tilt 始终纳入，tilt=0 自然退化为纯水平摆放）：
//   1. tilt 混合：绕单位 L 轴俯仰，把 F/H 分量在俯仰平面内旋转（tilt>0=头低屁股高，+F 偏下）
//      f' = f*cosT - h*sinT,  l' = l,  h' = f*sinT + h*cosT
//   2. 混合后的 (f', l', h') 整体走引擎 API GetFLHAbsoluteCoords——
//      内含 RA2 坐标系修正（RotateZ(dir) + Y 镜像，见 FLH.cpp GetFLHOffset "mirror it back"）。
//      禁止裸 cos/sin 手写旋转：引擎 DirStruct 弧度体系与数学弧度有 90° 偏置/镜像
//      （YRpp Dir.h GetRadian "LRotate 90 degrees"；BINARY_ANGLE_MAGIC 为负），
//      裸旋转会把"前方"摆成"左侧"。tilt=0 时 f'=f、h'=h，本函数退化为纯二维 API 调用。
CoordStruct VectorEffect::ApplyOriginFlh(const CoordStruct& basePos, const CoordStruct& flh,
	const DirStruct& facing, double tilt)
{
	double cosT = std::cos(tilt), sinT = std::sin(tilt);
	CoordStruct flhRot = flh;
	flhRot.X = static_cast<int>(flh.X * cosT - flh.Z * sinT);   // f'：F/H 俯仰混合
	flhRot.Z = static_cast<int>(flh.X * sinT + flh.Z * cosT);   // h'：H/F 俯仰混合
	flhRot.Y = flh.Y;                                          // l'：左右不受俯仰影响
	return GetFLHAbsoluteCoords(basePos, flhRot, facing); // 官方API，不得修改：引擎坐标系修正
}

// Target 多级读取链：缓存（跟随锁定单位）→ 弹体目标 → 弹体落点 → 单位目标 → Kamikaze/SpawnManager
bool VectorEffect::GetTargetPosFromChain(CoordStruct& out, bool preferCache)
{
	if (preferCache && pTechno)
	{
		if (TechnoStatus* status = GetStatus<TechnoExt, TechnoStatus>(pTechno))
		{
			if (status->HasVectorTargetCache())
			{
				out = status->GetVectorCachedCell();
				return true;
			}
		}
	}
	if (pBullet && pBullet->Target)
	{
		out = pBullet->Target->GetCoords(); // 抛射体目标 Cell 是真实落点
		return true;
	}
	if (pBullet)
	{
		out = pBullet->TargetCoords;
		return true;
	}
	if (pTechno && pTechno->Target)
	{
		out = pTechno->Target->GetCoords();
		return true;
	}
	if (pTechno)
	{
		CoordStruct kamikazePos{};
		if (TryGetKamikazeTarget(pTechno, kamikazePos) || TryGetSpawnManagerTarget(pTechno, kamikazePos))
		{
			out = kamikazePos;
			return true;
		}
	}
	return false;
}

// ============================================================================
// 目标缓存（挂 TechnoStatus，归一目标保护）——照搬旧版
// ============================================================================
void VectorEffect::CacheTargetNow()
{
	if (!pTechno)
		return;
	TechnoStatus* status = GetStatus<TechnoExt, TechnoStatus>(pTechno);
	if (!status)
		return;

	// --- 已有缓存：NoUpdate=no 只跟随锁定单位，不再读引擎参数 ---
	if (status->HasVectorTargetCache())
	{
		// NoUpdate=yes：锁定第一笔，不刷新
		if (Data->OriginNoUpdate)
			return;
		// NoUpdate=no：跟随锁定的单位实时位置（目标在导弹脚下也正常更新——真实目标就该下坠去打）
		AbstractClass* pCachedTarget = status->GetVectorCachedTarget();
		if (pCachedTarget)
		{
			TechnoClass* pTgt = abstract_cast<TechnoClass*>(pCachedTarget);
			if (pTgt && !IsDeadOrInvisible(pTgt))
			{
				status->SetVectorTargetCache(pCachedTarget, pTgt->GetCoords());
			}
			// 目标死亡：不写，冻结最后有效格子
		}
		// 锁定的是格子（无单位）：格子不会移动，不刷新
		return;
	}

	// --- 第一笔：引擎来源链取候选，优先单位 ---
	AbstractClass* candTarget = nullptr;
	CoordStruct candCell{};
	bool got = false;

	if (pTechno->SpawnOwner && !IsDeadOrInvisible(pTechno->SpawnOwner))
	{
		// Spawn 导弹：优先从发射者（SpawnOwner）自身攻击目标读——
		// 引擎集结阶段 SpawnManager 的 Target/Destination 可能是集结点，发射者自己瞄准的目标才是真实目标
		if (pTechno->SpawnOwner->Target)
		{
			TechnoClass* pTgt = abstract_cast<TechnoClass*>(pTechno->SpawnOwner->Target);
			if (!pTgt || !IsDeadOrInvisible(pTgt)) // 非 Techno 目标（格子）不做死亡判断
			{
				candTarget = pTechno->SpawnOwner->Target;
				candCell = pTechno->SpawnOwner->Target->GetCoords();
				got = true;
			}
		}
		// 原链兜底：SpawnManager（单位+坐标）→ Kamikaze Cell（只有格子）→ Target（单位+坐标）
		SpawnManagerClass* pSM = pTechno->SpawnOwner->SpawnManager;
		if (!got && pSM && pSM->Target)
		{
			TechnoClass* pTgt = abstract_cast<TechnoClass*>(pSM->Target);
			if (!pTgt || !IsDeadOrInvisible(pTgt)) // 非 Techno 目标（格子）不做死亡判断
			{
				candTarget = pSM->Target;
				candCell = pSM->Target->GetCoords();
				got = true;
			}
		}
		if (!got && pSM && pSM->Destination)
		{
			candCell = pSM->Destination->GetCoords();
			got = true;
		}
		if (!got)
		{
			CoordStruct kamikazePos{};
			if (TryGetKamikazeTarget(pTechno, kamikazePos))
			{
				candCell = kamikazePos;
				got = true;
			}
		}
		if (!got && pTechno->Target)
		{
			candTarget = pTechno->Target;
			candCell = pTechno->Target->GetCoords();
			got = true;
		}
	}
	else if (pTechno->Target)
	{
		// 普通单位：直接记 Target
		TechnoClass* pTgt = abstract_cast<TechnoClass*>(pTechno->Target);
		if (!pTgt || !IsDeadOrInvisible(pTgt))
		{
			candTarget = pTechno->Target;
			candCell = pTechno->Target->GetCoords();
			got = true;
		}
	}

	// 有效才写；取不到/目标死亡不写，保留最后有效值（冻结）
	if (got)
		status->SetVectorTargetCache(candTarget, candCell);
}

// ============================================================================
// OnStart 子步骤
// ============================================================================

// 计时/快照/Duration/AcquireZ
void VectorEffect::ParseCommon()
{
	_elapsedFrames = 0;
	_moveFrame = 0;
	_movementFrames = 0;
	_effectiveTimeStep = Data->TimeStep;
	// _prevCircleCenter 不在此初始化：圆心追踪依赖 Origin 移动系统首帧的 skipOriginUpdate 赋值

	_initialLocation = pObject->GetCoords();
	_vectorAcquireZ = _initialLocation.Z;  // Circle 圆心高度基准：获取 Vector 时的 Z
	_totalDuration = AE->AEData.GetDuration() / _effectiveTimeStep;
}

// TargetOffset 随机偏移（Radius / F/L/H 两套 + Angles）——照搬旧版
void VectorEffect::ParseTargetOffset()
{
	if (Data->TargetOffsetRadiusMin < Data->TargetOffsetRadiusMax)
	{
		// 半径模式：全向随机落点（与 F/L/H 互斥）
		double radius = (Data->TargetOffsetRadiusMin2 < Data->TargetOffsetRadiusMax2 && Random::RandomRanged(0, 1))
			? Random::RandomRanged(Data->TargetOffsetRadiusMin2, Data->TargetOffsetRadiusMax2)
			: Random::RandomRanged(Data->TargetOffsetRadiusMin, Data->TargetOffsetRadiusMax);
		if (Data->TargetOffsetSphere)
		{
			// 球面均匀分布：z=2u-1 面积均匀 + 经度 2πv，避免极区聚集
			double u = Random::RandomDouble() * 2.0 - 1.0;
			double phi = Random::RandomDouble() * 2.0 * M_PI;
			double rXY = radius * std::sqrt(1.0 - u * u);
			_randomTargetOffset.X = static_cast<int>(rXY * std::cos(phi));
			_randomTargetOffset.Y = static_cast<int>(rXY * std::sin(phi));
			_randomTargetOffset.Z = static_cast<int>(radius * u);
		}
		else
		{
			// XY 圆环：角度默认全向，TargetOffsetAngles 限制时按区间加权均匀
			// 消费端 GetFLHAbsoluteOffset 世界角 = -(mainFacingDir + flhAngle)，要求 = 近交点角 + deg：
			// flhAngle = -mainFacingDirSim - β - deg
			//   β = atan2 近交点世界角（目标点指向抛射体）
			//   mainFacingDirSim = 消费端 mainFacingDir 统一取 DirStruct 原值（NoUpdate 不影响坐标系）
			double flhAngle;
			bool hasAngleRange = (Data->TargetOffsetAngleMin < Data->TargetOffsetAngleMax)
				|| (Data->TargetOffsetAngleMin2 < Data->TargetOffsetAngleMax2);
			if (hasAngleRange)
			{
				// 区间加权均匀：u 落在总长度内，映射到对应区间
				double len1 = Data->TargetOffsetAngleMax - Data->TargetOffsetAngleMin;
				double len2 = Data->TargetOffsetAngleMax2 - Data->TargetOffsetAngleMin2;
				double total = (len1 > 0 ? len1 : 0.0) + (len2 > 0 ? len2 : 0.0);
				double u = Random::RandomDouble() * total;
				double deg;
				if (len1 > 0 && u < len1)
					deg = Data->TargetOffsetAngleMin + u;
				else
					deg = Data->TargetOffsetAngleMin2 + (u - (len1 > 0 ? len1 : 0.0));
				// 近交点基准：targetPos 取 pBullet->TargetCoords（与 OnStart 326 行 _facingDir 一致）
				bool hasBase = false;
				CoordStruct bulletPos = pObject->GetCoords();
				CoordStruct targetPos{};
				if (pBullet)
				{
					targetPos = pBullet->TargetCoords;
					hasBase = true;
				}
				else if (pTechno && pTechno->Target)
				{
					targetPos = pTechno->Target->GetCoords();
					hasBase = true;
				}
				if (hasBase)
				{
					double beta = std::atan2(bulletPos.Y - targetPos.Y, bulletPos.X - targetPos.X); // 近交点世界角
					double alpha = Point2Dir(targetPos, bulletPos).GetRadian(); // 近交点 DirStruct 角
					// 消费端 mainFacingDir 统一取 DirStruct 原值（NoUpdate 不影响坐标系），直接复刻
					double mainFacingDirSim = alpha;
					flhAngle = -mainFacingDirSim - beta - Math::deg2rad(deg);
				}
				else
				{
					flhAngle = Random::RandomDouble() * 2.0 * M_PI; // 拿不到连线方向：回退全向
				}
			}
			else
			{
				flhAngle = Random::RandomDouble() * 2.0 * M_PI;  // 无角度限制：全向
			}
			if (!Data->TargetOffsetNormal.IsEmpty())
			{
				// TargetOffsetNormal：随机落点在倾斜圆面上（法向量定义圆面），FLH 局部计算
				// 法向量分量即 FLH（.X=F、.Y=L、.Z=H），落点也是 FLH 坐标（消费端 GetFLHAbsoluteCoords 旋转到世界）
				// facing/tilt = 法线在 FL 平面的方位角 / 仰角（tilt=PI/2 法线朝上=水平圆环，与现有行为等价）
				// 倾斜圆面取点：rL=radius*cos(flhAngle) 沿 L 切向、rH=radius*sin(flhAngle) 沿 H 方向，映射回 XYZ
				// 注：flhAngle 的 0 度基准与水平圆环存在 90° 偏移（H 轴在法线朝上时指向 -F），全向随机时无影响
				double fwX = static_cast<double>(Data->TargetOffsetNormal.Y); // L → X
				double fwY = static_cast<double>(Data->TargetOffsetNormal.X); // F → Y
				double fwZ = static_cast<double>(Data->TargetOffsetNormal.Z); // H → Z
				double lenXY = std::sqrt(fwX * fwX + fwY * fwY);
				double facing = lenXY > 1e-6 ? std::atan2(fwY, fwX) : 0.0;
				double tilt = lenXY > 1e-6 ? std::atan2(fwZ, lenXY) : (fwZ > 0 ? M_PI / 2.0 : -M_PI / 2.0);
				double cosF = std::cos(facing), sinF = std::sin(facing);
				double cosT = std::cos(tilt), sinT = std::sin(tilt);
				double rL = radius * std::cos(flhAngle);
				double rH = radius * std::sin(flhAngle);
				_randomTargetOffset.X = static_cast<int>(rL * (-sinF) + rH * (-cosF * sinT));
				_randomTargetOffset.Y = static_cast<int>(rL * cosF + rH * (-sinF * sinT));
				_randomTargetOffset.Z = static_cast<int>(rH * cosT);
				// 选 B：倾斜面 Z 再叠加 TargetOffsetH 偏移（倾斜面 + 高度抖动）
				if (Data->TargetOffsetHMin2 < Data->TargetOffsetHMax2 && Random::RandomRanged(0, 1))
					_randomTargetOffset.Z += Random::RandomRanged(Data->TargetOffsetHMin2, Data->TargetOffsetHMax2);
				else if (Data->TargetOffsetHMin < Data->TargetOffsetHMax)
					_randomTargetOffset.Z += Random::RandomRanged(Data->TargetOffsetHMin, Data->TargetOffsetHMax);
			}
			else
			{
				// 原水平圆环：X/Y 在 FL 平面，Z 独立用 TargetOffsetH 随机
				_randomTargetOffset.X = static_cast<int>(radius * std::cos(flhAngle));
				_randomTargetOffset.Y = static_cast<int>(radius * std::sin(flhAngle));
				_randomTargetOffset.Z = (Data->TargetOffsetHMin2 < Data->TargetOffsetHMax2 && Random::RandomRanged(0, 1))
					? Random::RandomRanged(Data->TargetOffsetHMin2, Data->TargetOffsetHMax2)
					: (Data->TargetOffsetHMin < Data->TargetOffsetHMax
						? Random::RandomRanged(Data->TargetOffsetHMin, Data->TargetOffsetHMax) : 0);
			}
		}
	}
	else
	{
		// F/L/H 模式：区间2有效且50%取区间2，否则区间1（无效给0）
		_randomTargetOffset.X = (Data->TargetOffsetFMin2 < Data->TargetOffsetFMax2 && Random::RandomRanged(0, 1))
			? Random::RandomRanged(Data->TargetOffsetFMin2, Data->TargetOffsetFMax2)
			: (Data->TargetOffsetFMin < Data->TargetOffsetFMax
				? Random::RandomRanged(Data->TargetOffsetFMin, Data->TargetOffsetFMax) : 0);
		_randomTargetOffset.Y = (Data->TargetOffsetLMin2 < Data->TargetOffsetLMax2 && Random::RandomRanged(0, 1))
			? Random::RandomRanged(Data->TargetOffsetLMin2, Data->TargetOffsetLMax2)
			: (Data->TargetOffsetLMin < Data->TargetOffsetLMax
				? Random::RandomRanged(Data->TargetOffsetLMin, Data->TargetOffsetLMax) : 0);
		_randomTargetOffset.Z = (Data->TargetOffsetHMin2 < Data->TargetOffsetHMax2 && Random::RandomRanged(0, 1))
			? Random::RandomRanged(Data->TargetOffsetHMin2, Data->TargetOffsetHMax2)
			: (Data->TargetOffsetHMin < Data->TargetOffsetHMax
				? Random::RandomRanged(Data->TargetOffsetHMin, Data->TargetOffsetHMax) : 0);
	}
}

// 弧参数三件套（rotation/height/peakPercent 随机解析）——照搬旧版
// origin=false 主，true 大圆
void VectorEffect::ParseArcParams(bool origin)
{
	if (!origin)
	{
		// --- 主弧参数 ---
		_motion.arcRotation = Data->ArcRotation;
		if (Data->ArcRandomRotationMax > Data->ArcRandomRotationMin)
			_motion.arcRotation = Data->ArcRandomRotationMin + (Data->ArcRandomRotationMax - Data->ArcRandomRotationMin) * Random::RandomDouble();

		_motion.arcHeight = Data->ArcHeight;
		if (Data->ArcRandomHeightMax > Data->ArcRandomHeightMin)
			_motion.arcHeight = Random::RandomRanged(Data->ArcRandomHeightMin, Data->ArcRandomHeightMax);

		_motion.arcPeakPercent = Data->ArcPeakPercent / 100.0;
		if (Data->ArcPeakRandomPercent.X < Data->ArcPeakRandomPercent.Y)
			_motion.arcPeakPercent = Random::RandomRanged(Data->ArcPeakRandomPercent.X, Data->ArcPeakRandomPercent.Y) / 100.0;
		if (_motion.arcPeakPercent <= 0.0) _motion.arcPeakPercent = 0.5;
		if (_motion.arcPeakPercent >= 1.0) _motion.arcPeakPercent = 0.5;
	}
	else
	{
		// --- 大圆弧参数（镜像主弧）---
		_originMotion.arcRotation = Data->OriginArcRotation;
		if (Data->OriginArcRandomRotationMax > Data->OriginArcRandomRotationMin)
			_originMotion.arcRotation = Data->OriginArcRandomRotationMin + (Data->OriginArcRandomRotationMax - Data->OriginArcRandomRotationMin) * Random::RandomDouble();

		_originMotion.arcHeight = Data->OriginArcHeight;
		if (Data->OriginArcRandomHeightMax > Data->OriginArcRandomHeightMin)
			_originMotion.arcHeight = Random::RandomRanged(Data->OriginArcRandomHeightMin, Data->OriginArcRandomHeightMax);

		_originMotion.arcPeakPercent = Data->OriginArcPeakPercent / 100.0;
		if (Data->OriginArcPeakRandomPercent.X < Data->OriginArcPeakRandomPercent.Y)
			_originMotion.arcPeakPercent = Random::RandomRanged(Data->OriginArcPeakRandomPercent.X, Data->OriginArcPeakRandomPercent.Y) / 100.0;
		if (_originMotion.arcPeakPercent <= 0.0) _originMotion.arcPeakPercent = 0.5;
		if (_originMotion.arcPeakPercent >= 1.0) _originMotion.arcPeakPercent = 0.5;
	}
}

// 初始速度（LinearSpeed/单位 Speed/弹体 Speed/随机）——照搬旧版
void VectorEffect::ParseSpeed()
{
	_motion.speed = 0.0;
	if (Data->LinearSpeed >= 0)
	{
		_motion.speed = static_cast<double>(Data->LinearSpeed);
	}
	else if (pTechno)
	{
		TechnoTypeClass* pType = pTechno->GetTechnoType();
		if (GetLocoType(pTechno) == LocoType::Jumpjet)
			_motion.speed = pType->JumpjetSpeed;
		else
			_motion.speed = pType->Speed;
	}
	else if (pBullet)
	{
		_motion.speed = pBullet->Speed;
	}
	// Speed 模式随机速度
	if (Data->RandomSpeedMax > Data->RandomSpeedMin)
	{
		_motion.speed = Random::RandomRanged(Data->RandomSpeedMin, Data->RandomSpeedMax);
	}
}

// 基础参考系 + 目标缓存 + 按 Origin 锁定基线——照搬旧版
void VectorEffect::InitOrigin()
{
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

	// 挂载即初始化目标缓存（无论本段 Origin 是不是 Target，都记录）
	// 保证后续 Target 段（可能目标已死）有坐标可读；NoUpdate=yes 时这里就是"存一次即停"的第一笔
	CacheTargetNow();

	// --- Origin 初始化：锁定基线 ---
	switch (Data->Origin)
	{
	case VectorData::VectorOrigin::Target:
		// 无论 NoUpdate 都锁定基线（OnStart 时引擎目标还是真实的）：NoUpdate=yes 直接用它，
		// no 每帧刷新覆盖；引擎目标失效时回退此基线（保证打地面轨迹一致）
		// 锚点 = 引擎 Kamikaze 控制器存的目标点（Homing 更新/引擎写入，目标死亡冻结）→ 攻击目标 → 自身
		if (pTechno)
		{
			CoordStruct kamikazePos{};
			bool gotKamikaze = TryGetKamikazeTarget(pTechno, kamikazePos);
			// ① TechnoStatus 目标缓存（挂载时固化；只读格子，目标死活不影响）
			TechnoStatus* cacheStatus = GetStatus<TechnoExt, TechnoStatus>(pTechno);
			if (cacheStatus && cacheStatus->HasVectorTargetCache())
			{
				_initialOriginPos = cacheStatus->GetVectorCachedCell();
			}
			else if (gotKamikaze)
			{
				_initialOriginPos = kamikazePos;
			}
			else if (TryGetSpawnManagerTarget(pTechno, _initialOriginPos))
			{
				// 未进 Kamikaze 容器（导弹未全部发射）时读源头：SpawnManager 目标
			}
			else if (pTechno->Target)
			{
				_initialOriginPos = pTechno->Target->GetCoords();
			}
			else
			{
				// Kamikaze 容器此刻可能还没加入导弹（发射后才加入）：不锁自身，
				// 留空由 GetVectorResult 首帧补读
				_initialOriginPos = CoordStruct::Empty;
			}
		}
		else if (pBullet)
		{
			_initialOriginPos = pBullet->TargetCoords;
		}
		else
		{
			_initialOriginPos = pObject->GetCoords();
		}
		break;

	case VectorData::VectorOrigin::Launcher:
		// 无论 NoUpdate 都锁定基线（与 Target 分支一致）：NoUpdate=yes 直接用，
		// no 每帧快照刷新覆盖；launcher 死亡时冻结此基线作为 origin 解算起点
		if (pBullet && pBullet->Owner)
			_initialOriginPos = pBullet->Owner->GetCoords();
		else if (pTechno)
			_initialOriginPos = pTechno->GetCoords();
		else
			_initialOriginPos = pObject->GetCoords();
		break;

	case VectorData::VectorOrigin::Source:
		// 无论 NoUpdate 都锁定基线（与 Target/Launcher 分支一致）：死亡时冻结此基线
		if (AE && AE->pSource)
			_initialOriginPos = AE->pSource->GetCoords();
		else
			_initialOriginPos = pObject->GetCoords(); // 兜底与 Target 分支一致
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
}

// 朝向锁定：NormalVector 处理 + 法向量初始化 + 角速度解析 + 非 Normal 的 Origin 朝向——照搬旧版
void VectorEffect::LockFacing()
{
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
		// 法向量球坐标：facing = 法线在 XY 平面的方位角，tilt = 法线仰角（与水平面的夹角）
		// 注意：tilt 是"仰角"不是"偏离垂直的角度"——tilt=PI/2 表示法线垂直向上（水平圆面），
		// tilt=0 表示法线水平（侧立圆面）。与倾斜圆面取点数学（useTiltPlane 分支）的语义配套。
		_facingRad = lenXY > 1e-6 ? std::atan2(fwY, fwX) : 0.0;
		_tiltRad = lenXY > 1e-6 ? std::atan2(fwZ, lenXY) : (fwZ > 0 ? M_PI / 2.0 : -M_PI / 2.0);
	}
	else
	{
		_tiltRad = 0.0;
	}

	// 法线旋转角速度解析（常数优先，否则随机）
	_motion.normalStepF = ResolveAngleStep(Data->NormalFAnglePerStep, Data->NormalFAngleRMin, Data->NormalFAngleRMax, Data->NormalFAngleRMin2, Data->NormalFAngleRMax2);
	_motion.normalStepL = ResolveAngleStep(Data->NormalLAnglePerStep, Data->NormalLAngleRMin, Data->NormalLAngleRMax, Data->NormalLAngleRMin2, Data->NormalLAngleRMax2);
	_motion.normalStepH = ResolveAngleStep(Data->NormalHAnglePerStep, Data->NormalHAngleRMin, Data->NormalHAngleRMax, Data->NormalHAngleRMin2, Data->NormalHAngleRMax2);
	_motion.lissajousStep = Data->Lissajous;

	// 初始化 3D 法向量（从球坐标 _facingRad/_tiltRad 转换）
	// 球坐标→笛卡尔：X=cos(tilt)cos(facing), Y=cos(tilt)sin(facing), Z=sin(tilt)
	// _motion.normalX/Y/Z 是世界单位法向量（圆面法线方向）：
	//   facing 影响法线在 XY 平面的指向，tilt 影响法线仰角（tilt=PI/2 → (0,0,1) 垂直向上）
	{
		double ct = std::cos(_tiltRad), st = std::sin(_tiltRad);
		double cf = std::cos(_facingRad), sf = std::sin(_facingRad);
		_motion.normalX = ct * cf;
		_motion.normalY = ct * sf;
		_motion.normalZ = st;
	}

	// --- F 轴基准挂载锁定（摆放 FLH 用；与 NormalVector 彻底解耦）---
	// F 轴参考系来源：IsOnOrigin=yes 用 Origin 单位自身朝向，no 用 Origin→弹体连线
	// （默认值按 Origin 类型推导：Launcher/Self→yes，Target/Source→no，见 VectorData.h 解析处）
	// 无论设不设 NormalVector 都执行（NormalVector 只决定圆面倾斜，不得参与 F 轴基准取值）。
	// _facingRad 的同步只在 !hasNormal 做（hasNormal 时 _facingRad 归法向量维护，见上方球坐标段）。
	{
		switch (Data->Origin)
		{
		case VectorData::VectorOrigin::Launcher:
		{
			TechnoClass* pLauncherTechno = abstract_cast<TechnoClass*>(_pLauncher);
			if (pLauncherTechno && !IsDeadOrInvisible(pLauncherTechno))
			{
				if (Data->IsOnOrigin)
				{
					_facingDir = Data->OriginIsOnBody
						? pLauncherTechno->PrimaryFacing.Current()     // 官方API，不得修改
						: pLauncherTechno->TurretFacing().Current();   // 官方API，不得修改
				}
				else
				{
					_facingDir = Point2Dir(pLauncherTechno->GetCoords(), pObject->GetCoords()); // 发射者→弹体连线
				}
				if (!hasNormal) _facingRad = _facingDir.GetRadian();
			}
			break;
		}

		case VectorData::VectorOrigin::Target:
		{
			// F 轴：yes=目标单位自身朝向（目标无朝向/格子时回退连线），no=目标→弹体连线
			CoordStruct targetPos{};
			bool hasTarget = false;
			if (pTechno && pTechno->Target)
			{
				targetPos = pTechno->Target->GetCoords();
				hasTarget = true;
			}
			else if (pBullet)
			{
				targetPos = pBullet->TargetCoords;
				hasTarget = true;
			}
			if (hasTarget)
			{
				if (Data->IsOnOrigin)
				{
					AbstractClass* pTgt = pTechno ? pTechno->Target : (pBullet ? pBullet->Target : nullptr);
					TechnoClass* pTargetTechno = abstract_cast<TechnoClass*>(pTgt);
					if (pTargetTechno && !IsDeadOrInvisible(pTargetTechno))
					{
						_facingDir = Data->OriginIsOnBody
							? pTargetTechno->PrimaryFacing.Current()     // 官方API，不得修改
							: pTargetTechno->TurretFacing().Current();   // 官方API，不得修改
						if (!hasNormal) _facingRad = _facingDir.GetRadian();
						break;
					}
					// 目标无朝向（格子）：回退连线
				}
				_facingDir = Point2Dir(targetPos, pObject->GetCoords()); // 官方API，不得修改：目标→弹体连线
				if (!hasNormal) _facingRad = _facingDir.GetRadian();
			}
			break;
		}

		case VectorData::VectorOrigin::Source:
		{
			// F 轴：yes=Source 单位自身朝向（无朝向回退连线），no=Source→弹体连线
			if (AE && AE->pSource)
			{
				CoordStruct sourcePos = AE->pSource->GetCoords();
				if (Data->IsOnOrigin)
				{
					TechnoClass* pSourceTechno = abstract_cast<TechnoClass*>(AE->pSource);
					if (pSourceTechno && !IsDeadOrInvisible(pSourceTechno))
					{
						_facingDir = Data->OriginIsOnBody
							? pSourceTechno->PrimaryFacing.Current()     // 官方API，不得修改
							: pSourceTechno->TurretFacing().Current();   // 官方API，不得修改
						if (!hasNormal) _facingRad = _facingDir.GetRadian();
						break;
					}
					// 来源无朝向：回退连线
				}
				_facingDir = Point2Dir(sourcePos, pObject->GetCoords()); // 官方API：Source→弹体连线
				if (!hasNormal) _facingRad = _facingDir.GetRadian();
			}
			break;
		}

		default: // Self（自身朝向即 F 轴，无"另一单位朝向"，IsOnOrigin 不区分）
		{
			// Self 的 _facingDir/_facingRad 挂载值 InitOrigin 已按自身朝向设好；这里只在 !hasNormal
			// 重同步一次 _facingRad（hasNormal 时 _facingRad 归法向量维护，Self 也不例外）。
			if (!hasNormal)
			{
				if (pBullet)
					_facingRad = std::atan2(pBullet->Velocity.X, pBullet->Velocity.Y);
				else if (pTechno)
					_facingRad = pTechno->TurretFacing().Current().GetRadian();
			}
			break;
		}
		}
	}

	// OriginFLH 挂载偏移算进存档点：取基准时就把偏移算好，NoUpdate=yes 直接用（完整基准点冻结）。
	// 统一走"取基准点"管线 ApplyOriginFlh（挂载快照 = 生效瞬间一次，之后 yes 冻结）：
	//   AllowOriginTilt=no：facing=_facingDir（IsOnOrigin 锁定朝向，与 NormalVector 解耦），tilt=0
	//   AllowOriginTilt=yes：facing=Origin 单位车身朝向（PrimaryFacing——车身倾斜固定、
	//     不随炮塔转），tilt=挂载瞬间采样倾斜角。姿态随快照定死，NoUpdate=yes 后不跟随单位转身/俯仰。
	// Self 排除：Self 在 InitOrigin 已用自身朝向算过偏移（避免算两次）。
	// 存档点为空（techno 侧 Origin=Target 挂载瞬间目标未就绪，留待首帧补读）时不在此算，
	// 等补读段补一次同款计算。
	if (!Data->OriginFLH.IsEmpty() && Data->Origin != VectorData::VectorOrigin::Self
		&& !_initialOriginPos.IsEmpty())
	{
		if (Data->AllowOriginTilt)
		{
			TechnoClass* pOriginTechno = FindOriginTechno();
			if (pOriginTechno && !IsDeadOrInvisible(pOriginTechno))
			{
				_initialOriginPos = ApplyOriginFlh(_initialOriginPos, Data->OriginFLH,
					pOriginTechno->PrimaryFacing.Current(), SampleOriginTilt(pOriginTechno)); // 官方API，不得修改
			}
		}
		else
		{
			_initialOriginPos = ApplyOriginFlh(_initialOriginPos, Data->OriginFLH, _facingDir, 0.0);
		}
	}

	// TargetOffsetNormal 世界固定（IsNormalOnOrigin=no）：把 FLH 落点按锁定朝向转成世界坐标，
	// 消费端把偏移叠加在旋转后的 TargetFLH 上，不随 F 轴（单位朝向）转动。
	// 注：hasNormal 时 _facingDir 未锁定（默认朝向），该组合的世界固定基准按实测调整。
	if (!Data->IsNormalOnOrigin && !Data->TargetOffsetNormal.IsEmpty())
	{
		_randomTargetOffset = GetFLHAbsoluteCoords(CoordStruct::Empty, _randomTargetOffset, _facingDir); // 官方API
	}
}

// ============================================================================
// OnStart：编排
// ============================================================================
void VectorEffect::OnStart()
{
	if (pTechno && pTechno->WhatAmI() == AbstractType::Building)
	{
		Deactivate();
		return;
	}

	ParseCommon();

	// 影子坐标（Speed 模式弧高进度基准，不受弧高 Z 偏移污染）
	_motion.shadowX = _initialLocation.X;
	_motion.shadowY = _initialLocation.Y;
	_motion.shadowZ = _initialLocation.Z;
	_motion.shadowTraveled = 0.0;

	ParseTargetOffset();
	ParseArcParams(false); // 主弧
	ParseArcParams(true);  // 大圆弧
	ParseSpeed();
	InitOrigin();
	LockFacing();
}

// ============================================================================
VectorResult VectorEffect::GetVectorResult()
{
	VectorResult result;
	// Vector 接管期悬崖/撞地引爆免疫（Vector.SubjectToCliffs=no 默认免疫；yes 则受悬崖影响爆炸）
	// 即使本帧无位移（DisabledFrames/启动前），接管期内都应放行
	result.SubjectToCliffs = Data->SubjectToCliffs;

	// 首帧快照（仅一次，供弧高计算等）
	if (_elapsedFrames == 0)
		_initialLocation = pObject->GetCoords();

	// InitialDelay 期间 AE 存在但未启动，不施加任何位移
	if (!_started)
	{
		AdvanceFrame();
		return result;
	}

	// 每帧运动刷新目标缓存：挂载时（OnStart）已写第一笔，这里跟随目标移动刷新；
	// 目标死亡时停止写入，冻结最后有效值
	CacheTargetNow();

	// 目标坐标固化：OnStart 未锁定（Pending）时补读，读到即锁定到 _initialOriginPos，
	// 防止引擎后续清空（目标死亡/管理器清空）导致 frameTarget 失效
	if (Data->Origin == VectorData::VectorOrigin::Target
		&& _initialOriginPos.IsEmpty() && pTechno)
	{
		CoordStruct targetPos{};
		bool got = false;
		if (TechnoStatus* status = GetStatus<TechnoExt, TechnoStatus>(pTechno))
		{
			if (status->HasVectorTargetCache())
			{
				targetPos = status->GetVectorCachedCell();
				got = true;
			}
		}
		if (!got && TryGetKamikazeTarget(pTechno, targetPos)) got = true;
		if (!got && TryGetSpawnManagerTarget(pTechno, targetPos)) got = true;
		if (!got && pTechno->Target) { targetPos = pTechno->Target->GetCoords(); got = true; }
		if (got)
		{
			_initialOriginPos = targetPos;
			// NoUpdate=yes：每帧偏移计算（上方管线段）只对 no 执行，补读的裸坐标必须在此叠一次并冻结
			// （techno 侧 SpawnManager/Aircraft 挂载瞬间目标未就绪，挂载叠被空存档守卫拦住，
			//  目标到手后补上这次"位置 + 朝向 + 偏移"计算，与 LockFacing 末尾挂载叠同语义。
			//  二维路径走共享管线 ApplyOriginFlh；三维（AllowOriginTilt=yes）+ 补读组合暂未覆盖）
			if (Data->OriginNoUpdate && !Data->AllowOriginTilt && !Data->OriginFLH.IsEmpty())
			{
				// F 轴基准永远按 IsOnOrigin 现算（与 NormalVector 解耦——它只决定圆面倾斜）：
				// yes=目标单位自身朝向，无朝向（格子/死亡）回退 目标点→弹体 连线；no=连线。
				DirStruct flhFacing;
				if (Data->IsOnOrigin)
				{
					TechnoClass* pTT = abstract_cast<TechnoClass*>(pTechno->Target);
					if (pTT && !IsDeadOrInvisible(pTT))
						flhFacing = Data->OriginIsOnBody
							? pTT->PrimaryFacing.Current()     // 官方API，不得修改
							: pTT->TurretFacing().Current();   // 官方API，不得修改
					else
						flhFacing = Point2Dir(targetPos, pObject->GetCoords()); // 目标无朝向：回退连线
				}
				else
				{
					flhFacing = Point2Dir(targetPos, pObject->GetCoords()); // 目标→弹体连线（IsOnOrigin 默认 no）
				}
				_initialOriginPos = ApplyOriginFlh(_initialOriginPos, Data->OriginFLH, flhFacing, 0.0);
			}
		}
	}

	// Force 必须在闸门之前设，确保非运动帧也走 SetLocation（Freeze 等效）
	result.Force = Data->Force;
	result.AllowFallingDestroy = Data->AllowFallingDestroy;
	result.FallingDestroyHeight = Data->FallingDestroyHeight;
	result.AllowRotateUnit = Data->SyncFacing; // 成熟机制：单位端同步朝向，删改前确认

	// Circle 预初始化：在 DisabledFrames 冻结前完成，保证首帧后参数可用
	if (_elapsedFrames == 0)
	{
		_motion.circleSpeed = static_cast<double>(Data->CircleSpeed);
		if (_motion.circleSpeed <= 0.0)
		{
			if (pBullet)
				_motion.circleSpeed = pBullet->Speed;
			else if (pTechno)
				_motion.circleSpeed = pTechno->GetTechnoType()->Speed;
		}
		_motion.circleAngle = Data->CircleAnglePerStep;
		if (Data->CircleRandomAngleMax > Data->CircleRandomAngleMin)
		{
			if (Data->CircleRandomAngleMax2 > Data->CircleRandomAngleMin2 && Random::RandomRanged(0, 1))
				_motion.circleAngle = Data->CircleRandomAngleMin2 + (Data->CircleRandomAngleMax2 - Data->CircleRandomAngleMin2) * Random::RandomDouble();
			else
				_motion.circleAngle = Data->CircleRandomAngleMin + (Data->CircleRandomAngleMax - Data->CircleRandomAngleMin) * Random::RandomDouble();
		}
		_motion.circleRadius = static_cast<double>(Data->CircleRadius);
		if (Data->CircleRandomRadiusMax > Data->CircleRandomRadiusMin)
			_motion.circleRadius = Random::RandomRanged(Data->CircleRandomRadiusMin, Data->CircleRandomRadiusMax);
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
		result.MoveDisp = result.FrozenPos - currentPos;
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

	_motion.normalRotF += _motion.lissajousStep;
	// 3D 法向量增量旋转（绕世界 F=Y / L=X / H=Z 轴，正速度=顺时针）
	if (_motion.normalStepF != 0.0 || _motion.normalStepL != 0.0 || _motion.normalStepH != 0.0)
	{
		RotateNormal3D(_motion.normalX, _motion.normalY, _motion.normalZ,
			_motion.normalStepF, _motion.normalStepL, _motion.normalStepH);
	}

	CoordStruct currentPos = pObject->GetCoords();

	// ========================================================================
	// 动态 F 轴：非 NoUpdate 时每帧根据当前坐标重新计算 FLH 朝向
	// ========================================================================

	// originTerrainTilt：Origin 单位倾斜角（AngleRotatedForwards 动态倾斜优先，否则地形采样）。
	// 供多处使用：AllowOriginTilt 的 OriginFLH/CircleOrigin 旋转、大圆 OriginAllowOriginTilt 的
	// oTilt 叠加、IsNormalOnOrigin 的法向量随单位转动。只计算不注入（法向量跟随由 IsNormalOnOrigin
	// 段负责）。采样逻辑与挂载快照共用 SampleOriginTilt。
	double originTerrainTilt = 0.0;
	bool hasCircleForTilt = Data->CircleRadius > 0 || Data->CircleAnglePerStep > 0.0
		|| (Data->CircleRandomRadiusMax > Data->CircleRandomRadiusMin)
		|| (Data->CircleRandomAngleMax > Data->CircleRandomAngleMin);
	if ((Data->AllowOriginTilt || Data->OriginAllowOriginTilt || Data->IsNormalOnOrigin) && hasCircleForTilt && !Data->OriginIsOnWorld)
	{
		originTerrainTilt = SampleOriginTilt(FindOriginTechno());
	}

	double effectiveFacing = _facingRad;    // 倾斜域初始（hasNormal 时 = 法向量球坐标 facing；!hasNormal 时下方同步为 F 轴基准）
	double effectiveTilt = _tiltRad;        // 倾斜域初始（hasNormal 时 = 法向量 tilt）
	// F 轴基准（摆放 FLH 用）初值 = 挂载时按 IsOnOrigin 锁定的 _facingDir（与 NormalVector 解耦）。
	// 不用 Radians2Dir(_facingRad)：hasNormal 时 _facingRad 是法向量 facing，会污染摆放基准。
	// （Radians2Dir(GetRadian()) 往返另有 90° 偏置，_facingDir 保留 DirStruct 原值不往返）
	DirStruct mainFacingDir = _facingDir; // 官方API，不得修改
	// Target/Source/Launcher/Self：统一用 OnStart 存的 DirStruct 作基础朝向。
	// NoUpdate 只决定原点是否动态刷新，不影响坐标系计算。
	bool hasNormal = !Data->NormalVector.IsEmpty()
		|| Data->NormalRandomF.Y > Data->NormalRandomF.X
		|| Data->NormalRandomL.Y > Data->NormalRandomL.X
		|| Data->NormalRandomH.Y > Data->NormalRandomH.X;
	// 倾斜域同步：!hasNormal 时 effectiveFacing 取 F 轴基准（摆放与倾斜同源同值）；
	// hasNormal 时 effectiveFacing 归法向量维护，不在此同步。
	if (!hasNormal)
	{
		effectiveFacing = _facingDir.GetRadian();
	}

	// OriginIsOnWorld：锁定世界坐标系（朝北），覆盖 Origin 朝向
	if (Data->OriginIsOnWorld)
	{
		mainFacingDir = DirStruct{};
		effectiveFacing = 0.0;
		effectiveTilt = 0.0;
	}

	// 统一朝向算法（每帧）：计算 F 轴基准 mainFacingDir（摆放 FLH 用，与 NormalVector 解耦）——
	// F 轴来源：IsOnOrigin=yes 用 Origin 单位自身朝向，no 用 Origin→弹体连线（默认值按 Origin 类型推导）。
	// 无论设不设 NormalVector 都执行：NormalVector 只决定圆面倾斜，不得参与 F 轴基准取值。
	// 段内对倾斜量（effectiveFacing/effectiveTilt）的同步只在 !hasNormal 执行
	// （hasNormal 时倾斜量归法向量维护，见 IsNormalOnOrigin 段）。
	// 另：NoUpdate 只切换计算点（锁定 _initialOriginPos vs 实时坐标），不切换坐标系/朝向算法。
	if (!Data->AllowOriginTilt && !Data->OriginIsOnWorld)
	{
		switch (Data->Origin)
		{
		case VectorData::VectorOrigin::Source:
			// 计算点：NoUpdate=yes 用锁定值，no 每帧刷新（三态跟踪：死亡冻结）
			TrackOriginCoord(_pSource, Data->OriginNoUpdate, _initialOriginPos);
			if (!_initialOriginPos.IsEmpty())
			{
				if (Data->IsOnOrigin)
				{
					TechnoClass* pSourceTechno = abstract_cast<TechnoClass*>(_pSource);
					if (pSourceTechno && !IsDeadOrInvisible(pSourceTechno))
					{
						mainFacingDir = Data->OriginIsOnBody
							? pSourceTechno->PrimaryFacing.Current()     // 官方API，不得修改
							: pSourceTechno->TurretFacing().Current();   // 官方API，不得修改
						if (!hasNormal) effectiveFacing = mainFacingDir.GetRadian();
						break;
					}
					// 来源无朝向：回退连线
				}
				// 来源活着或已死亡：都用快照算朝向（死亡后冻结指向死亡点）
				mainFacingDir = Point2Dir(_initialOriginPos, currentPos); // 官方API，不得修改
				if (!hasNormal) effectiveFacing = mainFacingDir.GetRadian();
				double dx = currentPos.X - _initialOriginPos.X;
				double dy = currentPos.Y - _initialOriginPos.Y;
				double dz = currentPos.Z - _initialOriginPos.Z;
				double lenXY = std::sqrt(dx*dx + dy*dy);
				if (!hasNormal) effectiveTilt = (lenXY > 1e-6 && Data->AllowCircleTilt) ? std::atan2(dz, lenXY) : 0.0;
			}
			break;
		case VectorData::VectorOrigin::Target:
		{
			bool isGround = (pBullet && !abstract_cast<TechnoClass*>(pBullet->Target));
			if (isGround && _movementFrames > 1)
				break;
			// 计算点：默认锁定值起步，NoUpdate=no 才走缓存/引擎链动态获取
			CoordStruct targetPos = _initialOriginPos;
			bool gotTarget = !targetPos.IsEmpty();
			if (!gotTarget && !Data->OriginNoUpdate)
			{
				gotTarget = GetTargetPosFromChain(targetPos, true);
			}
			if (gotTarget)
				_initialOriginPos = targetPos; // 跟随：更新锁定值
			else if (targetPos.IsEmpty())
				break; // 从未有过目标：保持朝向
			if (Data->IsOnOrigin)
			{
				AbstractClass* pTgt = pBullet ? pBullet->Target : (pTechno ? pTechno->Target : nullptr);
				TechnoClass* pTargetTechno = abstract_cast<TechnoClass*>(pTgt);
				if (pTargetTechno && !IsDeadOrInvisible(pTargetTechno))
				{
					mainFacingDir = Data->OriginIsOnBody
						? pTargetTechno->PrimaryFacing.Current()     // 官方API，不得修改
						: pTargetTechno->TurretFacing().Current();   // 官方API，不得修改
					if (!hasNormal) effectiveFacing = mainFacingDir.GetRadian();
					break;
				}
				// 目标无朝向（格子）：回退连线
			}
			mainFacingDir = Point2Dir(targetPos, currentPos); // 官方API，不得修改
			if (!hasNormal) effectiveFacing = mainFacingDir.GetRadian();
			double dx = currentPos.X - targetPos.X, dy = currentPos.Y - targetPos.Y, dz = currentPos.Z - targetPos.Z;
			double lenXY = std::sqrt(dx*dx + dy*dy);
			if (!hasNormal) effectiveTilt = (lenXY > 1e-6 && Data->AllowCircleTilt) ? std::atan2(dz, lenXY) : 0.0;
		}
			break;

		case VectorData::VectorOrigin::Self:
			if (pTechno)
			{
				mainFacingDir = Data->OriginIsOnBody
					? pTechno->PrimaryFacing.Current()     // 官方API，不得修改
					: pTechno->TurretFacing().Current();   // 官方API，不得修改
				if (!hasNormal) effectiveFacing = mainFacingDir.GetRadian();
			}
			else if (pBullet)
			{
				mainFacingDir = Facing(pBullet, currentPos); // 官方API，不得修改
				if (!hasNormal) effectiveFacing = mainFacingDir.GetRadian();
			}
			break;

		case VectorData::VectorOrigin::Launcher:
			if (_pLauncher && !IsDeadOrInvisible(_pLauncher))
			{
				TechnoClass* pLauncherTechno = abstract_cast<TechnoClass*>(_pLauncher);
				if (pLauncherTechno)
				{
					if (Data->IsOnOrigin)
					{
						mainFacingDir = Data->OriginIsOnBody
							? pLauncherTechno->PrimaryFacing.Current()     // 官方API，不得修改
							: pLauncherTechno->TurretFacing().Current();   // 官方API，不得修改
						if (!hasNormal) effectiveFacing = mainFacingDir.GetRadian();
					}
					else
					{
						// 发射者→弹体连线
						mainFacingDir = Point2Dir(pLauncherTechno->GetCoords(), currentPos); // 官方API，不得修改
						if (!hasNormal) effectiveFacing = mainFacingDir.GetRadian();
					}
				}
			}
			break;
		}
	}

	// IsNormalOnOrigin：圆面法向量随 Origin 单位转动（facing + tilt 全跟随）
	// 基础法向量 = OnStart 锁定的球坐标（_facingRad/_tiltRad，来自 NormalVector/NormalRandom/默认水平），
	// 每帧按单位朝向（facingU）+ 单位倾斜（originTerrainTilt）转动：
	//   1. 绕 Z 轴转 facingU（单位水平朝向）
	//   2. 绕单位 L 轴（(-sinFU, cosFU, 0)，水平左方向）转 tiltU（Rodrigues：n' = n cosθ + (u×n) sinθ + u(u·n)(1-cosθ)）
	// no（显式）= 世界固定，法向量保持 LockFacing 初始化值，不随单位转。
	if (Data->IsNormalOnOrigin && !Data->OriginIsOnWorld)
	{
		double facingU = effectiveFacing;
		switch (Data->Origin)
		{
		case VectorData::VectorOrigin::Launcher:
			{
				TechnoClass* pLT = abstract_cast<TechnoClass*>(_pLauncher);
				if (pLT && !IsDeadOrInvisible(pLT))
					facingU = (Data->OriginIsOnBody ? pLT->PrimaryFacing.Current() : pLT->TurretFacing().Current()).GetRadian(); // 官方API，不得修改
			}
			break;
		case VectorData::VectorOrigin::Target:
			{
				AbstractClass* pTgt = pBullet ? pBullet->Target : (pTechno ? pTechno->Target : nullptr);
				TechnoClass* pTT = abstract_cast<TechnoClass*>(pTgt);
				if (pTT && !IsDeadOrInvisible(pTT))
					facingU = (Data->OriginIsOnBody ? pTT->PrimaryFacing.Current() : pTT->TurretFacing().Current()).GetRadian(); // 官方API，不得修改
				// 目标无朝向（格子）：保持 effectiveFacing（连线）
			}
			break;
		case VectorData::VectorOrigin::Source:
			{
				TechnoClass* pST = abstract_cast<TechnoClass*>(_pSource);
				if (pST && !IsDeadOrInvisible(pST))
					facingU = (Data->OriginIsOnBody ? pST->PrimaryFacing.Current() : pST->TurretFacing().Current()).GetRadian(); // 官方API，不得修改
			}
			break;
		default: // Self：自身朝向即 effectiveFacing
			break;
		}
		double tiltU = originTerrainTilt; // 单位倾斜（AngleRotatedForwards 动态/地形采样）

		// 基础法向量（OnStart 锁定）→ 笛卡尔；无自定义法线时默认水平圆面（法线朝上）
		double baseTilt = hasNormal ? _tiltRad : M_PI / 2.0;
		double bx = std::cos(baseTilt) * std::cos(_facingRad);
		double by = std::cos(baseTilt) * std::sin(_facingRad);
		double bz = std::sin(baseTilt);

		// 1. 绕 Z 轴转 facingU（单位水平朝向）
		double cz = std::cos(facingU), sz = std::sin(facingU);
		double x1 = bx * cz - by * sz;
		double y1 = bx * sz + by * cz;
		double z1 = bz;

		// 2. 绕单位 L 轴 u=(-sinFU, cosFU, 0) 转 tiltU（Rodrigues）
		double ct = std::cos(tiltU), st = std::sin(tiltU);
		double ux = -sz, uy = cz, uz = 0.0;
		double dot = ux * x1 + uy * y1;      // u·n
		double cx = uy * z1 - uz * y1;       // u×n
		double cy = uz * x1 - ux * z1;
		double cz2 = ux * y1 - uy * x1;
		_motion.normalX = x1 * ct + cx * st + ux * dot * (1.0 - ct);
		_motion.normalY = y1 * ct + cy * st + uy * dot * (1.0 - ct);
		_motion.normalZ = z1 * ct + cz2 * st;

		// 同步倾斜圆面数学输入（最终法向量 → 球坐标）
		double lenXY = std::sqrt(_motion.normalX * _motion.normalX + _motion.normalY * _motion.normalY);
		effectiveFacing = lenXY > 1e-6 ? std::atan2(_motion.normalY, _motion.normalX) : 0.0;
		effectiveTilt = lenXY > 1e-6 ? std::atan2(_motion.normalZ, lenXY) : (_motion.normalZ > 0 ? M_PI / 2.0 : -M_PI / 2.0);
	}

	// 3D 法向量旋转覆盖：当 NormalF/L/HAnglePerStep 设定时，无视基座变化
	// 只更新倾斜量（effectiveFacing/effectiveTilt）；不碰 mainFacingDir——
	// 法向量与 F 轴基准已解耦，法向量自旋不得污染摆放 FLH 的基准。
	if (_motion.normalStepF != 0.0 || _motion.normalStepL != 0.0 || _motion.normalStepH != 0.0)
	{
		double lenXY = std::sqrt(_motion.normalX * _motion.normalX + _motion.normalY * _motion.normalY);
		effectiveFacing = lenXY > 1e-6 ? std::atan2(_motion.normalY, _motion.normalX) : 0.0;
		effectiveTilt = lenXY > 1e-6 ? std::atan2(_motion.normalZ, lenXY) : (_motion.normalZ > 0 ? M_PI / 2.0 : -M_PI / 2.0);
	}

	// ========================================================================
	// Origin 坐标（主 Origin 计算点）
	// ========================================================================
	CoordStruct originPos = currentPos;

	switch (Data->Origin)
	{
	case VectorData::VectorOrigin::Target:
		if (Data->OriginNoUpdate)
			originPos = _initialOriginPos.IsEmpty() ? currentPos : _initialOriginPos; // 锁定初始目标
		else
		{
			// 允许更新（NoUpdate=no）：缓存优先（只跟随锁定单位，防引擎集结目标点污染），
			// 缓存空才回退引擎链；目标死亡后缓存冻结，回退锁定坐标
			CoordStruct updated{};
			bool gotUpdate = false;
			if (!gotUpdate)
			{
				gotUpdate = GetTargetPosFromChain(updated, true);
			}
			if (gotUpdate)
			{
				_initialOriginPos = updated; // 跟随：更新锁定值
				originPos = _initialOriginPos;
			}
			else
				originPos = _initialOriginPos.IsEmpty() ? currentPos : _initialOriginPos; // 抛弃 update → 回退锁定坐标
		}
		break;
	case VectorData::VectorOrigin::Launcher:
		if (Data->OriginNoUpdate)
			originPos = _initialOriginPos;
		else
		{
			TrackOriginCoord(_pLauncher, false, _initialOriginPos); // 发射者活着：每帧快照；死亡：冻结
			originPos = _initialOriginPos;
		}
		break;
	case VectorData::VectorOrigin::Source:
		if (Data->OriginNoUpdate)
			originPos = _initialOriginPos;
		else
		{
			TrackOriginCoord(_pSource, false, _initialOriginPos); // 来源活着：每帧快照；死亡：冻结
			originPos = _initialOriginPos;
		}
		break;
	case VectorData::VectorOrigin::Self:
		originPos = Data->OriginNoUpdate ? _initialOriginPos : currentPos;
		break;
	}

	// OriginFLH 偏移完整化：把基准点从"Origin 裸位置"变成"带挂载偏移的完整基准点"（圆心落点）。
	// 统一走"取基准点"管线 ApplyOriginFlh（与挂载快照同一套逻辑）：
	//   NoUpdate=yes 不在此算——存档点挂载时已含偏移，直接用（冻结）。
	//   NoUpdate=no 才执行：本帧裸基准已刷新，用实时坐标系重算偏移。
	// 二维（AllowOriginTilt=no）：facing=mainFacingDir（IsOnOrigin 实时朝向，与 NormalVector 解耦）；
	//   含 Self（Self+no 也应有偏移）。
	// 三维（AllowOriginTilt=yes）：facing=Origin 单位实时车身朝向（PrimaryFacing），tilt=本帧采样
	//   倾斜角；Origin 无单位（打格子/死亡）→ 保持裸基准（无姿态可参考）。Self 无三维（无法绕自己）。
	if (!Data->OriginNoUpdate && !Data->OriginFLH.IsEmpty())
	{
		if (Data->AllowOriginTilt && Data->Origin != VectorData::VectorOrigin::Self)
		{
			TechnoClass* pOriginTechno = FindOriginTechno();
			if (pOriginTechno && !IsDeadOrInvisible(pOriginTechno))
			{
				originPos = ApplyOriginFlh(originPos, Data->OriginFLH,
					pOriginTechno->PrimaryFacing.Current(), originTerrainTilt); // 官方API，不得修改
			}
		}
		else if (!Data->AllowOriginTilt)
		{
			originPos = ApplyOriginFlh(originPos, Data->OriginFLH, mainFacingDir, 0.0);
		}
	}

// GetVectorResult：每帧计算位移（主体内联，段落化）

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
		if (calcRadius < 1.0)
			calcRadius = 1.0;  // 防止除零：半径 + 角速度互推时必 > 0

		// 动态线速：每帧叠加加速度（初始值已在 DisabledFrames 前预初始化）
		_motion.circleSpeed += Data->CircleSpeedAcceleration;
		if (Data->CircleMaxSpeed != 0 && _motion.circleSpeed > Data->CircleMaxSpeed)
			_motion.circleSpeed = static_cast<double>(Data->CircleMaxSpeed);
		if (Data->CircleMinSpeed != 0 && _motion.circleSpeed < Data->CircleMinSpeed)
			_motion.circleSpeed = static_cast<double>(Data->CircleMinSpeed);

		// 角速度动态：每帧叠加加速度（初始值已在 DisabledFrames 前预初始化）
		_motion.circleAngle += Data->CircleAngleAcceleration;
		if (Data->CircleMaxAngle != 0.0 && _motion.circleAngle > Data->CircleMaxAngle)
			_motion.circleAngle = Data->CircleMaxAngle;
		if (Data->CircleMinAngle != 0.0 && _motion.circleAngle < Data->CircleMinAngle)
			_motion.circleAngle = Data->CircleMinAngle;

		double speed = _motion.circleSpeed;
		double angleStep = _motion.circleAngle;

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

	// （AllowOriginTilt=yes 的 OriginFLH 三维旋转已并入上方"取基准点"管线 ApplyOriginFlh，
	//   在 originPos 级完成——挂载快照定死 / no 每帧刷新，circleCenter 直接消费完整基准点，
	//   此处不再重复叠加。原 circleCenter 级三维分支已删除。）

	if (!Data->CircleOrigin.IsEmpty())
	{
		if (Data->AllowOriginTilt)
		{
			// CircleOrigin 作为 FLH 偏移并入"取基准点"管线 ApplyOriginFlh：
			// AllowOriginTilt=yes 时跟随转轴旋转（F=沿 facing，L=垂直 facing，H=Z），
			// facing=Origin 单位车身朝向（与 OriginFLH 三维同基准），tilt=本帧采样倾斜角。
			// 坐标系修正（RotateZ+Y 镜像）在引擎 API 内，禁止裸 cos/sin 手写。
			// 圆心 = 基准点(可能已含 OriginFLH 偏移) + CircleOrigin 旋转偏移。
			TechnoClass* pOriginTechno = FindOriginTechno();
			if (pOriginTechno && !IsDeadOrInvisible(pOriginTechno))
			{
				circleCenter = ApplyOriginFlh(circleCenter, adjustedCircleOrigin,
					pOriginTechno->PrimaryFacing.Current(), originTerrainTilt); // 官方API，不得修改
			}
			else
			{
				// Origin 无单位（打格子/死亡）：无转轴可跟随 → 按纯世界坐标加法处理
				circleCenter = originPos + adjustedCircleOrigin;
			}
		}
		else
		{
			// AllowOriginTilt=no：纯世界坐标加法、无视朝向（与 yes 分支刻意不同）
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
					{
						if (!Data->OriginOriginNoUpdate)
							_initialBaseCenter = _pLauncher->GetCoords(); // 发射者活着：每帧快照（NoUpdate=yes 冻结首帧不更新）
						baseCenter = _pLauncher->GetCoords();
					}
					else
						baseCenter = _initialBaseCenter; // 发射者死亡：冻结快照（首帧或最后跟随值），不再读指针
					break;
				case VectorData::VectorOrigin::Target:
					{
						CoordStruct targetBase{};
						bool gotTargetBase = false;
						if (pTechno && pTechno->Target)
						{
							targetBase = pTechno->Target->GetCoords();
							gotTargetBase = true;
						}
						else if (pTechno)
						{
							FootClass* pFoot = abstract_cast<FootClass*>(pTechno);
							if (pFoot && pFoot->Destination)
							{
								targetBase = pFoot->Destination->GetCoords();
								gotTargetBase = true;
							}
						}
						else if (pBullet && pBullet->Target)
						{
							targetBase = pBullet->Target->GetCoords();
							gotTargetBase = true;
						}
						else if (pBullet && pBullet->Owner && pBullet->Owner->Target)
						{
							targetBase = pBullet->Owner->Target->GetCoords();
							gotTargetBase = true;
						}
						else if (pBullet)
						{
							targetBase = pBullet->TargetCoords;
							gotTargetBase = true;
						}
						if (gotTargetBase)
						{
							if (!Data->OriginOriginNoUpdate)
								_initialBaseCenter = targetBase; // 目标活着：每帧快照（NoUpdate=yes 冻结首帧不更新）
							baseCenter = targetBase;
						}
						else
							baseCenter = _initialBaseCenter; // 目标失效：冻结快照（首帧或最后跟随值），不再掉回 originPos
					}
					break;
				case VectorData::VectorOrigin::Source:
					if (_pSource && !IsDeadOrInvisible(_pSource))
					{
						if (!Data->OriginOriginNoUpdate)
							_initialBaseCenter = _pSource->GetCoords(); // 来源活着：每帧快照（NoUpdate=yes 冻结首帧不更新）
						baseCenter = _pSource->GetCoords();
					}
					else
						baseCenter = _initialBaseCenter; // 来源死亡：冻结快照（首帧或最后跟随值），不再读指针
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
				// 初始偏移 = 0：大圆圆心直接用大圆基座（baseCenter，已含 Origin.CircleOrigin 偏移），
				// 不绑定主圆圆心（circleCenter）。主圆围绕大圆转，主圆圆心坐标对大圆无意义。
				_originOffset = {};
				// Circle 初始化
				_originMotion.circleRadius = Data->OriginCircleRadius;
				_originMotion.circleSpeed = Data->OriginCircleSpeed;
				_originMotion.angle = 0.0; // 初始相位
				// 未显式设半径：取当前偏移的水平距离
				if (_originMotion.circleRadius < 0)
					_originMotion.circleRadius = (int)std::sqrt(
						(double)_originOffset.X * _originOffset.X +
						(double)_originOffset.Y * _originOffset.Y +
						(double)_originOffset.Z * _originOffset.Z);
				// 随机
				if (Data->OriginCircleRandomRadiusMax > Data->OriginCircleRandomRadiusMin)
					_originMotion.circleRadius = Random::RandomRanged(Data->OriginCircleRandomRadiusMin, Data->OriginCircleRandomRadiusMax);
				if (Data->OriginCircleRandomAngleMax > Data->OriginCircleRandomAngleMin)
					_originMotion.angle = Data->OriginCircleRandomAngleMin + (Data->OriginCircleRandomAngleMax - Data->OriginCircleRandomAngleMin) * Random::RandomDouble();
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
				_originMotion.normalStepF = ResolveAngleStep(Data->OriginNormalFAnglePerStep, Data->OriginNormalFAngleRMin, Data->OriginNormalFAngleRMax, Data->OriginNormalFAngleRMin2, Data->OriginNormalFAngleRMax2);
				_originMotion.normalStepL = ResolveAngleStep(Data->OriginNormalLAnglePerStep, Data->OriginNormalLAngleRMin, Data->OriginNormalLAngleRMax, Data->OriginNormalLAngleRMin2, Data->OriginNormalLAngleRMax2);
				_originMotion.normalStepH = ResolveAngleStep(Data->OriginNormalHAnglePerStep, Data->OriginNormalHAngleRMin, Data->OriginNormalHAngleRMax, Data->OriginNormalHAngleRMin2, Data->OriginNormalHAngleRMax2);
				_originMotion.lissajousStep = Data->OriginLissajous;
				// 无 OriginNormalVector 时：默认水平圆面（法向量朝上）
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
				// 有 OriginNormalVector 时：facing/tilt 均取它的 F/L/H 分量（彻底世界固定）。
				// IsNormalOnOrigin=yes 时的 OriginOrigin 朝向跟随在下方每帧段处理。
				// 锁定基础法向量球坐标（OriginIsNormalOnOrigin 每帧旋转的基准，不被 OriginAllowCircleTilt 覆盖污染）
				_baseOriginFacing = _originFacing;
				_baseOriginTilt = _originTilt;
				// 初始化大圆 3D 法向量
				{
					double ct = std::cos(_originTilt), st = std::sin(_originTilt);
					double cf = std::cos(_originFacing), sf = std::sin(_originFacing);
					_originMotion.normalX = ct * cf;
					_originMotion.normalY = ct * sf;
					_originMotion.normalZ = st;
				}
			}
		// OriginIsNormalOnOrigin：大圆法向量随 OriginOrigin 单位转动（facing + tilt 全跟随，同主圆）。
		// 基础 = 首帧锁定的 _baseOriginFacing/_baseOriginTilt（OriginNormalVector/随机/默认水平），
		// 每帧按 OriginOrigin 单位朝向（facingU）+ 单位倾斜（originTerrainTilt）转动（Rodrigues）。
		// 更新 _originMotion.normalX/Y/Z（Circle 运动消费点在段外从法向量现算球坐标）。
		// 不回写成员 _originFacing/_originTilt（保持首帧锁定值，同主圆 effectiveFacing/effectiveTilt 模式）。
		// no（显式）= 世界固定，保持首帧值。
		if (Data->OriginIsNormalOnOrigin && !Data->OriginNormalVector.IsEmpty())
		{
			double facingU = _originFacing;
			switch (Data->OriginOrigin)
			{
			case VectorData::VectorOrigin::Launcher:
				{
					TechnoClass* pLT = abstract_cast<TechnoClass*>(_pLauncher);
					if (pLT && !IsDeadOrInvisible(pLT))
						facingU = pLT->TurretFacing().Current().GetRadian(); // 官方API，不得修改
					else if (pTechno) facingU = pTechno->TurretFacing().Current().GetRadian(); // 官方API，不得修改
				}
				break;
			case VectorData::VectorOrigin::Target:
				{
					AbstractClass* pTgt = pBullet ? pBullet->Target : (pTechno ? pTechno->Target : nullptr);
					TechnoClass* pTT = abstract_cast<TechnoClass*>(pTgt);
					if (pTT && !IsDeadOrInvisible(pTT))
						facingU = pTT->TurretFacing().Current().GetRadian(); // 官方API，不得修改
					// 目标无朝向（格子/地面）：保持 facingU 初值（世界固定），不回退连线（同主圆 IsNormalOnOrigin 处理）
				}
				break;
			case VectorData::VectorOrigin::Source:
				{
					TechnoClass* pST = abstract_cast<TechnoClass*>(AE && AE->pSource ? AE->pSource : nullptr);
					if (pST && !IsDeadOrInvisible(pST))
						facingU = pST->TurretFacing().Current().GetRadian(); // 官方API，不得修改
					else if (AE && AE->pSource) { auto sp = AE->pSource->GetCoords(); auto bp = pObject->GetCoords(); facingU = std::atan2(bp.Y-sp.Y, bp.X-sp.X); } // 非单位：回退连线
				}
				break;
			default: // FLH
				if (!Data->OriginOriginFLH.IsEmpty())
				{
					double fy = Data->OriginOriginFLH.X, fx = Data->OriginOriginFLH.Y;
					facingU = std::atan2(fy, fx);
				}
				else if (pBullet) facingU = pBullet->Velocity.Magnitude() > 0 ? std::atan2(pBullet->Velocity.X, pBullet->Velocity.Y) : 0.0;
				else if (pTechno) facingU = pTechno->TurretFacing().Current().GetRadian(); // 官方API，不得修改
				break;
			}
			double tiltU = originTerrainTilt; // 单位倾斜

			// 基础法向量（首帧锁定）→ 笛卡尔
			double bx = std::cos(_baseOriginTilt) * std::cos(_baseOriginFacing);
			double by = std::cos(_baseOriginTilt) * std::sin(_baseOriginFacing);
			double bz = std::sin(_baseOriginTilt);
			// 1. 绕 Z 轴转 facingU（单位水平朝向）
			double cz = std::cos(facingU), sz = std::sin(facingU);
			double x1 = bx * cz - by * sz;
			double y1 = bx * sz + by * cz;
			double z1 = bz;
			// 2. 绕单位 L 轴 u=(-sinFU, cosFU, 0) 转 tiltU（Rodrigues：n' = n cosθ + (u×n) sinθ + u(u·n)(1-cosθ)）
			double ct = std::cos(tiltU), st = std::sin(tiltU);
			double ux = -sz, uy = cz, uz = 0.0;
			double dot = ux * x1 + uy * y1;      // u·n
			double cx = uy * z1 - uz * y1;       // u×n
			double cy = uz * x1 - ux * z1;
			double cz2 = ux * y1 - uy * x1;
			_originMotion.normalX = x1 * ct + cx * st + ux * dot * (1.0 - ct);
			_originMotion.normalY = y1 * ct + cy * st + uy * dot * (1.0 - ct);
			_originMotion.normalZ = z1 * ct + cz2 * st;
			// 不回写 _originFacing/_originTilt（同主圆 IsNormalOnOrigin：基座球坐标永远保持首帧锁定值，
			// 段外消费点从 _originMotion.normalX/Y/Z 现算，杜绝法向量每帧自反馈累计旋转）
		}
		// 每帧累加 Lissajous + 3D 法向量增量旋转
		_originMotion.normalRotF += _originMotion.lissajousStep;
		if (_originMotion.normalStepF != 0.0 || _originMotion.normalStepL != 0.0 || _originMotion.normalStepH != 0.0)
		{
			RotateNormal3D(_originMotion.normalX, _originMotion.normalY, _originMotion.normalZ,
				_originMotion.normalStepF, _originMotion.normalStepL, _originMotion.normalStepH);
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

			// 从法向量现算球坐标（同主圆 effectiveFacing/effectiveTilt 消费模式）：
			// 段内 OriginIsNormalOnOrigin 每帧旋转结果已在 _originMotion.normalX/Y/Z，
			// 这里现算 oFacing/oFacingTilt 供 Circle 运动消费；不回读成员 _originFacing/_originTilt（保持首帧锁定值）。
			double oFacing = 0.0, oTilt = 0.0;
			{
				double lenXY = std::sqrt(_originMotion.normalX * _originMotion.normalX + _originMotion.normalY * _originMotion.normalY);
				oFacing = lenXY > 1e-6 ? std::atan2(_originMotion.normalY, _originMotion.normalX) : 0.0;
				oTilt = lenXY > 1e-6 ? std::atan2(_originMotion.normalZ, lenXY) : (_originMotion.normalZ > 0 ? M_PI / 2.0 : -M_PI / 2.0);
			}
			// OriginIsNormalOnOrigin=yes 时法向量已按单位倾斜每帧旋转，OriginAllowOriginTilt 不再叠加，避免重复
			oTilt += (Data->OriginAllowOriginTilt && !Data->OriginIsNormalOnOrigin ? originTerrainTilt : 0.0);
			// 3D 法向量旋转覆盖
			if (_originMotion.normalStepF != 0.0 || _originMotion.normalStepL != 0.0 || _originMotion.normalStepH != 0.0)
			{
				double lenXY = std::sqrt(_originMotion.normalX * _originMotion.normalX + _originMotion.normalY * _originMotion.normalY);
				oFacing = lenXY > 1e-6 ? std::atan2(_originMotion.normalY, _originMotion.normalX) : 0.0;
				oTilt = lenXY > 1e-6 ? std::atan2(_originMotion.normalZ, lenXY) : (_originMotion.normalZ > 0 ? M_PI / 2.0 : -M_PI / 2.0);
			}
			DirStruct oFacingDir = Radians2Dir(oFacing); // 官方API，不得修改：弧度→DirStruct

			// 当前圆心绝对位置 = 基座 + 偏移
			CoordStruct originCenter = baseCenter + _originOffset;

			CoordStruct disp;
			if (!Data->OriginMoveTo.IsEmpty())
			{
				// MoveTo 模式：GrowRate 随帧数线性增长
				_originMotion.angle += Data->OriginAnglePerStep;
				CoordStruct growOffset;
				growOffset = Data->OriginGrowRate * _originMotion.elapsed;
				disp = GetFLHAbsoluteOffset(Data->OriginMoveTo + growOffset, Radians2Dir(oFacing + Math::deg2rad(_originMotion.angle))); // 官方API，不得修改
			}
			else if (Data->OriginReachTarget || Data->OriginLinearSpeed >= 0 || !Data->OriginTargetFLH.IsEmpty())
			{
				// Speed / ReachTarget
				if (_originMotion.elapsed == 0)
				{
					_originMotion.speed = Data->OriginLinearSpeed >= 0 ? Data->OriginLinearSpeed : (pTechno ? pTechno->GetTechnoType()->Speed : 40.0);
					_originMotion.arcStartCenter = originCenter;
				}

				// OriginOrigin 的 F 轴基准（摆放 OriginTargetFLH 用；与法向量彻底解耦）：
				// 单位存活 → 单位自身炮塔朝向；打格子/地面/目标死亡 → 基座点指向抛射体的水平连线；
				// OriginOrigin=Self（基座跟主圆）→ 跟主圆 F 轴基准（mainFacingDir）。
				DirStruct originOriginFacing = mainFacingDir;
				if (Data->OriginOrigin != VectorData::VectorOrigin::Self)
				{
					TechnoClass* pOO = nullptr;
					if (Data->OriginOrigin == VectorData::VectorOrigin::Launcher)
						pOO = abstract_cast<TechnoClass*>(_pLauncher);
					else if (Data->OriginOrigin == VectorData::VectorOrigin::Source)
						pOO = abstract_cast<TechnoClass*>(_pSource);
					else if (Data->OriginOrigin == VectorData::VectorOrigin::Target)
					{
						AbstractClass* pTgt = pBullet ? pBullet->Target : (pTechno ? pTechno->Target : nullptr);
						pOO = abstract_cast<TechnoClass*>(pTgt);
					}
					if (pOO && !IsDeadOrInvisible(pOO))
						originOriginFacing = pOO->TurretFacing().Current(); // 官方API，不得修改：单位自身朝向
					else
						originOriginFacing = Point2Dir(baseCenter, pObject->GetCoords()); // 官方API，不得修改：基座点→弹体水平连线
				}

				CoordStruct targetWorld = GetFLHAbsoluteCoords(baseCenter, Data->OriginTargetFLH + _originTargetOffset, originOriginFacing); // 官方API，不得修改
				if (Data->OriginReachTarget)
				{
					int effectiveSteps = (AE->AEData.GetDuration() - Data->DisabledFrames) / _effectiveTimeStep;
					if (effectiveSteps < 1) effectiveSteps = 1;
					int rem = effectiveSteps - _movementFrames;
					if (rem <= 0)
					{
						disp = targetWorld - originCenter;
						_originOffset += disp;
						circleCenter = baseCenter + _originOffset;
						_prevCircleCenter = circleCenter;
						Deactivate();
						goto skipOriginUpdate;
					}
					disp.X = (targetWorld.X - originCenter.X) / rem;
					disp.Y = (targetWorld.Y - originCenter.Y) / rem;
					disp.Z = (targetWorld.Z - originCenter.Z) / rem;
					if (_originMotion.arcHeight != 0)
					{
						double t = static_cast<double>(_movementFrames) / effectiveSteps;
						double arcOffset = CalcArcOffsetAt(static_cast<int>(_originMotion.arcHeight), _originMotion.arcPeakPercent, t);
						double baseX = _originMotion.arcStartCenter.X + (targetWorld.X - _originMotion.arcStartCenter.X) * t;
						double baseY = _originMotion.arcStartCenter.Y + (targetWorld.Y - _originMotion.arcStartCenter.Y) * t;
						double baseZ = _originMotion.arcStartCenter.Z + (targetWorld.Z - _originMotion.arcStartCenter.Z) * t;
						if (_originMotion.arcRotation == 0.0)
						{
							disp.Z = static_cast<int>(baseZ + arcOffset) - originCenter.Z;
						}
						else
						{
							CoordStruct arcD{
								targetWorld.X - _originMotion.arcStartCenter.X,
								targetWorld.Y - _originMotion.arcStartCenter.Y,
								targetWorld.Z - _originMotion.arcStartCenter.Z };
							ArcDelta3D ad = RotateArcDelta(arcD, _originMotion.arcRotation, arcOffset);
							disp.X = static_cast<int>(baseX + ad.x) - originCenter.X;
							disp.Y = static_cast<int>(baseY + ad.y) - originCenter.Y;
							disp.Z = static_cast<int>(baseZ + ad.z) - originCenter.Z;
						}
					}
				}
				else
				{
					_originMotion.speed += Data->OriginAcceleration;
					if (Data->OriginMaxSpeed >= 0 && _originMotion.speed > Data->OriginMaxSpeed) _originMotion.speed = Data->OriginMaxSpeed;
					if (Data->OriginMinSpeed >= 0 && _originMotion.speed < Data->OriginMinSpeed) _originMotion.speed = Data->OriginMinSpeed;
					int dx = targetWorld.X - originCenter.X, dy = targetWorld.Y - originCenter.Y, dz = targetWorld.Z - originCenter.Z;
					double dist = std::sqrt((double)dx*dx + dy*dy + dz*dz);
					if (dist < 1.0) disp = {};
					else if (Data->OriginSpeedEndOnReach && _originMotion.speed >= dist)
					{
						disp.X = dx; disp.Y = dy; disp.Z = dz;
						Deactivate();
					}
					else { double s = _originMotion.speed / dist; disp.X = (int)(dx*s); disp.Y = (int)(dy*s); disp.Z = (int)(dz*s); }

					// 弧高增量叠加（与 OriginReachTarget 一致，支持 ArcPeakPercent / ArcRotation）
					if (_originMotion.arcHeight != 0 && dist >= 1.0)
					{
						if (_originMotion.arcTotalDist < 0.0)
							_originMotion.arcTotalDist = dist;
						double t = (_originMotion.arcTotalDist > 1e-6) ? 1.0 - dist / _originMotion.arcTotalDist : 0.0;
						if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0;
						double arcThis = CalcArcOffsetAt(static_cast<int>(_originMotion.arcHeight), _originMotion.arcPeakPercent, t);
						double arcDelta = arcThis - _originMotion.prevArcOffset;
						_originMotion.prevArcOffset = arcThis;
						CoordStruct arcD{
							targetWorld.X - _originMotion.arcStartCenter.X,
							targetWorld.Y - _originMotion.arcStartCenter.Y,
							targetWorld.Z - _originMotion.arcStartCenter.Z };
						ArcDelta3D ad = RotateArcDelta(arcD, _originMotion.arcRotation, arcDelta);
						disp.X += static_cast<int>(ad.x);
						disp.Y += static_cast<int>(ad.y);
						disp.Z += static_cast<int>(ad.z);
					}
				}
			}
			else // Circle 模式
			{
				_originMotion.circleRadius += Data->OriginCircleRadiusGrow;
				double tr = _originMotion.circleRadius;
				if (Data->OriginCircleMaxRadius > 0 && tr > Data->OriginCircleMaxRadius) tr = Data->OriginCircleMaxRadius;
				if (Data->OriginCircleMinRadius > 0 && tr < Data->OriginCircleMinRadius) tr = Data->OriginCircleMinRadius;
				// 角步长：优先线速度/半径推算，否则用固定角速度
				double originAngleStep = Data->OriginCircleAnglePerStep;
				if (Data->OriginCircleSpeed != 0 && tr > 0)
					originAngleStep = Math::rad2deg(Data->OriginCircleSpeed / tr);
			// Lissajous>0: 累积大角旋转（增减边震荡），==0: 每帧仅增量旋转（平滑行星）
			_originMotion.angle += originAngleStep;
			double r = Data->OriginLissajous > 0.0 ? Math::deg2rad(_originMotion.angle + _originMotion.normalRotF) : Math::deg2rad(originAngleStep + _originMotion.normalRotF);
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
		_originOffset += disp;
		circleCenter = baseCenter + _originOffset;
		_originMotion.elapsed++;
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
			// 倾斜圆面：把世界向量投影到圆面局部 LH 平面（L=水平切向，H=圆面"上"方向）
			// 圆面局部正交基（由法线球坐标 facing/tilt 构造）：
			//   L 轴 = (-sinF, cosF, 0)                       （水平切向）
			//   H 轴 = (-cosF*sinT, -sinF*sinT, cosT)          （圆面"上"）
			//   法线 = L×H = (cosF*cosT, sinF*cosT, sinT)      （tilt=法线仰角）
			// 语义验证：tilt=PI/2 → 法线 (0,0,1) 垂直向上 → 圆面水平（Z 恒定，落在 XY 平面）；
			//          tilt=0   → 法线水平 → 圆面侧立（Z 由 H 分量独立决定）
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
			_motion.circleRadius = static_cast<double>(Data->CircleRadius);
			if (_motion.circleRadius <= 0.0)
				_motion.circleRadius = currentDist;
			if (Data->CircleRandomRadiusMax > Data->CircleRandomRadiusMin)
				_motion.circleRadius = Random::RandomRanged(Data->CircleRandomRadiusMin, Data->CircleRandomRadiusMax);
		}
		_motion.circleRadius += Data->CircleRadiusGrow;

		double targetRadius = _motion.circleRadius;
		// 钳位
		if (Data->CircleMaxRadius > 0 && targetRadius > Data->CircleMaxRadius)
			targetRadius = static_cast<double>(Data->CircleMaxRadius);
		if (Data->CircleMinRadius > 0 && targetRadius < Data->CircleMinRadius)
			targetRadius = static_cast<double>(Data->CircleMinRadius);

		double rad = Math::deg2rad(angleStep + _motion.normalRotF);
		double cosA = std::cos(rad), sinA = std::sin(rad);

		if (useTiltPlane)
		{
			// 倾斜圆面取点（与上方投影同一套正交基，见 useTiltPlane 距离投影处注释）：
			//   1. 世界 → LH 平面投影（dL/dH）
			//   2. 在 LH 平面内旋转角度 A（rL = ndL*cosA - ndH*sinA, rH = ndL*sinA + ndH*cosA）
			//   3. LH → 世界 回映射：X = rL*L.x + rH*H.x, Y = rL*L.y + rH*H.y, Z = rH*H.z
			// 即落点 = 圆心 + rL*L轴 + rH*H轴，保证始终落在法线 (cosF*cosT, sinF*cosT, sinT) 定义的圆面上
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
			&& _motion.circleRadius >= Data->CircleMaxRadius)
		{
			Deactivate();
		}
		if (Data->CircleEndOnMinRadius && Data->CircleMinRadius > 0
			&& _motion.circleRadius <= Data->CircleMinRadius)
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
					// 与 AutoWeapon IsOnTarget 同款：Point2Dir 内部处理 RA2 坐标系（裸 atan2+Radians2Dir 有 90° 偏置）
					moveDir = Point2Dir(currentPos, tgt); // 抛射体→目标 连线方向
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
				_motion.angle = 0.0;
			_motion.angle += Data->AnglePerStep;
			// SetRadian 与 GetRadian 互逆（Radians2Dir 往返存在 90° 偏置，不能用）
			moveDir.SetRadian(moveDir.GetRadian() + Math::deg2rad(_motion.angle));
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
	// TargetOffsetNormal 世界固定：偏移已由 LockFacing 转成世界坐标，叠加在旋转后的 TargetFLH 上（不随 F 轴转）
	bool targetOffsetWorld = !Data->IsNormalOnOrigin && !Data->TargetOffsetNormal.IsEmpty();
	frameTargetFlh.X = Data->TargetFLH.X + (targetOffsetWorld ? 0 : _randomTargetOffset.X);
	frameTargetFlh.Y = Data->TargetFLH.Y + (targetOffsetWorld ? 0 : _randomTargetOffset.Y);
	frameTargetFlh.Z = Data->TargetFLH.Z + (targetOffsetWorld ? 0 : _randomTargetOffset.Z);

	// TargetFLH → 世界坐标：AutoWeapon 同款管线
	// 坐标系统一：矩阵偏移（含 IsOnTurret 炮塔/车身）+ NoUpdate 控制的计算点 originPos
	CoordStruct frameTarget;
	// NoUpdate=yes：目标点锁定。首帧正常计算一次缓存，后续每帧直接复用 _lockedTarget，
	// 不再执行"读发射者实时坐标/朝向 → 算新目标点"的每帧刷新（Origin=Launcher 时
	// mtxPos = 发射者实时坐标+实时朝向旋转 FLH，NoUpdate 若不隔离这里，目标点每帧被重写）
	if (Data->OriginNoUpdate && !_lockedTarget.IsEmpty())
	{
		frameTarget = _lockedTarget;
	}
	else
	{
	bool isOnTurret = !Data->OriginIsOnBody; // AutoWeapon 语义：yes=炮塔指向，no=车身指向
	switch (Data->Origin)
	{
	case VectorData::VectorOrigin::Launcher:
		{
			TechnoClass* pLT = abstract_cast<TechnoClass*>(_pLauncher);
			if (pLT && !IsDeadOrInvisible(pLT))
			{
			// 已知不一致（用户决定不修）：OriginIsOnWorld=yes 时此路径仍用发射者炮塔矩阵旋转 FLH，
			// 没有完全"无视单位朝向"（Self 分支有 OriginIsOnWorld 判断，Launcher 没有）。
			// 实际中 OriginIsOnWorld + Origin=Launcher 组合过于怪异，不做处理。
			CoordStruct mtxPos = GetFLHAbsoluteCoords(pLT, frameTargetFlh, isOnTurret);
				frameTarget = originPos + (mtxPos - pLT->GetCoords());
			}
			else
				frameTarget = GetFLHAbsoluteCoords(originPos, frameTargetFlh, mainFacingDir); // 官方API，不得修改
		}
		break;
	case VectorData::VectorOrigin::Self:
		if (Data->OriginIsOnWorld)
			frameTarget = GetFLHAbsoluteCoords(originPos, frameTargetFlh, DirStruct{}); // 世界坐标系，无视倾斜
		else if (pTechno)
		{
			// AutoWeapon 同款：Locomotor 矩阵 + TurretOffset + 炮塔旋转角
			CoordStruct mtxPos = GetFLHAbsoluteCoords(pTechno, frameTargetFlh, isOnTurret);
			frameTarget = originPos + (mtxPos - pTechno->GetCoords());
		}
		else
			frameTarget = GetFLHAbsoluteCoords(originPos, frameTargetFlh, mainFacingDir); // 官方API，不得修改
		break;
	default: // Target / Source
		frameTarget = GetFLHAbsoluteCoords(originPos, frameTargetFlh, mainFacingDir); // 官方API，不得修改
		break;
	}
	} // 关闭 NoUpdate 缓存的 else 块

	// TargetOffsetNormal 世界固定：偏移（世界坐标）叠加在旋转后的 TargetFLH 上，不随 F 轴转
	if (targetOffsetWorld)
	{
		frameTarget.X += _randomTargetOffset.X;
		frameTarget.Y += _randomTargetOffset.Y;
		frameTarget.Z += _randomTargetOffset.Z;
	}

	// NoUpdate=yes：首帧算完缓存锁定，后续帧走缓存，不再每帧重算
	if (Data->OriginNoUpdate)
	{
		_lockedTarget = frameTarget;
	}

	CoordStruct dirVec;
	dirVec.X = frameTarget.X - currentPos.X;
	dirVec.Y = frameTarget.Y - currentPos.Y;
	dirVec.Z = frameTarget.Z - currentPos.Z;
	double dirLen = std::sqrt(static_cast<double>(dirVec.X * dirVec.X + dirVec.Y * dirVec.Y + dirVec.Z * dirVec.Z));

	// 同步 Rocket loco 俯仰角：引擎自主移动姿态与 Vector 移动向量一致，
	// 防止 Vector 结束后引擎按错误的 Pitch 继续飞行导致命中偏移（Spawn 导弹用 RocketLocomotionClass）
	if (pTechno)
	{
		if (RocketLocomotionClass* rLoco = dynamic_cast<RocketLocomotionClass*>(
			abstract_cast<FootClass*, true>(pTechno)->Locomotor.get()))
		{
			double lenXY = std::sqrt(static_cast<double>(dirVec.X * dirVec.X + dirVec.Y * dirVec.Y));
			rLoco->CurrentPitch = static_cast<float>(std::atan2(dirVec.Z, lenXY));
		}
	}

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
		if (dirLen > 1e-6)
		{
			// AE 根基缺陷：Duration=N 实际只执行 N-1 个运动帧（末帧 AE 已移除）
			// 轨迹按"实际帧数 = 总帧数 - 1"均分，保证 AE 实际删除的那一帧恰好到位
			double adjustedSpeed = dirLen / (remainingFrames > 1 ? remainingFrames - 1 : 1);
			resultDisp.X = static_cast<int>(dirVec.X / dirLen * adjustedSpeed);
			resultDisp.Y = static_cast<int>(dirVec.Y / dirLen * adjustedSpeed);
			resultDisp.Z = static_cast<int>(dirVec.Z / dirLen * adjustedSpeed);

			// 抛物线弧高（Speed 模式影子算法：增量叠加，t 跟随实际路程，目标移动自动校准）
			if (_motion.arcHeight != 0)
			{
				// 影子沿 frameTarget 方向推进 adjustedSpeed（与直线均分同步）
				double ux = dirVec.X / dirLen, uy = dirVec.Y / dirLen, uz = dirVec.Z / dirLen;
				_motion.shadowX += ux * adjustedSpeed;
				_motion.shadowY += uy * adjustedSpeed;
				_motion.shadowZ += uz * adjustedSpeed;
				_motion.shadowTraveled += adjustedSpeed;

				// 剩余影子距离（3D）
				double sdx = frameTarget.X - _motion.shadowX;
				double sdy = frameTarget.Y - _motion.shadowY;
				double sdz = frameTarget.Z - _motion.shadowZ;
				double shadowDist = std::sqrt(sdx * sdx + sdy * sdy + sdz * sdz);

				// t = 已走路程 / 总路程（动态更新，目标移动时自动调整）
				double total = _motion.shadowTraveled + shadowDist;
				double t = (total > 1e-6) ? _motion.shadowTraveled / total : 0.0;
				if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0;

				// 弧高增量叠加（不覆盖直线位移）
				double arcOffset = CalcArcOffsetAt(t);
				double arcDelta = arcOffset - _motion.prevArcOffset;
				_motion.prevArcOffset = arcOffset;

				CoordStruct arcD{
					frameTarget.X - _initialLocation.X,
					frameTarget.Y - _initialLocation.Y,
					frameTarget.Z - _initialLocation.Z };
				ArcDelta3D ad = RotateArcDelta(arcD, _motion.arcRotation, arcDelta);
				resultDisp.X += static_cast<int>(ad.x);
				resultDisp.Y += static_cast<int>(ad.y);
				resultDisp.Z += static_cast<int>(ad.z);
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
		double speed = _motion.speed;

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

		// SpeedEndOnReach=yes/no 都走影子系统：弧线（影子坐标弧高）不依赖 SpeedEndOnReach；
		// no 时不做到达瞬移，继续追踪直到引擎自然触地爆炸

		// 以下：SpeedEndOnReach=yes，走影子系统
		// 有弧线：影子追踪完整 3D，弧高增量叠加
		double sdx = frameTarget.X - _motion.shadowX;
		double sdy = frameTarget.Y - _motion.shadowY;
		double sdz, shadowDist;
		if (_motion.arcHeight != 0)
		{
			sdz = frameTarget.Z - _motion.shadowZ;
			shadowDist = std::sqrt(sdx * sdx + sdy * sdy + sdz * sdz);
		}
		else
		{
			// 无弧线：仅 XY 距离，避免影子 Z 追踪目标导致 shadowDist 虚小
			sdz = 0.0;
			shadowDist = std::sqrt(sdx * sdx + sdy * sdy);
		}

		// SpeedEndOnReach：瞬移到目标位置，引擎正常到达检测会自然引爆（不手动Detonate，避免重复伤害）
		// 到达判定用真实 3D 距离（含 Z），避免高空俯冲时 XY 先对齐导致误判到达
		if (Data->SpeedEndOnReach)
		{
			double realDX = frameTarget.X - currentPos.X;
			double realDY = frameTarget.Y - currentPos.Y;
			double realDZ = frameTarget.Z - currentPos.Z;
			double realDist = std::sqrt(realDX * realDX + realDY * realDY + realDZ * realDZ);
			if (realDist <= speed)
			{
				// 强制挪移：到达帧直接把对象坐标设为目标格子坐标（完全重合），消除到位抖动
				if (pBullet)
				{
					pBullet->SetLocation(frameTarget);
					// 零位移：位置已由 SetLocation 设定，不再让 Vector.cpp 回退
					result.MoveDisp = { 0, 0, 0 };
					result.Force = false;
				}
				else if (pTechno)
				{
					// 强制挪移：直接 SetLocation 到目标格子坐标（含占位更新），不再经 MoveDisp 管线
					bool onBridge = pTechno->OnBridge;
					pTechno->UpdatePlacement(PlacementType::Remove);
					pTechno->OnBridge = onBridge;
					pTechno->SetLocation(frameTarget);
					pTechno->UpdatePlacement(PlacementType::Put);
					result.MoveDisp = { 0, 0, 0 };
					result.Force = false;
				}
				Deactivate();
				AdvanceFrame();
				return result;
			}
		}

		if (shadowDist > 1e-6)
		{
			// 影子沿 shadow→target 方向推进，步长钳位到剩余距离：
			// 末段 speed > 剩余距离时若仍推进满 speed 会越过目标，sdx 变号导致来回振荡（原地抽搐）
			double step = (speed < shadowDist) ? speed : shadowDist;
			double sInv = 1.0 / shadowDist;
			double shadowStepX = sdx * sInv * step;
			double shadowStepY = sdy * sInv * step;
			double shadowStepZ = 0.0;
			_motion.shadowX += shadowStepX;
			_motion.shadowY += shadowStepY;
			if (_motion.arcHeight != 0)
			{
				shadowStepZ = sdz * sInv * step;
				_motion.shadowZ += shadowStepZ;
			}
			// 无弧线：_shadowPosZ 不变（始终 = _initialLocation.Z），Z 由 t 插值
			_motion.shadowTraveled += step;

			// 重新计算影子距离（影子已移动）
			sdx = frameTarget.X - _motion.shadowX;
			sdy = frameTarget.Y - _motion.shadowY;
			if (_motion.arcHeight != 0)
			{
				sdz = frameTarget.Z - _motion.shadowZ;
				shadowDist = std::sqrt(sdx * sdx + sdy * sdy + sdz * sdz);
			}
			else
			{
				shadowDist = std::sqrt(sdx * sdx + sdy * sdy);
			}

			// t = 已走路程 / 总路程（动态更新，目标移动时自动调整）
			double total = _motion.shadowTraveled + shadowDist;
			double t = (total > 1e-6) ? _motion.shadowTraveled / total : 0.0;
			if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0;

			// 实际位移：影子步长（XY）
			resultDisp.X = static_cast<int>(shadowStepX);
			resultDisp.Y = static_cast<int>(shadowStepY);

			if (_motion.arcHeight != 0)
			{
				// 有弧线：Z 用影子增量 + 弧高增量叠加
				resultDisp.Z = static_cast<int>(shadowStepZ);
			}
			else
			{
				// 无弧线：Z 从抛射体起始高度 lerp 到目标高度
				// _shadowPosZ 在无弧线时冻结为抛射体起始 Z，_initialLocation.Z 可能是目标 Z（Origin=Target 时不同）
				double targetZ = _motion.shadowZ + (frameTarget.Z - _motion.shadowZ) * t;
				resultDisp.Z = static_cast<int>(targetZ - currentPos.Z);
			}

			if (_motion.arcHeight != 0)
			{
				double arcOffset = CalcArcOffsetAt(t);
				double arcDelta = arcOffset - _motion.prevArcOffset;
				_motion.prevArcOffset = arcOffset;

				CoordStruct arcD{
					frameTarget.X - _initialLocation.X,
					frameTarget.Y - _initialLocation.Y,
					frameTarget.Z - _initialLocation.Z };
				ArcDelta3D ad = RotateArcDelta(arcD, _motion.arcRotation, arcDelta);
				resultDisp.X += static_cast<int>(ad.x);
				resultDisp.Y += static_cast<int>(ad.y);
				resultDisp.Z += static_cast<int>(ad.z);
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