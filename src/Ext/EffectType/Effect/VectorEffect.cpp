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
// 法向量随单位姿态旋转（小圆/大圆共享数学——无状态纯函数，严禁在函数内共享任何状态）：
// 基础法向量（球坐标 facing0/tilt0）先绕世界 Z 轴转 facingU（单位水平朝向角），
// 再绕单位 L 轴 u=(-sinFU, cosFU, 0) 转 tiltU（单位倾斜角，Rodrigues）。
// 小圆与大圆各自喂各自的输入（facing0/tilt0 来源不同、状态分别存 _motion/_originMotion），
// 两套法向量互相孤立——竖小圆+水平大圆等任意组合不产生耦合。
// ============================================================================
static void RotateNormalByUnit(double facing0, double tilt0, double facingU, double tiltU,
	double& nx, double& ny, double& nz)
{
	// 基础法向量（球坐标）→ 笛卡尔
	double bx = std::cos(tilt0) * std::cos(facing0);
	double by = std::cos(tilt0) * std::sin(facing0);
	double bz = std::sin(tilt0);
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
	nx = x1 * ct + cx * st + ux * dot * (1.0 - ct);
	ny = y1 * ct + cy * st + uy * dot * (1.0 - ct);
	nz = z1 * ct + cz2 * st;
}

// ============================================================================
// 连线坐标系（OriginIsOnVectorOrigin=no，Target/Launcher/Source 通吃）——解算起始点管线用：
// F 轴 = 单位→弹体的连线方向（水平投影角，RA2 坐标系由 Point2Dir 处理）。
// CoordinateTilt 决定这条线取真实 3D（高低差进 tilt，ResolveTilting 混合出斜向摆放）
// 还是水平投影（tilt=0，与地面平行）。
// ============================================================================
static DirStruct ResolveLinePose(TechnoClass* pUnit, const CoordStruct& bulletPos, bool use3D, double& tiltOut)
{
	tiltOut = 0.0;
	CoordStruct uPos = pUnit->GetCoords();
	double ddx = static_cast<double>(bulletPos.X - uPos.X);
	double ddy = static_cast<double>(bulletPos.Y - uPos.Y);
	double ddz = static_cast<double>(bulletPos.Z - uPos.Z);
	double lenXY = std::sqrt(ddx * ddx + ddy * ddy);
	if (use3D && lenXY > 1e-6)
		tiltOut = std::atan2(ddz, lenXY); // 连线高低角（弹体高于单位为正，F 朝上斜）
	return Point2Dir(uPos, bulletPos); // 官方API，不得修改
}

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

	_firstFramePos = {};
	_startPoint = {};
	_bigCircleStartPoint = {};
	_lockedSmallCircleTarget = {};
	_vectorAcquireZ = 0;
	_pLauncher = nullptr;
	_pSource = nullptr;

	_fAxisRad = 0.0;
	_fAxisDir = DirStruct(0);
	_tiltRad = 0.0;

	_originFacing = 0.0;
	_originTilt = 0.0;
	_baseOriginFacing = 0.0;
	_baseOriginTilt = M_PI / 2.0;

	_randomTargetOffset = {};
	_originTargetOffset = {};

	_bigCircleOffset = {};
	_prevBigCircleCenter = {};
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
// 主 Origin（_startPoint）与大圆解算起始点（_bigCircleStartPoint）共用
CoordStruct VectorEffect::TrackOriginCoord(ObjectClass* pUnit, bool noUpdate, CoordStruct& last)
{
	if (!noUpdate && pUnit && !IsDeadOrInvisible(pUnit))
		last = pUnit->GetCoords();
	return last;
}

// ========================================================================
// "解算倾斜"管线辅助实现（OriginFLH 偏移完整化）
// 时序统一：取基准单位 → 定坐标系(facing+tilt) → 定偏移量(FLH) → 算完整基准点。
// 挂载快照（LockFacing/补读）与每帧刷新（GetVectorResult）共用同一套逻辑；
// 其他散落取点机制（smallCircleTarget/大圆解算起始点等）可并入本管线。
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
CoordStruct VectorEffect::ResolveTilting(const CoordStruct& basePos, const CoordStruct& flh,
	const DirStruct& facing, double tilt)
{
	double cosT = std::cos(tilt), sinT = std::sin(tilt);
	CoordStruct flhRot = flh;
	flhRot.X = static_cast<int>(flh.X * cosT - flh.Z * sinT);   // f'：F/H 俯仰混合
	flhRot.Z = static_cast<int>(flh.X * sinT + flh.Z * cosT);   // h'：H/F 俯仰混合
	flhRot.Y = flh.Y;                                          // l'：左右不受俯仰影响
	return GetFLHAbsoluteCoords(basePos, flhRot, facing); // 官方API，不得修改：引擎坐标系修正
}

// ========================================================================
// "坐标点取值管线"唯一摆点实现（归一化：OriginFLH/TargetFLH/大圆挂点共用）。
// 语义定稿见"坐标点取值管线归一化_改动点.txt"四种摆法：
//   ① UnitOwn+随倾斜 = AutoWeapon 深度（Locomotor 矩阵 + TurretOffset 转轴 + 炮塔/车身差角）
//   ② UnitOwn+水平   = 只按炮塔/车身水平朝向摆（不随坡面俯仰）
//   ③ LineC2P        = F 轴 = lineFrom 起点（Origin 本体中心）→ 弹体现在位置，
//                       use3DLine(=CoordinateTilt)=yes 高低差进 3D（ResolveTilting tilt 混合）
//   ④ World          = 纯世界轴（DirStruct{} 直摆，无视姿态）
// 基准点 base 由调用方按 NoUpdate 刷新；矩阵路径剥单位位移只留姿态偏移再叠 base。
// ========================================================================
CoordStruct VectorEffect::ResolveTiltingFrame(const CoordStruct& base, const CoordStruct& flh,
	FlhFrame frame, TechnoClass* pAnchor, bool isOnTurret, bool sameTilt,
	const CoordStruct* lineFrom, bool use3DLine,
	const DirStruct& fallbackFacing, const CoordStruct& currentPos)
{
	switch (frame)
	{
	case FlhFrame::World:
		return GetFLHAbsoluteCoords(base, flh, DirStruct{}); // 官方API，不得修改：世界轴，无视姿态
	case FlhFrame::UnitOwn:
		if (pAnchor && !IsDeadOrInvisible(pAnchor))
		{
			if (sameTilt)
			{
				// ① AutoWeapon 深度：完整姿态矩阵（Locomotor 矩阵 + TurretOffset 转轴 + 炮塔/车身差角）。
				// 剥掉单位位移只留姿态偏移，叠加到调用方计算点 base（NoUpdate 控制的基准）
				CoordStruct mtxPos = GetFLHAbsoluteCoords(pAnchor, flh, isOnTurret); // 官方API，不得修改
				return base + (mtxPos - pAnchor->GetCoords());
			}
			// ② 水平 2D：不随倾斜，只按炮塔/车身水平朝向
			DirStruct hFacing = isOnTurret
				? pAnchor->TurretFacing().Current()   // 官方API，不得修改
				: pAnchor->PrimaryFacing.Current();   // 官方API，不得修改
			return GetFLHAbsoluteCoords(base, flh, hFacing); // 官方API，不得修改
		}
		// 锚死/无锚：落水平兜底（与 smallCircleTarget 死锚回退同语义）
		return GetFLHAbsoluteCoords(base, flh, fallbackFacing); // 官方API，不得修改
	case FlhFrame::LineC2P:
	{
		// ③ 连线坐标系：F 轴 = lineFrom 起点（Origin 本体中心）→ 弹体现在位置
		CoordStruct lineFromCopy;
		if (lineFrom)
			lineFromCopy = *lineFrom; // 拷贝再判空（IsEmpty 非 const，不能经 const 指针直调）
		if (lineFrom && !lineFromCopy.IsEmpty())
		{
			double ddx = static_cast<double>(currentPos.X - lineFromCopy.X);
			double ddy = static_cast<double>(currentPos.Y - lineFromCopy.Y);
			double ddz = static_cast<double>(currentPos.Z - lineFromCopy.Z);
			double lenXY = std::sqrt(ddx * ddx + ddy * ddy);
			double lineTilt = 0.0;
			DirStruct lineDir = fallbackFacing;
			if (lenXY > 1e-6)
			{
				if (use3DLine)
					lineTilt = std::atan2(ddz, lenXY); // 连线高低角（弹体高于起点为正，F 朝上斜）
				lineDir = Point2Dir(lineFromCopy, currentPos); // 官方API，不得修改
			}
			return ResolveTilting(base, flh, lineDir, lineTilt);
		}
		return GetFLHAbsoluteCoords(base, flh, fallbackFacing); // 官方API，不得修改：无起点水平兜底
	}
	default: // FlhFrame::Fallback2D
		return GetFLHAbsoluteCoords(base, flh, fallbackFacing); // 官方API，不得修改
	}
}

