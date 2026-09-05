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
	void LockFacing();               // _fAxisDir/_fAxisRad（F 轴基准）+ 法向量 FLH 分量初始化 + 角速度解析

	// ========================================================================
	// "解算倾斜"管线辅助（OriginFLH 偏移完整化，挂载快照/每帧刷新共用）
	// 时序统一：取基准单位 → 定坐标系(facing+tilt) → 定偏移量(FLH) → 算完整基准点。
	// 散落的取点机制（smallCircleTarget/大圆解算起始点/补读等）后续可并入本管线。
	// ========================================================================
	TechnoClass* FindOriginTechno();               // Origin=Target/Source/Launcher/Self 对应的单位（无单位返回 nullptr）
	double SampleOriginTilt(TechnoClass* pUnit);   // 单位倾斜角：动态倾斜(AngleRotatedForwards)优先，为 0 时地形采样
	// 消费坐标三轴（法向量系统一）：取锚单位姿态矩阵 / 弹体水平姿态的 F/L/H 轴世界方向（×1000 int）。
	// pAnchor=Techno → onTurret 矩阵链（含炮塔差角/坡面）；pAnchor=载体弹体（Self）→ 官方弹体水平姿态。
	// 返回 false = 无可用锚（此时调用方用世界轴固化）。
	bool GetNormalFrameAxes(ObjectClass* pAnchor, bool onTurret,
		CoordStruct& axisF, CoordStruct& axisL, CoordStruct& axisH);
		// ========================================================================
	// 解算倾斜（局部偏移 FLH → 世界坐标点）—— 归一化单一实现
	// 姿态参数包 = 一个请求的全部姿态输入；ResolveTilting 按固定优先级消费：
	//   1. worldDirect=true      → 世界直加（唯一直加来源 = OriginIsOnWorld/TargetIsOnWorld，
	//                             不由 AllowOriginTilt 决定）
	//   2. useUnitPose=true 且锚活 → 引擎单位完整姿态（onTurret=true 挂炮塔：矩阵 + TurretOffset
	//                             转轴 + 炮塔差角；false 挂车身。剥单位位移只留姿态叠 base）
	//   3. 其余                  → facing 水平朝向 + tilt 俯仰（tilt 混合 F/H 后整体引擎旋转；
	//                             tilt=0 自然退化为纯水平摆放）
	// 调用点（origin 流程/target 流程）只负责"读自己的标签 → 填包 → 调 ResolveTilting"，
	// 无任何坐标系枚举/分派层。
	// ========================================================================
	struct PoseParams
	{
		TechnoClass* anchor = nullptr;    // 引擎单位姿态的锚单位（useUnitPose 时有效）
		bool useUnitPose = false;         // true=走引擎单位完整姿态（矩阵/转轴/差角）
		bool onTurret = true;             // useUnitPose 时：true=挂炮塔（含 TurretOffset），false=挂车身
		bool worldDirect = false;         // true=纯世界直加（无视一切姿态）
		DirStruct facing{};               // 水平朝向（facing+tilt 路径）
		double tilt = 0.0;                // 俯仰角（facing+tilt 路径；0=纯水平）
	};
	CoordStruct ResolveTilting(const CoordStruct& base, const CoordStruct& flh, const PoseParams& pose);
	// OriginFLH 解算流程：读 Origin 系标签（OriginIsOnWorld/AllowOriginTilt/OriginIsOnTurret/
	// OriginIsOnVectorOrigin/CoordinateTilt）填 PoseParams → ResolveTilting。
	// 解算偏移 = OriginFLH + CircleOrigin（主圆圆心偏移，同姿态线性合并一次摆）。
	// base=Origin 单位坐标；fallbackFacing=水平兜底朝向（挂载期 _fAxisDir，每帧 fAxisDir）；
	// currentPos=弹体现在位置（连线终点）。死亡/无锚 = 停止计算：直接返回 base
	//（保持死亡帧完整解算点，调用点写回）；AllowOriginTilt 不参与死后判定。
	CoordStruct ResolveOriginTilting(const CoordStruct& base, const DirStruct& fallbackFacing,
		const CoordStruct& currentPos);

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

		// --- 圆面法线（NormalVector 系统，2026-09-05 归一化）---
		// 状态 normalX/Y/Z = 法向量 FLH 分量（相对当前坐标系三轴的分量，不是世界坐标）；
		// 旋转 = 每帧在上一帧状态上转固定 step（定速增量式，系统二）——normalSpin 累计角
		// 写法已废除（越转越快只属于将来 Normal*Lissajous，本结构暂不存其累计状态）。
		double normalRotF = 0.0;        // 圆周 Lissajous 相位累加（圆上点相位，与法向量无关）
		double normalStepF = 0.0;       // 法线每帧固定转角 step（已解析：常数/区间随机，度/帧）
		double normalStepL = 0.0;
		double normalStepH = 0.0;
		double normalLissajousF = 0.0;  // 法向量自旋 Lissajous 累计角（每帧 += NormalFLissajous，度；文档六节）
		double normalLissajousL = 0.0;
		double normalLissajousH = 0.0;
		double normalX = 0.0, normalY = 0.0, normalZ = 1.0; // 法向量 FLH 分量 (F,L,H)；默认 (0,0,1)=竖直（水平圆面）
		double normalWorldX = 0.0, normalWorldY = 0.0, normalWorldZ = 1.0; // 每帧消费合成结果（世界方向；倾斜面消费输入）
		bool normalWorldValid = false;  // 坐标系轴是否已建立：false=从未有锚（首帧以世界轴固化）；true 后锚死不再刷新轴（冻结）
		CoordStruct normalAxisF{};      // 坐标系 F 轴世界方向（×1000 int；锚活=单位姿态矩阵轴，锚死冻结/世界轴固化）
		CoordStruct normalAxisL{};      // 坐标系 L 轴
		CoordStruct normalAxisH{};      // 坐标系 H 轴
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
		double arcTotalDist = -1.0;     // 【已废弃 2026-09-05】首帧初始总距离。原 t = 1-dist/arcTotalDist 读圆心实时
		                                // 3D 距离，被弧抬升污染（同主直线旧病灶）；已改为影子推进算 t，本字段仅保留
		                                // 占位与存档兼容，逻辑不再使用（勿删，Serial Process 顺序依赖）
		CoordStruct arcStartCenter{};   // 弧线起始圆心位置（影子起点同源）

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
				.Process(this->normalLissajousF)
				.Process(this->normalLissajousL)
				.Process(this->normalLissajousH)
				.Process(this->normalX)
				.Process(this->normalY)
				.Process(this->normalZ)
				.Process(this->normalWorldX)
				.Process(this->normalWorldY)
				.Process(this->normalWorldZ)
				.Process(this->normalWorldValid)
				.Process(this->normalAxisF)
				.Process(this->normalAxisL)
				.Process(this->normalAxisH)
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
	// D = 总位移向量（smallCircleTarget - 起点），rotDeg = 弧面旋转角，arcDelta = 本帧弧高增量
	// 返回 double 分量：取整时机由调用点决定（绝对位置用法先加后取整，增量用法先取整后加）
	struct ArcDelta3D { double x, y, z; };
	static ArcDelta3D RotateArcDelta(const CoordStruct& D, double rotDeg, double arcDelta);

	// 弧影子推进（主直线 Speed / ReachTarget / 大圆 OriginSpeed 三处共用，2026-09-05 归一化）：
	// 影子 = 不受弧抬升污染的直线进度基准：沿"影子自己→target"方向推进 step（clamp 不越过），
	// 同步 shadowTraveled，更新 prevArcOffset。调用方弹体/圆心位移 = 本帧影子位移 + 弧面旋转后的弧增量。
	// 统一保证"直线位移走影子（纯直线）、弧只叠影子之上"——直线不会被弹体被弧抬高的实时位置带偏
	// （旧 ReachTarget/OriginSpeed 直位移用"弹体当前位置→目标"方向，弧把弹体抬高后直线每帧向下拽、吃掉弧）。
	// 参数：m = 影子状态（shadowX/Y/Z、shadowTraveled、prevArcOffset、arcHeight/arcPeakPercent）；
	//       target = 目标点；step = 本帧影子步长（各模式自定：Speed=速度，ReachTarget=剩余距离/剩余帧）。
	// 返回：推进后剩余影子距离；影子本帧位移三分量（double，int 化交给调用方）；进度 t；弧高增量 arcDelta。
	static double AdvanceArcShadow(MotionState& m, const CoordStruct& target, double step,
		double& dispX, double& dispY, double& dispZ, double& outT, double& arcDelta);

	// 法向量 FLH 分量增量旋转（分量空间，2026-09-05 归一化；主/大圆共用）：
	// 绕 F 分量轴 = 保持 F、L/H 在 yz 平面 2D 转；绕 L = 保持 L、F/H 在 xz 转；
	// 绕 H = 保持 H、F/L 在 xy 转。参数 = 每帧固定 step（度/帧），与坐标系解耦。
	static void RotateNormal3D(double& nx, double& ny, double& nz,
		double stepF, double stepL, double stepH);

	// 法线角速度解析：常数优先，否则区间2 50% 随机，否则区间1 随机（主/大圆共用）
	static double ResolveAngleStep(double perStep, double m1, double M1, double m2, double M2);

	// 三态跟踪：NoUpdate=yes → 冻结 last；no + 单位存活 → 每帧快照 last；死亡 → 冻结 last
	// 主 Origin（_lastPoint）与大圆解算起始点（_bigCircleStartPoint）共用
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
	CoordStruct _firstFramePos{};     // 首帧位置快照（弧线基准/Freeze 锚点）
	CoordStruct _lastPoint{};    // 主 Origin 最后有效坐标（OnStart 锁定 + 每帧跟随）。
										// OriginNoUpdate=yes：挂载复合算入 OriginFLH 后冻结（完整解算点）；
										// =no 且 OriginFLH 非空：每帧摆完写回完整解算点，参照单位死亡后
										// 刷新链停刷，此值停在死亡帧的完整解算点（死亡=停止计算基线）。
	CoordStruct _bigCircleStartPoint{};   // 大圆解算起始点最后有效坐标（OriginOriginNoUpdate 冻结用）
	CoordStruct _lockedSmallCircleTarget{};        // 目标点停更缓存（2026-09-05 语义扩展）：NoUpdate=yes 首帧锁定 / 无锚（目标死亡、打格子）首帧固化——锚活帧每帧刷新为最后完整目标点，死亡帧起命中直接复用（原定打哪还打哪）；offset 直加只走计算分支，命中路径不重复叠加
	int _vectorAcquireZ = 0;            // 【废弃 2026-09-05】获取 Vector 时的抛射体 Z（圆心高度基准旧规则已废
										// ——圆心 Z 由解算点决定；字段仅保留存档顺序兼容，勿删勿用）
	ObjectClass* _pLauncher = nullptr;  // 发射者（OnTechnoDelete 置空防悬垂）
	ObjectClass* _pSource = nullptr;    // AE 来源（同上）

	// --- 朝向（主模式参考系）---
	double _fAxisRad = 0.0;            // OnStart 锁定的朝向弧度（F 轴基准，摆放 FLH 用；与法向量解耦）
	DirStruct _fAxisDir;               // OnStart 锁定的朝向（Point2Dir 结果，Target/Source）

	// --- 目标偏移 ---
	CoordStruct _randomTargetOffset{};  // 主 TargetFLH 随机偏移（首帧解析）
	bool _targetOffsetActive = false;   // Radius/F-L/H 任一区间有效 = 本次配了偏移（消费/预转门槛，与 Normal 是否填写解耦，2026-09-05 用户拍板）
	CoordStruct _originTargetOffset{};  // 大圆 TargetFLH 随机偏移

	// --- 大圆圆心运动 ---
	CoordStruct _bigCircleOffset{};        // 圆心相对解算起始点偏移（首帧 0，每帧累加 disp）
	CoordStruct _prevBigCircleCenter{};    // 上一帧圆心位置（计算叠加位移用）
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
			.Process(this->_firstFramePos)
			.Process(this->_lastPoint)
			.Process(this->_bigCircleStartPoint)
			.Process(this->_lockedSmallCircleTarget)
			.Process(this->_vectorAcquireZ)
			.Process(this->_pLauncher)
			.Process(this->_pSource)
			.Process(this->_fAxisRad)
			.Process(this->_fAxisDir)
			.Process(this->_randomTargetOffset)
			.Process(this->_originTargetOffset)
			.Process(this->_bigCircleOffset)
			.Process(this->_prevBigCircleCenter)
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
