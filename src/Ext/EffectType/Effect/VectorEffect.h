#pragma once

#include <string>
#include <vector>

#include <GeneralDefinitions.h>

#include "../EffectScript.h"
#include "VectorData.h"

class VectorEffect : public EffectScript
{
public:
	EFFECT_SCRIPT(Vector);

	virtual void Clean() override
	{
		EffectScript::Clean();

		_elapsedFrames = 0;
		_moveFrame = 0;
		_currentSpeed = 0.0;
		_effectiveTimeStep = 1;
		_initialLocation = {};
		_initialOriginPos = {};
		_arcStartLocation = {};
		_pLauncher = nullptr;
		_pSource = nullptr;
		_totalDuration = 0;
		_randomTargetOffset = {};
		_facingRad = 0.0;
		_facingDir = DirStruct(0);
		_currentAngle = 0.0;
		_tiltRad = 0.0;                  // F 轴俯仰角（AllowedTilt 用）
		_currentCircleRadius = 0.0;
		_currentCircleSpeed = 0.0;
		_currentCircleAngle = 0.0;
		_normalRotF = 0.0;
		_normalStepF = 0.0;
		_normalStepL = 0.0;
		_normalStepH = 0.0;
		_normalX = 0.0; _normalY = 0.0; _normalZ = 1.0;
		_lissajousStep = 0.0;
		_originOffset = {};
		_originElapsed = 0;
		_originSpeed = 0.0;
		_originAngle = 0.0;
		_originCircleRadius = 0.0;
		_originCircleSpeed = 0.0;
		_originCircleAngle = 0.0;
		_originFacing = 0.0;
		_originTilt = 0.0;
		_originNormalRotF = 0.0;
		_originNormalStepF = 0.0;
		_originNormalStepL = 0.0;
		_originNormalStepH = 0.0;
		_originNormalX = 0.0; _originNormalY = 0.0; _originNormalZ = 1.0;
		_originLissajousStep = 0.0;
		_originTargetOffset = {};
		_prevCircleCenter = {};
		_circlePos = {};
		_movementFrames = 0;
		_arcRotation = 0.0;
		_arcHeight = 0;
		_arcPeakPercent = 0.5;
		_shadowPosX = 0.0;
		_shadowPosY = 0.0;
		_shadowPosZ = 0.0;
		_shadowTraveled = 0.0;
		_prevArcOffset = 0.0;
		_originArcTotalDist = -1.0;
		_originPrevArcOffset = 0.0;
		_originArcHeight = 0;
		_originArcPeakPercent = 0.5;
		_originArcRotation = 0.0;
		_originArcStartCenter = {};
		_initialBaseCenter = {};
		_vectorAcquireZ = 0;
	}

	virtual void OnStart() override;

	VectorResult GetVectorResult();

	// ========================================================================
	// V2 设计文档 §5 状态变量
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
		return CalcArcOffsetAt(_arcHeight, _arcPeakPercent, t);
	}

