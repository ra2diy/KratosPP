#pragma once
// ============================================================================
// VectorEffect — 逻辑层（从零重写版）
//
// 已取消 ReVibed 后缀，重写版直接以标准文件名 VectorEffect.h 存在；
// 类名保持 VectorEffect，注册键/INI 前缀与旧版完全一致。
//
// 设计目标：
//   1. 编排层 + 子函数：OnStart/GetVectorResult 只做时序编排，计算全部拆出
//   2. 主/大圆共用 MotionState + 数据驱动 StepMotion，消灭成对状态变量
//   3. 公共数学函数：弧旋转/法向量旋转/三态跟踪/Target 链 全部归一
//   4. 行为等价铁律：数学公式逐字照搬旧版，不改任何 INI 语义
// ============================================================================

#include <map>
#include <string>
#include <vector>

#include <GeneralDefinitions.h>

#include "../EffectScript.h"
#include "VectorData.h"

class VectorEffect : public EffectScript
{
public:
	EFFECT_SCRIPT(Vector);

	// ========================================================================
	// 生命周期
	// ========================================================================

	virtual void Awake() override;    // 注册 ObjectUnInitEvent（悬垂指针防护）
	virtual void Destroy() override;  // 注销 handler
	virtual void Clean() override;    // 组件复用：重置全部状态

	virtual void OnStart() override;  // 挂载：解析 INI → 运行时参数

	// 参照 MissileHoming 先例：目标单位 UnInit 时清空指针，防止悬垂
	void OnTechnoDelete(EventSystem* sender, Event e, void* args)
	{
		if (args == _pLauncher)
			_pLauncher = nullptr;
		if (args == _pSource)
			_pSource = nullptr;
	}

	// ========================================================================
	// 目标缓存（挂 TechnoStatus，归一目标保护）：
	// Techno 获得 Vector 时写入目标单位+格子（Spawn 从 SpawnManager/Kamikaze 取，普通单位记 Target）。
	// NoUpdate=yes 存一次即停；no 每帧跟随锁定单位刷新，目标死亡冻结最后有效格子
	// ========================================================================
	void CacheTargetNow();

	// ========================================================================
	// 主入口：每帧计算位移
	// ========================================================================
	VectorResult GetVectorResult();

	// ========================================================================
	// OnStart 子步骤（.cpp 实现）
	// ========================================================================
	void ParseCommon();              // 计时/快照/Duration/AcquireZ
	void ParseTargetOffset();        // _randomTargetOffset（Radius/F/L/H 两套 + Angles）
	void ParseArcParams(bool origin); // 弧参数三件套：origin=false 主，true 大圆
	void ParseSpeed();               // 初始速度（LinearSpeed/单位 Speed/弹体 Speed/随机）
	void InitOrigin();               // _pLauncher/_pSource + CacheTargetNow + 按 Origin 锁定基线
	void LockFacing();               // _facingDir/_facingRad/_tiltRad + 法向量初始化 + 角速度解析

	// ========================================================================
	// "取基准点"管线辅助（OriginFLH 偏移完整化，挂载快照/每帧刷新共用）
	// 时序统一：取基准单位 → 定坐标系(facing+tilt) → 定偏移量(FLH) → 算完整基准点。
	// 散落的取点机制（frameTarget/大圆基座/补读等）后续可并入本管线。
	// ========================================================================
	TechnoClass* FindOriginTechno();               // Origin=Target/Source/Launcher/Self 对应的单位（无单位返回 nullptr）
	double SampleOriginTilt(TechnoClass* pUnit);   // 单位倾斜角：动态倾斜(AngleRotatedForwards)优先，为 0 时地形采样
	CoordStruct ApplyOriginFlh(const CoordStruct& basePos, const CoordStruct& flh,
		const DirStruct& facing, double tilt);
	// 把偏移量按坐标系转成世界偏移加到基准坐标 → 完整基准点。
	// 统一公式（无二维/三维分叉）：tilt 俯仰混合 F/H 后整体走引擎 API GetFLHAbsoluteCoords
	// （内含 RA2 坐标系修正 RotateZ + Y 镜像，禁止裸 cos/sin 手写——引擎弧度体系有 90° 偏置）。
	// tilt=0 时自然退化为纯水平摆放（等价二维）。

	// ========================================================================
	// 运动状态（主/大圆共用结构；VectorEffect 持两份：_motion + _originMotion）
	// ========================================================================
	enum class MotionKind : int
	{
		None = 0,
		MoveTo,        // 纯 FLH 位移 + GrowRate + AnglePerStep 自旋
		ReachTarget,   // 剩余帧数均分位移，强制到达
		Speed,         // 直线追踪 + 加速度 + 影子坐标弧高
		Circle,        // 圆周运动（三选二参数）
	};

