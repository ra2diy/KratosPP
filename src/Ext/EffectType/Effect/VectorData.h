#pragma once

#include <string>
#include <vector>
#include <cmath>

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
	bool OriginIsOnBody = false;          // yes=单位取车身PrimaryFacing，无视炮塔TurretFacing

	enum class VectorOrigin : int
	{
		Self = 0, Launcher = 1, Target = 2, Source = 3,
	};
	VectorOrigin Origin = VectorOrigin::Self;
	CoordStruct OriginFLH{};
	bool OriginNoUpdate = false;
	bool Force = true;                  // yes=SetLocation 硬控，默认所有 Vector 模式 Force
	bool Freeze = false;
	bool AllowedTilt = false;           // yes=允许坐标系倾斜
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

	// ========================================================================
	// MoveTo 模式（纯 FLH 位移 + GrowRate）
	// ========================================================================

	CoordStruct MoveTo{};
	CoordStruct GrowRate{};             // 每帧 FLH 增量（呼吸/螺旋/振幅）
	double AnglePerStep = 0.0;         // MoveTo 模式角度自增（°/step）

	// ========================================================================
	// Circle 模式（圆周，独立于 MoveTo）
	// 三选二：Radius / Speed / AnglePerFrame，未设的一项自动推算
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
	bool AllowOriginTilt = false;     // yes=圆心偏移跟随转轴倾斜
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

	// --- 圆心移动（Vector.Origin.* 系列，Circle 模式专用） ---
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
	// NormalVector
	CoordStruct OriginNormalVector{};
	CoordStruct OriginNormalRandomF{}, OriginNormalRandomL{}, OriginNormalRandomH{};
	double OriginNormalFAnglePerStep = 0.0, OriginNormalLAnglePerStep = 0.0, OriginNormalHAnglePerStep = 0.0;
	double OriginNormalFAngleRMin = 0, OriginNormalFAngleRMax = 0, OriginNormalFAngleRMin2 = 0, OriginNormalFAngleRMax2 = 0;
	double OriginNormalLAngleRMin = 0, OriginNormalLAngleRMax = 0, OriginNormalLAngleRMin2 = 0, OriginNormalLAngleRMax2 = 0;
	double OriginNormalHAngleRMin = 0, OriginNormalHAngleRMax = 0, OriginNormalHAngleRMin2 = 0, OriginNormalHAngleRMax2 = 0;
	bool OriginAllowedTilt = false;
	CoordStruct OriginCircleOffset{};     // 圆心原点偏移（世界坐标）
	bool OriginAllowOriginTilt = false;
	bool OriginLissajous = false;         // yes=独立圆面（Lissajous 模式），no=圆心位移叠加
	VectorOrigin OriginOrigin = VectorOrigin::Self; // 圆心运动参考系
	CoordStruct OriginOriginFLH{};      // OriginOrigin=FLH 时的 FLH 偏移

	// ========================================================================
	// Speed 模式（直线追踪 + 加速度）
	// ========================================================================

	CoordStruct TargetFLH{};
	int TargetOffsetFMin = 0;
	int TargetOffsetFMax = 0;
	int TargetOffsetLMin = 0;
	int TargetOffsetLMax = 0;
	int TargetOffsetHMin = 0;
	int TargetOffsetHMax = 0;

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

		std::string originStr = reader->Get(title + "Origin", std::string{ "Self" });
		if (originStr == "Launcher") Origin = VectorOrigin::Launcher;
		else if (originStr == "Target") Origin = VectorOrigin::Target;
		else if (originStr == "Source") Origin = VectorOrigin::Source;
		else Origin = VectorOrigin::Self;

		OriginFLH = reader->Get(title + "OriginFLH", OriginFLH);
		OriginNoUpdate = reader->Get(title + "OriginNoUpdate", OriginNoUpdate);
		Force = reader->Get(title + "Force", Force);
		Freeze = reader->Get(title + "Freeze", Freeze);
		AllowedTilt = reader->Get(title + "AllowedTilt", AllowedTilt);
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
		OriginTargetOffsetFMin = reader->Get(title + "Origin.TargetOffsetF.Min", OriginTargetOffsetFMin);
		OriginTargetOffsetFMax = reader->Get(title + "Origin.TargetOffsetF.Max", OriginTargetOffsetFMax);
		OriginTargetOffsetLMin = reader->Get(title + "Origin.TargetOffsetL.Min", OriginTargetOffsetLMin);
		OriginTargetOffsetLMax = reader->Get(title + "Origin.TargetOffsetL.Max", OriginTargetOffsetLMax);
		OriginTargetOffsetHMin = reader->Get(title + "Origin.TargetOffsetH.Min", OriginTargetOffsetHMin);
		OriginTargetOffsetHMax = reader->Get(title + "Origin.TargetOffsetH.Max", OriginTargetOffsetHMax);
		OriginCircleRadius = reader->Get(title + "Origin.CircleRadius", -1);
		OriginCircleSpeed = reader->Get(title + "Origin.CircleSpeed", 0);
		OriginCircleAnglePerStep = reader->Get(title + "Origin.CircleAnglePerStep", 0.0);
		OriginCircleRandomRadiusMin = reader->Get(title + "Origin.CircleRandomRadius.Min", OriginCircleRandomRadiusMin);
		OriginCircleRandomRadiusMax = reader->Get(title + "Origin.CircleRandomRadius.Max", OriginCircleRandomRadiusMax);
		OriginCircleRandomAngleMin = reader->Get(title + "Origin.CircleRandomAngle.Min", OriginCircleRandomAngleMin);
		OriginCircleRandomAngleMax = reader->Get(title + "Origin.CircleRandomAngle.Max", OriginCircleRandomAngleMax);
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
		OriginAllowedTilt = reader->Get(title + "Origin.AllowedTilt", false);
		OriginCircleOffset = reader->Get(title + "Origin.CircleOffset", OriginCircleOffset);
		OriginAllowOriginTilt = reader->Get(title + "Origin.AllowOriginTilt", false);
		OriginLissajous = reader->Get(title + "Origin.Lissajous", false);
		std::string originOriginStr = reader->Get(title + "Origin.Origin", std::string{ "Self" });
		if (originOriginStr == "Launcher") OriginOrigin = VectorOrigin::Launcher;
		else if (originOriginStr == "Target") OriginOrigin = VectorOrigin::Target;
		else if (originOriginStr == "Source") OriginOrigin = VectorOrigin::Source;
		else OriginOrigin = VectorOrigin::Self;
		OriginOriginFLH = reader->Get(title + "Origin.OriginFLH", OriginOriginFLH);

		// --- Speed / ReachTarget ---
		TargetFLH = reader->Get(title + "TargetFLH", TargetFLH);
		std::string targetOffsetFStr = reader->Get(title + "TargetOffsetF", std::string{ "" });
		std::string targetOffsetLStr = reader->Get(title + "TargetOffsetL", std::string{ "" });
		std::string targetOffsetHStr = reader->Get(title + "TargetOffsetH", std::string{ "" });
		ParseMinMax(targetOffsetFStr, TargetOffsetFMin, TargetOffsetFMax);
		ParseMinMax(targetOffsetLStr, TargetOffsetLMin, TargetOffsetLMax);
		ParseMinMax(targetOffsetHStr, TargetOffsetHMin, TargetOffsetHMax);
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
			.Process(this->TimeStep).Process(this->DisabledFrames).Process(this->SyncFacing).Process(this->OriginIsOnWorld).Process(this->OriginIsOnBody)
			.Process(this->Origin)
			.Process(this->OriginFLH)
			.Process(this->OriginNoUpdate)
			.Process(this->Force)
			.Process(this->Freeze)
			.Process(this->AllowedTilt)
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
			.Process(this->OriginAllowedTilt).Process(this->OriginCircleOffset)
			.Process(this->OriginAllowOriginTilt)

			.Process(this->TargetFLH)
			.Process(this->TargetOffsetFMin)
			.Process(this->TargetOffsetFMax)
			.Process(this->TargetOffsetLMin)
			.Process(this->TargetOffsetLMax)
			.Process(this->TargetOffsetHMin)
			.Process(this->TargetOffsetHMax)
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
			.Process(this->SpeedEndOnReach);
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