// OriginFLH 复合解算入口：把 OriginFLH 按 OriginIsOnWorld / AllowOriginTilt / IsOnOrigin /
// OriginIsOnBody 分派摆成世界偏移叠加到 base（挂载期/补读期/每帧共用，消灭三处手写拷贝）。
// base = Origin 单位坐标（挂载/补读期）或本帧已刷新的单位坐标（每帧）；
// fallbackFacing = 水平兜底朝向（挂载期 _fAxisDir，每帧 fAxisDir）；
// currentPos = 弹体现在位置（连线终点）。
// 死亡 = 停止计算（基线）：参照单位死亡后不再刷新/回退/重算，本函数直接返回传入的
// base（此时 base 已是死亡帧写回的完整解算点，见每帧调用点注释），与 NoUpdate=yes 同构；
// AllowOriginTilt 只决定活时的深度（是否随姿态），不参与死后判定。
CoordStruct VectorEffect::ResolveOriginTilting(const CoordStruct& base, const DirStruct& fallbackFacing,
	const CoordStruct& currentPos)
{
	// ④ OriginIsOnWorld=yes：OriginFLH 纯世界轴（无视单位朝向/姿态）——最高优先
	if (Data->OriginIsOnWorld)
	{
		return ResolveTiltingFrame(base, Data->OriginFLH, FlhFrame::World, nullptr, false, false,
			nullptr, false, fallbackFacing, currentPos);
	}

	// Self：载体（pTechno）或弹体（pBullet）活着是 Vector 运行的前提，无死锚概念
	if (Data->Origin == VectorData::VectorOrigin::Self)
	{
		if (Data->AllowOriginTilt)
		{
			if (pTechno)
			{
				// ① 载体单位：AutoWeapon 矩阵深度（onTurret/onbody 由 OriginIsOnBody 分）
				return ResolveTiltingFrame(base, Data->OriginFLH, FlhFrame::UnitOwn, pTechno,
					!Data->OriginIsOnBody, true, nullptr, false, fallbackFacing, currentPos);
			}
			if (pBullet)
			{
				// 弹体侧无单位锚：弹体自身朝向水平摆（fallbackFacing = 锁定弹体朝向）
				return ResolveTiltingFrame(base, Data->OriginFLH, FlhFrame::Fallback2D, nullptr, false, false,
					nullptr, false, fallbackFacing, currentPos);
			}
			return base;
		}
		// ② Self + AllowOriginTilt=no：水平 2D
		return ResolveTiltingFrame(base, Data->OriginFLH, FlhFrame::Fallback2D, nullptr, false, false,
			nullptr, false, fallbackFacing, currentPos);
	}

	// 非 Self：参照单位死亡 → 停止计算，OriginFLH 不再参与（无论 AllowOriginTilt）
	TechnoClass* pOriginTechno = FindOriginTechno();
	if (!pOriginTechno || IsDeadOrInvisible(pOriginTechno))
	{
		return base;
	}

	if (Data->AllowOriginTilt)
	{
		if (Data->IsOnOrigin)
		{
			// ① 单位自身坐标系：AutoWeapon 矩阵深度（退役"水平朝向+地形采样倾斜"，7a 决议）
			return ResolveTiltingFrame(base, Data->OriginFLH, FlhFrame::UnitOwn, pOriginTechno,
				!Data->OriginIsOnBody, true, nullptr, false, fallbackFacing, currentPos);
		}
		// ③ 连线坐标系：F 轴 = Origin 本体中心 → 弹体现在位置
		// （CoordinateTilt=yes 高低差进 3D，no=水平投影）
		CoordStruct lineFromPos = pOriginTechno->GetCoords(); // 连线起点 = Origin 本体中心
		return ResolveTiltingFrame(base, Data->OriginFLH, FlhFrame::LineC2P, nullptr, false, false,
			&lineFromPos, Data->CoordinateTilt, fallbackFacing, currentPos);
	}

	// ② 水平 2D（AllowOriginTilt=no，活锚）：fallbackFacing = fAxisDir
	//（每帧已按 IsOnOrigin 实时刷新单位/连线水平朝向）
	return ResolveTiltingFrame(base, Data->OriginFLH, FlhFrame::Fallback2D, nullptr, false, false,
		nullptr, false, fallbackFacing, currentPos);
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
	// _prevBigCircleCenter 不在此初始化：圆心追踪依赖 Origin 移动系统首帧的 skipOriginUpdate 赋值

	_firstFramePos = pObject->GetCoords();
	_vectorAcquireZ = _firstFramePos.Z;  // Circle 圆心高度基准：获取 Vector 时的 Z
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
			// 消费端 GetFLHAbsoluteOffset 世界角 = -(fAxisDir + flhAngle)，要求 = 近交点角 + deg：
			// flhAngle = -fAxisDirSim - β - deg
			//   β = atan2 近交点世界角（目标点指向抛射体）
			//   fAxisDirSim = 消费端 fAxisDir 统一取 DirStruct 原值（NoUpdate 不影响坐标系）
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
				// 近交点基准：targetPos 取 pBullet->TargetCoords（与 OnStart 326 行 _fAxisDir 一致）
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
					// 消费端 fAxisDir 统一取 DirStruct 原值（NoUpdate 不影响坐标系），直接复刻
					double fAxisDirSim = alpha;
					flhAngle = -fAxisDirSim - beta - Math::deg2rad(deg);
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
				_startPoint = cacheStatus->GetVectorCachedCell();
			}
			else if (gotKamikaze)
			{
				_startPoint = kamikazePos;
			}
			else if (TryGetSpawnManagerTarget(pTechno, _startPoint))
			{
				// 未进 Kamikaze 容器（导弹未全部发射）时读源头：SpawnManager 目标
			}
			else if (pTechno->Target)
			{
				_startPoint = pTechno->Target->GetCoords();
			}
			else
			{
				// Kamikaze 容器此刻可能还没加入导弹（发射后才加入）：不锁自身，
				// 留空由 GetVectorResult 首帧补读
				_startPoint = CoordStruct::Empty;
			}
		}
		else if (pBullet)
		{
			_startPoint = pBullet->TargetCoords;
		}
		else
		{
			_startPoint = pObject->GetCoords();
		}
		break;

	case VectorData::VectorOrigin::Launcher:
		// 无论 NoUpdate 都锁定基线（与 Target 分支一致）：NoUpdate=yes 直接用，
		// no 每帧快照刷新覆盖；launcher 死亡时冻结此基线作为 origin 解算起点
		if (pBullet && pBullet->Owner)
			_startPoint = pBullet->Owner->GetCoords();
		else if (pTechno)
			_startPoint = pTechno->GetCoords();
		else
			_startPoint = pObject->GetCoords();
		break;

	case VectorData::VectorOrigin::Source:
		// 无论 NoUpdate 都锁定基线（与 Target/Launcher 分支一致）：死亡时冻结此基线
		if (AE && AE->pSource)
			_startPoint = AE->pSource->GetCoords();
		else
			_startPoint = pObject->GetCoords(); // 兜底与 Target 分支一致
		if (AE && AE->pSource)
			_pSource = AE->pSource;
		break;

	case VectorData::VectorOrigin::Self:
		// OriginFLH 摆点不在 InitOrigin 做（原此处 GetFLHAbsoluteCoords 直摆 = Self 特例，
		// 且 pTechno 侧恒炮塔不查 OriginIsOnBody）——统一移交 LockFacing 尾挂载复合
		// ResolveOriginTilting（归一化：所有 Origin 一条"坐标点取值"管线）。
		// 此处只定朝向与单位坐标：pBullet=弹体速度朝向；pTechno=载体单位炮塔/车身朝向。
		if (pBullet)
		{
			double bulletRad = std::atan2(pBullet->Velocity.X, pBullet->Velocity.Y);
			DirStruct bulletFacing;
			bulletFacing.SetValue(static_cast<short>(bulletRad * 32768.0 / M_PI));
			_startPoint = pBullet->GetCoords(); // 单位坐标，OriginFLH 偏移由挂载复合复合
			_fAxisDir = bulletFacing; // 锁定初始朝向
			_fAxisRad = _fAxisDir.GetRadian();
		}
		else if (pTechno)
		{
			DirStruct unitFacing = Data->OriginIsOnBody
				? pTechno->PrimaryFacing.Current()     // 官方API，不得修改
				: pTechno->TurretFacing().Current();   // 官方API，不得修改
			_startPoint = pTechno->GetCoords(); // 单位坐标，OriginFLH 偏移由挂载复合复合
			_fAxisDir = unitFacing; // 锁定初始朝向
			_fAxisRad = _fAxisDir.GetRadian();
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
		_fAxisRad = lenXY > 1e-6 ? std::atan2(fwY, fwX) : 0.0;
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

	// 初始化 3D 法向量（从球坐标 _fAxisRad/_tiltRad 转换）
	// 球坐标→笛卡尔：X=cos(tilt)cos(facing), Y=cos(tilt)sin(facing), Z=sin(tilt)
	// _motion.normalX/Y/Z 是世界单位法向量（圆面法线方向）：
	//   facing 影响法线在 XY 平面的指向，tilt 影响法线仰角（tilt=PI/2 → (0,0,1) 垂直向上）
	{
		double ct = std::cos(_tiltRad), st = std::sin(_tiltRad);
		double cf = std::cos(_fAxisRad), sf = std::sin(_fAxisRad);
		_motion.normalX = ct * cf;
		_motion.normalY = ct * sf;
		_motion.normalZ = st;
	}

	// --- F 轴基准挂载锁定（摆放 FLH 用；与 NormalVector 彻底解耦）---
	// F 轴参考系来源：IsOnOrigin=yes 用 Origin 单位自身朝向，no 用 Origin→弹体连线
	// （默认值按 Origin 类型推导：Launcher/Self→yes，Target/Source→no，见 VectorData.h 解析处）
	// 无论设不设 NormalVector 都执行（NormalVector 只决定圆面倾斜，不得参与 F 轴基准取值）。
	// _fAxisRad 的同步只在 !hasNormal 做（hasNormal 时 _fAxisRad 归法向量维护，见上方球坐标段）。
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
					_fAxisDir = Data->OriginIsOnBody
						? pLauncherTechno->PrimaryFacing.Current()     // 官方API，不得修改
						: pLauncherTechno->TurretFacing().Current();   // 官方API，不得修改
				}
				else
				{
					_fAxisDir = Point2Dir(pLauncherTechno->GetCoords(), pObject->GetCoords()); // 发射者→弹体连线
				}
				if (!hasNormal) _fAxisRad = _fAxisDir.GetRadian();
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
						_fAxisDir = Data->OriginIsOnBody
							? pTargetTechno->PrimaryFacing.Current()     // 官方API，不得修改
							: pTargetTechno->TurretFacing().Current();   // 官方API，不得修改
						if (!hasNormal) _fAxisRad = _fAxisDir.GetRadian();
						break;
					}
					// 目标无朝向（格子）：回退连线
				}
				_fAxisDir = Point2Dir(targetPos, pObject->GetCoords()); // 官方API，不得修改：目标→弹体连线
				if (!hasNormal) _fAxisRad = _fAxisDir.GetRadian();
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
						_fAxisDir = Data->OriginIsOnBody
							? pSourceTechno->PrimaryFacing.Current()     // 官方API，不得修改
							: pSourceTechno->TurretFacing().Current();   // 官方API，不得修改
						if (!hasNormal) _fAxisRad = _fAxisDir.GetRadian();
						break;
					}
					// 来源无朝向：回退连线
				}
				_fAxisDir = Point2Dir(sourcePos, pObject->GetCoords()); // 官方API：Source→弹体连线
				if (!hasNormal) _fAxisRad = _fAxisDir.GetRadian();
			}
			break;
		}

		default: // Self（自身朝向即 F 轴，无"另一单位朝向"，IsOnOrigin 不区分）
		{
			// Self 的 _fAxisDir/_fAxisRad 挂载值 InitOrigin 已按自身朝向设好；这里只在 !hasNormal
			// 重同步一次 _fAxisRad（hasNormal 时 _fAxisRad 归法向量维护，Self 也不例外）。
			if (!hasNormal)
			{
				if (pBullet)
					_fAxisRad = std::atan2(pBullet->Velocity.X, pBullet->Velocity.Y);
				else if (pTechno)
					_fAxisRad = pTechno->TurretFacing().Current().GetRadian();
			}
			break;
		}
		}
	}

	// OriginFLH 挂载偏移算进存档点：取基准时就把偏移算好，NoUpdate=yes 直接用（完整基准点冻结）。
	// 统一走"坐标点取值管线" ResolveOriginTilting（挂载快照 = 生效瞬间一次，之后 yes 冻结）。
	// 所有 Origin（含 Self，原 Self 在 InitOrigin 自摆 = 特例）一视同仁：Self 的 pTechno 载体
	// 可走①矩阵深度（原只按水平朝向摆），弹体侧按自身朝向水平摆；姿态随快照定死。
	// 存档点为空（techno 侧 Origin=Target 挂载瞬间目标未就绪，留待首帧补读）时不在此算，
	// 等补读段补一次同款计算。
	if (!Data->OriginFLH.IsEmpty() && !_startPoint.IsEmpty())
	{
		_startPoint = ResolveOriginTilting(_startPoint, _fAxisDir, pObject->GetCoords());
	}

	// TargetOffsetNormal 世界固定（IsNormalOnOrigin=no）：把 FLH 落点按锁定朝向转成世界坐标，
	// 消费端把偏移叠加在旋转后的 TargetFLH 上，不随 F 轴（单位朝向）转动。
	// 注：hasNormal 时 _fAxisDir 未锁定（默认朝向），该组合的世界固定基准按实测调整。
	if (!Data->IsNormalOnOrigin && !Data->TargetOffsetNormal.IsEmpty())
	{
		_randomTargetOffset = GetFLHAbsoluteCoords(CoordStruct::Empty, _randomTargetOffset, _fAxisDir); // 官方API
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
	_motion.shadowX = _firstFramePos.X;
	_motion.shadowY = _firstFramePos.Y;
	_motion.shadowZ = _firstFramePos.Z;
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
		_firstFramePos = pObject->GetCoords();

	// InitialDelay 期间 AE 存在但未启动，不施加任何位移
	if (!_started)
	{
		AdvanceFrame();
		return result;
	}

	// 每帧运动刷新目标缓存：挂载时（OnStart）已写第一笔，这里跟随目标移动刷新；
	// 目标死亡时停止写入，冻结最后有效值
	CacheTargetNow();

	// 目标坐标固化：OnStart 未锁定（Pending）时补读，读到即锁定到 _startPoint，
	// 防止引擎后续清空（目标死亡/管理器清空）导致 smallCircleTarget 失效
	if (Data->Origin == VectorData::VectorOrigin::Target
		&& _startPoint.IsEmpty() && pTechno)
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
			_startPoint = targetPos;
			// NoUpdate=yes：每帧偏移计算（下方管线段）只对 no 执行，补读的单位坐标必须在此复合一次并冻结
			// （techno 侧 SpawnManager/Aircraft 挂载瞬间目标未就绪，挂载复合被空存档守卫拦住，
			//  目标到手后补上这次"位置 + 朝向 + 偏移"计算，与 LockFacing 末尾挂载复合同语义，
			//  统一走 ResolveOriginTilting——三维（AllowOriginTilt=yes）+ 补读组合缺口随归一化补齐）。
			if (Data->OriginNoUpdate && !Data->OriginFLH.IsEmpty())
			{
				// fallbackFacing（② 水平/兜底出口）按 IsOnOrigin 现算（fAxisDir 段在本段之后才跑）：
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
				_startPoint = ResolveOriginTilting(_startPoint, flhFacing, pObject->GetCoords());
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
			result.FrozenPos = _firstFramePos;
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
	// 供多处使用：AllowOriginTilt 的 OriginFLH/CircleOrigin 旋转（解算起始点）、小圆/大圆法向量随单位
	// 倾斜转动（IsNormalOnOrigin / OriginIsNormalOnOrigin 的 tiltU）。只计算不注入（法向量跟随由
	// 对应 IsNormalOnOrigin 段负责）。采样逻辑与挂载快照共用 SampleOriginTilt。
	// 触发条件 = 谁消费谁触发：解算起始点三维（AllowOriginTilt）、小圆法向量随动（IsNormalOnOrigin）、
	// 大圆法向量随动（OriginIsNormalOnOrigin）——三者任一需要且配了 Circle 参数才采样。
	double originTerrainTilt = 0.0;
	bool hasCircleForTilt = Data->CircleRadius > 0 || Data->CircleAnglePerStep > 0.0
		|| (Data->CircleRandomRadiusMax > Data->CircleRandomRadiusMin)
		|| (Data->CircleRandomAngleMax > Data->CircleRandomAngleMin);
	if ((Data->AllowOriginTilt || Data->OriginIsNormalOnOrigin || Data->IsNormalOnOrigin) && hasCircleForTilt && !Data->OriginIsOnWorld)
	{
		originTerrainTilt = SampleOriginTilt(FindOriginTechno());
	}

	double effectiveFacing = _fAxisRad;    // 倾斜域初始（hasNormal 时 = 法向量球坐标 facing；!hasNormal 时下方同步为 F 轴基准）
	double effectiveTilt = _tiltRad;        // 倾斜域初始（hasNormal 时 = 法向量 tilt）
	// F 轴基准（摆放 FLH 用）初值 = 挂载时按 IsOnOrigin 锁定的 _fAxisDir（与 NormalVector 解耦）。
	// 不用 Radians2Dir(_fAxisRad)：hasNormal 时 _fAxisRad 是法向量 facing，会污染摆放基准。
	// （Radians2Dir(GetRadian()) 往返另有 90° 偏置，_fAxisDir 保留 DirStruct 原值不往返）
	DirStruct fAxisDir = _fAxisDir; // 官方API，不得修改
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
		effectiveFacing = _fAxisDir.GetRadian();
	}

	// OriginIsOnWorld：锁定世界坐标系（朝北），覆盖 Origin 朝向
	if (Data->OriginIsOnWorld)
	{
		fAxisDir = DirStruct{};
		effectiveFacing = 0.0;
		effectiveTilt = 0.0;
	}

	// 统一朝向算法（每帧）：计算 F 轴基准 fAxisDir（摆放 FLH 用，与 NormalVector 解耦）——
	// F 轴来源：IsOnOrigin=yes 用 Origin 单位自身朝向，no 用 Origin→弹体连线（默认值按 Origin 类型推导）。
	// 无论设不设 NormalVector 都执行：NormalVector 只决定圆面倾斜，不得参与 F 轴基准取值。
	// 段内对倾斜量（effectiveFacing/effectiveTilt）的同步只在 !hasNormal 执行
	// （hasNormal 时倾斜量归法向量维护，见 IsNormalOnOrigin 段）。
	// 另：NoUpdate 只切换计算点（锁定 _startPoint vs 实时坐标），不切换坐标系/朝向算法。
	if (!Data->AllowOriginTilt && !Data->OriginIsOnWorld)
	{
		switch (Data->Origin)
		{
		case VectorData::VectorOrigin::Source:
			// 计算点：NoUpdate=yes 用锁定值，no 每帧刷新（三态跟踪：死亡冻结）
			TrackOriginCoord(_pSource, Data->OriginNoUpdate, _startPoint);
			if (!_startPoint.IsEmpty())
			{
				if (Data->IsOnOrigin)
				{
					TechnoClass* pSourceTechno = abstract_cast<TechnoClass*>(_pSource);
					if (pSourceTechno && !IsDeadOrInvisible(pSourceTechno))
					{
						fAxisDir = Data->OriginIsOnBody
							? pSourceTechno->PrimaryFacing.Current()     // 官方API，不得修改
							: pSourceTechno->TurretFacing().Current();   // 官方API，不得修改
						if (!hasNormal) effectiveFacing = fAxisDir.GetRadian();
						break;
					}
					// 来源无朝向：回退连线
				}
				// 来源活着或已死亡：都用快照算朝向（死亡后冻结指向死亡点）
				fAxisDir = Point2Dir(_startPoint, currentPos); // 官方API，不得修改
				if (!hasNormal) effectiveFacing = fAxisDir.GetRadian();
				// 连线高低角不再进圆面倾斜（effectiveTilt）——圆面倾斜唯一来源 = IsNormalOnOrigin 法向量随动
			}
			break;
		case VectorData::VectorOrigin::Target:
		{
			bool isGround = (pBullet && !abstract_cast<TechnoClass*>(pBullet->Target));
			if (isGround && _movementFrames > 1)
				break;
			// 计算点：默认锁定值起步，NoUpdate=no 才走缓存/引擎链动态获取
			CoordStruct targetPos = _startPoint;
			bool gotTarget = !targetPos.IsEmpty();
			if (!gotTarget && !Data->OriginNoUpdate)
			{
				gotTarget = GetTargetPosFromChain(targetPos, true);
			}
			if (gotTarget)
				_startPoint = targetPos; // 跟随：更新锁定值
			else if (targetPos.IsEmpty())
				break; // 从未有过目标：保持朝向
			if (Data->IsOnOrigin)
			{
				AbstractClass* pTgt = pBullet ? pBullet->Target : (pTechno ? pTechno->Target : nullptr);
				TechnoClass* pTargetTechno = abstract_cast<TechnoClass*>(pTgt);
				if (pTargetTechno && !IsDeadOrInvisible(pTargetTechno))
				{
					fAxisDir = Data->OriginIsOnBody
						? pTargetTechno->PrimaryFacing.Current()     // 官方API，不得修改
						: pTargetTechno->TurretFacing().Current();   // 官方API，不得修改
					if (!hasNormal) effectiveFacing = fAxisDir.GetRadian();
					break;
				}
				// 目标无朝向（格子）：回退连线
			}
			fAxisDir = Point2Dir(targetPos, currentPos); // 官方API，不得修改
			if (!hasNormal) effectiveFacing = fAxisDir.GetRadian();
			// 连线高低角不再进圆面倾斜（effectiveTilt）——圆面倾斜唯一来源 = IsNormalOnOrigin 法向量随动
		}
			break;

		case VectorData::VectorOrigin::Self:
			if (pTechno)
			{
				fAxisDir = Data->OriginIsOnBody
					? pTechno->PrimaryFacing.Current()     // 官方API，不得修改
					: pTechno->TurretFacing().Current();   // 官方API，不得修改
				if (!hasNormal) effectiveFacing = fAxisDir.GetRadian();
			}
			else if (pBullet)
			{
				fAxisDir = Facing(pBullet, currentPos); // 官方API，不得修改
				if (!hasNormal) effectiveFacing = fAxisDir.GetRadian();
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
						fAxisDir = Data->OriginIsOnBody
							? pLauncherTechno->PrimaryFacing.Current()     // 官方API，不得修改
							: pLauncherTechno->TurretFacing().Current();   // 官方API，不得修改
						if (!hasNormal) effectiveFacing = fAxisDir.GetRadian();
					}
					else
					{
						// 发射者→弹体连线
						fAxisDir = Point2Dir(pLauncherTechno->GetCoords(), currentPos); // 官方API，不得修改
						if (!hasNormal) effectiveFacing = fAxisDir.GetRadian();
					}
				}
			}
			break;
		}
	}

	// IsNormalOnOrigin：圆面法向量随 Origin 单位转动（facing + tilt 全跟随）
	// 基础法向量 = OnStart 锁定的球坐标（_fAxisRad/_tiltRad，来自 NormalVector/NormalRandom/默认水平），
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

		// 基础法向量（OnStart 锁定）随单位姿态旋转（共享管线 RotateNormalByUnit，无状态）；
		// 无自定义法线时基础法向量默认竖直（水平圆面），单位倾斜 → 法向量转 → 圆面自然倾斜。
		double baseTilt = hasNormal ? _tiltRad : M_PI / 2.0;
		RotateNormalByUnit(_fAxisRad, baseTilt, facingU, tiltU,
			_motion.normalX, _motion.normalY, _motion.normalZ);

		// 同步倾斜圆面数学输入（最终法向量 → 球坐标）
		double lenXY = std::sqrt(_motion.normalX * _motion.normalX + _motion.normalY * _motion.normalY);
		effectiveFacing = lenXY > 1e-6 ? std::atan2(_motion.normalY, _motion.normalX) : 0.0;
		effectiveTilt = lenXY > 1e-6 ? std::atan2(_motion.normalZ, lenXY) : (_motion.normalZ > 0 ? M_PI / 2.0 : -M_PI / 2.0);
	}

	// 3D 法向量旋转覆盖：当 NormalF/L/HAnglePerStep 设定时，无视 F 轴基准变化
	// 只更新倾斜量（effectiveFacing/effectiveTilt）；不碰 fAxisDir——
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
	CoordStruct startPoint = currentPos;

	switch (Data->Origin)
	{
	case VectorData::VectorOrigin::Target:
		if (Data->OriginNoUpdate)
			startPoint = _startPoint.IsEmpty() ? currentPos : _startPoint; // 锁定初始目标
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
				_startPoint = updated; // 跟随：更新锁定值
				startPoint = _startPoint;
			}
			else
				startPoint = _startPoint.IsEmpty() ? currentPos : _startPoint; // 抛弃 update → 回退锁定坐标
		}
		break;
	case VectorData::VectorOrigin::Launcher:
		if (Data->OriginNoUpdate)
			startPoint = _startPoint;
		else
		{
			TrackOriginCoord(_pLauncher, false, _startPoint); // 发射者活着：每帧快照；死亡：冻结
			startPoint = _startPoint;
		}
		break;
	case VectorData::VectorOrigin::Source:
		if (Data->OriginNoUpdate)
			startPoint = _startPoint;
		else
		{
			TrackOriginCoord(_pSource, false, _startPoint); // 来源活着：每帧快照；死亡：冻结
			startPoint = _startPoint;
		}
		break;
	case VectorData::VectorOrigin::Self:
		startPoint = Data->OriginNoUpdate ? _startPoint : currentPos;
		break;
	}

	// OriginFLH 完整解算：解算点的定义 = Origin 单位坐标 + OriginFLH 经
	// OriginIsOnBody/AllowOriginTilt/CoordinateTilt 等复合计算的最终偏移（设计目标，偏移是
	// 解算点的组成部分）。统一"坐标点取值管线" ResolveOriginTilting（与挂载复合/补读同一入口）：
	//   NoUpdate=yes 不在此算——挂载复合已算入偏移冻结，直接用（冻结）。
	//   NoUpdate=no 才执行：本帧单位坐标已刷新，按实时姿态复合计算偏移。分派：
	//   ④ OriginIsOnWorld=yes → 纯世界轴；
	//   ① AllowOriginTilt=yes+IsOnOrigin=yes → AutoWeapon 矩阵深度（onTurret/onbody 由
	//      OriginIsOnBody 分；退役"水平朝向+地形倾斜近似"，7a 决议——tilt 由矩阵姿态接管）；
	//   ③ AllowOriginTilt=yes+IsOnOrigin=no → 连线坐标系（CoordinateTilt=yes 取真实 3D 连线）；
	//   ② AllowOriginTilt=no → 水平 2D（facing=fAxisDir，主朝向段已按 IsOnOrigin 实时刷新）；
	//   Self 不再特例排除（7b bug 修复：pTechno 载体 → ①；弹体无锚 → 弹体朝向水平摆）。
	// 算完把完整解算点写回最后有效坐标 _startPoint。
	if (!Data->OriginNoUpdate && !Data->OriginFLH.IsEmpty())
	{
		startPoint = ResolveOriginTilting(startPoint, fAxisDir, currentPos);
		// 死亡维持（基线：参照单位死亡后不再计算，解算点停在死亡前最后一刻的完整值）：
		// 每帧把复合计算完的完整解算点写回最后有效坐标。参照单位活着时下一帧的
		// 坐标刷新（TrackOriginCoord/目标链）会先覆盖它再重算；一旦死亡，刷新链停刷
		// 不再覆盖 → 该值自然停在死亡帧的完整解算点（含 OriginFLH 偏移），后续帧
		// 恒等返回，不回退不重算。与 OriginNoUpdate=yes 的"算一次后不重算"同构，
		// 只是停点由死亡触发。
		_startPoint = startPoint;
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
			double tdx = currentPos.X - startPoint.X;
			double tdy = currentPos.Y - startPoint.Y;
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

	CoordStruct smallCircleCenter = startPoint;

	// （AllowOriginTilt=yes 的 OriginFLH 三维旋转已并入上方"解算倾斜"管线 ResolveTilting，
	//   在 startPoint 级完成——挂载快照定死 / no 每帧刷新，smallCircleCenter 直接消费完整基准点，
	//   此处不再重复叠加。原 smallCircleCenter 级三维分支已删除。）

	if (!Data->CircleOrigin.IsEmpty())
	{
		if (Data->AllowOriginTilt)
		{
			// CircleOrigin 作为 FLH 偏移并入"解算倾斜"管线 ResolveTilting：
			// AllowOriginTilt=yes 时跟随转轴旋转（F=沿 facing，L=垂直 facing，H=Z），
			// facing/倾斜按 IsOnOrigin 规则（F 轴归一，同 OriginFLH 三维）：
			// yes=单位自身朝向（OriginIsOnBody 分炮塔/车身）+ 本帧采样倾斜角；
			// no=单位→弹体连线（CoordinateTilt=yes 取真实 3D 连线，no=水平投影）。
			// 坐标系修正（RotateZ+Y 镜像）在引擎 API 内，禁止裸 cos/sin 手写。
			// 圆心 = 基准点(可能已含 OriginFLH 偏移) + CircleOrigin 旋转偏移。
			TechnoClass* pOriginTechno = FindOriginTechno();
			if (pOriginTechno && !IsDeadOrInvisible(pOriginTechno))
			{
				if (Data->IsOnOrigin)
				{
					smallCircleCenter = ResolveTilting(smallCircleCenter, adjustedCircleOrigin,
						(Data->OriginIsOnBody ? pOriginTechno->PrimaryFacing.Current() : pOriginTechno->TurretFacing().Current()), // 官方API，不得修改
						originTerrainTilt);
				}
				else
				{
					// 连线坐标系（IsOnOrigin=no）：CoordinateTilt=yes 取真实 3D 连线，no=水平
					double lineTilt = 0.0;
					smallCircleCenter = ResolveTilting(smallCircleCenter, adjustedCircleOrigin,
						ResolveLinePose(pOriginTechno, currentPos, Data->CoordinateTilt, lineTilt), lineTilt);
				}
			}
			else
			{
				// Origin 无单位（打格子/死亡）：无转轴可跟随 → 按纯世界坐标加法处理
				smallCircleCenter = startPoint + adjustedCircleOrigin;
			}
		}
		else
		{
			// AllowOriginTilt=no：纯世界坐标加法、无视朝向（与 yes 分支刻意不同）
			smallCircleCenter = startPoint + adjustedCircleOrigin;
		}
	}
	else if (!Data->OriginFLH.IsEmpty())
	{
		// 仅 OriginFLH：CircleOrigin 为空时不走 FLH 转换，手动设 Z
		smallCircleCenter.Z = _vectorAcquireZ + Data->OriginFLH.Z;
	}

		// 圆心移动：Vector.Origin.* 系统
		if (!Data->OriginMoveTo.IsEmpty() || Data->OriginReachTarget || Data->OriginLinearSpeed >= 0 || !Data->OriginTargetFLH.IsEmpty()
			|| Data->OriginCircleRadius >= 0 || Data->OriginCircleSpeed != 0 || Data->OriginCircleAnglePerStep != 0)
		{
			// 解算起始点：默认 startPoint，OriginOrigin 可替换为独立参考系
			CoordStruct bigCircleStartPoint = startPoint;
			if (Data->OriginOrigin != VectorData::VectorOrigin::Self)
			{
				switch (Data->OriginOrigin)
				{
				case VectorData::VectorOrigin::Launcher:
					if (_pLauncher && !IsDeadOrInvisible(_pLauncher))
					{
						if (!Data->OriginOriginNoUpdate)
							_bigCircleStartPoint = _pLauncher->GetCoords(); // 发射者活着：每帧快照（NoUpdate=yes 冻结首帧不更新）
						bigCircleStartPoint = _pLauncher->GetCoords();
					}
					else
						bigCircleStartPoint = _bigCircleStartPoint; // 发射者死亡：冻结快照（首帧或最后跟随值），不再读指针
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
								_bigCircleStartPoint = targetBase; // 目标活着：每帧快照（NoUpdate=yes 冻结首帧不更新）
							bigCircleStartPoint = targetBase;
						}
						else
							bigCircleStartPoint = _bigCircleStartPoint; // 目标失效：冻结快照（首帧或最后跟随值），不再掉回 startPoint
					}
					break;
				case VectorData::VectorOrigin::Source:
					if (_pSource && !IsDeadOrInvisible(_pSource))
					{
						if (!Data->OriginOriginNoUpdate)
							_bigCircleStartPoint = _pSource->GetCoords(); // 来源活着：每帧快照（NoUpdate=yes 冻结首帧不更新）
						bigCircleStartPoint = _pSource->GetCoords();
					}
					else
						bigCircleStartPoint = _bigCircleStartPoint; // 来源死亡：冻结快照（首帧或最后跟随值），不再读指针
					break;
				}
			}

			// OriginOriginFLH 挂点偏移（对齐小圆参照FLH——原实现只在 OriginOrigin=Self 时生效且纯加法，
			// 属历史残缺；现对任意 OriginOrigin 生效）：
			//   Origin.AllowOriginTilt=yes 且 OriginOrigin 有存活单位 → 按单位姿态 3D 摆放
			//     （facing=单位自身朝向 TurretFacing，tilt=单位倾斜采样）；
			//   no / 单位死 / 无单位（打格子）→ 纯世界坐标加法（无姿态可跟随）
			if (!Data->OriginOriginFLH.IsEmpty())
			{
				TechnoClass* pOO = nullptr; // OriginOrigin 对应单位（Self → 小圆参照 单位）
				if (Data->OriginOrigin == VectorData::VectorOrigin::Launcher)
					pOO = abstract_cast<TechnoClass*>(_pLauncher);
				else if (Data->OriginOrigin == VectorData::VectorOrigin::Source)
					pOO = abstract_cast<TechnoClass*>(_pSource);
				else if (Data->OriginOrigin == VectorData::VectorOrigin::Target)
				{
					AbstractClass* pTgt = pBullet ? pBullet->Target : (pTechno ? pTechno->Target : nullptr);
					pOO = abstract_cast<TechnoClass*>(pTgt);
				}
				else
					pOO = FindOriginTechno(); // OriginOrigin=Self：解算起始点跟小圆，姿态跟小圆参照 单位
				if (Data->OriginAllowOriginTilt && pOO && !IsDeadOrInvisible(pOO))
				{
					bigCircleStartPoint = ResolveTilting(bigCircleStartPoint, Data->OriginOriginFLH,
						pOO->TurretFacing().Current(), SampleOriginTilt(pOO)); // 官方API，不得修改
				}
				else
				{
					bigCircleStartPoint.X += Data->OriginOriginFLH.X;
					bigCircleStartPoint.Y += Data->OriginOriginFLH.Y;
					bigCircleStartPoint.Z += Data->OriginOriginFLH.Z;
				}
			}

			// Origin.CircleOffset 世界偏移
			if (!Data->OriginCircleOffset.IsEmpty())
				bigCircleStartPoint = bigCircleStartPoint + Data->OriginCircleOffset;

			// OriginNoUpdate：首帧快照解算起始点，后续帧冻结
			if (_elapsedFrames == 0)
				_bigCircleStartPoint = bigCircleStartPoint;
			else if (Data->OriginOriginNoUpdate)
				bigCircleStartPoint = _bigCircleStartPoint;

			if (_elapsedFrames == 0)
			{
				// 初始偏移 = 0：大圆圆心直接用大圆解算起始点（bigCircleStartPoint，已含 Origin.CircleOrigin 偏移），
				// 不绑定小圆圆心（smallCircleCenter）。小圆围绕大圆转，小圆圆心坐标对大圆无意义。
				_bigCircleOffset = {};
				// Circle 初始化
				_originMotion.circleRadius = Data->OriginCircleRadius;
				_originMotion.circleSpeed = Data->OriginCircleSpeed;
				_originMotion.angle = 0.0; // 初始相位
				// 未显式设半径：取当前偏移的水平距离
				if (_originMotion.circleRadius < 0)
					_originMotion.circleRadius = (int)std::sqrt(
						(double)_bigCircleOffset.X * _bigCircleOffset.X +
						(double)_bigCircleOffset.Y * _bigCircleOffset.Y +
						(double)_bigCircleOffset.Z * _bigCircleOffset.Z);
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
				}
				// 有 OriginNormalVector 时：facing/tilt 均取它的 F/L/H 分量（彻底世界固定）。
				// IsNormalOnOrigin=yes 时的 OriginOrigin 朝向跟随在下方每帧段处理。
				// 锁定基础法向量球坐标（OriginIsNormalOnOrigin 每帧旋转的基准）
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
		// OriginIsNormalOnOrigin：大圆法向量随 OriginOrigin 单位转动（facing + tilt 全跟随，同小圆）。
		// 基础 = 首帧锁定的 _baseOriginFacing/_baseOriginTilt（OriginNormalVector/随机/默认水平），
		// 每帧按 OriginOrigin 单位朝向（facingU）+ 单位倾斜（originTerrainTilt）转动（Rodrigues）。
		// 更新 _originMotion.normalX/Y/Z（Circle 运动消费点在段外从法向量现算球坐标）。
		// 不回写成员 _originFacing/_originTilt（保持首帧锁定值，同小圆 effectiveFacing/effectiveTilt 模式）。
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
					// 目标无朝向（格子/地面）：保持 facingU 初值（世界固定），不回退连线（同小圆 IsNormalOnOrigin 处理）
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

			// 基础法向量（首帧锁定）随单位姿态旋转（共享管线 RotateNormalByUnit，无状态）；
			// 大圆法向量状态独立于小圆（各自喂输入、各存 _originMotion），两套互不耦合
			RotateNormalByUnit(_baseOriginFacing, _baseOriginTilt, facingU, tiltU,
				_originMotion.normalX, _originMotion.normalY, _originMotion.normalZ);
			// 不回写 _originFacing/_originTilt（同小圆 IsNormalOnOrigin：基础法向量球坐标永远保持首帧锁定值，
			// 段外消费点从 _originMotion.normalX/Y/Z 现算，杜绝法向量每帧自反馈累计旋转）
		}
		// 每帧累加 Lissajous + 3D 法向量增量旋转
		_originMotion.normalRotF += _originMotion.lissajousStep;
		if (_originMotion.normalStepF != 0.0 || _originMotion.normalStepL != 0.0 || _originMotion.normalStepH != 0.0)
		{
			RotateNormal3D(_originMotion.normalX, _originMotion.normalY, _originMotion.normalZ,
				_originMotion.normalStepF, _originMotion.normalStepL, _originMotion.normalStepH);
		}

			// 从法向量现算球坐标（同小圆 effectiveFacing/effectiveTilt 消费模式）：
			// 段内 OriginIsNormalOnOrigin 每帧旋转结果已在 _originMotion.normalX/Y/Z，
			// 这里现算 oFacing/oFacingTilt 供 Circle 运动消费；不回读成员 _originFacing/_originTilt（保持首帧锁定值）。
			double oFacing = 0.0, oTilt = 0.0;
			{
				double lenXY = std::sqrt(_originMotion.normalX * _originMotion.normalX + _originMotion.normalY * _originMotion.normalY);
				oFacing = lenXY > 1e-6 ? std::atan2(_originMotion.normalY, _originMotion.normalX) : 0.0;
				oTilt = lenXY > 1e-6 ? std::atan2(_originMotion.normalZ, lenXY) : (_originMotion.normalZ > 0 ? M_PI / 2.0 : -M_PI / 2.0);
			}
			// 大圆面倾斜唯一来源 = 大圆法向量（OriginNormalVector + OriginIsNormalOnOrigin 随动）：
			// OriginAllowOriginTilt 不再叠加单位倾斜进 oTilt（它只管大圆解算起始点，见 VectorData.h 注释）。
			// 3D 法向量旋转覆盖
			if (_originMotion.normalStepF != 0.0 || _originMotion.normalStepL != 0.0 || _originMotion.normalStepH != 0.0)
			{
				double lenXY = std::sqrt(_originMotion.normalX * _originMotion.normalX + _originMotion.normalY * _originMotion.normalY);
				oFacing = lenXY > 1e-6 ? std::atan2(_originMotion.normalY, _originMotion.normalX) : 0.0;
				oTilt = lenXY > 1e-6 ? std::atan2(_originMotion.normalZ, lenXY) : (_originMotion.normalZ > 0 ? M_PI / 2.0 : -M_PI / 2.0);
			}
			DirStruct oFacingDir = Radians2Dir(oFacing); // 官方API，不得修改：弧度→DirStruct

			// 当前圆心绝对位置 = 解算起始点 + 位移
			CoordStruct bigCircleCenter = bigCircleStartPoint + _bigCircleOffset;

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
					_originMotion.arcStartCenter = bigCircleCenter;
				}

				// OriginOrigin 的 F 轴基准（摆放 OriginTargetFLH 用；与法向量彻底解耦）：
				// 单位存活 → 单位自身炮塔朝向；打格子/地面/目标死亡 → 解算起始点指向抛射体的水平连线；
				// OriginOrigin=Self（解算起始点跟小圆）→ 跟小圆 F 轴基准（fAxisDir）。
				DirStruct originOriginFacing = fAxisDir;
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
						originOriginFacing = Point2Dir(bigCircleStartPoint, pObject->GetCoords()); // 官方API，不得修改：解算起始点→弹体水平连线
				}

				CoordStruct bigCircleTarget = GetFLHAbsoluteCoords(bigCircleStartPoint, Data->OriginTargetFLH + _originTargetOffset, originOriginFacing); // 官方API，不得修改
				if (Data->OriginReachTarget)
				{
					int effectiveSteps = (AE->AEData.GetDuration() - Data->DisabledFrames) / _effectiveTimeStep;
					if (effectiveSteps < 1) effectiveSteps = 1;
					int rem = effectiveSteps - _movementFrames;
					if (rem <= 0)
					{
						disp = bigCircleTarget - bigCircleCenter;
						_bigCircleOffset += disp;
						smallCircleCenter = bigCircleStartPoint + _bigCircleOffset;
						_prevBigCircleCenter = smallCircleCenter;
						Deactivate();
						goto skipOriginUpdate;
					}
					disp.X = (bigCircleTarget.X - bigCircleCenter.X) / rem;
					disp.Y = (bigCircleTarget.Y - bigCircleCenter.Y) / rem;
					disp.Z = (bigCircleTarget.Z - bigCircleCenter.Z) / rem;
					if (_originMotion.arcHeight != 0)
					{
						double t = static_cast<double>(_movementFrames) / effectiveSteps;
						double arcOffset = CalcArcOffsetAt(static_cast<int>(_originMotion.arcHeight), _originMotion.arcPeakPercent, t);
						double baseX = _originMotion.arcStartCenter.X + (bigCircleTarget.X - _originMotion.arcStartCenter.X) * t;
						double baseY = _originMotion.arcStartCenter.Y + (bigCircleTarget.Y - _originMotion.arcStartCenter.Y) * t;
						double baseZ = _originMotion.arcStartCenter.Z + (bigCircleTarget.Z - _originMotion.arcStartCenter.Z) * t;
						if (_originMotion.arcRotation == 0.0)
						{
							disp.Z = static_cast<int>(baseZ + arcOffset) - bigCircleCenter.Z;
						}
						else
						{
							CoordStruct arcD{
								bigCircleTarget.X - _originMotion.arcStartCenter.X,
								bigCircleTarget.Y - _originMotion.arcStartCenter.Y,
								bigCircleTarget.Z - _originMotion.arcStartCenter.Z };
							ArcDelta3D ad = RotateArcDelta(arcD, _originMotion.arcRotation, arcOffset);
							disp.X = static_cast<int>(baseX + ad.x) - bigCircleCenter.X;
							disp.Y = static_cast<int>(baseY + ad.y) - bigCircleCenter.Y;
							disp.Z = static_cast<int>(baseZ + ad.z) - bigCircleCenter.Z;
						}
					}
				}
				else
				{
					_originMotion.speed += Data->OriginAcceleration;
					if (Data->OriginMaxSpeed >= 0 && _originMotion.speed > Data->OriginMaxSpeed) _originMotion.speed = Data->OriginMaxSpeed;
					if (Data->OriginMinSpeed >= 0 && _originMotion.speed < Data->OriginMinSpeed) _originMotion.speed = Data->OriginMinSpeed;
					int dx = bigCircleTarget.X - bigCircleCenter.X, dy = bigCircleTarget.Y - bigCircleCenter.Y, dz = bigCircleTarget.Z - bigCircleCenter.Z;
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
							bigCircleTarget.X - _originMotion.arcStartCenter.X,
							bigCircleTarget.Y - _originMotion.arcStartCenter.Y,
							bigCircleTarget.Z - _originMotion.arcStartCenter.Z };
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
				// 当前圆心相对解算起始点的偏移在 LH 平面投影
				double dx = (double)_bigCircleOffset.X, dy = (double)_bigCircleOffset.Y, dz = (double)_bigCircleOffset.Z;
				double cf = std::cos(oFacing), sf = std::sin(oFacing), ct = std::cos(oTilt), st = std::sin(oTilt);
				double dL = dx*(-sf) + dy*cf;
				double dH = dx*(-cf*st) + dy*(-sf*st) + dz*ct;
				double cd = std::sqrt(dL*dL + dH*dH);
				// 圆心在解算起始点上（偏移≈0），初始化到半径位置
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
				disp.X = newOffset.X - _bigCircleOffset.X;
				disp.Y = newOffset.Y - _bigCircleOffset.Y;
				disp.Z = newOffset.Z - _bigCircleOffset.Z;
			}
	skipOriginUpdate:
		_bigCircleOffset += disp;
		smallCircleCenter = bigCircleStartPoint + _bigCircleOffset;
		_originMotion.elapsed++;
	}

	// 圆心位移叠加：Circle 模式追踪圆心→调整 currentPos
	CoordStruct centerDelta{ 0, 0, 0 };  // 初始化避免 C4701 警告
	bool useCenterTracking = false;
	if (_prevBigCircleCenter.X || _prevBigCircleCenter.Y || _prevBigCircleCenter.Z)
	{
		centerDelta = smallCircleCenter - _prevBigCircleCenter;
		if (Data->OriginLissajous <= 0.0 && (Data->OriginCircleRadius >= 0 || Data->OriginCircleSpeed != 0 || Data->OriginLinearSpeed >= 0 || Data->OriginCircleAnglePerStep != 0.0))
			useCenterTracking = true;
	}
	_prevBigCircleCenter = smallCircleCenter;

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
	double dx = static_cast<double>(trackPos.X - smallCircleCenter.X);
	double dy = static_cast<double>(trackPos.Y - smallCircleCenter.Y);
	double dz = static_cast<double>(trackPos.Z - smallCircleCenter.Z);
		double currentDist;
		// 圆面取点：法向量域（hasNormal 显式法向量，或 IsNormalOnOrigin 随动产生的 effectiveTilt）→ 倾斜面；
		// 二者皆无（无法向量且 IsNormalOnOrigin=no，世界固定水平圆面）→ 传统 2D 平面。
		// 原 AllowCircleTilt 接受闸门已删——圆面倾斜唯一来源 = 法向量体系。
		bool useTiltPlane = hasNormal || effectiveTilt != 0.0;
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
			result.MoveDisp.X = smallCircleCenter.X + static_cast<int>(rL * (-sinF) + rH * (-cosF * sinT)) - _circlePos.X;
			result.MoveDisp.Y = smallCircleCenter.Y + static_cast<int>(rL * cosF + rH * (-sinF * sinT)) - _circlePos.Y;
			result.MoveDisp.Z = smallCircleCenter.Z + static_cast<int>(rH * cosT) - _circlePos.Z;
		}
		else
		{
			// 传统 2D 圆面（XY 平面）
			double ndx = (dx / currentDist * targetRadius);
			double ndy = (dy / currentDist * targetRadius);
			double rx = ndx * cosA - ndy * sinA;
			double ry = ndx * sinA + ndy * cosA;
			result.MoveDisp.X = smallCircleCenter.X + static_cast<int>(rx) - _circlePos.X;
			result.MoveDisp.Y = smallCircleCenter.Y + static_cast<int>(ry) - _circlePos.Y;
			result.MoveDisp.Z = Data->CircleOrigin.IsEmpty() && Data->OriginFLH.IsEmpty()
				? 0 : smallCircleCenter.Z - _circlePos.Z;  // 有显式高度指定时拉 Z，否则维持抛射体自身高度
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
		DirStruct moveDir = fAxisDir;
		double useCosT = 1.0, useSinT = 0.0;
		// 非 Target 的 MoveTo F 轴恒水平（不再吃圆面 effectiveTilt——MoveTo 直线移动与圆面/法向量域无关）；
		// 仅 Target + CoordinateTilt（连线 F 轴取真实 3D）时才带倾斜。
		bool hasTilt = false;

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
					// CoordinateTilt=yes：连线 F 轴取真实 3D（含高低差 Z 分量），弹体沿斜线飞向目标；
					// no（默认）= 仅水平连线，Z 不追（弹体水平飞，到不到目标由配置自己负责）
					if (Data->CoordinateTilt)
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
			// hasTilt 仅由 Target+CoordinateTilt 触发，tilt 即连线高低角（useCosT/useSinT）
			double cosT = useCosT;
			double sinT = useSinT;
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
	// TargetFLH 与 TargetOffset 的结算起点 = Origin（单位自身坐标系=Origin 单位/挂点，连线坐标系=
	// Origin 本体中心）：什么都不写（TargetFLH=0 且无偏移）时目标点即 Origin 单位中心，弹体直冲中心。
	CoordStruct smallCircleTargetFlh;
	// TargetOffsetNormal 世界固定：偏移已由 LockFacing 转成世界坐标，叠加在旋转后的 TargetFLH 上（不随 F 轴转）
	bool targetOffsetWorld = !Data->IsNormalOnOrigin && !Data->TargetOffsetNormal.IsEmpty();
	smallCircleTargetFlh.X = Data->TargetFLH.X + (targetOffsetWorld ? 0 : _randomTargetOffset.X);
	smallCircleTargetFlh.Y = Data->TargetFLH.Y + (targetOffsetWorld ? 0 : _randomTargetOffset.Y);
	smallCircleTargetFlh.Z = Data->TargetFLH.Z + (targetOffsetWorld ? 0 : _randomTargetOffset.Z);

	// TargetFLH → 世界坐标：AutoWeapon 同款管线
	// 坐标系统一：矩阵偏移（含 IsOnTurret 炮塔/车身）+ NoUpdate 控制的计算点 startPoint
	CoordStruct smallCircleTarget;
	// NoUpdate=yes：目标点锁定。首帧正常计算一次缓存，后续每帧直接复用 _lockedSmallCircleTarget，
	// 不再执行"读发射者实时坐标/朝向 → 算新目标点"的每帧刷新（Origin=Launcher 时
	// mtxPos = 发射者实时坐标+实时朝向旋转 FLH，NoUpdate 若不隔离这里，目标点每帧被重写）
	if (Data->OriginNoUpdate && !_lockedSmallCircleTarget.IsEmpty())
	{
		smallCircleTarget = _lockedSmallCircleTarget;
	}
	else
	{
	// 单位自身坐标系（TargetFLH 挂单位）——统一找"坐标系所属单位"：
	//   IsOnOrigin=yes 且对应对象是活单位才挂（Target 打格子/单位死 → 无锚回退 2D）。
	//   连线坐标系（IsOnOrigin=no）不锚定，走下方 C→P 连线分支（含 Launcher——管线第 3 步补齐）。
	TechnoClass* pAnchorUnit = nullptr;
	switch (Data->Origin)
	{
	case VectorData::VectorOrigin::Launcher:
		if (Data->IsOnOrigin)
			pAnchorUnit = abstract_cast<TechnoClass*>(_pLauncher);
		break;
	case VectorData::VectorOrigin::Self:
		if (!Data->OriginIsOnWorld && !Data->TargetIsOnWorld)
			pAnchorUnit = pTechno; // 单位本体（弹体侧无单位，回退 2D）
		break;
	case VectorData::VectorOrigin::Target:
		if (Data->IsOnOrigin)
		{
			AbstractClass* pTgt = pBullet ? pBullet->Target : (pTechno ? pTechno->Target : nullptr);
			pAnchorUnit = abstract_cast<TechnoClass*>(pTgt);
		}
		break;
	case VectorData::VectorOrigin::Source:
		if (Data->IsOnOrigin)
			pAnchorUnit = abstract_cast<TechnoClass*>(_pSource);
		break;
	}
	if (pAnchorUnit && !IsDeadOrInvisible(pAnchorUnit))
	{
		// 单位自身坐标系挂点（统一入口 ResolveTiltingFrame，模式①/②）：
		//   TargetSameTilt=yes（默认）= ① AutoWeapon 完整姿态（Locomotor 矩阵 + TurretOffset +
		//     炮塔旋转角），含车体倾斜——成熟算法保默认；
		//   TargetSameTilt=no = ② 抛弃倾斜（水平基准）：FLH 只按单位水平朝向旋转，
		//     不随单位坡面俯仰（转轴 TurretOffset 不参与，近似水平挂点）。
		//   TargetIsOnTurret=yes（默认）= 对齐炮塔（onTurret：矩阵落转轴+叠炮塔差角），
		//   no = 对齐车身（onbody：车体矩阵+车身差角）。
		//   基准点 startPoint（NoUpdate 控制的计算点）取代单位位置，剥掉单位位移只留姿态偏移。
		smallCircleTarget = ResolveTiltingFrame(startPoint, smallCircleTargetFlh, FlhFrame::UnitOwn, pAnchorUnit,
			Data->TargetIsOnTurret, Data->TargetSameTilt, nullptr, false, fAxisDir, currentPos);
	}
	else
	{
		// 无单位可锚：
		// 1. 世界模式（TargetIsOnWorld 通用，或 Self+OriginIsOnWorld 兼容老配置）→ ④ 纯世界轴无视朝向
		// 2. 连线坐标系（Origin≠Self 且 IsOnOrigin=no，含 Launcher/Source/Target）→ ③
		//    F 轴 = Origin 本体中心（_startPoint，主 Origin 段维护：no=每帧跟实时，yes=冻结）→ 抛射体；
		//    CoordinateTilt=yes 取真实 3D 连线（高低差进 tilt），no=水平投影。抛射体从自身 FireFLH 出发，
		//    不瞬移——连线的 C→P 方向只决定 TargetFLH 往哪摆（与解算起始点连线共用 ResolveTiltingFrame 实现）
		// 3. 其余（单位死/弹体侧 Self）→ fAxisDir 水平旋转（② 兜底）
		if (Data->TargetIsOnWorld || (Data->Origin == VectorData::VectorOrigin::Self && Data->OriginIsOnWorld))
		{
			smallCircleTarget = ResolveTiltingFrame(startPoint, smallCircleTargetFlh, FlhFrame::World, nullptr, false, false,
				nullptr, false, fAxisDir, currentPos);
		}
		else if (Data->Origin != VectorData::VectorOrigin::Self && !Data->IsOnOrigin)
		{
			smallCircleTarget = ResolveTiltingFrame(startPoint, smallCircleTargetFlh, FlhFrame::LineC2P, nullptr, false, false,
				&_startPoint, Data->CoordinateTilt, fAxisDir, currentPos);
		}
		else
		{
			smallCircleTarget = ResolveTiltingFrame(startPoint, smallCircleTargetFlh, FlhFrame::Fallback2D, nullptr, false, false,
				nullptr, false, fAxisDir, currentPos);
		}
	}
	} // 关闭 NoUpdate 缓存的 else 块

	// TargetOffsetNormal 世界固定：偏移（世界坐标）叠加在旋转后的 TargetFLH 上，不随 F 轴转
	if (targetOffsetWorld)
	{
		smallCircleTarget.X += _randomTargetOffset.X;
		smallCircleTarget.Y += _randomTargetOffset.Y;
		smallCircleTarget.Z += _randomTargetOffset.Z;
	}

	// NoUpdate=yes：首帧算完缓存锁定，后续帧走缓存，不再每帧重算
	if (Data->OriginNoUpdate)
	{
		_lockedSmallCircleTarget = smallCircleTarget;
	}

	CoordStruct dirVec;
	dirVec.X = smallCircleTarget.X - currentPos.X;
	dirVec.Y = smallCircleTarget.Y - currentPos.Y;
	dirVec.Z = smallCircleTarget.Z - currentPos.Z;
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


		// 到位帧（dirLen≈0）清零 Velocity（帧首即清，覆盖 EarlyEnd 等提前结束路径）：
		// Vector 期间引擎仍对 Velocity 转向+加速（位置被帧尾 SetLocation 覆盖所以看不出），
		// 若不清，Vector 结束（AE 移除）后引擎按累积的残留速度接管，会把停在目标点的弹体
		// 推走（ReachTarget 结束偏移的根因）。清零后引擎从静止接管，目标检测自然引爆于原位。
		// 飞行中（dirLen 大）不触发——引擎还需速度续飞（EarlyEnd 提前结束由引擎飞完最后段）。
		if (pBullet && dirLen <= 1e-6)
		{
			pBullet->Velocity.X = 0;
			pBullet->Velocity.Y = 0;
			pBullet->Velocity.Z = 0;
		}
		if (Data->ReachTargetEarlyEnd > 0 && Data->ReachTargetEarlyEnd < effectiveDuration
			&& remainingFrames <= Data->ReachTargetEarlyEnd)
		{
			Deactivate();
			AdvanceFrame();
			return result;
		}

		// 最后运动帧（rem<=1）：均分位移有 int 截断残余（最后一帧常差 0~1.5 lepton 到位），
		// 且帧首清 vel 只覆盖"已精确到位"的弹体——差最后一点的弹体位移补到位后 Velocity 仍残留
		// （Vector 期间引擎累积的转向+加速），结束接管会把弹体推离目标（爆点偏移十几个 lepton）。
		// 直接强制 SetLocation 精确 snap 到位 + 清 Velocity，引擎静止接管自然引爆于原位。
		if (remainingFrames <= 1 && pBullet)
		{
			pBullet->SetLocation(smallCircleTarget); // 官方API，不得修改：精确到位（同 SpeedEndOnReach 先例）
			pBullet->SourceCoords = smallCircleTarget;
			pBullet->Velocity.X = 0;
			pBullet->Velocity.Y = 0;
			pBullet->Velocity.Z = 0;
			result.MoveDisp = { 0, 0, 0 };
			result.Force = false;
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
				// 影子沿 smallCircleTarget 方向推进 adjustedSpeed（与直线均分同步）
				double ux = dirVec.X / dirLen, uy = dirVec.Y / dirLen, uz = dirVec.Z / dirLen;
				_motion.shadowX += ux * adjustedSpeed;
				_motion.shadowY += uy * adjustedSpeed;
				_motion.shadowZ += uz * adjustedSpeed;
				_motion.shadowTraveled += adjustedSpeed;

				// 剩余影子距离（3D）
				double sdx = smallCircleTarget.X - _motion.shadowX;
				double sdy = smallCircleTarget.Y - _motion.shadowY;
				double sdz = smallCircleTarget.Z - _motion.shadowZ;
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
					smallCircleTarget.X - _firstFramePos.X,
					smallCircleTarget.Y - _firstFramePos.Y,
					smallCircleTarget.Z - _firstFramePos.Z };
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
	// 影子沿 _shadowPos→smallCircleTarget 方向推进，不受弧高Z偏移污染
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
		double sdx = smallCircleTarget.X - _motion.shadowX;
		double sdy = smallCircleTarget.Y - _motion.shadowY;
		double sdz, shadowDist;
		if (_motion.arcHeight != 0)
		{
			sdz = smallCircleTarget.Z - _motion.shadowZ;
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
			double realDX = smallCircleTarget.X - currentPos.X;
			double realDY = smallCircleTarget.Y - currentPos.Y;
			double realDZ = smallCircleTarget.Z - currentPos.Z;
			double realDist = std::sqrt(realDX * realDX + realDY * realDY + realDZ * realDZ);
			// A：影子理论到位（shadowDist<=speed）也判到达——弧线时影子沿直线先到目标点，
			// 弹体实际走弧线被甩在后面（realDist 仍 > speed），只判 realDist 永不满足 →
			// 影子停推（disp=0）弹体僵直空中（日志实证：mf=38 起 disp=(0,0,0)、cur 恒定点、
			// realDist≈43 > speed=30）。影子到位 = 理论飞行完成，立即 snap。
			// B：判据内执行体 = SetLocation(smallCircleTarget) 精确到位 + 清残留 Velocity
			// （同 ReachTarget snap：引擎从静止接管，不再按累积速度把弹体推离爆点）
			if (realDist <= speed || shadowDist <= speed)
			{
				// 强制挪移：到达帧直接把对象坐标设为目标格子坐标（完全重合），消除到位抖动
				if (pBullet)
				{
					pBullet->SetLocation(smallCircleTarget);
					// 清残留 Velocity（同 ReachTarget snap 处理）：Vector 期间引擎仍累积转向+加速，
					// Deactivate 后引擎接管会按残留速度把停在目标点的弹体推走（爆点偏移）
					pBullet->Velocity.X = 0;
					pBullet->Velocity.Y = 0;
					pBullet->Velocity.Z = 0;
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
					pTechno->SetLocation(smallCircleTarget);
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
			// 无弧线：_shadowPosZ 不变（始终 = _firstFramePos.Z），Z 由 t 插值
			_motion.shadowTraveled += step;

			// 重新计算影子距离（影子已移动）
			sdx = smallCircleTarget.X - _motion.shadowX;
			sdy = smallCircleTarget.Y - _motion.shadowY;
			if (_motion.arcHeight != 0)
			{
				sdz = smallCircleTarget.Z - _motion.shadowZ;
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
				// _shadowPosZ 在无弧线时冻结为抛射体起始 Z，_firstFramePos.Z 可能是目标 Z（Origin=Target 时不同）
				double targetZ = _motion.shadowZ + (smallCircleTarget.Z - _motion.shadowZ) * t;
				resultDisp.Z = static_cast<int>(targetZ - currentPos.Z);
			}

			if (_motion.arcHeight != 0)
			{
				double arcOffset = CalcArcOffsetAt(t);
				double arcDelta = arcOffset - _motion.prevArcOffset;
				_motion.prevArcOffset = arcOffset;

				CoordStruct arcD{
					smallCircleTarget.X - _firstFramePos.X,
					smallCircleTarget.Y - _firstFramePos.Y,
					smallCircleTarget.Z - _firstFramePos.Z };
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