#pragma region Save/Load
	template <typename T>
	bool Serialize(T& stream) {
		stream
			.Process(this->_elapsedFrames)
			.Process(this->_moveFrame)
			.Process(this->_currentSpeed)
			.Process(this->_effectiveTimeStep)
			.Process(this->_initialLocation)
			.Process(this->_initialOriginPos)
			.Process(this->_arcStartLocation)
			.Process(this->_pLauncher)
			.Process(this->_pSource)
			.Process(this->_totalDuration)
			.Process(this->_randomTargetOffset)
			.Process(this->_facingRad)
			.Process(this->_facingDir)
			.Process(this->_tiltRad)
			.Process(this->_currentAngle)
			.Process(this->_currentCircleRadius)
			.Process(this->_currentCircleSpeed)
			.Process(this->_currentCircleAngle)
			.Process(this->_normalRotF)
			.Process(this->_normalStepF)
			.Process(this->_normalStepL)
			.Process(this->_normalStepH)
			.Process(this->_normalX).Process(this->_normalY).Process(this->_normalZ)
			.Process(this->_lissajousStep)
			.Process(this->_originOffset)
			.Process(this->_originElapsed)
			.Process(this->_originSpeed)
			.Process(this->_originAngle)
			.Process(this->_originCircleRadius)
			.Process(this->_originCircleSpeed)
			.Process(this->_originCircleAngle)
			.Process(this->_originFacing)
			.Process(this->_originTilt)
			.Process(this->_originNormalRotF)
			.Process(this->_originNormalStepF)
			.Process(this->_originNormalStepL)
			.Process(this->_originNormalStepH)
			.Process(this->_originNormalX).Process(this->_originNormalY).Process(this->_originNormalZ)
			.Process(this->_originLissajousStep)
			.Process(this->_originTargetOffset)
			.Process(this->_prevCircleCenter)
			.Process(this->_circlePos)
			.Process(this->_movementFrames)
			.Process(this->_arcRotation)
			.Process(this->_arcHeight)
			.Process(this->_arcPeakPercent)
		.Process(this->_shadowPosX)
		.Process(this->_shadowPosY)
		.Process(this->_shadowPosZ)
		.Process(this->_shadowTraveled)
		.Process(this->_prevArcOffset)
			.Process(this->_originArcTotalDist)
			.Process(this->_originPrevArcOffset)
			.Process(this->_originArcHeight)
			.Process(this->_originArcPeakPercent)
			.Process(this->_originArcRotation)
			.Process(this->_originArcStartCenter)
			.Process(this->_initialBaseCenter)
			.Process(this->_vectorAcquireZ);
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

	int _elapsedFrames = 0;             // 已执行运动帧数
	int _moveFrame = 0;                 // 真实帧计数
	double _currentSpeed = 0.0;         // 当前速度
	int _effectiveTimeStep = 1;         // 有效 TimeStep

	CoordStruct _initialLocation{};     // 初始位置快照
	CoordStruct _initialOriginPos{};    // 初始 Origin 快照（NoUpdate 用）
	CoordStruct _arcStartLocation{};   // 弧线起点快照（DisFrames结束后抓取）
	ObjectClass* _pLauncher = nullptr;
	ObjectClass* _pSource = nullptr;

	int _totalDuration = 0;             // AE 总持续时间（ReachTarget 用）
	CoordStruct _randomTargetOffset{};  // 随机偏移
	double _facingRad = 0.0;           // OnStart 时锁定的朝向弧度（FLH 旋转用）
	DirStruct _facingDir;               // OnStart 时锁定的朝向（Point2Dir 结果，Target/Source）
	double _tiltRad = 0.0;             // F 轴俯仰角（AllowedTilt 用）
	double _currentAngle = 0.0;        // MoveTo 模式自增角度（°）
	double _currentCircleRadius = 0.0; // Circle 模式动态半径
	double _currentCircleSpeed = 0.0;  // Circle 模式动态线速度
	double _currentCircleAngle = 0.0;  // Circle 模式动态角速度
	double _normalRotF = 0.0;          // 法线绕 F 轴累计旋转（°），Lissajous 累加用
	double _normalStepF = 0.0;         // 法线每步角速度（已解析）
	double _normalStepL = 0.0;
	double _normalStepH = 0.0;
	double _normalX = 0.0, _normalY = 0.0, _normalZ = 1.0;  // 3D 法向量（世界 X/Y/Z），增量旋转维护
	double _lissajousStep = 0.0;         // Lissajous 圆周 F 偏移每步角度
	// Origin 圆心运动状态
	CoordStruct _originOffset{};            // 圆心相对基座偏移（首帧 0）
	int _originElapsed = 0;               // 圆心已执行运动帧数
	double _originSpeed = 0.0;            // 圆心当前速度
	double _originAngle = 0.0;            // 圆心 MoveTo 自旋角度
	double _originCircleRadius = 0.0;     // 圆心 Circle 动态半径
	double _originCircleSpeed = 0.0;      // 圆心 Circle 动态线速度
	double _originCircleAngle = 0.0;      // 圆心 Circle 动态角速度
	double _originFacing = 0.0;           // 圆心有效 facing
	double _originTilt = 0.0;             // 圆心有效 tilt
	double _originNormalRotF = 0.0;       // 圆心法线旋转累计（Lissajous 累加用）
	double _originNormalStepF = 0.0;      // 圆心法线每步角速度
	double _originNormalStepL = 0.0;
	double _originNormalStepH = 0.0;
	double _originNormalX = 0.0, _originNormalY = 0.0, _originNormalZ = 1.0;
	double _originLissajousStep = 0.0;
	CoordStruct _originTargetOffset{};    // 圆心 Target 随机偏移
	CoordStruct _prevCircleCenter{};      // 上一帧圆心位置（计算叠加位移用）
	CoordStruct _circlePos{};            // 圆上内部跟踪位置（增量位移，不打架 MoveTo）
	int _movementFrames = 0;              // 有效运动帧数（不含 InitialDelay/TimeStep 跳帧）
	double _arcRotation = 0.0;           // 弧面旋转角（OnStart 解析，ReachTarget / Speed）
	int _arcHeight = 0;                 // 弧高（OnStart 解析随机后写入，ReachTarget / Speed）
	double _arcPeakPercent = 0.5;        // 弧高点比率 0..1（OnStart 解析随机后写入）
	// Speed 模式影子坐标（三维，不受弧高污染，用于计算干净进度 t）
	double _shadowPosX = 0.0;
	double _shadowPosY = 0.0;
	double _shadowPosZ = 0.0;
	double _shadowTraveled = 0.0;        // 影子累计行走距离（含加速度/变速）
	double _prevArcOffset = 0.0;        // 上一帧弧高绝对值
	// Speed 模式弧高增量计算（Origin 圆心）
	double _originArcTotalDist = -1.0;  // Origin 首帧初始总距离（<0=未初始化）
	double _originPrevArcOffset = 0.0;  // Origin 上一帧弧高绝对值
	int _originArcHeight = 0;          // Origin 弧高（OnStart 解析随机后写入）
	double _originArcPeakPercent = 0.5; // Origin 弧高点比率 0..1
	double _originArcRotation = 0.0;   // Origin 弧面旋转角（OnStart 解析）
	CoordStruct _originArcStartCenter{}; // Origin 弧线起始圆心位置
	CoordStruct _initialBaseCenter{};   // Origin 基座初始快照（OriginNoUpdate 用）
	int _vectorAcquireZ = 0;            // 获取 Vector 时的抛射体 Z（Circle 圆心高度基准）
};
