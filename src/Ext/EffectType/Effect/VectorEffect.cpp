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
// 连线坐标系（OriginIsOnVectorOrigin=no，Target/Launcher/Source 通吃）——解算起始点管线用：
// F 轴 = 单位→弹体的连线方向（水平投影角，RA2 坐标系由 Point2Dir 处理）。
// CoordinateTilt 决定这条线取真实 3D（高低差进 tilt，ResolveTilting 混合出斜向摆放）
// 还是水平投影（tilt=0，与地面平行）。
// ============================================================================
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
	_lastPoint = {};
	_bigCircleStartPoint = {};
	_lockedSmallCircleTarget = {};
	_vectorAcquireZ = 0;
	_pLauncher = nullptr;
	_pSource = nullptr;

	_fAxisRad = 0.0;
	_fAxisDir = DirStruct(0);

	_randomTargetOffset = {};
	_originTargetOffset = {};

	_bigCircleOffset = {};
	_prevBigCircleCenter = {};
	_circlePos = {};

	_motion = MotionState{};
	_originMotion = MotionState{};

	_circleDynamicSampled = false;
	_originDynamicSampled = false;
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

// 弧影子推进（主直线 Speed / ReachTarget / 大圆 OriginSpeed 三处共用，2026-09-05 归一化）：
// 影子 = 不受弧抬升污染的直线进度基准：沿"影子自己→target"方向推进 step（clamp 不越过），
// 同步 shadowTraveled / prevArcOffset。弹体/圆心位移 = 影子本帧位移 + 弧增量（弧面旋转由调用方做）。
// 统一保证"直线位移走影子（纯直线）、弧只叠影子之上"——直线不会被弹体被弧抬高的实时位置带偏。
// 参数：m = 影子状态（shadowX/Y/Z、shadowTraveled、prevArcOffset、arcHeight/arcPeakPercent）；
//       target = 目标点；step = 本帧影子步长（Speed=速度；ReachTarget=剩余影子距离/剩余帧）。
// 返回：推进后剩余影子距离；影子本帧位移三分量（double）；进度 t；本帧弧高增量 arcDelta。
double VectorEffect::AdvanceArcShadow(MotionState& m, const CoordStruct& target, double step,
	double& dispX, double& dispY, double& dispZ, double& outT, double& arcDelta)
{
	dispX = dispY = dispZ = 0.0;
	outT = 0.0;
	arcDelta = 0.0;

	// 影子方向 = 影子自己→目标（不读弹体/圆心实时位置——那是弧污染的来源）
	double sdx = target.X - m.shadowX;
	double sdy = target.Y - m.shadowY;
	double sdz = target.Z - m.shadowZ;
	double shadowDist = std::sqrt(sdx * sdx + sdy * sdy + sdz * sdz);
	if (shadowDist > 1e-6)
	{
		double s = (step < shadowDist) ? step : shadowDist; // clamp 不越过
		dispX = sdx / shadowDist * s;
		dispY = sdy / shadowDist * s;
		dispZ = sdz / shadowDist * s;
		m.shadowX += dispX;
		m.shadowY += dispY;
		m.shadowZ += dispZ;
		m.shadowTraveled += s;
		// 推进后重算剩余影子距离（t 的分母用最新值，目标移动实时反映）
		sdx = target.X - m.shadowX;
		sdy = target.Y - m.shadowY;
		sdz = target.Z - m.shadowZ;
		shadowDist = std::sqrt(sdx * sdx + sdy * sdy + sdz * sdz);
	}

	// t = 已走路程 / 总路程（动态更新，目标移动时自动调整）
	double total = m.shadowTraveled + shadowDist;
	outT = (total > 1e-6) ? m.shadowTraveled / total : 0.0;
	if (outT < 0.0) outT = 0.0; else if (outT > 1.0) outT = 1.0;

	// 弧高增量（相对上一帧，不覆盖直线位移）
	double arcOffset = CalcArcOffsetAt(static_cast<int>(m.arcHeight), m.arcPeakPercent, outT);
	arcDelta = arcOffset - m.prevArcOffset;
	m.prevArcOffset = arcOffset;
	return shadowDist;
}

// 法向量 FLH 分量增量旋转（2026-09-05 归一化：分量空间，轴映射相对旧"世界槽位"版已置换）：
// 输入 (nx,ny,nz) = 状态分量 (F,L,H)；每帧只转固定 step（度/帧），不做任何累计角。
//   stepF 绕 F 分量轴：保持 F（nx），L/H（ny/nz）在 yz 平面 2D 转
//   stepL 绕 L 分量轴：保持 L（ny），F/H（nx/nz）在 xz 平面 2D 转
//   stepH 绕 H 分量轴：保持 H（nz），F/L（nx/ny）在 xy 平面 2D 转
// 案例校验：法向量 (1,0,0)，stepL 转 90° → (0,0,1)（文档第三节 Lstep 语义 ✓）；
// Fstep 时 (1,0,0) 躺 F 轴 → 不动（几何正确非 bug ✓）。
void VectorEffect::RotateNormal3D(double& nx, double& ny, double& nz,
	double stepF, double stepL, double stepH)
{
	if (stepF != 0.0)
	{
		// 绕 F（x 轴）：yz 平面
		double rad = Math::deg2rad(stepF), c = std::cos(rad), s = std::sin(rad);
		double y = ny, z = nz;
		ny = y * c - z * s;
		nz = y * s + z * c;
	}
	if (stepL != 0.0)
	{
		// 绕 L（y 轴）：xz 平面。符号按文档第三节案例：Lstep 90° → (1,0,0) 到 (0,0,1)
		double rad = Math::deg2rad(stepL), c = std::cos(rad), s = std::sin(rad);
		double x = nx, z = nz;
		nx = x * c - z * s;
		nz = x * s + z * c;
	}
	if (stepH != 0.0)
	{
		// 绕 H（z 轴）：xy 平面
		double rad = Math::deg2rad(stepH), c = std::cos(rad), s = std::sin(rad);
		double x = nx, y = ny;
		nx = x * c - y * s;
		ny = x * s + y * c;
	}
}

// 世界三轴直摆（法向量消费的固定坐标系，抄 Stand IsOnWorld：空 Dir 世界 FLH 轴直摆，
// 官方 API 消化 90° 偏置/Y 镜像）。返回值 = ×1000 的世界轴方向（int）。
static void FillWorldAxes(CoordStruct& axisF, CoordStruct& axisL, CoordStruct& axisH)
{
	axisF = GetFLHAbsoluteCoords(CoordStruct::Empty, CoordStruct{ 1000, 0, 0 }, DirStruct{}); // 官方API，不得修改
	axisL = GetFLHAbsoluteCoords(CoordStruct::Empty, CoordStruct{ 0, 1000, 0 }, DirStruct{});
	axisH = GetFLHAbsoluteCoords(CoordStruct::Empty, CoordStruct{ 0, 0, 1000 }, DirStruct{});
}