	struct MotionState
	{
		// --- 运动进度 ---
		int elapsed = 0;                // 已执行运动帧数（主=_movementFrames 同源，大圆独立）
		double speed = 0.0;             // 当前线速度（含加速度累加）
		double angle = 0.0;             // 当前角度：MoveTo 自旋 / Circle 相位
		double circleRadius = 0.0;      // Circle 动态半径
		double circleSpeed = 0.0;       // Circle 线速度（含加速度累加）
		double circleAngle = 0.0;       // Circle 角速度（含加速度累加）

		// --- 倾斜圆面法线（NormalVector 系统）---
		double normalRotF = 0.0;        // 法线绕 F 轴累计旋转（Lissajous 累加用）
		double normalStepF = 0.0;       // 法线每步角速度（已解析：常数/区间随机）
		double normalStepL = 0.0;
		double normalStepH = 0.0;
		double normalX = 0.0, normalY = 0.0, normalZ = 1.0; // 3D 法向量（世界坐标，增量旋转维护）
		double lissajousStep = 0.0;     // 圆周 F 偏移角速度（°/step），0=不偏移

		// --- 弧线（ReachTarget/Speed）---
		double arcHeight = 0.0;         // 弧高（OnStart 解析随机后写入）
		double arcPeakPercent = 0.5;    // 弧高点比率 0..1
		double arcRotation = 0.0;       // 弧面旋转角（°），0=朝上

		// --- Speed 影子坐标（弧高进度基准，不受弧高 Z 偏移污染）---
		double shadowX = 0.0;
		double shadowY = 0.0;
		double shadowZ = 0.0;
		double shadowTraveled = 0.0;    // 影子累计行走距离（含加速度/变速）
		double prevArcOffset = 0.0;     // 上一帧弧高绝对值（增量叠加用）

		// --- 大圆 Speed 弧高专用（主模式不用）---
		double arcTotalDist = -1.0;     // 首帧初始总距离（<0=未初始化）
		CoordStruct arcStartCenter{};   // 弧线起始圆心位置

		template <typename T>
		bool Process(T& stream)
		{
			return stream
				.Process(this->elapsed)
				.Process(this->speed)
				.Process(this->angle)
				.Process(this->circleRadius)
				.Process(this->circleSpeed)
				.Process(this->circleAngle)
				.Process(this->normalRotF)
				.Process(this->normalStepF)
				.Process(this->normalStepL)
				.Process(this->normalStepH)
				.Process(this->normalX)
				.Process(this->normalY)
				.Process(this->normalZ)
				.Process(this->lissajousStep)
				.Process(this->arcHeight)
				.Process(this->arcPeakPercent)
				.Process(this->arcRotation)
				.Process(this->shadowX)
				.Process(this->shadowY)
				.Process(this->shadowZ)
				.Process(this->shadowTraveled)
				.Process(this->prevArcOffset)
				.Process(this->arcTotalDist)
				.Process(this->arcStartCenter)
				.Success();
		}
	};

	// ========================================================================
	// 公共数学函数（行为等价：逐字照搬旧版公式）
	// ========================================================================

	// 弧高二次曲线（ReachTarget / Speed 共用），t∈[0,1] 返回弧高绝对值
	static double CalcArcOffsetAt(int height, double peakPercent, double t)
	{
		if (height == 0) return 0.0;
		if (t <= peakPercent)
		{
			double u = t / peakPercent;
			return height * u * (2.0 - u);
		}
		else
		{
			double u = (peakPercent < 1.0) ? (t - peakPercent) / (1.0 - peakPercent) : 0.0;
			return height * (1.0 - u * u);
		}
	}
	double CalcArcOffsetAt(double t) const
	{
		return CalcArcOffsetAt(static_cast<int>(_motion.arcHeight), _motion.arcPeakPercent, t);
	}

	// 弧面旋转：把 arcDelta 按 Rodrigues 正交基分解到 XYZ（ReachTarget/Speed 4 处共用）
	// D = 总位移向量（frameTarget - 起点），rotDeg = 弧面旋转角，arcDelta = 本帧弧高增量
	// 返回 double 分量：取整时机由调用点决定（绝对位置用法先加后取整，增量用法先取整后加）
	struct ArcDelta3D { double x, y, z; };
	static ArcDelta3D RotateArcDelta(const CoordStruct& D, double rotDeg, double arcDelta);

	// 3D 法向量增量旋转（绕世界 F=Y / L=X / H=Z 轴，正速度=顺时针；主/大圆共用）
	static void RotateNormal3D(double& nx, double& ny, double& nz,
		double stepF, double stepL, double stepH);

	// 法线角速度解析：常数优先，否则区间2 50% 随机，否则区间1 随机（主/大圆共用）
	static double ResolveAngleStep(double perStep, double m1, double M1, double m2, double M2);

	// 三态跟踪：NoUpdate=yes → 冻结 last；no + 单位存活 → 每帧快照 last；死亡 → 冻结 last
	// 主 Origin（_initialOriginPos）与大圆基座（_initialBaseCenter）共用
	static CoordStruct TrackOriginCoord(ObjectClass* pUnit, bool noUpdate, CoordStruct& last);

