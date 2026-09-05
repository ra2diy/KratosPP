#pragma once
// ============================================================================
// VectorData — 数据层（从零重写版）
//
// 已取消 ReVibed 后缀，重写版直接以标准文件名 VectorData.h 存在；
// 类名保持 VectorData，注册键/INI 前缀完全一致（Vector.*）。
//
// 行为等价铁律：本文件所有 INI 标签名、默认值、解析语义与旧版逐字一致，
// 不得借重写之机改动任何标签行为。Vector.xlsx 作为标签核对清单。
// ============================================================================

#include <string>
#include <vector>
#include <cmath>
#include <sstream>

#include <GeneralStructures.h>

#include <Ext/EffectType/Effect/EffectData.h>
#include <Ext/Helper/CastEx.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class VectorData : public EffectData
{
public:
	EFFECT_DATA(Vector);

	// ========================================================================
	// 通用设定
	// ========================================================================

	int TimeStep = 1;
	int DisabledFrames = 0;              // 首帧快照后冻结 N 帧，不计入运动时间
	bool SyncFacing = false;             // yes=抛射体朝运动方向/单位转动，no=抛射体朝目标
	bool OriginIsOnWorld = false;        // yes=OriginFLH用世界FLH(朝北)，不使用单位/弹体朝向
	bool OriginIsOnBody = false;         // yes=单位取车身PrimaryFacing，无视炮塔TurretFacing
	bool SubjectToCliffs = false;        // Vector 接管期是否受悬崖/撞地影响（与原版弹体标签同义；默认 no=接管期无视，免疫引爆）

	enum class VectorOrigin : int
	{
		Self = 0, Launcher = 1, Target = 2, Source = 3,
	};
	VectorOrigin Origin = VectorOrigin::Self;
	CoordStruct OriginFLH{};
	bool OriginNoUpdate = false;
	bool Force = true;                  // yes=SetLocation 硬控，默认所有 Vector 模式 Force
	bool Freeze = false;
	// 圆面倾斜唯一来源 = 法向量体系（NormalVector + IsNormalOnOrigin 随单位转）——
	// 原 AllowCircleTilt 已删除（语义被 IsNormalOnOrigin=no 世界固定取代，连线高低角倾斜作废）
	bool IsOnOrigin = false;             // （INI: Vector.OriginIsOnVectorOrigin）FLH 参考系（F 轴）来源：yes=Origin 单位自身朝向，no=Origin→弹体连线
										 // 默认按 Origin 类型推导（Launcher/Self→yes，Target/Source→no），与旧版行为一致
	bool IsNormalOnOrigin = true;        // 圆面法向量：yes（默认）=每帧跟随 Origin 单位自身朝向转动，no=世界固定
	bool CoordinateTilt = false;         // 连线坐标系（IsOnVectorOrigin=no 的 Target/Launcher/Source 通吃）F 轴是否取真实 3D 连线
										 // （含 Origin→抛射体 高低差）；no（默认）=F 轴水平投影，与地面平行。只影响坐标系摆放，不碰圆面法向量。

	// ========================================================================
	// NormalVector 圆面法线
	// ========================================================================

	CoordStruct NormalVector{};          // 圆面法向量（FLH 坐标系），F/L/H
	CoordStruct NormalRandomF{};         // F 分量随机范围 .X=Min .Y=Max
	CoordStruct NormalRandomL{};         // L 分量随机范围
	CoordStruct NormalRandomH{};         // H 分量随机范围
	double NormalFAnglePerStep = 0.0;   // 绕 F 轴角速度 (°/step)，常数模式
	double NormalLAnglePerStep = 0.0;   // 绕 L 轴
	double NormalHAnglePerStep = 0.0;   // 绕 H 轴
	double NormalFAngleRMin = 0.0, NormalFAngleRMax = 0.0;     // 绕 F 角速度区间1
	double NormalFAngleRMin2 = 0.0, NormalFAngleRMax2 = 0.0;  // 绕 F 角速度区间2
	double NormalLAngleRMin = 0.0, NormalLAngleRMax = 0.0;
	double NormalLAngleRMin2 = 0.0, NormalLAngleRMax2 = 0.0;
	double NormalHAngleRMin = 0.0, NormalHAngleRMax = 0.0;
	double NormalHAngleRMin2 = 0.0, NormalHAngleRMax2 = 0.0;
	double Lissajous = 0.0;            // 小圆圆周 F 轴偏移角速度（°/step），0=不偏移

	// ========================================================================
	// MoveTo 模式（纯 FLH 位移 + GrowRate）
	// ========================================================================

	CoordStruct MoveTo{};
	CoordStruct GrowRate{};             // 每帧 FLH 增量（呼吸/螺旋/振幅）
	double AnglePerStep = 0.0;         // MoveTo 模式角度自增（°/step）

	// ========================================================================
	// Circle 模式（圆周，独立于 MoveTo）
	// 三选二：Radius / Speed / AnglePerStep，未设的一项自动推算
	// 圆心 = Origin（同 Origin 标签，动态刷新除非 NoUpdate）
	// ========================================================================

	int CircleRadius = -1;              // 圆半径（lepton），-1=自动取当前XY距离
	int CircleSpeed = 0;               // 线速度（lepton/step沿圆周），0=不由它推算
	int CircleSpeedAcceleration = 0;   // 线速度每步加速度
	int CircleMaxSpeed = 0;            // 线速度上限，0=不限
	int CircleMinSpeed = 0;            // 线速度下限，0=不限
	double CircleAnglePerStep = 0.0;  // 角速度（°/step），0=不由它推算
	double CircleAngleAcceleration = 0.0; // 角速度每步加速度
	double CircleMaxAngle = 0.0;     // 角速度上限，0=不限
	double CircleMinAngle = 0.0;     // 角速度下限，0=不限
	CoordStruct CircleOrigin{};       // 圆心偏移（默认世界坐标，AllowOriginTilt=yes 时 FLH 旋转）
	bool AllowOriginTilt = true;      // yes=圆心偏移跟随转轴倾斜
	int CircleRandomRadiusMin = 0;    // 初始半径随机下限
	int CircleRandomRadiusMax = 0;    // 初始半径随机上限
	double CircleRandomAngleMin = 0.0; // 初始角速度随机下限
	double CircleRandomAngleMax = 0.0; // 初始角速度随机上限
	double CircleRandomAngleMin2 = 0.0; // 第二区间下限（4 值模式）
	double CircleRandomAngleMax2 = 0.0; // 第二区间上限（4 值模式）
	int CircleRadiusGrow = 0;          // 半径每步增长量（lepton/step），正=外扩，负=内缩
	int CircleMaxRadius = 0;           // 半径上限，0=不限
	int CircleMinRadius = 0;           // 半径下限，0=不限
	bool CircleEndOnMaxRadius = false; // 半径达到上限时结束 AE
	bool CircleEndOnMinRadius = false; // 半径达到下限时结束 AE

	// ========================================================================
	// 圆心运动（Vector.Origin.* 系列，Circle 模式专用）
	// ========================================================================

	// MoveTo 模式
	CoordStruct OriginMoveTo{};           // 圆心 FLH 位移（lepton/step）
	CoordStruct OriginGrowRate{};         // 每步增量
	double OriginAnglePerStep = 0.0;    // 自旋角度（°/step）
	// Speed / ReachTarget 模式
	CoordStruct OriginTargetFLH{};        // 追踪目标 FLH
	int OriginLinearSpeed = -1;          // 初始速度，-1=读取单位Speed
	int OriginAcceleration = 0;           // 加速度（lepton/step²）
	int OriginMaxSpeed = -1;              // 最大速度，-1=不限
	int OriginMinSpeed = -1;              // 最小速度，-1=不限
	bool OriginReachTarget = false;       // 到达模式
	bool OriginSpeedEndOnReach = true;    // Speed模式抵达目标即结束AE
	int OriginArcHeight = 0;            // 到达模式弧高
	double OriginArcPeakPercent = 50.0; // 弧高点百分比（0-100），默认50=中点
	Point2D OriginArcPeakRandomPercent{ 0, 0 };// 随机弧高点百分比
	int OriginArcRandomHeightMin = 0;   // 随机弧高下限
	int OriginArcRandomHeightMax = 0;   // 随机弧高上限
	double OriginArcRotation = 0.0;     // 弧面旋转角（°），0=朝上
	double OriginArcRandomRotationMin = 0.0;// 随机旋转下限
	double OriginArcRandomRotationMax = 0.0;// 随机旋转上限
	int OriginTargetOffsetFMin = 0, OriginTargetOffsetFMax = 0;
	int OriginTargetOffsetLMin = 0, OriginTargetOffsetLMax = 0;
	int OriginTargetOffsetHMin = 0, OriginTargetOffsetHMax = 0;
	// Circle 模式
	int OriginCircleRadius = -1;
	int OriginCircleSpeed = 0;
	double OriginCircleAnglePerStep = 0.0;
	int OriginCircleRandomRadiusMin = 0, OriginCircleRandomRadiusMax = 0;
	double OriginCircleRandomAngleMin = 0, OriginCircleRandomAngleMax = 0;
	int OriginCircleRadiusGrow = 0;
	int OriginCircleMaxRadius = 0, OriginCircleMinRadius = 0;
	bool OriginCircleEndOnMaxRadius = false, OriginCircleEndOnMinRadius = false;
	// 法线
	CoordStruct OriginNormalVector{};
	CoordStruct OriginNormalRandomF{}, OriginNormalRandomL{}, OriginNormalRandomH{};
	double OriginNormalFAnglePerStep = 0.0, OriginNormalLAnglePerStep = 0.0, OriginNormalHAnglePerStep = 0.0;
	double OriginNormalFAngleRMin = 0, OriginNormalFAngleRMax = 0, OriginNormalFAngleRMin2 = 0, OriginNormalFAngleRMax2 = 0;
	double OriginNormalLAngleRMin = 0, OriginNormalLAngleRMax = 0, OriginNormalLAngleRMin2 = 0, OriginNormalLAngleRMax2 = 0;
	double OriginNormalHAngleRMin = 0, OriginNormalHAngleRMax = 0, OriginNormalHAngleRMin2 = 0, OriginNormalHAngleRMax2 = 0;
	// 圆心通用
	// 大圆面倾斜唯一来源 = 大圆法向量体系（OriginNormalVector + OriginIsNormalOnOrigin）——
	// 原 OriginAllowCircleTilt（跟随目标 Z 差）已删除
	bool OriginIsNormalOnOrigin = true;   // 大圆法向量：yes（默认）=每帧跟随 OriginOrigin 单位自身朝向转动，no=世界固定
	CoordStruct OriginCircleOffset{};     // 圆心原点偏移（世界坐标）
	bool OriginAllowOriginTilt = true;
	bool OriginOriginNoUpdate = false;   // yes=解算起始点冻结在初始位置，不随目标移动
	double OriginLissajous = 0.0;        // 大圆圆周 F 轴偏移角速度（°/step），0=不偏移
	VectorOrigin OriginOrigin = VectorOrigin::Self; // 圆心运动参考系
	CoordStruct OriginOriginFLH{};      // OriginOrigin=FLH 时的 FLH 偏移
	bool OriginOriginIsOnBody = false;  // OriginOriginFLH 挂点坐标系：yes=挂 OriginOrigin 单位车身（PrimaryFacing），no=挂炮塔（TurretFacing，默认）

	// ========================================================================
	// Speed 模式（直线追踪 + 加速度）
	// ========================================================================

	CoordStruct TargetFLH{};
	// TargetFLH 坐标系标签（只作用于 Speed/ReachTarget 直线模式的目标点取值）：
	// 单位自身坐标系（OriginIsOnVectorOrigin=yes，Target/Source/Launcher 为活单位）时——
	//   TargetIsOnTurret：yes（默认）=挂点对齐炮塔（炮塔转轴+随炮塔转），no=对齐车身
	//   TargetSameTilt：yes（默认）=挂点随单位倾斜（坡上车身斜则挂点斜），no=抛弃倾斜按水平基准
	// 连线坐标系（OriginIsOnVectorOrigin=no）下两者不适用（无单位坐标系概念），3D 归 CoordinateTilt
	// TargetIsOnWorld：yes=TargetFLH 当纯世界偏移（无视单位朝向/姿态）；默认 no
	bool TargetIsOnTurret = true;
	bool TargetSameTilt = true;
	bool TargetIsOnWorld = false;
	int TargetOffsetFMin = 0;
	int TargetOffsetFMax = 0;
	int TargetOffsetLMin = 0;
	int TargetOffsetLMax = 0;
	int TargetOffsetHMin = 0;
	int TargetOffsetHMax = 0;
	// 四参数版（TargetOffsetFRanges）：区间1复用上方 Min/Max，区间2独立存储，双区间50%取一
	int TargetOffsetFMin2 = 0;
	int TargetOffsetFMax2 = 0;
	int TargetOffsetLMin2 = 0;
	int TargetOffsetLMax2 = 0;
	int TargetOffsetHMin2 = 0;
	int TargetOffsetHMax2 = 0;
	// 半径模式（TargetOffsetRadius）：与 F/L/H 互斥，全向随机落点
	int TargetOffsetRadiusMin = 0;
	int TargetOffsetRadiusMax = 0;
	int TargetOffsetRadiusMin2 = 0;  // 四参数版（TargetOffsetRadiusRanges）区间2，区间1复用 Min/Max
	int TargetOffsetRadiusMax2 = 0;
	bool TargetOffsetSphere = false;   // yes=球面全向（含H），no=XY圆环+H用TargetOffsetH
	CoordStruct TargetOffsetNormal{};  // 圆环法向量（FLH），非空时 TargetOffsetSphere=no 的落点在倾斜圆面上（法向量定义圆面）
	// 角度限制（TargetOffsetAngles，仅圆环模式）：双区间，0度=目标点指向抛射体（近交点）
	int TargetOffsetAngleMin = 0;
	int TargetOffsetAngleMax = 0;
	int TargetOffsetAngleMin2 = 0;
	int TargetOffsetAngleMax2 = 0;

	int LinearSpeed = -1;              // -1 = 读取单位 Speed
	int RandomSpeedMin = 0;             // Speed 模式随机速度下限
	int RandomSpeedMax = 0;             // Speed 模式随机速度上限
	int MaxSpeed = -1;                  // -1 = 不限
	int MinSpeed = -1;                  // -1 = 不限
	int Acceleration = 0;               // 每帧速度增量
	bool SpeedEndOnReach = true;        // 抵达目标坐标点即强制结束AE（修复飞越后抽搐）

	// ========================================================================
	// ReachTarget 模式（剩余帧数强制到达）
	// ========================================================================

	bool ReachTarget = false;           // 与 TargetFLH 配合使用
	int ReachTargetEarlyEnd = 0;        // 提前结束 AE 的帧数，0=禁用，>0 时提前 N 帧交还引擎
	int ArcHeight = 0;                  // ReachTarget 弧高（lepton），0=直线，正=上凸
	double ArcPeakPercent = 50.0;       // 弧高点所在 Duration 百分比（0-100），默认50=中点
	Point2D ArcPeakRandomPercent{ 0, 0 };// 随机弧高百分比范围 (Min, Max)
	int ArcRandomHeightMin = 0;         // 随机弧高下限
	int ArcRandomHeightMax = 0;         // 随机弧高上限
	double ArcRotation = 0.0;           // 弧面旋转角（°），0=默认朝上，顺时针
	double ArcRandomRotationMin = 0.0;  // 随机旋转下限
	double ArcRandomRotationMax = 0.0;  // 随机旋转上限
	bool AllowFallingDestroy = false;   // 向量结束时摔死
	int FallingDestroyHeight = Unsorted::LevelHeight;   // 摔死高度

	// ========================================================================
	// 内部
	// ========================================================================

	VectorData() : EffectData()
	{
		AffectBuilding = false;
	}

	virtual void Read(INIBufferReader* reader) override
	{
		Read(reader, "Vector.");
	}

	virtual void Read(INIBufferReader* reader, std::string title) override
	{
		EffectData::Read(reader, title);

		// --- 通用 ---
		TimeStep = reader->Get(title + "TimeStep", 1);
		if (TimeStep < 1) TimeStep = 1;
		DisabledFrames = reader->Get(title + "DisabledFrames", 0);
		SyncFacing = reader->Get(title + "SyncFacing", SyncFacing);
		OriginIsOnWorld = reader->Get(title + "OriginIsOnWorld", OriginIsOnWorld);
		OriginIsOnBody = reader->Get(title + "OriginIsOnBody", OriginIsOnBody);
		SubjectToCliffs = reader->Get(title + "SubjectToCliffs", SubjectToCliffs);

		std::string originStr = reader->Get(title + "Origin", std::string{ "Self" });
		if (originStr == "Launcher") Origin = VectorOrigin::Launcher;
		else if (originStr == "Target") Origin = VectorOrigin::Target;
		else if (originStr == "Source") Origin = VectorOrigin::Source;
		else Origin = VectorOrigin::Self;

		OriginFLH = reader->Get(title + "OriginFLH", OriginFLH);
		OriginNoUpdate = reader->Get(title + "OriginNoUpdate", OriginNoUpdate);
		Force = reader->Get(title + "Force", Force);
		Freeze = reader->Get(title + "Freeze", Freeze);
		// 默认按 Origin 类型推导旧版行为：Launcher/Self=单位自身朝向(yes)，Target/Source=连线(no)
		IsOnOrigin = reader->Get(title + "OriginIsOnVectorOrigin", Origin == VectorOrigin::Launcher || Origin == VectorOrigin::Self);
		IsNormalOnOrigin = reader->Get(title + "IsNormalOnOrigin", true); // 默认跟随 Origin 单位，no 才世界固定
		CoordinateTilt = reader->Get(title + "CoordinateTilt", false);    // 连线坐标系 3D：默认 no=水平，显式 yes 才取真实连线
		NormalVector = reader->Get(title + "NormalVector", NormalVector);
		NormalRandomF = reader->Get(title + "NormalRandomF", NormalRandomF);
		NormalRandomL = reader->Get(title + "NormalRandomL", NormalRandomL);
		NormalRandomH = reader->Get(title + "NormalRandomH", NormalRandomH);
		NormalFAnglePerStep = reader->Get(title + "NormalFAnglePerStep", 0.0);
		NormalLAnglePerStep = reader->Get(title + "NormalLAnglePerStep", 0.0);
		NormalHAnglePerStep = reader->Get(title + "NormalHAnglePerStep", 0.0);
		{
			auto parse4 = [&](const char* key, double& m1, double& M1, double& m2, double& M2) {
				auto s = reader->Get(title + key, std::string{ "" });
				if (s.empty()) return;
				std::vector<double> v;
				std::stringstream ss(s);
				std::string t;
				while (std::getline(ss, t, ',')) v.push_back(std::stod(t));
				if (v.size() >= 4) { m1 = v[0]; M1 = v[1]; m2 = v[2]; M2 = v[3]; }
			};
			parse4("NormalFAngleRanges", NormalFAngleRMin, NormalFAngleRMax, NormalFAngleRMin2, NormalFAngleRMax2);
			parse4("NormalLAngleRanges", NormalLAngleRMin, NormalLAngleRMax, NormalLAngleRMin2, NormalLAngleRMax2);
			parse4("NormalHAngleRanges", NormalHAngleRMin, NormalHAngleRMax, NormalHAngleRMin2, NormalHAngleRMax2);
		}
		Lissajous = reader->Get(title + "Lissajous", 0.0);

		// --- MoveTo ---
		MoveTo = reader->Get(title + "MoveTo", MoveTo);
		GrowRate = reader->Get(title + "GrowRate", GrowRate);
		AnglePerStep = reader->Get(title + "AnglePerStep", 0.0);

		// --- Circle ---
		CircleRadius = reader->Get(title + "CircleRadius", -1);
		CircleSpeed = reader->Get(title + "CircleSpeed", 0);
		CircleSpeedAcceleration = reader->Get(title + "CircleSpeedAcceleration", 0);
		CircleMaxSpeed = reader->Get(title + "CircleMaxSpeed", 0);
		CircleMinSpeed = reader->Get(title + "CircleMinSpeed", 0);
		CircleAnglePerStep = reader->Get(title + "CircleAnglePerStep", 0.0);
		CircleAngleAcceleration = reader->Get(title + "CircleAngleAcceleration", 0.0);
		std::string circleRandomRadiusStr = reader->Get(title + "CircleRandomRadius", std::string{ "" });
		ParseMinMax(circleRandomRadiusStr, CircleRandomRadiusMin, CircleRandomRadiusMax);
		std::string circleRandomAngleStr = reader->Get(title + "CircleRandomAngle", std::string{ "" });
		if (!circleRandomAngleStr.empty())
		{
			auto comma = circleRandomAngleStr.find(',');
			if (comma != std::string::npos)
			{
				CircleRandomAngleMin = std::stod(circleRandomAngleStr.substr(0, comma));
				CircleRandomAngleMax = std::stod(circleRandomAngleStr.substr(comma + 1));
			}
		}
		std::string circleRandomAngleRangesStr = reader->Get(title + "CircleRandomAngleRanges", std::string{ "" });
		if (!circleRandomAngleRangesStr.empty())
		{
			std::vector<double> angles;
			std::stringstream ss(circleRandomAngleRangesStr);
			std::string token;
			while (std::getline(ss, token, ','))
				angles.push_back(std::stod(token));
			if (angles.size() >= 4)
			{
				CircleRandomAngleMin = angles[0];
				CircleRandomAngleMax = angles[1];
				CircleRandomAngleMin2 = angles[2];
				CircleRandomAngleMax2 = angles[3];
			}
		}
		CircleMaxAngle = reader->Get(title + "CircleMaxAngle", 0.0);
		CircleMinAngle = reader->Get(title + "CircleMinAngle", 0.0);
		CircleOrigin = reader->Get(title + "CircleOrigin", CircleOrigin);
		AllowOriginTilt = reader->Get(title + "AllowOriginTilt", AllowOriginTilt);
		CircleRadiusGrow = reader->Get(title + "CircleRadiusGrow", 0);
		CircleMaxRadius = reader->Get(title + "CircleMaxRadius", 0);
		CircleMinRadius = reader->Get(title + "CircleMinRadius", 0);
		CircleEndOnMaxRadius = reader->Get(title + "CircleEndOnMaxRadius", false);
		CircleEndOnMinRadius = reader->Get(title + "CircleEndOnMinRadius", false);

		// --- Origin ---
		OriginMoveTo = reader->Get(title + "Origin.MoveTo", OriginMoveTo);
		OriginGrowRate = reader->Get(title + "Origin.GrowRate", OriginGrowRate);
		OriginAnglePerStep = reader->Get(title + "Origin.AnglePerStep", 0.0);
		OriginTargetFLH = reader->Get(title + "Origin.TargetFLH", OriginTargetFLH);
		OriginLinearSpeed = reader->Get(title + "Origin.LinearSpeed", -1);
		OriginAcceleration = reader->Get(title + "Origin.Acceleration", 0);
		OriginMaxSpeed = reader->Get(title + "Origin.MaxSpeed", -1);
		OriginMinSpeed = reader->Get(title + "Origin.MinSpeed", -1);
		OriginReachTarget = reader->Get(title + "Origin.ReachTarget", false);
		OriginSpeedEndOnReach = reader->Get(title + "Origin.SpeedEndOnReach", OriginSpeedEndOnReach);
		OriginArcHeight = reader->Get(title + "Origin.ArcHeight", 0);
		OriginArcPeakPercent = reader->Get(title + "Origin.ArcPeakPercent", OriginArcPeakPercent);
		OriginArcPeakRandomPercent = reader->Get(title + "Origin.RandomArcPeakPercent", OriginArcPeakRandomPercent);
		std::string originArcRandomHeightStr = reader->Get(title + "Origin.RandomArcHeight", std::string{ "" });
		ParseMinMax(originArcRandomHeightStr, OriginArcRandomHeightMin, OriginArcRandomHeightMax);
		OriginArcRotation = reader->Get(title + "Origin.ArcRotation", 0.0);
		std::string originArcRandomRotationStr = reader->Get(title + "Origin.RandomArcRotation", std::string{ "" });
		ParseMinMaxDouble(originArcRandomRotationStr, OriginArcRandomRotationMin, OriginArcRandomRotationMax);
		std::string originTargetOffsetFStr = reader->Get(title + "Origin.TargetOffsetF", std::string{""});
		ParseMinMax(originTargetOffsetFStr, OriginTargetOffsetFMin, OriginTargetOffsetFMax);
		std::string originTargetOffsetLStr = reader->Get(title + "Origin.TargetOffsetL", std::string{""});
		ParseMinMax(originTargetOffsetLStr, OriginTargetOffsetLMin, OriginTargetOffsetLMax);
		std::string originTargetOffsetHStr = reader->Get(title + "Origin.TargetOffsetH", std::string{""});
		ParseMinMax(originTargetOffsetHStr, OriginTargetOffsetHMin, OriginTargetOffsetHMax);
		OriginCircleRadius = reader->Get(title + "Origin.CircleRadius", -1);
		OriginCircleSpeed = reader->Get(title + "Origin.CircleSpeed", 0);
		OriginCircleAnglePerStep = reader->Get(title + "Origin.CircleAnglePerStep", 0.0);
		std::string originCircleRandomRadiusStr = reader->Get(title + "Origin.CircleRandomRadius", std::string{""});
		ParseMinMax(originCircleRandomRadiusStr, OriginCircleRandomRadiusMin, OriginCircleRandomRadiusMax);
		std::string originCircleRandomAngleStr = reader->Get(title + "Origin.CircleRandomAngle", std::string{""});
		ParseMinMaxDouble(originCircleRandomAngleStr, OriginCircleRandomAngleMin, OriginCircleRandomAngleMax);
		OriginCircleRadiusGrow = reader->Get(title + "Origin.CircleRadiusGrow", 0);
		OriginCircleMaxRadius = reader->Get(title + "Origin.CircleMaxRadius", 0);
		OriginCircleMinRadius = reader->Get(title + "Origin.CircleMinRadius", 0);
		OriginCircleEndOnMaxRadius = reader->Get(title + "Origin.CircleEndOnMaxRadius", false);
		OriginCircleEndOnMinRadius = reader->Get(title + "Origin.CircleEndOnMinRadius", false);
		OriginNormalVector = reader->Get(title + "Origin.NormalVector", OriginNormalVector);
		OriginNormalRandomF = reader->Get(title + "Origin.NormalRandomF", OriginNormalRandomF);
		OriginNormalRandomL = reader->Get(title + "Origin.NormalRandomL", OriginNormalRandomL);
		OriginNormalRandomH = reader->Get(title + "Origin.NormalRandomH", OriginNormalRandomH);
		OriginNormalFAnglePerStep = reader->Get(title + "Origin.NormalFAnglePerStep", 0.0);
		OriginNormalLAnglePerStep = reader->Get(title + "Origin.NormalLAnglePerStep", 0.0);
		OriginNormalHAnglePerStep = reader->Get(title + "Origin.NormalHAnglePerStep", 0.0);
		OriginIsNormalOnOrigin = reader->Get(title + "Origin.IsNormalOnOrigin", true); // 默认跟随 OriginOrigin 单位，no 才世界固定
		OriginCircleOffset = reader->Get(title + "Origin.CircleOrigin", OriginCircleOffset);
		OriginAllowOriginTilt = reader->Get(title + "Origin.AllowOriginTilt", OriginAllowOriginTilt);
		OriginOriginNoUpdate = reader->Get(title + "Origin.OriginNoUpdate", false);
		OriginLissajous = reader->Get(title + "Origin.Lissajous", 0.0);
		std::string originOriginStr = reader->Get(title + "Origin.Origin", std::string{ "Self" });
		if (originOriginStr == "Launcher") OriginOrigin = VectorOrigin::Launcher;
		else if (originOriginStr == "Target") OriginOrigin = VectorOrigin::Target;
		else if (originOriginStr == "Source") OriginOrigin = VectorOrigin::Source;
		else OriginOrigin = VectorOrigin::Self;
		OriginOriginFLH = reader->Get(title + "Origin.OriginFLH", OriginOriginFLH);
		OriginOriginIsOnBody = reader->Get(title + "Origin.OriginIsOnBody", false); // 默认挂炮塔（no），行为与旧实现一致

		// --- Speed / ReachTarget ---
		TargetFLH = reader->Get(title + "TargetFLH", TargetFLH);
		TargetIsOnTurret = reader->Get(title + "TargetIsOnTurret", true); // 单位坐标系下默认对齐炮塔
		TargetSameTilt = reader->Get(title + "TargetSameTilt", true);    // 默认随单位倾斜（成熟算法）
		TargetIsOnWorld = reader->Get(title + "TargetIsOnWorld", false); // 纯世界偏移需显式
		std::string targetOffsetFStr = reader->Get(title + "TargetOffsetF", std::string{ "" });
		std::string targetOffsetLStr = reader->Get(title + "TargetOffsetL", std::string{ "" });
		std::string targetOffsetHStr = reader->Get(title + "TargetOffsetH", std::string{ "" });
		ParseMinMax(targetOffsetFStr, TargetOffsetFMin, TargetOffsetFMax);
		ParseMinMax(targetOffsetLStr, TargetOffsetLMin, TargetOffsetLMax);
		ParseMinMax(targetOffsetHStr, TargetOffsetHMin, TargetOffsetHMax);
		{
			// 四参数版：min1,max1,min2,max2。前两位覆盖区间1（Min/Max），后两位写区间2（Min2/Max2）
			// 双区间全无效时保留两参数值（回退语义）
			auto parse4i = [&](const char* key, int& m1, int& M1, int& m2, int& M2) {
				auto s = reader->Get(title + key, std::string{ "" });
				if (s.empty()) return;
				std::vector<int> v;
				std::stringstream ss(s);
				std::string t;
				while (std::getline(ss, t, ',')) v.push_back(std::stoi(t));
				if (v.size() >= 4 && (v[0] < v[1] || v[2] < v[3]))
				{
					m1 = v[0]; M1 = v[1];
					m2 = v[2]; M2 = v[3];
				}
			};
			parse4i("TargetOffsetFRanges", TargetOffsetFMin, TargetOffsetFMax, TargetOffsetFMin2, TargetOffsetFMax2);
			parse4i("TargetOffsetLRanges", TargetOffsetLMin, TargetOffsetLMax, TargetOffsetLMin2, TargetOffsetLMax2);
			parse4i("TargetOffsetHRanges", TargetOffsetHMin, TargetOffsetHMax, TargetOffsetHMin2, TargetOffsetHMax2);
			parse4i("TargetOffsetAngles", TargetOffsetAngleMin, TargetOffsetAngleMax, TargetOffsetAngleMin2, TargetOffsetAngleMax2);
			parse4i("TargetOffsetRadiusRanges", TargetOffsetRadiusMin, TargetOffsetRadiusMax, TargetOffsetRadiusMin2, TargetOffsetRadiusMax2);
		}
		std::string targetOffsetRadiusStr = reader->Get(title + "TargetOffsetRadius", std::string{ "" });
		ParseMinMax(targetOffsetRadiusStr, TargetOffsetRadiusMin, TargetOffsetRadiusMax);
		TargetOffsetSphere = reader->Get(title + "TargetOffsetSphere", TargetOffsetSphere);
		TargetOffsetNormal = reader->Get(title + "TargetOffsetNormal", TargetOffsetNormal);
		ReachTarget = reader->Get(title + "ReachTarget", ReachTarget);
		ReachTargetEarlyEnd = reader->Get(title + "ReachTargetEarlyEnd", ReachTargetEarlyEnd);
		ArcHeight = reader->Get(title + "ArcHeight", 0);
		ArcPeakPercent = reader->Get(title + "ArcPeakPercent", ArcPeakPercent);
		ArcPeakRandomPercent = reader->Get(title + "RandomArcPeakPercent", ArcPeakRandomPercent);
		std::string arcRandomHeightStr = reader->Get(title + "RandomArcHeight", std::string{ "" });
		ParseMinMax(arcRandomHeightStr, ArcRandomHeightMin, ArcRandomHeightMax);
		ArcRotation = reader->Get(title + "ArcRotation", 0.0);
		std::string arcRandomRotationStr = reader->Get(title + "RandomArcRotation", std::string{ "" });
		ParseMinMaxDouble(arcRandomRotationStr, ArcRandomRotationMin, ArcRandomRotationMax);
		AllowFallingDestroy = reader->Get(title + "AllowFallingDestroy", AllowFallingDestroy);
		FallingDestroyHeight = reader->Get(title + "FallingDestroyHeight", FallingDestroyHeight);

		// --- 速度 ---
		LinearSpeed = reader->Get(title + "LinearSpeed", -1);
		std::string randomSpeedStr = reader->Get(title + "RandomSpeed", std::string{ "" });
		ParseMinMax(randomSpeedStr, RandomSpeedMin, RandomSpeedMax);
		MaxSpeed = reader->Get(title + "MaxSpeed", -1);
		MinSpeed = reader->Get(title + "MinSpeed", -1);
		Acceleration = reader->Get(title + "Acceleration", Acceleration);
		SpeedEndOnReach = reader->Get(title + "SpeedEndOnReach", SpeedEndOnReach);

		Enable = !MoveTo.IsEmpty() || Freeze || ReachTarget
			|| (LinearSpeed >= 0)
			|| (CircleRadius > 0) || (CircleSpeed != 0) || (CircleAnglePerStep > 0.0)
			|| (CircleRandomRadiusMax > CircleRandomRadiusMin)
			|| (CircleRandomAngleMax > CircleRandomAngleMin)
			|| (CircleRandomAngleMax2 > CircleRandomAngleMin2);
	}

	static void ParseMinMaxDouble(const std::string& str, double& min, double& max)
	{
		if (str.empty()) return;
		size_t commaPos = str.find(',');
		if (commaPos != std::string::npos)
		{
			min = std::stod(str.substr(0, commaPos));
			max = std::stod(str.substr(commaPos + 1));
		}
		else
		{
			min = std::stod(str);
			max = min;
		}
	}

private:
	static void ParseMinMax(const std::string& str, int& min, int& max)
	{
		if (str.empty()) return;
		size_t commaPos = str.find(',');
		if (commaPos != std::string::npos)
		{
			min = std::stoi(str.substr(0, commaPos));
			max = std::stoi(str.substr(commaPos + 1));
		}
		else
		{
			min = std::stoi(str);
			max = min;
		}
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		stream
			.Process(this->TimeStep).Process(this->DisabledFrames).Process(this->SyncFacing).Process(this->OriginIsOnWorld).Process(this->OriginIsOnBody).Process(this->SubjectToCliffs)
			.Process(this->Origin)
			.Process(this->OriginFLH)
			.Process(this->OriginNoUpdate)
			.Process(this->Force)
			.Process(this->Freeze)
			.Process(this->IsOnOrigin)
			.Process(this->IsNormalOnOrigin)
			.Process(this->CoordinateTilt)
			.Process(this->NormalVector)
			.Process(this->NormalRandomF)
			.Process(this->NormalRandomL)
			.Process(this->NormalRandomH)
			.Process(this->NormalFAnglePerStep)
			.Process(this->NormalLAnglePerStep)
			.Process(this->NormalHAnglePerStep)
			.Process(this->NormalFAngleRMin).Process(this->NormalFAngleRMax)
			.Process(this->NormalFAngleRMin2).Process(this->NormalFAngleRMax2)
			.Process(this->NormalLAngleRMin).Process(this->NormalLAngleRMax)
			.Process(this->NormalLAngleRMin2).Process(this->NormalLAngleRMax2)
			.Process(this->NormalHAngleRMin).Process(this->NormalHAngleRMax)
			.Process(this->NormalHAngleRMin2).Process(this->NormalHAngleRMax2)
			.Process(this->Lissajous)

			.Process(this->MoveTo)
			.Process(this->GrowRate)
			.Process(this->AnglePerStep)
			.Process(this->CircleRadius)
			.Process(this->CircleSpeed)
			.Process(this->CircleSpeedAcceleration)
			.Process(this->CircleMaxSpeed)
			.Process(this->CircleMinSpeed)
			.Process(this->CircleAnglePerStep)
			.Process(this->CircleAngleAcceleration)
			.Process(this->CircleMaxAngle)
			.Process(this->CircleMinAngle)
			.Process(this->CircleOrigin)
			.Process(this->AllowOriginTilt)
			.Process(this->CircleRandomRadiusMin)
			.Process(this->CircleRandomRadiusMax)
			.Process(this->CircleRandomAngleMin)
			.Process(this->CircleRandomAngleMax)
			.Process(this->CircleRandomAngleMin2)
			.Process(this->CircleRandomAngleMax2)
			.Process(this->CircleRadiusGrow)
			.Process(this->CircleMaxRadius)
			.Process(this->CircleMinRadius)
			.Process(this->CircleEndOnMaxRadius)
			.Process(this->CircleEndOnMinRadius)
			.Process(this->OriginMoveTo).Process(this->OriginGrowRate)
			.Process(this->OriginAnglePerStep).Process(this->OriginTargetFLH)
			.Process(this->OriginLinearSpeed).Process(this->OriginReachTarget)
			.Process(this->OriginSpeedEndOnReach)
			.Process(this->OriginArcHeight)
			.Process(this->OriginArcPeakPercent)
			.Process(this->OriginArcPeakRandomPercent)
			.Process(this->OriginArcRandomHeightMin)
			.Process(this->OriginArcRandomHeightMax)
			.Process(this->OriginArcRotation)
			.Process(this->OriginArcRandomRotationMin)
			.Process(this->OriginArcRandomRotationMax)
			.Process(this->OriginTargetOffsetFMin).Process(this->OriginTargetOffsetFMax)
			.Process(this->OriginTargetOffsetLMin).Process(this->OriginTargetOffsetLMax)
			.Process(this->OriginTargetOffsetHMin).Process(this->OriginTargetOffsetHMax)
			.Process(this->OriginCircleRadius).Process(this->OriginCircleSpeed)
			.Process(this->OriginCircleAnglePerStep)
			.Process(this->OriginCircleRandomRadiusMin).Process(this->OriginCircleRandomRadiusMax)
			.Process(this->OriginCircleRandomAngleMin).Process(this->OriginCircleRandomAngleMax)
			.Process(this->OriginCircleRadiusGrow).Process(this->OriginCircleMaxRadius)
			.Process(this->OriginCircleMinRadius)
			.Process(this->OriginCircleEndOnMaxRadius).Process(this->OriginCircleEndOnMinRadius)
			.Process(this->OriginNormalVector)
			.Process(this->OriginNormalRandomF).Process(this->OriginNormalRandomL)
			.Process(this->OriginNormalRandomH)
			.Process(this->OriginNormalFAnglePerStep).Process(this->OriginNormalLAnglePerStep)
			.Process(this->OriginNormalHAnglePerStep)
			.Process(this->OriginNormalFAngleRMin).Process(this->OriginNormalFAngleRMax)
			.Process(this->OriginNormalFAngleRMin2).Process(this->OriginNormalFAngleRMax2)
			.Process(this->OriginNormalLAngleRMin).Process(this->OriginNormalLAngleRMax)
			.Process(this->OriginNormalLAngleRMin2).Process(this->OriginNormalLAngleRMax2)
			.Process(this->OriginNormalHAngleRMin).Process(this->OriginNormalHAngleRMax)
			.Process(this->OriginNormalHAngleRMin2).Process(this->OriginNormalHAngleRMax2)
			.Process(this->OriginIsNormalOnOrigin).Process(this->OriginCircleOffset)
			.Process(this->OriginAllowOriginTilt).Process(this->OriginOriginNoUpdate)
			.Process(this->OriginLissajous)
			.Process(this->OriginOrigin)
			.Process(this->OriginOriginFLH)

			.Process(this->TargetFLH)
			.Process(this->TargetIsOnTurret).Process(this->TargetSameTilt).Process(this->TargetIsOnWorld)
			.Process(this->TargetOffsetFMin)
			.Process(this->TargetOffsetFMax)
			.Process(this->TargetOffsetLMin)
			.Process(this->TargetOffsetLMax)
			.Process(this->TargetOffsetHMin)
			.Process(this->TargetOffsetHMax)
			.Process(this->TargetOffsetFMin2)
			.Process(this->TargetOffsetFMax2)
			.Process(this->TargetOffsetLMin2)
			.Process(this->TargetOffsetLMax2)
			.Process(this->TargetOffsetHMin2)
			.Process(this->TargetOffsetHMax2)
			.Process(this->TargetOffsetRadiusMin)
			.Process(this->TargetOffsetRadiusMax)
			.Process(this->TargetOffsetRadiusMin2)
			.Process(this->TargetOffsetRadiusMax2)
			.Process(this->TargetOffsetSphere)
			.Process(this->TargetOffsetNormal)
			.Process(this->TargetOffsetAngleMin)
			.Process(this->TargetOffsetAngleMax)
			.Process(this->TargetOffsetAngleMin2)
			.Process(this->TargetOffsetAngleMax2)
			.Process(this->ReachTarget)
			.Process(this->ReachTargetEarlyEnd)
			.Process(this->ArcHeight)
			.Process(this->ArcPeakPercent)
			.Process(this->ArcPeakRandomPercent)
			.Process(this->ArcRandomHeightMin)
			.Process(this->ArcRandomHeightMax)
			.Process(this->ArcRotation)
			.Process(this->ArcRandomRotationMin)
			.Process(this->ArcRandomRotationMax)
			.Process(this->AllowFallingDestroy)
			.Process(this->FallingDestroyHeight)

			.Process(this->LinearSpeed)
			.Process(this->RandomSpeedMin)
			.Process(this->RandomSpeedMax)
			.Process(this->MaxSpeed)
			.Process(this->MinSpeed)
			.Process(this->Acceleration)
			.Process(this->SpeedEndOnReach)
			.Process(this->OriginOriginIsOnBody); // 2026-09-05 新增（追加尾部保证旧存档顺序兼容）
		return stream.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		EffectData::Load(stream, registerForChange);
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		EffectData::Save(stream);
		return const_cast<VectorData*>(this)->Serialize(stream);
	}
#pragma endregion
};