// 消费坐标三轴（法向量系统一）：锚单位姿态矩阵 / 载体弹体水平姿态的 F/L/H 轴世界方向（×1000 int）。
// 数学依据：GetFLHAbsoluteCoords 是线性变换 mtx·flh（单位矩阵链），单次消费 mtx·(1000·n) ≡
// Σ nᵢ·mtx·(1000·eᵢ)（三轴合成），因此缓存三轴后可随时对任意状态分量做线性合成消费——
// 锚单位死亡 = 坐标系轴冻结，但法向量自旋（系统二）在冻结坐标系中继续。
// 返回 false = 无可用锚（调用方用世界轴固化）。
bool VectorEffect::GetNormalFrameAxes(ObjectClass* pAnchor, bool onTurret,
	CoordStruct& axisF, CoordStruct& axisL, CoordStruct& axisH)
{
	if (!pAnchor)
		return false;
	TechnoClass* pAnchorT = abstract_cast<TechnoClass*>(pAnchor);
	if (pAnchorT)
	{
		if (IsDeadOrInvisible(pAnchorT))
			return false;
		PoseParams pose;
		pose.useUnitPose = true;
		pose.anchor = pAnchorT;
		pose.onTurret = onTurret;
		axisF = ResolveTilting(CoordStruct::Empty, CoordStruct{ 1000, 0, 0 }, pose); // = mtx点 − 锚坐标
		axisL = ResolveTilting(CoordStruct::Empty, CoordStruct{ 0, 1000, 0 }, pose);
		axisH = ResolveTilting(CoordStruct::Empty, CoordStruct{ 0, 0, 1000 }, pose);
		return true;
	}
	BulletClass* pBulletAnchor = abstract_cast<BulletClass*>(pAnchor);
	if (pBulletAnchor)
	{
		// 弹体水平姿态（无坡面/炮塔概念）；flipY 默认 1=不镜像
		CoordStruct c0 = pBulletAnchor->GetCoords();
		axisF = GetFLHAbsoluteCoords(pBulletAnchor, CoordStruct{ 1000, 0, 0 }) - c0; // 官方API，不得修改
		axisL = GetFLHAbsoluteCoords(pBulletAnchor, CoordStruct{ 0, 1000, 0 }) - c0;
		axisH = GetFLHAbsoluteCoords(pBulletAnchor, CoordStruct{ 0, 0, 1000 }) - c0;
		return true;
	}
	return false;
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
// 主 Origin（_lastPoint）与大圆解算起始点（_bigCircleStartPoint）共用
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

// ========================================================================
// 解算倾斜单一实现：把局部偏移 FLH 按姿态参数包摆成世界坐标点（归一化，无分派层）。
// 固定优先级消费 PoseParams：
//   1. worldDirect      → 世界直加（DirStruct{} 三轴直摆；唯一直加来源 = IsOnWorld 标签，
//                         不由 AllowOriginTilt 决定）
//   2. useUnitPose+锚活 → 引擎单位完整姿态：GetFLHAbsoluteCoords(锚, FLH, onTurret)
//      （Locomotor 矩阵 + TurretOffset 转轴 + 炮塔差角；onTurret=false 挂车身，无转轴/差角）；
//      剥单位位移只留姿态偏移叠 base（base=调用方按 NoUpdate 刷新的计算点）
//   3. 其余             → facing 水平朝向 + tilt 俯仰：
//      绕 L 轴俯仰混合 F/H（f'=f·cosT−h·sinT, h'=f·sinT+h·cosT, l'=l），整体再走引擎 API
//      GetFLHAbsoluteCoords（内含 RA2 坐标系修正 RotateZ(dir)+Y 镜像——引擎弧度体系有
//      90° 偏置/镜像，禁止裸 cos/sin 手写旋转）；tilt=0 时 f'=f、h'=h 退化为纯水平摆放。
// 锚单位死亡/不可用：useUnitPose 自动落 3（facing+tilt）。
// ========================================================================
CoordStruct VectorEffect::ResolveTilting(const CoordStruct& base, const CoordStruct& flh,
	const PoseParams& pose)
{
	// 1. 世界直加（OriginIsOnWorld/TargetIsOnWorld 语义）
	if (pose.worldDirect)
	{
		return GetFLHAbsoluteCoords(base, flh, DirStruct{}); // 官方API，不得修改：世界轴，无视姿态
	}
	// 2. 引擎单位完整姿态（矩阵含 TurretOffset 转轴/炮塔差角；剥单位位移只留姿态叠 base）
	if (pose.useUnitPose && pose.anchor && !IsDeadOrInvisible(pose.anchor))
	{
		CoordStruct mtxPos = GetFLHAbsoluteCoords(pose.anchor, flh, pose.onTurret); // 官方API，不得修改
		return base + (mtxPos - pose.anchor->GetCoords());
	}
	// 3. facing 水平朝向 + tilt 俯仰（tilt=0 退纯水平）
	double cosT = std::cos(pose.tilt), sinT = std::sin(pose.tilt);
	CoordStruct flhRot = flh;
	flhRot.X = static_cast<int>(flh.X * cosT - flh.Z * sinT);   // f'：F/H 俯仰混合
	flhRot.Z = static_cast<int>(flh.X * sinT + flh.Z * cosT);   // h'：H/F 俯仰混合
	flhRot.Y = flh.Y;                                          // l'：左右不受俯仰影响
	return GetFLHAbsoluteCoords(base, flhRot, pose.facing); // 官方API，不得修改：引擎坐标系修正
}

// Origin 解算流程（挂载复合/补读/每帧共用，消灭三处手写拷贝）：
// 解算偏移 = OriginFLH + CircleOrigin（主圆圆心偏移，同姿态线性合并一次摆——
// 小圆圆心 ≡ 完整解算起始点；两偏移组件不再各自现算，CircleOrigin 原"AllowOriginTilt=no
// 世界直加"尾巴删除：纯直加只归 OriginIsOnWorld）。读 Origin 系标签填 PoseParams → ResolveTilting。
// base = Origin 参考坐标（单位坐标/格子坐标；无锚停更帧 = 快照完整值，调用点不调本函数）；
// fallbackFacing = 水平兜底朝向（仅 Self 弹体侧用：挂载 _fAxisDir / 每帧按载体刷新）；
// currentPos = 弹体现在位置（无锚兜底连线的终点）。
// 坐标系（2026-09-05 修正）：Origin 解算点恒从 Origin 单位自身出发——锚活 = 单位矩阵姿态
// （AllowOriginTilt=yes，OriginIsOnTurret 定炮塔/车身）或单位自身水平朝向（no）；不再读
// IsOnOrigin/fAxisDir（那是 TargetFLH 语义）。无锚（打格子/参照被清空）= 兜底连线或纯竖直
// 直加，不再 return base 丢偏移。无锚停更（死亡后复用最后完整解算点）由调用点快照承载。
CoordStruct VectorEffect::ResolveOriginTilting(const CoordStruct& base, const DirStruct& fallbackFacing,
	const CoordStruct& currentPos)
{
	PoseParams pose;

	// 解算偏移组合：OriginFLH + CircleOrigin 直加合并（线性旋转等价于旧"先摆 OriginFLH
	// 再摆 CircleOrigin"两段；不再有 adjusted Z 覆写——见下方 2026-09-05 修正注）
	CoordStruct resolveFlh = Data->OriginFLH;
	if (!Data->CircleOrigin.IsEmpty())
	{
		// CircleOrigin 链式直加（2026-09-05 用户确认：合计 = OriginFLH + CircleOrigin，
		// 两偏移同姿态一次线性合并——原 adj.Z = OriginFLH.Z + adj.Z 覆写把 OriginFLH.Z 双计：
		// 800+800 得 2400 是错的，正确 = 1600）
		resolveFlh.X += Data->CircleOrigin.X;
		resolveFlh.Y += Data->CircleOrigin.Y;
		resolveFlh.Z += Data->CircleOrigin.Z;
	}

	// 世界直加（OriginIsOnWorld=yes，唯一直加来源——不由 AllowOriginTilt 决定）
	if (Data->OriginIsOnWorld)
	{
		pose.worldDirect = true;
		return ResolveTilting(base, resolveFlh, pose);
	}

	// Self：载体（pTechno）或弹体（pBullet）活着是 Vector 运行的前提，无死锚概念
	if (Data->Origin == VectorData::VectorOrigin::Self)
	{
		if (!pTechno && !pBullet)
			return base;
		if (pTechno && Data->AllowOriginTilt)
		{
			// ① 载体单位：引擎完整姿态（onTurret 由 OriginIsOnTurret 分）
			pose.useUnitPose = true;
			pose.anchor = pTechno;
			pose.onTurret = Data->OriginIsOnTurret;
		}
		else
		{
			// 弹体侧无单位锚（自身朝向水平摆）/ Self 水平：fallbackFacing = 锁定弹体朝向 / fAxisDir
			pose.facing = fallbackFacing;
		}
		return ResolveTilting(base, resolveFlh, pose);
	}

	// 非 Self：Origin 解算点坐标系自足（2026-09-05 修正——不再读 IsOnOrigin / fallbackFacing，
	// 那是 TargetFLH 侧语义：OriginIsOnVectorOrigin 只决定 TargetFLH 取谁的坐标系；OriginFLH
	// 恒从 Origin 单位自身出发。原 ③"单位→弹体连线"分支是把 TargetFLH 标签套到 Origin 上的
	// 错误耦合，删除）。
	TechnoClass* pOriginTechno = FindOriginTechno();
	if (pOriginTechno && !IsDeadOrInvisible(pOriginTechno))
	{
		if (Data->AllowOriginTilt)
		{
			// ① 引擎完整姿态（Locomotor 矩阵 + TurretOffset 转轴 + 炮塔差角；onTurret 由 OriginIsOnTurret 分）
			pose.useUnitPose = true;
			pose.anchor = pOriginTechno;
			pose.onTurret = Data->OriginIsOnTurret;
		}
		else
		{
			// ② 水平：单位自身朝向（OriginIsOnTurret 选炮塔/车身；不再用 fallbackFacing——
			// fAxisDir 由主朝向段按 IsOnOrigin 维护、属 TargetFLH 语义，会引入连线污染）
			pose.facing = Data->OriginIsOnTurret
				? pOriginTechno->TurretFacing().Current()     // 官方API，不得修改：挂炮塔
				: pOriginTechno->PrimaryFacing.Current();     // 官方API，不得修改：挂车身
		}
	}
	else
	{
		// 无锚（打格子 / 参照被清空）：调用点仅在"首帧兜底"进入本函数（此后由快照停更承载），
		// 不可 return base 丢偏移——原实现把裸坐标当解算点返回是打格子圆心钉地面的根因。
		// resolveFlh 带 FL 分量 → 借"base→弹体"连线定水平朝向；tilt=0 水平投影——
		// CoordinateTilt 是 TargetFLH 连线高低差语义，OriginFLH 解算不读（2026-09-05 确认）。
		// 纯竖直（仅 H）→ 任意水平 facing 结果不变（等效世界直加）。
		pose.facing = Point2Dir(base, currentPos); // 官方API，不得修改：无锚兜底水平朝向
	}
	return ResolveTilting(base, resolveFlh, pose);
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
	// _vectorAcquireZ 设置已删（2026-09-05：圆心 Z 由解算点决定，弹体接管高度基准旧规则废除；
	// 字段保留仅存档兼容，见 VectorEffect.h 废弃注释）
	_totalDuration = AE->AEData.GetDuration() / _effectiveTimeStep;
}

// TargetOffset 随机偏移（Radius / F/L/H 两套 + Angles）——照搬旧版
void VectorEffect::ParseTargetOffset()
{
	// 偏移激活 = Radius 或 F/L/H 任一区间有效（与 TargetOffsetNormal 是否填写解耦：不写 Normal
	// 的 Radius/Sphere/F-L-H 配置同样生效，2026-09-05 用户拍板"不应该有门槛"）。Normal 只在此
	// 函数内决定圆面朝向（非空=倾斜圆面 / 空=水平环），不参与消费/预转门槛。
	_targetOffsetActive = Data->TargetOffsetRadiusMin < Data->TargetOffsetRadiusMax
		|| Data->TargetOffsetFMin < Data->TargetOffsetFMax || Data->TargetOffsetFMin2 < Data->TargetOffsetFMax2
		|| Data->TargetOffsetLMin < Data->TargetOffsetLMax || Data->TargetOffsetLMin2 < Data->TargetOffsetLMax2
		|| Data->TargetOffsetHMin < Data->TargetOffsetHMax || Data->TargetOffsetHMin2 < Data->TargetOffsetHMax2;
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
				// TargetOffsetNormal：随机落点在倾斜圆面上（法向量定义圆面），分量空间局部计算（C1 解耦后）。
				// TargetOffsetNormal = (F,L,H) 法线分量，在"当前分量空间"（TargetOffsetNormalOnOrigin
				//   yes=Origin 单位 FLH 轴系 / no=世界 FLH 轴系）里解释——此处不做任何槽位重排/世界换算，
				//   产出 _randomTargetOffset 同为该空间的分量三元组（H 抖动沿分量 H 轴 = yes 单位头顶 /
				//   no 世界 Z，与 OnOrigin 统一语义一致；用户 2026-09-05 拍板 yes 用单位 H 轴）。
				// 参数化：facing = 法线在 FL 平面投影方位角，tilt = 法线仰角（tilt=PI/2 法线朝上=水平圆环）
				// 倾斜圆面取点：rL=radius*cos(flhAngle) 沿 L 切向、rH=radius*sin(flhAngle) 沿 H 方向，映射回分量。
				// 注：flhAngle 的 0 度基准与水平圆环存在 90° 偏移（H 轴在法线朝上时指向 -F），全向随机时无影响；
				//     TargetOffsetAngles 扇区限位组合 yes 模式的行为随环面转，实测确认。
				double fwF = static_cast<double>(Data->TargetOffsetNormal.X); // F 分量（前方）
				double fwL = static_cast<double>(Data->TargetOffsetNormal.Y); // L 分量（左）
				double fwH = static_cast<double>(Data->TargetOffsetNormal.Z); // H 分量（上）
				double lenFL = std::sqrt(fwF * fwF + fwL * fwL);
				double facing = lenFL > 1e-6 ? std::atan2(fwL, fwF) : 0.0;
				double tilt = lenFL > 1e-6 ? std::atan2(fwH, lenFL) : (fwH > 0 ? M_PI / 2.0 : -M_PI / 2.0);
				double cosF = std::cos(facing), sinF = std::sin(facing);
				double cosT = std::cos(tilt), sinT = std::sin(tilt);
				double rL = radius * std::cos(flhAngle);
				double rH = radius * std::sin(flhAngle);
				_randomTargetOffset.X = static_cast<int>(rL * (-sinF) + rH * (-cosF * sinT)); // F 分量
				_randomTargetOffset.Y = static_cast<int>(rL * cosF + rH * (-sinF * sinT));    // L 分量
				_randomTargetOffset.Z = static_cast<int>(rH * cosT);                          // H 分量
				// 选 B：倾斜面 H 分量再叠加 TargetOffsetH 偏移（倾斜面 + 高度抖动）
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
				_lastPoint = cacheStatus->GetVectorCachedCell();
			}
			else if (gotKamikaze)
			{
				_lastPoint = kamikazePos;
			}
			else if (TryGetSpawnManagerTarget(pTechno, _lastPoint))
			{
				// 未进 Kamikaze 容器（导弹未全部发射）时读源头：SpawnManager 目标
			}
			else if (pTechno->Target)
			{
				_lastPoint = pTechno->Target->GetCoords();
			}
			else
			{
				// Kamikaze 容器此刻可能还没加入导弹（发射后才加入）：不锁自身，
				// 留空由 GetVectorResult 首帧补读
				_lastPoint = CoordStruct::Empty;
			}
		}
		else if (pBullet)
		{
			_lastPoint = pBullet->TargetCoords;
		}
		else
		{
			_lastPoint = pObject->GetCoords();
		}
		break;

	case VectorData::VectorOrigin::Launcher:
		// 无论 NoUpdate 都锁定基线（与 Target 分支一致）：NoUpdate=yes 直接用，
		// no 每帧快照刷新覆盖；launcher 死亡时冻结此基线作为 origin 解算起点
		if (pBullet && pBullet->Owner)
			_lastPoint = pBullet->Owner->GetCoords();
		else if (pTechno)
			_lastPoint = pTechno->GetCoords();
		else
			_lastPoint = pObject->GetCoords();
		break;

	case VectorData::VectorOrigin::Source:
		// 无论 NoUpdate 都锁定基线（与 Target/Launcher 分支一致）：死亡时冻结此基线
		if (AE && AE->pSource)
			_lastPoint = AE->pSource->GetCoords();
		else
			_lastPoint = pObject->GetCoords(); // 兜底与 Target 分支一致
		if (AE && AE->pSource)
			_pSource = AE->pSource;
		break;

	case VectorData::VectorOrigin::Self:
		// OriginFLH 摆点不在 InitOrigin 做（原此处 GetFLHAbsoluteCoords 直摆 = Self 特例，
		// 且 pTechno 侧恒炮塔不查 OriginIsOnTurret）——统一移交 LockFacing 尾挂载复合
		// ResolveOriginTilting（归一化：所有 Origin 一条"坐标点取值"管线）。
		// 此处只定朝向与单位坐标：pBullet=弹体速度朝向；pTechno=载体单位炮塔/车身朝向。
		if (pBullet)
		{
			_lastPoint = pBullet->GetCoords(); // 单位坐标，OriginFLH 偏移由挂载复合复合
			_fAxisDir = Facing(pBullet); // 官方API：弹体速度朝向（Point2Dir 内含 RA2 修正），锁定初始朝向
			_fAxisRad = _fAxisDir.GetRadian();
		}
		else if (pTechno)
		{
			DirStruct unitFacing = Data->OriginIsOnTurret
				? pTechno->TurretFacing().Current()   // 官方API，不得修改：挂炮塔
				: pTechno->PrimaryFacing.Current();   // 官方API，不得修改：挂车身（默认）
			_lastPoint = pTechno->GetCoords(); // 单位坐标，OriginFLH 偏移由挂载复合复合
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

	// --- 法向量状态初始化（2026-09-05 归一化：状态存 FLH 分量，无槽位重排/球坐标中转）---
	// NormalVector 直接按 (F,L,H) 存入 normalX/Y/Z，Random 覆盖同分量；未配保持默认
	// (0,0,1)（竖直=水平圆面）。消费换算（分量→世界方向）在每帧法向量段统一执行，挂载不预转。
	if (hasNormal)
	{
		_motion.normalX = static_cast<double>(Data->NormalVector.X); // F 分量
		_motion.normalY = static_cast<double>(Data->NormalVector.Y); // L 分量
		_motion.normalZ = static_cast<double>(Data->NormalVector.Z); // H 分量
		if (Data->NormalRandomF.Y > Data->NormalRandomF.X)
			_motion.normalX = Random::RandomRanged(Data->NormalRandomF.X, Data->NormalRandomF.Y);
		if (Data->NormalRandomL.Y > Data->NormalRandomL.X)
			_motion.normalY = Random::RandomRanged(Data->NormalRandomL.X, Data->NormalRandomL.Y);
		if (Data->NormalRandomH.Y > Data->NormalRandomH.X)
			_motion.normalZ = Random::RandomRanged(Data->NormalRandomH.X, Data->NormalRandomH.Y);
	}
	// 法线旋转角速度解析（常数优先，否则随机）
	_motion.normalStepF = ResolveAngleStep(Data->NormalFAnglePerStep, Data->NormalFAngleRMin, Data->NormalFAngleRMax, Data->NormalFAngleRMin2, Data->NormalFAngleRMax2);
	_motion.normalStepL = ResolveAngleStep(Data->NormalLAnglePerStep, Data->NormalLAngleRMin, Data->NormalLAngleRMax, Data->NormalLAngleRMin2, Data->NormalLAngleRMax2);
	_motion.normalStepH = ResolveAngleStep(Data->NormalHAnglePerStep, Data->NormalHAngleRMin, Data->NormalHAngleRMax, Data->NormalHAngleRMin2, Data->NormalHAngleRMax2);
	_motion.lissajousStep = Data->Lissajous;
	// 重置消费缓存（新挂载丢弃旧世界方向，由每帧消费段重建）
	_motion.normalWorldValid = false;
	_motion.normalLissajousF = 0.0; // 法向量 Lissajous 累计角从 0 开始（挂载清零）
	_motion.normalLissajousL = 0.0;
	_motion.normalLissajousH = 0.0;

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
					_fAxisDir = Data->OriginIsOnTurret
						? pLauncherTechno->TurretFacing().Current()   // 官方API，不得修改：挂炮塔
						: pLauncherTechno->PrimaryFacing.Current();  // 官方API，不得修改：挂车身（默认）
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
						_fAxisDir = Data->OriginIsOnTurret
							? pTargetTechno->TurretFacing().Current()   // 官方API，不得修改：挂炮塔
							: pTargetTechno->PrimaryFacing.Current();  // 官方API，不得修改：挂车身（默认）
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
						_fAxisDir = Data->OriginIsOnTurret
							? pSourceTechno->TurretFacing().Current()   // 官方API，不得修改：挂炮塔
							: pSourceTechno->PrimaryFacing.Current();  // 官方API，不得修改：挂车身（默认）
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
					_fAxisRad = _fAxisDir.GetRadian(); // 官方API：InitOrigin 已用 Facing(pBullet) 锁定弹体朝向，同帧同源直接取
				else if (pTechno)
					_fAxisRad = pTechno->TurretFacing().Current().GetRadian(); // 官方API，不得修改
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
	if ((!Data->OriginFLH.IsEmpty() || !Data->CircleOrigin.IsEmpty()) && !_lastPoint.IsEmpty())
	{
		_lastPoint = ResolveOriginTilting(_lastPoint, _fAxisDir, pObject->GetCoords());
	}

	// TargetOffsetNormalOnOrigin=no：把偏移分量按世界 FLH 轴（空 Dir=朝北）直摆成世界坐标偏移
	// （官方 API 消化 90° 偏置/Y 镜像），消费端叠加在旋转后的 TargetFLH 上，不随任何单位转动。
	// 条件只看 no + 配了偏移（_targetOffsetActive），与 TargetOffsetNormal 是否填写解耦
	// （不写 Normal 的 Radius/Sphere/F-L-H 的 no 模式同样预转，2026-09-05 用户拍板）。
	if (!Data->TargetOffsetNormalOnOrigin && _targetOffsetActive)
	{
		_randomTargetOffset = GetFLHAbsoluteCoords(CoordStruct::Empty, _randomTargetOffset, DirStruct{}); // 官方API
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

	// 目标坐标固化：OnStart 未锁定（Pending）时补读，读到即锁定到 _lastPoint，
	// 防止引擎后续清空（目标死亡/管理器清空）导致 smallCircleTarget 失效
	if (Data->Origin == VectorData::VectorOrigin::Target
		&& _lastPoint.IsEmpty() && pTechno)
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
			_lastPoint = targetPos;
			// NoUpdate=yes：每帧偏移计算（下方管线段）只对 no 执行，补读的单位坐标必须在此复合一次并冻结
			// （techno 侧 SpawnManager/Aircraft 挂载瞬间目标未就绪，挂载复合被空存档守卫拦住，
			//  目标到手后补上这次"位置 + 朝向 + 偏移"计算，与 LockFacing 末尾挂载复合同语义，
			//  统一走 ResolveOriginTilting——三维（AllowOriginTilt=yes）+ 补读组合缺口随归一化补齐）。
			if (Data->OriginNoUpdate && (!Data->OriginFLH.IsEmpty() || !Data->CircleOrigin.IsEmpty()))
			{
				// fallbackFacing（② 水平/兜底出口）按 IsOnOrigin 现算（fAxisDir 段在本段之后才跑）：
				// yes=目标单位自身朝向，无朝向（格子/死亡）回退 目标点→弹体 连线；no=连线。
				DirStruct flhFacing;
				if (Data->IsOnOrigin)
				{
					TechnoClass* pTT = abstract_cast<TechnoClass*>(pTechno->Target);
					if (pTT && !IsDeadOrInvisible(pTT))
					flhFacing = Data->OriginIsOnTurret
						? pTT->TurretFacing().Current()     // 官方API，不得修改：挂炮塔
						: pTT->PrimaryFacing.Current();     // 官方API，不得修改：挂车身（默认）
					else
						flhFacing = Point2Dir(targetPos, pObject->GetCoords()); // 目标无朝向：回退连线
				}
				else
				{
					flhFacing = Point2Dir(targetPos, pObject->GetCoords()); // 目标→弹体连线（IsOnOrigin 默认 no）
				}
				_lastPoint = ResolveOriginTilting(_lastPoint, flhFacing, pObject->GetCoords());
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
	// 法向量自旋不在此执行——已并入下方归一化法向量段（系统二分量自旋 + 系统一消费换算），
	// 2026-09-05 重写后无累计角逻辑。

	CoordStruct currentPos = pObject->GetCoords();

	// ========================================================================
	// 动态 F 轴：非 NoUpdate 时每帧根据当前坐标重新计算 FLH 朝向
	// ========================================================================

	// 注：originTerrainTilt 地形采样链已随 Rodrigues 随动删除（2026-09-05 归一化后法向量消费
	// 走 useUnitPose 矩阵，坡面由 Locomotor 矩阵自带；SampleOriginTilt 函数保留供将来地形相关需求）。

	double effectiveFacing = _fAxisRad;   // 倾斜域初始（!hasNormal 时 = F 轴基准弧度；hasNormal 时每帧法向量段覆写为圆面法向 facing）
	double effectiveTilt = 0.0;            // 倾斜域初始（hasNormal/随动时每帧法向量段覆写；!hasNormal+no 保持 0 = 传统 2D 平面）
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
	// 另：NoUpdate 只切换计算点（锁定 _lastPoint vs 实时坐标），不切换坐标系/朝向算法。
	if (!Data->AllowOriginTilt && !Data->OriginIsOnWorld)
	{
		switch (Data->Origin)
		{
		case VectorData::VectorOrigin::Source:
			// 计算点：NoUpdate=yes 用锁定值，no 每帧刷新（三态跟踪：死亡冻结）
			TrackOriginCoord(_pSource, Data->OriginNoUpdate, _lastPoint);
			if (!_lastPoint.IsEmpty())
			{
				if (Data->IsOnOrigin)
				{
					TechnoClass* pSourceTechno = abstract_cast<TechnoClass*>(_pSource);
					if (pSourceTechno && !IsDeadOrInvisible(pSourceTechno))
					{
					fAxisDir = Data->OriginIsOnTurret
						? pSourceTechno->TurretFacing().Current()   // 官方API，不得修改：挂炮塔
						: pSourceTechno->PrimaryFacing.Current();  // 官方API，不得修改：挂车身（默认）
						if (!hasNormal) effectiveFacing = fAxisDir.GetRadian();
						break;
					}
					// 来源无朝向：回退连线
				}
				// 来源活着或已死亡：都用快照算朝向（死亡后冻结指向死亡点）
				fAxisDir = Point2Dir(_lastPoint, currentPos); // 官方API，不得修改
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
			CoordStruct targetPos = _lastPoint;
			bool gotTarget = !targetPos.IsEmpty();
			if (!gotTarget && !Data->OriginNoUpdate)
			{
				gotTarget = GetTargetPosFromChain(targetPos, true);
			}
			if (gotTarget)
				_lastPoint = targetPos; // 跟随：更新锁定值
			else if (targetPos.IsEmpty())
				break; // 从未有过目标：保持朝向
			if (Data->IsOnOrigin)
			{
				AbstractClass* pTgt = pBullet ? pBullet->Target : (pTechno ? pTechno->Target : nullptr);
				TechnoClass* pTargetTechno = abstract_cast<TechnoClass*>(pTgt);
				if (pTargetTechno && !IsDeadOrInvisible(pTargetTechno))
				{
					fAxisDir = Data->OriginIsOnTurret
						? pTargetTechno->TurretFacing().Current()   // 官方API，不得修改：挂炮塔
						: pTargetTechno->PrimaryFacing.Current();  // 官方API，不得修改：挂车身（默认）
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
				fAxisDir = Data->OriginIsOnTurret
					? pTechno->TurretFacing().Current()   // 官方API，不得修改：挂炮塔
					: pTechno->PrimaryFacing.Current();  // 官方API，不得修改：挂车身（默认）
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
					fAxisDir = Data->OriginIsOnTurret
						? pLauncherTechno->TurretFacing().Current()   // 官方API，不得修改：挂炮塔
						: pLauncherTechno->PrimaryFacing.Current();  // 官方API，不得修改：挂车身（默认）
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

	// ========================================================================
	// 圆面法线（NormalVector 体系，2026-09-05 归一化——替换旧 Rodrigues 随动 + 累计角自旋）
	// 状态 _motion.normalX/Y/Z = FLH 分量（挂载初值；默认 (0,0,1) 竖直=水平圆面），与坐标系解耦。
	//   系统二（旋转）：每帧在上一帧状态上转固定 step（定速增量式，无累计角——越转越快只属
	//   将来 Normal*Lissajous）。
	//   系统一（消费换算）：状态分量按 IsNormalOnOrigin 选坐标系换成世界方向——
	//     yes = 挂 Origin 单位姿态（useUnitPose 矩阵：Locomotor + TurretOffset 转轴 + 炮塔差角
	//            + 坡面，抄 Stand IsOnTurret）；锚死/打格子 = 停止计算，复用最后存活帧缓存；
	//     no  = 世界 FLH 轴直摆（GetFLHAbsoluteCoords(Empty, 分量, 空 Dir)，抄 Stand IsOnWorld）。
	//   产物写 _motion.normalWorldX/Y/Z（跨帧缓存）；球坐标回读进 effectiveFacing/effectiveTilt
	//   供倾斜圆面取点；不碰 fAxisDir（法向量与 FLH 摆放基准解耦）。
	// 注：消费走 int lepton 官方 API，分量 ×1000 放大防截断（矩阵线性，方向 atan2 比例不变）。
	// ========================================================================
	// OriginIsOnWorld（世界简化模式）时法向量体系整体停用：effectiveTilt 保持 0 → 传统 2D 水平圆面
	// （与旧行为一致：OnWorld 强制无倾斜）
	bool normalActive = !Data->OriginIsOnWorld && (hasNormal
		|| (Data->IsNormalOnOrigin && !Data->OriginIsOnWorld)
		|| _motion.normalStepF != 0.0 || _motion.normalStepL != 0.0 || _motion.normalStepH != 0.0
		|| Data->NormalFLissajous > 0.0 || Data->NormalLLissajous > 0.0 || Data->NormalHLissajous > 0.0);
	if (normalActive)
	{
		// 系统二：分量空间旋转，每帧按轴独立合成转角（文档第六节）——
		//   轴配了 Lissajous（>0）：累计角驱动（normalLissajousX += 角速度；实际旋转角 = 不断增大的
		//   累计角 → 越转越快，球面缠绕）；轴没配：走 AnglePerStep 定速增量（normalStepX 固定）。
		//   RotateNormal3D 轴映射见其注释（文档第三节案例校验）。
		double rotF = Data->NormalFLissajous > 0.0 ? (_motion.normalLissajousF += Data->NormalFLissajous) : _motion.normalStepF;
		double rotL = Data->NormalLLissajous > 0.0 ? (_motion.normalLissajousL += Data->NormalLLissajous) : _motion.normalStepL;
		double rotH = Data->NormalHLissajous > 0.0 ? (_motion.normalLissajousH += Data->NormalHLissajous) : _motion.normalStepH;
		RotateNormal3D(_motion.normalX, _motion.normalY, _motion.normalZ, rotF, rotL, rotH);

		// 系统一：消费换算（坐标系轴 → 三轴合成，见下方）
		if (Data->IsNormalOnOrigin && !Data->OriginIsOnWorld)
		{
			// yes：随 Origin 单位姿态（参照单位 = Origin 对应单位；Self=载体/弹体）
			ObjectClass* pAnchorObj = nullptr;
			switch (Data->Origin)
			{
			case VectorData::VectorOrigin::Launcher:
				pAnchorObj = _pLauncher;
				break;
			case VectorData::VectorOrigin::Target:
				{
					// 目标锚只收单位：打格子/地面时 pBullet->Target 是格子对象（非 Techno）→ 无锚
					AbstractClass* pTgt = pBullet ? pBullet->Target : (pTechno ? pTechno->Target : nullptr);
					pAnchorObj = abstract_cast<TechnoClass*>(pTgt);
				}
				break;
			case VectorData::VectorOrigin::Source:
				pAnchorObj = _pSource;
				break;
			default: // Self：载体自身（pTechno=矩阵含坡面；pBullet=弹体水平姿态）
				pAnchorObj = pObject;
				break;
			}
			// 坐标系轴（2026-09-05 用户修正：锚死亡 = 停坐标轴更新，转动保持）：
			//   锚活 → 单位姿态三轴每帧刷新（随单位转/斜）；
			//   无锚且从未有轴（打格子/参照从未存在）→ 世界三轴固化；
			//   无锚且已有轴（参照死亡/目标被清空）→ 轴冻结在此帧最后姿态，不再刷新——
			//   法向量自旋（系统二）继续，消费合成在冻结坐标系中持续输出（圆面继续翻滚）。
			if (GetNormalFrameAxes(pAnchorObj, Data->NormalIsOnTurret,
				_motion.normalAxisF, _motion.normalAxisL, _motion.normalAxisH))
			{
				_motion.normalWorldValid = true; // 坐标系轴建立
			}
			else if (!_motion.normalWorldValid)
			{
				// 从未有锚：世界三轴固化（配了 NormalVector 时呈现其世界方向）
				FillWorldAxes(_motion.normalAxisF, _motion.normalAxisL, _motion.normalAxisH);
				_motion.normalWorldValid = true;
			}
			// 已有轴且无锚：轴冻结（死亡帧最后姿态）
		}
		else
		{
			// no / OriginIsOnWorld：世界固定三轴（只固化一次，其后每帧合成）
			if (!_motion.normalWorldValid)
			{
				FillWorldAxes(_motion.normalAxisF, _motion.normalAxisL, _motion.normalAxisH);
				_motion.normalWorldValid = true;
			}
		}

		// 消费合成（每帧无条件执行——转动保持，坐标系轴可能冻结）：
		// worldN = nF·axisF + nL·axisL + nH·axisH（等价 mtx·(1000n)，见 GetNormalFrameAxes 注释）
		{
			double nF = _motion.normalX, nL = _motion.normalY, nH = _motion.normalZ;
			_motion.normalWorldX = nF * _motion.normalAxisF.X + nL * _motion.normalAxisL.X + nH * _motion.normalAxisH.X;
			_motion.normalWorldY = nF * _motion.normalAxisF.Y + nL * _motion.normalAxisL.Y + nH * _motion.normalAxisH.Y;
			_motion.normalWorldZ = nF * _motion.normalAxisF.Z + nL * _motion.normalAxisL.Z + nH * _motion.normalAxisH.Z;
		}

		// 世界方向（放大 int，比例与方向无关）→ 球坐标 → 倾斜圆面取点输入
		double wx = _motion.normalWorldX, wy = _motion.normalWorldY, wz = _motion.normalWorldZ;
		double lenXY = std::sqrt(wx * wx + wy * wy);
		effectiveFacing = lenXY > 1e-6 ? std::atan2(wy, wx) : 0.0;
		effectiveTilt = lenXY > 1e-6 ? std::atan2(wz, lenXY) : (wz > 0 ? M_PI / 2.0 : -M_PI / 2.0);
	}

	// ========================================================================
	// Origin 坐标（主 Origin 计算点）
	// ========================================================================
	CoordStruct startPoint = currentPos;

	switch (Data->Origin)
	{
	case VectorData::VectorOrigin::Target:
		if (Data->OriginNoUpdate)
			startPoint = _lastPoint.IsEmpty() ? currentPos : _lastPoint; // 锁定初始目标
		else
		{
			// 允许更新（NoUpdate=no）：只跟随活 Techno 目标——死亡/打格子（Target 非单位）
			// 不刷新裸坐标，_lastPoint 保持上一帧完整解算点，由下方出口停更读取
			// （快照语义统一 2026-09-05：死亡 = 复用最后存活帧完整值，打格子 = 挂载已固化）。
			AbstractClass* pTgt = pBullet ? pBullet->Target : (pTechno ? pTechno->Target : nullptr);
			TechnoClass* pTgtT = abstract_cast<TechnoClass*>(pTgt);
			if (pTgtT && !IsDeadOrInvisible(pTgtT))
			{
				_lastPoint = pTgtT->GetCoords(); // 跟随：活目标坐标（出口会覆写为完整解算点）
				startPoint = _lastPoint;
			}
			else if (_lastPoint.IsEmpty())
			{
				// 首帧未就绪（挂载没拿到目标）：坐标链裸取一次，供下方出口首次兜底算完整快照
				CoordStruct updated{};
				if (GetTargetPosFromChain(updated, true))
					startPoint = updated;
				else
					startPoint = currentPos;
			}
			else
				startPoint = _lastPoint; // 死亡/打格子停更：复用最后完整解算点（含偏移）
		}
		break;
	case VectorData::VectorOrigin::Launcher:
		if (Data->OriginNoUpdate)
			startPoint = _lastPoint;
		else
		{
			TrackOriginCoord(_pLauncher, false, _lastPoint); // 发射者活着：每帧快照；死亡：冻结
			startPoint = _lastPoint;
		}
		break;
	case VectorData::VectorOrigin::Source:
		if (Data->OriginNoUpdate)
			startPoint = _lastPoint;
		else
		{
			TrackOriginCoord(_pSource, false, _lastPoint); // 来源活着：每帧快照；死亡：冻结
			startPoint = _lastPoint;
		}
		break;
	case VectorData::VectorOrigin::Self:
		startPoint = Data->OriginNoUpdate ? _lastPoint : currentPos;
		break;
	}

	// OriginFLH 完整解算：解算点的定义 = Origin 参考坐标 + OriginFLH/CircleOrigin 经
	// OriginIsOnTurret/AllowOriginTilt 复合计算的最终偏移。统一"坐标点取值管线"
	// ResolveOriginTilting（与挂载复合/补读同一入口）：
	//   NoUpdate=yes 不在此算——挂载复合已算入偏移冻结，直接用（冻结）。
	//   NoUpdate=no：锚活（或 Self 载体/快照空首算）→ 每帧重算完整解算点写回 _lastPoint；
	//                 无锚且快照已建立（死亡/打格子）→ 停更——startPoint 已是 _lastPoint
	//                 （最后完整解算点，含偏移），不调函数（防偏移双计/防死亡后连线重算）。
	// 快照语义统一（2026-09-05）：_lastPoint 在帧末恒 = 完整解算点；死亡/打格子停更复用，
	// 与 OriginNoUpdate=yes 的"算一次后不重算"同构，只是停点由无锚触发。
	if (!Data->OriginNoUpdate && (!Data->OriginFLH.IsEmpty() || !Data->CircleOrigin.IsEmpty()))
	{
		bool originRecalc = Data->Origin == VectorData::VectorOrigin::Self; // Self：载体活 = Vector 活，无死锚
		if (!originRecalc)
		{
			TechnoClass* pOriginTechno = FindOriginTechno();
			originRecalc = pOriginTechno && !IsDeadOrInvisible(pOriginTechno);
		}
		if (originRecalc || _lastPoint.IsEmpty())
		{
			startPoint = ResolveOriginTilting(startPoint, fAxisDir, currentPos);
			_lastPoint = startPoint; // 完整解算点快照（锚活每帧刷新 / 无锚首帧兜底固化）
		}
		// 无锚 + 快照已建立：停更，startPoint 保持 _lastPoint（完整解算点），不覆写
	}

// GetVectorResult：每帧计算位移（主体内联，段落化）

	// ========================================================================
	// 成熟机制，别乱动 — 模式 C: Circle（独立圆周，圆心=Origin，三选二参数）
	// ========================================================================
	bool hasCircle = Data->CircleRadius > 0 || Data->CircleAnglePerStep > 0.0
		|| (Data->CircleRandomRadiusMax > Data->CircleRandomRadiusMin)
		|| (Data->CircleRandomAngleMax > Data->CircleRandomAngleMin)
		|| Data->CircleDynamic; // 动态：进入帧现算半径，即使 CircleRadius=-1 圆模式也须生效
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

	// 小圆圆心 = 完整解算起始点 startPoint：OriginFLH + CircleOrigin 已并入上方
	// ResolveOriginTilting 一次姿态解算（同姿态线性合并），死亡停刷/NoUpdate 冻结随之
	// 生效——无独立圆心状态，圆心不跳变。此处不再二次摆 CircleOrigin（原独立段已删除，
	// 其 "AllowOriginTilt=no 世界直加 / 死锚直加" 分支作废：纯直加只归 OriginIsOnWorld，
	// 死亡=停止计算由 _lastPoint 冻结承载）。
	// 注（2026-09-05）：原"CircleOrigin 空 + OriginFLH 非空时圆心 Z = _vectorAcquireZ
	// （弹体接管 Vector 瞬间高度）+ OriginFLH.Z"的旧规则已废除——圆心高度恒由解算点
	// （Origin 参考点/格子/冻结值）决定，不再依赖弹体历史位置（打目标/打格子的
	// OriginFLH 竖直偏移直接抬升参考点）。
	CoordStruct smallCircleCenter = startPoint;

		// 圆心移动：Vector.Origin.* 系统
		if (!Data->OriginMoveTo.IsEmpty() || Data->OriginReachTarget || Data->OriginLinearSpeed >= 0 || !Data->OriginTargetFLH.IsEmpty()
			|| Data->OriginCircleRadius >= 0 || Data->OriginCircleSpeed != 0 || Data->OriginCircleAnglePerStep != 0
			|| Data->OriginCircleDynamic) // 动态：进入帧现算大圆半径，即使 OriginCircleRadius=-1 也须生效（大圆必须配小圆，hasCircle 已保证）
		{
			// 解算起始点：默认 startPoint，OriginOrigin 可替换为独立参考系
			CoordStruct bigCircleStartPoint = startPoint;

			// ====================================================================
			// 大圆基准点快照机制（_bigCircleStartPoint 只存"完整最终结果"）：
			//   完整结果 = OriginOrigin 参考坐标 + OriginOriginFLH 挂点 + OriginCircleOffset，全部算完后的值。
			//   首次（快照未建立）：无条件完整算一次写入快照——打格子/目标未就绪在此固化初始结果。
			//   后续帧：读到活锚单位（OriginOriginNoUpdate=no）→ 完整重算并刷新快照；
			//           读不到锚单位（目标死亡被清空 / 打格子从无单位 / NoUpdate 冻结）
			//           → 直接取快照（停止更新，不重算挂点/偏移，无跳变）。
			//   注：单位死亡 = "变 null 前的缓存结果"，由快照承载；打格子 = 格子静止，快照恒正确。
			// ====================================================================
			TechnoClass* pBigCircleAnchorUnit = nullptr; // OriginOrigin 对应单位（Self → 小圆参照 单位；无单位=打格子）
			if (Data->OriginOrigin == VectorData::VectorOrigin::Launcher)
				pBigCircleAnchorUnit = abstract_cast<TechnoClass*>(_pLauncher);
			else if (Data->OriginOrigin == VectorData::VectorOrigin::Source)
				pBigCircleAnchorUnit = abstract_cast<TechnoClass*>(_pSource);
			else if (Data->OriginOrigin == VectorData::VectorOrigin::Target)
			{
				AbstractClass* pTgt = pBullet ? pBullet->Target : (pTechno ? pTechno->Target : nullptr);
				pBigCircleAnchorUnit = abstract_cast<TechnoClass*>(pTgt);
			}
			else
				pBigCircleAnchorUnit = FindOriginTechno(); // OriginOrigin=Self：解算起始点跟小圆，姿态跟小圆参照 单位
			bool anchorAlive = pBigCircleAnchorUnit && !IsDeadOrInvisible(pBigCircleAnchorUnit); // 本帧是否锁定到活锚单位
			// 首次（快照未建立）必算：固化初始快照（打格子/目标未就绪在此落底）；之后仅在
			// 允许更新 + 锚单位活着 时重算。快照建立与否以 IsEmpty 判定（不依赖帧计数，
			// DisabledFrames>0 时首次到达此处帧计数已过 0）。
			bool recalc = _bigCircleStartPoint.IsEmpty() || (!Data->OriginOriginNoUpdate && anchorAlive);
			if (recalc)
			{
				// 参考坐标：锚单位活 → 其坐标；首帧无锚（打格子/目标未就绪）→ startPoint 兜底，
				// Target 打格子另有读链兜底（见下）
				bigCircleStartPoint = startPoint;
				if (Data->OriginOrigin != VectorData::VectorOrigin::Self)
				{
					switch (Data->OriginOrigin)
					{
					case VectorData::VectorOrigin::Launcher:
						if (anchorAlive)
							bigCircleStartPoint = pBigCircleAnchorUnit->GetCoords(); // 发射者活着：每帧跟随
						break;
					case VectorData::VectorOrigin::Source:
						if (anchorAlive)
							bigCircleStartPoint = pBigCircleAnchorUnit->GetCoords(); // 来源活着：每帧跟随
						break;
					case VectorData::VectorOrigin::Target:
						if (anchorAlive)
						{
							bigCircleStartPoint = pBigCircleAnchorUnit->GetCoords(); // 目标单位活着：每帧跟随
						}
						else
						{
							// 快照未建立 + 无活锚单位（目标未就绪/打格子）首次落底：读链兜底取一次参考坐标
							// （补读只为获取目标；此后快照非空即恒用快照，不再走本兜底）
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
								bigCircleStartPoint = targetBase; // 格子坐标（静态，固化后恒用快照）
						}
						// 其余（快照已建立 + 目标死亡/格子）：recalc=false 已拦截，走快照
						break;
					}
				}

				// OriginOriginFLH + OriginCircleOffset 合并偏移（C7：OriginCircleOffset 跟随
				// Origin.AllowOriginTilt，两偏移相加后整体只做一次姿态摆放——与小圆 OriginFLH+
				// CircleOrigin 线性合并同构，避免双重旋转误差）：
				//   Origin.AllowOriginTilt=yes 且 OriginOrigin 有存活单位 → 按单位完整姿态矩阵摆放
				//     （useUnitPose：GetFLHAbsoluteCoords 含车身矩阵 + TurretOffset 转轴 + 炮塔差角；
				//       onTurret 由 Origin.OriginIsOnTurret 决定：yes=挂炮塔，no=挂车身（默认））；
				//   no / 单位死 / 无单位（打格子）→ 纯世界坐标加法（无姿态可跟随，配置语义）
				CoordStruct originOffsetSum = Data->OriginOriginFLH + Data->OriginCircleOffset;
				if (!originOffsetSum.IsEmpty())
				{
					if (Data->OriginAllowOriginTilt && anchorAlive)
					{
						PoseParams pose;
						pose.useUnitPose = true;
						pose.anchor = pBigCircleAnchorUnit;
						pose.onTurret = Data->OriginOriginIsOnTurret;
						bigCircleStartPoint = ResolveTilting(bigCircleStartPoint, originOffsetSum, pose);
					}
					else
					{
						bigCircleStartPoint.X += originOffsetSum.X;
						bigCircleStartPoint.Y += originOffsetSum.Y;
						bigCircleStartPoint.Z += originOffsetSum.Z;
					}
				}

				// 快照 = 完整最终结果（首帧或锚单位活时每帧刷新；NoUpdate=yes 只有首帧走这里）
				_bigCircleStartPoint = bigCircleStartPoint;
			}
			else
			{
				// 无活锚单位（目标死亡 / 打格子 / NoUpdate 冻结）：停止更新，直接用快照
				bigCircleStartPoint = _bigCircleStartPoint;
			}

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
				// Normal 初始化（2026-09-05 归一化：状态存 FLH 分量 (F,L,H)，无槽位重排/球坐标中转；
				// 未配 OriginNormalVector 时保持默认 (0,0,1) 竖直=水平圆面——是否随 OriginOrigin
				// 单位姿态只由 OriginIsNormalOnOrigin 决定，大小圆统一规则）
				if (!Data->OriginNormalVector.IsEmpty())
				{
					_originMotion.normalX = Data->OriginNormalVector.X; // F 分量
					_originMotion.normalY = Data->OriginNormalVector.Y; // L 分量
					_originMotion.normalZ = Data->OriginNormalVector.Z; // H 分量
					if (Data->OriginNormalRandomF.Y > Data->OriginNormalRandomF.X)
						_originMotion.normalX = Random::RandomRanged(Data->OriginNormalRandomF.X, Data->OriginNormalRandomF.Y);
					if (Data->OriginNormalRandomL.Y > Data->OriginNormalRandomL.X)
						_originMotion.normalY = Random::RandomRanged(Data->OriginNormalRandomL.X, Data->OriginNormalRandomL.Y);
					if (Data->OriginNormalRandomH.Y > Data->OriginNormalRandomH.X)
						_originMotion.normalZ = Random::RandomRanged(Data->OriginNormalRandomH.X, Data->OriginNormalRandomH.Y);
				}
				// Normal 角速度
				_originMotion.normalStepF = ResolveAngleStep(Data->OriginNormalFAnglePerStep, Data->OriginNormalFAngleRMin, Data->OriginNormalFAngleRMax, Data->OriginNormalFAngleRMin2, Data->OriginNormalFAngleRMax2);
				_originMotion.normalStepL = ResolveAngleStep(Data->OriginNormalLAnglePerStep, Data->OriginNormalLAngleRMin, Data->OriginNormalLAngleRMax, Data->OriginNormalLAngleRMin2, Data->OriginNormalLAngleRMax2);
				_originMotion.normalStepH = ResolveAngleStep(Data->OriginNormalHAnglePerStep, Data->OriginNormalHAngleRMin, Data->OriginNormalHAngleRMax, Data->OriginNormalHAngleRMin2, Data->OriginNormalHAngleRMax2);
				_originMotion.lissajousStep = Data->OriginLissajous;
				_originMotion.normalWorldValid = false; // 重置消费缓存（对象复用/重新挂载时丢弃旧世界方向）
				_originMotion.normalLissajousF = 0.0;   // 法向量 Lissajous 累计角从 0 开始
				_originMotion.normalLissajousL = 0.0;
				_originMotion.normalLissajousH = 0.0;
			}
		// OriginIsNormalOnOrigin：大圆法向量（2026-09-05 归一化，同小圆结构——替换 Rodrigues 随动）：
		// 状态 _originMotion.normalX/Y/Z = FLH 分量（首帧初始化）；每帧定速自旋（系统二，增量式）；
		// 消费换算随 OriginOrigin 单位姿态（yes，矩阵含 TurretOffset 转轴/炮塔差角/坡面；onTurret 由
		// Origin.NormalIsOnTurret 独立决定）或世界 FLH 轴直摆（no）；产物写 normalWorldX/Y/Z
		// （无锚停止计算复用最后存活帧）。oFacing/oTilt 供 Circle/MoveTo/Speed 运动消费。
		{
			bool originNormalActive = !Data->OriginNormalVector.IsEmpty()
				|| Data->OriginNormalRandomF.Y > Data->OriginNormalRandomF.X
				|| Data->OriginNormalRandomL.Y > Data->OriginNormalRandomL.X
				|| Data->OriginNormalRandomH.Y > Data->OriginNormalRandomH.X
				|| Data->OriginIsNormalOnOrigin
				|| _originMotion.normalStepF != 0.0 || _originMotion.normalStepL != 0.0 || _originMotion.normalStepH != 0.0
				|| Data->OriginNormalFLissajous > 0.0 || Data->OriginNormalLLissajous > 0.0 || Data->OriginNormalHLissajous > 0.0;
			double oFacing = 0.0, oTilt = 0.0;
			if (originNormalActive)
			{
				// 系统二：分量空间旋转，每帧按轴独立合成转角（文档第六节，同小圆）——
				//   轴配 Lissajous（>0）= 累计角驱动（越转越快）；没配 = AnglePerStep 定速
				double rotF = Data->OriginNormalFLissajous > 0.0 ? (_originMotion.normalLissajousF += Data->OriginNormalFLissajous) : _originMotion.normalStepF;
				double rotL = Data->OriginNormalLLissajous > 0.0 ? (_originMotion.normalLissajousL += Data->OriginNormalLLissajous) : _originMotion.normalStepL;
				double rotH = Data->OriginNormalHLissajous > 0.0 ? (_originMotion.normalLissajousH += Data->OriginNormalHLissajous) : _originMotion.normalStepH;
				RotateNormal3D(_originMotion.normalX, _originMotion.normalY, _originMotion.normalZ, rotF, rotL, rotH);

				// 系统一：消费换算（坐标系轴 → 三轴合成，见下方）
				if (Data->OriginIsNormalOnOrigin)
				{
					// yes：随 OriginOrigin 单位姿态（参照单位 = OriginOrigin 对应单位；Self=跟小圆 Origin 单位，无则弹体）
					ObjectClass* pAnchorObj = nullptr;
					switch (Data->OriginOrigin)
					{
					case VectorData::VectorOrigin::Launcher:
						pAnchorObj = _pLauncher;
						break;
				case VectorData::VectorOrigin::Target:
					{
						// 目标锚只收单位：打格子/地面时 Target 是格子对象（非 Techno）→ 无锚
						AbstractClass* pTgt = pBullet ? pBullet->Target : (pTechno ? pTechno->Target : nullptr);
						pAnchorObj = abstract_cast<TechnoClass*>(pTgt);
					}
					break;
				case VectorData::VectorOrigin::Source:
					pAnchorObj = _pSource;
					break;
				default: // Self：大圆基准跟小圆 → 姿态跟小圆 Origin 单位；无则弹体水平
					pAnchorObj = FindOriginTechno();
					if (!pAnchorObj) pAnchorObj = pObject;
					break;
				}
				// 坐标系轴（2026-09-05 用户修正：锚死亡 = 停坐标轴更新，转动保持，同小圆）：
				//   锚活 → 单位姿态三轴每帧刷新；无锚且从未有轴 → 世界三轴固化；
				//   无锚且已有轴 → 轴冻结在此帧最后姿态，法向量自旋在冻结坐标系继续。
				if (GetNormalFrameAxes(pAnchorObj, Data->OriginNormalIsOnTurret,
					_originMotion.normalAxisF, _originMotion.normalAxisL, _originMotion.normalAxisH))
				{
					_originMotion.normalWorldValid = true;
				}
				else if (!_originMotion.normalWorldValid)
				{
					// 从未有锚：世界三轴固化
					FillWorldAxes(_originMotion.normalAxisF, _originMotion.normalAxisL, _originMotion.normalAxisH);
					_originMotion.normalWorldValid = true;
				}
				// 已有轴且无锚：轴冻结（死亡帧最后姿态）
				}
				else
				{
					// no：世界固定三轴（只固化一次，其后每帧合成）
					if (!_originMotion.normalWorldValid)
					{
						FillWorldAxes(_originMotion.normalAxisF, _originMotion.normalAxisL, _originMotion.normalAxisH);
						_originMotion.normalWorldValid = true;
					}
				}

				// 消费合成（每帧无条件执行——转动保持，坐标系轴可能冻结）
				{
					double nF = _originMotion.normalX, nL = _originMotion.normalY, nH = _originMotion.normalZ;
					_originMotion.normalWorldX = nF * _originMotion.normalAxisF.X + nL * _originMotion.normalAxisL.X + nH * _originMotion.normalAxisH.X;
					_originMotion.normalWorldY = nF * _originMotion.normalAxisF.Y + nL * _originMotion.normalAxisL.Y + nH * _originMotion.normalAxisH.Y;
					_originMotion.normalWorldZ = nF * _originMotion.normalAxisF.Z + nL * _originMotion.normalAxisL.Z + nH * _originMotion.normalAxisH.Z;
				}

				// 世界方向（放大 int）→ 球坐标 → Circle/MoveTo/Speed 消费
				double wx = _originMotion.normalWorldX, wy = _originMotion.normalWorldY, wz = _originMotion.normalWorldZ;
				double lenXY = std::sqrt(wx * wx + wy * wy);
				oFacing = lenXY > 1e-6 ? std::atan2(wy, wx) : 0.0;
				oTilt = lenXY > 1e-6 ? std::atan2(wz, lenXY) : (wz > 0 ? M_PI / 2.0 : -M_PI / 2.0);
			}
			// 大圆面倾斜唯一来源 = 大圆法向量（OriginNormalVector + OriginIsNormalOnOrigin 随动）：
			// OriginAllowOriginTilt 不再叠加单位倾斜进 oTilt（它只管大圆解算起始点，见 VectorData.h 注释）。
			// 圆周 Lissajous 相位累加（圆上点相位，与法向量无关）
			_originMotion.normalRotF += _originMotion.lissajousStep;
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
					// 弧影子起点（Speed 弧高进度基准）：影子从弧起始圆心出发走干净直线，
					// 不读圆心实际位置——圆心被弧抬升后圆心→目标 3D 距离失真会污染 t
					_originMotion.shadowX = bigCircleCenter.X;
					_originMotion.shadowY = bigCircleCenter.Y;
					_originMotion.shadowZ = bigCircleCenter.Z;
					_originMotion.shadowTraveled = 0.0;
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

					if (_originMotion.arcHeight != 0)
					{
						// 弧存在：直线位移走公共影子（AdvanceArcShadow，主直线 Speed/ReachTarget 同款）——
						// 影子沿"影子自己→bigCircleTarget"推进（首帧起点=arcStartCenter，不受弧抬升污染），
						// 圆心位移 = 影子步长 + 弧增量；圆心被弧抬不会反过来把直线拽向目标吃掉弧。
						// 到达判定并入影子距离（同主直线 Speed 670f402）：弧线时圆心走弧、影子先到直线目标，
						// 只判圆心实时 dist 永不满足 → 影子停推圆心僵直。影子到位 = 理论飞行完成，立即收尾。
						double sdx = bigCircleTarget.X - _originMotion.shadowX;
						double sdy = bigCircleTarget.Y - _originMotion.shadowY;
						double sdz = bigCircleTarget.Z - _originMotion.shadowZ;
						double shadowDist = std::sqrt(sdx * sdx + sdy * sdy + sdz * sdz);
						if (dist < 1.0)
						{
							disp = {}; // 圆心已贴脸：停住等 AE 自然结束（原无弧行为）
						}
						else if (Data->OriginSpeedEndOnReach && (_originMotion.speed >= dist || shadowDist <= _originMotion.speed))
						{
							disp.X = dx; disp.Y = dy; disp.Z = dz;
							Deactivate();
						}
						else
						{
							double dispX = 0.0, dispY = 0.0, dispZ = 0.0;
							double t = 0.0, arcDelta = 0.0;
							AdvanceArcShadow(_originMotion, bigCircleTarget, _originMotion.speed,
								dispX, dispY, dispZ, t, arcDelta);
							disp.X = static_cast<int>(dispX);
							disp.Y = static_cast<int>(dispY);
							disp.Z = static_cast<int>(dispZ);
							// 弧面旋转增量叠加
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
					else
					{
						// 无弧：圆心实时方向追踪（原逻辑）
						if (dist < 1.0) disp = {};
						else if (Data->OriginSpeedEndOnReach && _originMotion.speed >= dist)
						{
							disp.X = dx; disp.Y = dy; disp.Z = dz;
							Deactivate();
						}
						else { double s = _originMotion.speed / dist; disp.X = (int)(dx*s); disp.Y = (int)(dy*s); disp.Z = (int)(dz*s); }
					}
				}
			}
			else // Circle 模式
			{
				// Vector.Origin.CircleDynamic=yes：进入大圆定格帧（大圆 Circle 首次消费 = 冻结期后首运动帧）现算初始值，只此一次。
				// 语义（2026-09-06 用户拍板，与小圆同构）：Origin.CircleRadius = 弹体到大圆基准点水平距（丢弃高度差，
				// 0 → 回退配置 OriginCircleRadius → 648）；基准点 = 管线解算 XY 不变，Z 丢弃改为弹体进入帧高度——
				// 活摆（每帧重摆基准点）覆写 Origin.CircleOrigin（OriginCircleOffset）Z 一次；冻结（NoUpdate/死锚/打格子，
				// 快照停更）直接改基准点快照 Z 一次。后续消费管线零改动。
				if (Data->OriginCircleDynamic && !_originDynamicSampled)
				{
					_originDynamicSampled = true;
					// 基准点高度：仅当存在摆点偏移才需覆写（全空时基准点=参考点本身，Z 由参考点决定，无偏移可改）
					if (!Data->OriginOriginFLH.IsEmpty() || !Data->OriginCircleOffset.IsEmpty())
					{
						if (!Data->OriginOriginNoUpdate && anchorAlive)
						{
							// 活摆：覆写 OriginCircleOffset.Z（偏移输入一次，INI 的 Z 作废），下帧解算自然抬基准点
							Data->OriginCircleOffset.Z += currentPos.Z - bigCircleStartPoint.Z;
							bigCircleStartPoint.Z = currentPos.Z; // 本帧消费同步（自愈式）
						}
						else
						{
							// 冻结：基准点 = 快照 _bigCircleStartPoint 不再重摆 → 直接改快照与本地 Z 一次
							_bigCircleStartPoint.Z = currentPos.Z;
							bigCircleStartPoint.Z = currentPos.Z;
						}
					}
					// Origin.CircleRadius = 弹体到大圆基准点水平距（丢弃高度差）
					double rdx = currentPos.X - bigCircleStartPoint.X;
					double rdy = currentPos.Y - bigCircleStartPoint.Y;
					double dynBigRadius = std::sqrt(rdx * rdx + rdy * rdy);
					if (dynBigRadius < 1.0) // 弹体恰在基准点正上/下方：回退已配置 OriginCircleRadius，未配置硬编码 648（用户拍板）
						dynBigRadius = Data->OriginCircleRadius > 0 ? static_cast<double>(Data->OriginCircleRadius) : 648.0;
					_originMotion.circleRadius = dynBigRadius;
				}
				_originMotion.circleRadius += Data->OriginCircleRadiusGrow;
				double tr = _originMotion.circleRadius;
				if (Data->OriginCircleMaxRadius > 0 && tr > Data->OriginCircleMaxRadius) tr = Data->OriginCircleMaxRadius;
				if (Data->OriginCircleMinRadius > 0 && tr < Data->OriginCircleMinRadius) tr = Data->OriginCircleMinRadius;
				// 角步长：优先线速度/半径推算，否则用固定角速度
				double originAngleStep = Data->OriginCircleAnglePerStep;
				if (Data->OriginCircleSpeed != 0 && tr > 0)
					originAngleStep = Math::rad2deg(Data->OriginCircleSpeed / tr);			// Lissajous>0: 累积大角旋转（增减边震荡），==0: 每帧仅增量旋转（平滑行星）
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
		} // 归一化法向量块结束（oFacing/oTilt 作用域覆盖上方 MoveTo/Speed/Circle 消费）
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
		if (!Data->CircleDynamic && _elapsedFrames == 0)
		{
			_motion.circleRadius = static_cast<double>(Data->CircleRadius);
			if (_motion.circleRadius <= 0.0)
				_motion.circleRadius = currentDist;
			if (Data->CircleRandomRadiusMax > Data->CircleRandomRadiusMin)
				_motion.circleRadius = Random::RandomRanged(Data->CircleRandomRadiusMin, Data->CircleRandomRadiusMax);
		}
		// Vector.CircleDynamic=yes：进入圆定格帧（冻结期后首个实际运动帧 = 消费段首次到达）现算初始值，只此一次。
		// 语义（2026-09-06 用户拍板）：半径 = 弹体到管线圆心水平距（丢弃高度差，0 → 回退 CircleRadius → 648）；
		// 圆心 = 管线解算 XY 不变，Z 丢弃改为弹体进入帧高度——活摆（每帧重摆）覆写 CircleOrigin.Z 一次，
		// 冻结（NoUpdate=yes/死锚/打格子，不再重摆）直接改圆心坐标 Z 一次。后续消费管线零改动。
		if (Data->CircleDynamic && !_circleDynamicSampled)
		{
			_circleDynamicSampled = true;
			// 圆心高度：仅当存在偏移摆点才需覆写（无偏移时圆心 Z 不参与消费，弹体天然保持自身高度）
			if (!Data->OriginFLH.IsEmpty() || !Data->CircleOrigin.IsEmpty())
			{
				// 活摆判定（同 ResolveOriginTilting 每帧重摆条件 1611）：NoUpdate=no 且锚单位活（Self 恒活）
				bool originAlive = Data->Origin == VectorData::VectorOrigin::Self;
				if (!originAlive)
				{
					TechnoClass* pAnchor = FindOriginTechno();
					originAlive = pAnchor && !IsDeadOrInvisible(pAnchor);
				}
				if (!Data->OriginNoUpdate && originAlive)
				{
					// 活摆：覆写 CircleOrigin.Z（偏移输入一次，INI 的 Z 作废），后续帧解算自然抬圆心到弹体进入高度
					Data->CircleOrigin.Z += currentPos.Z - smallCircleCenter.Z;
					smallCircleCenter.Z = currentPos.Z; // 本帧消费同步（自愈式，下帧起解算自带）
				}
				else
				{
					// 冻结：圆心 = 最后完整解算点（_lastPoint）不再重摆 → 直接改其 Z 一次
					_lastPoint.Z = currentPos.Z;
					smallCircleCenter.Z = currentPos.Z;
				}
			}
			// 半径 = 弹体到圆心水平距（丢弃高度差；dx/dy 为上方 2173 现成的 首帧弹体→圆心 向量）
			double dynRadius = std::sqrt(dx * dx + dy * dy);
			if (dynRadius < 1.0) // 弹体恰在圆心正上/下方：回退已设置 CircleRadius，未设置硬编码 648（用户拍板）
				dynRadius = Data->CircleRadius > 0 ? static_cast<double>(Data->CircleRadius) : 648.0;
			_motion.circleRadius = dynRadius;
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
			// 零投影兜底（2026-09-05 bug 修复）：弹体接管时恰在圆心正上/正下（XY 与圆心重合，
			// 打格子垂直下落常见）→ dL=dH=0，归一化无方向 → 半径退化为 0 → 绕圈钉死。
			// 此时给默认起始相位（圆面 0° = 沿 L 轴方向，ndH=0），转过一帧后位置离开圆心自续。
			double ndL = 0.0, ndH = 0.0;
			if (std::fabs(dL) < 1e-3 && std::fabs(dH) < 1e-3)
			{
				ndL = targetRadius;
			}
			else
			{
				ndL = dL / curDist * targetRadius;
				ndH = dH / curDist * targetRadius;
			}
			double rL = ndL * cosA - ndH * sinA;
			double rH = ndL * sinA + ndH * cosA;
			// 位移基准 = 弹体实际位置 currentPos（2026-09-06 自愈式，用户拍板方案 A）：
			// desired = 圆周下一相位绝对点。任意单帧丢失（段切换首帧 OnUpdateEnd 未触发等）
			// 会被下一帧自动补偿（径向 + Z 修正），弹体最终必收敛到圆周——不再依赖内部
			// _circlePos 与弹体位置严格同步（首帧大位移丢失曾导致圆高/圆心随进入点漂移）。
			result.MoveDisp.X = smallCircleCenter.X + static_cast<int>(rL * (-sinF) + rH * (-cosF * sinT)) - currentPos.X;
			result.MoveDisp.Y = smallCircleCenter.Y + static_cast<int>(rL * cosF + rH * (-sinF * sinT)) - currentPos.Y;
			result.MoveDisp.Z = smallCircleCenter.Z + static_cast<int>(rH * cosT) - currentPos.Z;
		}
		else
		{
			// 传统 2D 圆面（XY 平面）
			// 零投影兜底（2026-09-05）：弹体 XY 恰与圆心重合 → dx=dy=0 归一化无方向 → 半径退化钉死；
			// 给默认起始相位（0° 沿 +X 方向），转过一帧后自续
			double ndx = 0.0, ndy = 0.0;
			if (dx == 0.0 && dy == 0.0)
			{
				ndx = targetRadius;
			}
			else
			{
				ndx = dx / currentDist * targetRadius;
				ndy = dy / currentDist * targetRadius;
			}
			double rx = ndx * cosA - ndy * sinA;
			double ry = ndx * sinA + ndy * cosA;
			// 位移基准 = 弹体实际位置 currentPos（2026-09-06 自愈式，同倾斜分支注释）
			result.MoveDisp.X = smallCircleCenter.X + static_cast<int>(rx) - currentPos.X;
			result.MoveDisp.Y = smallCircleCenter.Y + static_cast<int>(ry) - currentPos.Y;
			result.MoveDisp.Z = Data->CircleOrigin.IsEmpty() && Data->OriginFLH.IsEmpty()
				? 0 : smallCircleCenter.Z - currentPos.Z;  // 有显式高度指定时拉 Z（自愈），否则维持抛射体自身高度
		}
		// _circlePos 更新 = 本帧目标圆周点（绝对，2026-09-06）：MoveDisp 基准已改为 currentPos
		// （自愈式），若 _circlePos 仍 += disp，弹体与内部相位脱节（段切换首帧应用丢失）时
		// 差值会被累进 _circlePos → 相位飞离圆周（曾实证 cPos 距圆心 2064、Z 发散 2998）。
		// 直接设到 currentPos + MoveDisp（= 圆周下一相位点绝对坐标）→ _circlePos 恒在圆周，
		// trackPos 方向不漂移。
		_circlePos.X = currentPos.X + result.MoveDisp.X;
		_circlePos.Y = currentPos.Y + result.MoveDisp.Y;
		_circlePos.Z = currentPos.Z + result.MoveDisp.Z;
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
	CoordStruct smallCircleTargetFlh = Data->TargetFLH;
	// TargetOffset 坐标系自足（2026-09-05 解耦补完）：不再并入 TargetFLH 摆点系——
	// TargetOffsetNormalOnOrigin 决定 offset 分量自己的坐标系（yes=Origin 单位系 / no=世界系，
	// LockFacing 已预转），统一走下方世界化通道直加。此前并入 flh 会让 offset 分量随 TargetFLH
	// 摆点系旋转：IsOnOrigin=no 打格子时该系 = "格子→弹体"连线、每帧随弹体方位变 → 目标点
	// 绕格子转 → ReachTarget 追动目标抽搐（用户裁决：无锚 = 停止更新/世界直摆）。

	// TargetFLH → 世界坐标：AutoWeapon 同款管线
	// 坐标系统一：矩阵偏移（含 IsOnTurret 炮塔/车身）+ NoUpdate 控制的计算点 startPoint
	CoordStruct smallCircleTarget;
	// NoUpdate=yes：目标点锁定。首帧正常计算一次缓存，后续每帧直接复用 _lockedSmallCircleTarget，
	// 不再执行"读发射者实时坐标/朝向 → 算新目标点"的每帧刷新（Origin=Launcher 时
	// mtxPos = 发射者实时坐标+实时朝向旋转 FLH，NoUpdate 若不隔离这里，目标点每帧被重写）
	// 坐标系所属单位查找提前（停更判定需要锚活状态）——单位自身坐标系（TargetFLH 挂单位）：
	//   IsOnOrigin=yes 且对应对象是活单位才挂（Target 打格子/单位死 → 无锚回退 2D）。
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
	const bool anchorAlive = pAnchorUnit && !IsDeadOrInvisible(pAnchorUnit);
	// 停更 = NoUpdate=yes（永久锁定）|| 无锚（死亡/打格子——2026-09-05 用户拍板：
	// 目标死亡瞬间弹体目标 = 死亡前最后锁定坐标，原定打哪里还打哪里，永不变化）。
	// 缓存 _lockedSmallCircleTarget 每帧在 else 尾写（锚活帧 = 最后完整目标点；
	// NoUpdate 首帧 / 无锚首帧 = 固化值）；命中路径（NoUpdate 或 无锚）直接复用不再重摆。
	if ((Data->OriginNoUpdate || !anchorAlive) && !_lockedSmallCircleTarget.IsEmpty())
	{
		smallCircleTarget = _lockedSmallCircleTarget;
	}
	else
	{
	if (pAnchorUnit && !IsDeadOrInvisible(pAnchorUnit))
	{
		// 单位自身坐标系（模式①/②，填 PoseParams 一次调用）：
		//   TargetSameTilt=yes（默认）= ① 引擎单位完整姿态（Locomotor 矩阵 + TurretOffset +
		//     炮塔旋转角），含车体倾斜——成熟算法保默认；
		//   no = ② 抛弃倾斜（水平基准）：FLH 只按单位水平朝向旋转，不随单位坡面俯仰。
		//   TargetIsOnTurret：yes=挂炮塔（onTurret：矩阵落转轴+叠炮塔差角），no（默认）=挂车身
		//     （2026-09-05 用户裁决：瞄准单位必然瞄准车身，不该管炮塔朝向——默认 no，写 yes 才挂炮塔）。
		//   基准点 startPoint（NoUpdate 控制的计算点）取代单位位置，剥掉单位位移只留姿态偏移。
		PoseParams pose;
		pose.anchor = pAnchorUnit;
		pose.onTurret = Data->TargetIsOnTurret;
		if (Data->TargetSameTilt)
			pose.useUnitPose = true;
		else
			pose.facing = Data->TargetIsOnTurret
				? pAnchorUnit->TurretFacing().Current()   // 官方API，不得修改：炮塔水平朝向
				: pAnchorUnit->PrimaryFacing.Current();   // 官方API，不得修改：车身水平朝向
		smallCircleTarget = ResolveTilting(startPoint, smallCircleTargetFlh, pose);
	}
	else
	{
		// 无单位可锚：
		// 1. 世界模式（TargetIsOnWorld=yes，TargetFLH 直接世界坐标系，无视倾斜旋转）→ 世界直加
		// 2. 连线坐标系（Origin≠Self 且 IsOnOrigin=no，含 Launcher/Source/Target）→ ③
		//    F 轴 = Origin 本体中心（_lastPoint，主 Origin 段维护：no=每帧跟实时，yes=冻结）→ 抛射体；
		//    CoordinateTilt=yes 取真实 3D 连线（高低差进 tilt），no=水平投影。抛射体从自身 FireFLH 出发，
		//    不瞬移——连线的 C→P 方向只决定 TargetFLH 往哪摆
		// 3. 其余（单位死/弹体侧 Self）→ fAxisDir 水平旋转（② 兜底）
		PoseParams pose;
		if (Data->TargetIsOnWorld)
		{
			pose.worldDirect = true;
		}
		else if (Data->Origin != VectorData::VectorOrigin::Self && !Data->IsOnOrigin)
		{
			// ③ 连线：起点 = Origin 本体中心（_lastPoint）→ 弹体现在位置
			pose.facing = fAxisDir;
			if (!_lastPoint.IsEmpty())
			{
				double ddx = static_cast<double>(currentPos.X - _lastPoint.X);
				double ddy = static_cast<double>(currentPos.Y - _lastPoint.Y);
				double ddz = static_cast<double>(currentPos.Z - _lastPoint.Z);
				double lenXY = std::sqrt(ddx * ddx + ddy * ddy);
				if (lenXY > 1e-6)
				{
					if (Data->CoordinateTilt)
						pose.tilt = std::atan2(ddz, lenXY); // 连线高低角（弹体高于起点为正）
					pose.facing = Point2Dir(_lastPoint, currentPos); // 官方API，不得修改：Origin 本体→弹体
				}
			}
		}
		else
		{
			pose.facing = fAxisDir;
		}
		smallCircleTarget = ResolveTilting(startPoint, smallCircleTargetFlh, pose);
	}
		// TargetOffset 世界化直加 —— 在 else（计算）分支内统一执行一次：
		//   （2026-09-05 修复：原块位于 if(缓存)/else(计算) 结构之外，NoUpdate=yes 时
		//    缓存命中路径每帧重复 += offsetWorld → 目标点每帧外推乱飞（日志实测 Δ 恒定 =
		//    同一随机偏移向量逐帧叠加）。现只走计算分支加一次，缓存分支直接用已含
		//    offset 的缓存终值。）
		// 消费与否只看 _targetOffsetActive = 配了 Radius/F-L-H，与 TargetOffsetNormal
		// 是否填写解耦；Normal 只决定圆面朝向。坐标系由 TargetOffsetNormalOnOrigin 自足决定：
		//   yes = Origin 单位系分量：有活锚单位（FindOriginTechno 非空）→ 单位姿态矩阵转世界
		//         （随单位转/斜，每帧重算 = 落点随姿态走）；无锚（打格子/参照死亡/目标 null）
		//         → 世界直摆固化（停止更新，不随弹体方位转——消除抽搐）
		//   no  = LockFacing 已按世界轴预转（no + 偏移激活），直接用
		if (_targetOffsetActive)
		{
			CoordStruct offC{ _randomTargetOffset.X, _randomTargetOffset.Y, _randomTargetOffset.Z };
			CoordStruct offsetWorld;
			if (Data->TargetOffsetNormalOnOrigin)
			{
				TechnoClass* pOffAnchor = FindOriginTechno();
				if (pOffAnchor && !IsDeadOrInvisible(pOffAnchor))
				{
					PoseParams offPose;
					offPose.anchor = pOffAnchor;
					offPose.onTurret = Data->TargetIsOnTurret;
					if (Data->TargetSameTilt)
						offPose.useUnitPose = true;
					else
						offPose.facing = Data->TargetIsOnTurret
							? pOffAnchor->TurretFacing().Current()   // 官方API，不得修改
							: pOffAnchor->PrimaryFacing.Current();   // 官方API，不得修改
					offsetWorld = ResolveTilting(CoordStruct::Empty, offC, offPose); // = mtx点 − 锚坐标
				}
				else
					offsetWorld = GetFLHAbsoluteCoords(CoordStruct::Empty, offC, DirStruct{}); // 官方API：无锚世界直摆
			}
			else
				offsetWorld = _randomTargetOffset; // no：LockFacing 已预转世界坐标（1123）
			smallCircleTarget.X += offsetWorld.X;
			smallCircleTarget.Y += offsetWorld.Y;
			smallCircleTarget.Z += offsetWorld.Z;
		}
		// 每帧写目标缓存（2026-09-05 死亡冻结语义）：锚活帧每帧刷新 = 最后完整目标点
		// （死亡瞬间命中路径复用的正是此值）；NoUpdate 首帧 / 无锚首帧 = 固化值
		_lockedSmallCircleTarget = smallCircleTarget;
	} // 关闭 停更缓存 的 else 块

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
			if (_motion.arcHeight != 0)
			{
				// 弧存在：直线位移走影子（公共 AdvanceArcShadow，Speed/大圆同款）——
				// 影子沿"影子自己→目标"直线推进（不读弹体实时位置），弹体位移 = 影子步长 + 弧增量。
				// 旧实现直线位移用 dirVec（弹体→目标）均分：弹体被弧抬升后 dirLen 失真且直线每帧
				// 向下拽目标方向、抵消弧（日志实证：峰被压到 1112、下降段砸穿到 Z=-822）。
				// 步长 = 影子剩余距离 / 剩余帧（保证 Duration 末恰好到位；目标移动自动校准）
				double preDist = std::sqrt(
					(double)(smallCircleTarget.X - _motion.shadowX) * (smallCircleTarget.X - _motion.shadowX)
					+ (double)(smallCircleTarget.Y - _motion.shadowY) * (smallCircleTarget.Y - _motion.shadowY)
					+ (double)(smallCircleTarget.Z - _motion.shadowZ) * (smallCircleTarget.Z - _motion.shadowZ));
				double step = preDist / (remainingFrames > 1 ? remainingFrames - 1 : 1);

				double shadowDX = 0.0, shadowDY = 0.0, shadowDZ = 0.0;
				double t = 0.0, arcDelta = 0.0;
				AdvanceArcShadow(_motion, smallCircleTarget, step,
					shadowDX, shadowDY, shadowDZ, t, arcDelta);
				resultDisp.X = static_cast<int>(shadowDX);
				resultDisp.Y = static_cast<int>(shadowDY);
				resultDisp.Z = static_cast<int>(shadowDZ);

				// 弧面旋转增量叠加
				CoordStruct arcD{
					smallCircleTarget.X - _firstFramePos.X,
					smallCircleTarget.Y - _firstFramePos.Y,
					smallCircleTarget.Z - _firstFramePos.Z };
				ArcDelta3D ad = RotateArcDelta(arcD, _motion.arcRotation, arcDelta);
				resultDisp.X += static_cast<int>(ad.x);
				resultDisp.Y += static_cast<int>(ad.y);
				resultDisp.Z += static_cast<int>(ad.z);
			}
			else
			{
				// 无弧：纯直线均分（不经过影子）
				double adjustedSpeed = dirLen / (remainingFrames > 1 ? remainingFrames - 1 : 1);
				resultDisp.X = static_cast<int>(dirVec.X / dirLen * adjustedSpeed);
				resultDisp.Y = static_cast<int>(dirVec.Y / dirLen * adjustedSpeed);
				resultDisp.Z = static_cast<int>(dirVec.Z / dirLen * adjustedSpeed);
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
			if (_motion.arcHeight != 0)
			{
				// 有弧线：直线位移走公共影子（AdvanceArcShadow，ReachTarget/大圆同款）——
				// 影子沿"影子自己→目标"3D 推进 step=min(speed,剩余)，位移=影子步长+弧增量
				double dispX = 0.0, dispY = 0.0, dispZ = 0.0;
				double t = 0.0, arcDelta = 0.0;
				AdvanceArcShadow(_motion, smallCircleTarget, speed, dispX, dispY, dispZ, t, arcDelta);
				resultDisp.X = static_cast<int>(dispX);
				resultDisp.Y = static_cast<int>(dispY);
				resultDisp.Z = static_cast<int>(dispZ);

				CoordStruct arcD{
					smallCircleTarget.X - _firstFramePos.X,
					smallCircleTarget.Y - _firstFramePos.Y,
					smallCircleTarget.Z - _firstFramePos.Z };
				ArcDelta3D ad = RotateArcDelta(arcD, _motion.arcRotation, arcDelta);
				resultDisp.X += static_cast<int>(ad.x);
				resultDisp.Y += static_cast<int>(ad.y);
				resultDisp.Z += static_cast<int>(ad.z);
			}
			else
			{
				// 无弧线：影子仅 XY 推进（shadowZ 冻结），步长钳位到剩余距离：
				// 末段 speed > 剩余距离时若仍推进满 speed 会越过目标，sdx 变号导致来回振荡（原地抽搐）
				double step = (speed < shadowDist) ? speed : shadowDist;
				double sInv = 1.0 / shadowDist;
				double shadowStepX = sdx * sInv * step;
				double shadowStepY = sdy * sInv * step;
				_motion.shadowX += shadowStepX;
				_motion.shadowY += shadowStepY;
				_motion.shadowTraveled += step;

				// 重新计算影子距离（影子已移动）
				sdx = smallCircleTarget.X - _motion.shadowX;
				sdy = smallCircleTarget.Y - _motion.shadowY;
				shadowDist = std::sqrt(sdx * sdx + sdy * sdy);

				// t = 已走路程 / 总路程（动态更新，目标移动时自动调整）
				double total = _motion.shadowTraveled + shadowDist;
				double t = (total > 1e-6) ? _motion.shadowTraveled / total : 0.0;
				if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0;

				// 实际位移：影子步长（XY）
				resultDisp.X = static_cast<int>(shadowStepX);
				resultDisp.Y = static_cast<int>(shadowStepY);

				// Z 从抛射体起始高度 lerp 到目标高度
				// shadowZ 冻结为抛射体起始 Z，_firstFramePos.Z 可能是目标 Z（Origin=Target 时不同）
				double targetZ = _motion.shadowZ + (smallCircleTarget.Z - _motion.shadowZ) * t;
				resultDisp.Z = static_cast<int>(targetZ - currentPos.Z);
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