	// Target 多级读取链（缓存 → 弹体 → 单位 → Kamikaze/SpawnManager），返回是否取到
	bool GetTargetPosFromChain(CoordStruct& out, bool preferCache = true);

	// ========================================================================
	// 状态变量（分组）
	// ========================================================================

	// --- 计时/帧 ---
	int _elapsedFrames = 0;             // 已执行运动帧数（含 TimeStep 跳帧）
	int _moveFrame = 0;                 // 真实帧计数
	int _movementFrames = 0;            // 有效运动帧数（不含 InitialDelay/TimeStep 跳帧）
	int _effectiveTimeStep = 1;         // 有效 TimeStep
	int _totalDuration = 0;             // AE 总持续时间（ReachTarget 用，已除 TimeStep）

	// --- 快照/引用 ---
	CoordStruct _initialLocation{};     // 首帧位置快照（弧线基准/Freeze 锚点）
	CoordStruct _initialOriginPos{};    // 主 Origin 最后有效坐标（OnStart 锁定 + 每帧跟随）
	CoordStruct _initialBaseCenter{};   // 大圆基座最后有效坐标（OriginOriginNoUpdate 冻结用）
	CoordStruct _lockedTarget{};        // Speed 模式 NoUpdate 锁定的目标点（首帧计算一次，后续帧直接复用，不反复写入新目标点）
	int _vectorAcquireZ = 0;            // 获取 Vector 时的抛射体 Z（Circle 圆心高度基准）
	ObjectClass* _pLauncher = nullptr;  // 发射者（OnTechnoDelete 置空防悬垂）
	ObjectClass* _pSource = nullptr;    // AE 来源（同上）

	// --- 朝向（主模式参考系）---
	double _facingRad = 0.0;            // OnStart 锁定的朝向弧度（FLH 旋转用）
	DirStruct _facingDir;               // OnStart 锁定的朝向（Point2Dir 结果，Target/Source）
	double _tiltRad = 0.0;              // F 轴俯仰角（AllowedTilt 用）

	// --- 大圆朝向（独立参考系）---
	double _originFacing = 0.0;         // 大圆有效 facing
	double _originTilt = 0.0;           // 大圆有效 tilt
	// 首帧锁定的基础法向量球坐标（OriginNormalVector/OriginNormalRandom/默认水平）：
	// OriginIsNormalOnOrigin=yes 时每帧以此为基础随 OriginOrigin 单位转动（保持首帧锁定值，
	// 不回写 _originFacing/_originTilt，杜绝法向量每帧自反馈累计旋转）
	double _baseOriginFacing = 0.0;
	double _baseOriginTilt = M_PI / 2.0;

	// --- 目标偏移 ---
	CoordStruct _randomTargetOffset{};  // 主 TargetFLH 随机偏移（首帧解析）
	CoordStruct _originTargetOffset{};  // 大圆 TargetFLH 随机偏移

	// --- 大圆圆心运动 ---
	CoordStruct _originOffset{};        // 圆心相对基座偏移（首帧 0，每帧累加 disp）
	CoordStruct _prevCircleCenter{};    // 上一帧圆心位置（计算叠加位移用）
	CoordStruct _circlePos{};           // 圆上内部跟踪位置（增量位移，不打架 MoveTo）

	// --- 运动状态（主 + 大圆）---
	MotionState _motion{};              // 主运动状态
	MotionState _originMotion{};        // 大圆圆心运动状态

	// ========================================================================
	// 帧工具
	// ========================================================================
	bool ShouldMoveThisFrame() const
	{
		return (_moveFrame % _effectiveTimeStep) == 0;
	}
	void AdvanceFrame()
	{
		_elapsedFrames++;
		_moveFrame++;
	}

#pragma region Save/Load
	template <typename T>
	bool Serialize(T& stream)
	{
		stream
			.Process(this->_elapsedFrames)
			.Process(this->_moveFrame)
			.Process(this->_movementFrames)
			.Process(this->_effectiveTimeStep)
			.Process(this->_totalDuration)
			.Process(this->_initialLocation)
			.Process(this->_initialOriginPos)
			.Process(this->_initialBaseCenter)
			.Process(this->_lockedTarget)
			.Process(this->_vectorAcquireZ)
			.Process(this->_pLauncher)
			.Process(this->_pSource)
			.Process(this->_facingRad)
			.Process(this->_facingDir)
			.Process(this->_tiltRad)
			.Process(this->_originFacing)
			.Process(this->_originTilt)
			.Process(this->_baseOriginFacing)
			.Process(this->_baseOriginTilt)
			.Process(this->_randomTargetOffset)
			.Process(this->_originTargetOffset)
			.Process(this->_originOffset)
			.Process(this->_prevCircleCenter)
			.Process(this->_circlePos)
			.Process(this->_motion)
			.Process(this->_originMotion);
		return stream.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		EffectScript::Load(stream, registerForChange);
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		EffectScript::Save(stream);
		return const_cast<VectorEffect*>(this)->Serialize(stream);
	}
#pragma endregion
};
