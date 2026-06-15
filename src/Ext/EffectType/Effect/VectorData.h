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

	enum class VectorOrigin : int
	{
		FLH = 0, Launcher = 1, Target = 2, Source = 3,
	};
	VectorOrigin Origin = VectorOrigin::FLH;
	CoordStruct OriginFLH{};
	bool OriginNoUpdate = false;
	bool Force = true;                  // yes=SetLocation 硬控，默认所有 Vector 模式 Force
	bool Freeze = false;

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
	int CircleRandomRadiusMin = 0;    // 初始半径随机下限
	int CircleRandomRadiusMax = 0;    // 初始半径随机上限
	double CircleRandomAngleMin = 0.0; // 初始角速度随机下限
	double CircleRandomAngleMax = 0.0; // 初始角速度随机上限
	int CircleRadiusGrow = 0;          // 半径每步增长量（lepton/step），正=外扩，负=内缩
	int CircleMaxRadius = 0;           // 半径上限，0=不限
	int CircleMinRadius = 0;           // 半径下限，0=不限
	bool CircleEndOnMaxRadius = false; // 半径达到上限时结束 AE
	bool CircleEndOnMinRadius = false; // 半径达到下限时结束 AE

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

	int InitialSpeed = -1;              // -1 = 读取单位 Speed
	int RandomSpeedMin = 0;             // Speed 模式随机速度下限
	int RandomSpeedMax = 0;             // Speed 模式随机速度上限
	int MaxSpeed = -1;                  // -1 = 不限
	int MinSpeed = -1;                  // -1 = 不限
	int Acceleration = 0;               // 每帧速度增量

	// ========================================================================
	// ReachTarget 模式（剩余帧数强制到达）
	// ========================================================================

	bool ReachTarget = false;           // 与 TargetFLH 配合使用
	int ArcHeight = 0;                  // ReachTarget 弧高（lepton），0=直线，正=上凸
	bool AllowFallingDestroy = false;   // 向量结束时摔死
	int FallingDestroyHeight = 2 * Unsorted::LevelHeight;   // 摔死高度

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

		std::string originStr = reader->Get(title + "Origin", std::string{ "FLH" });
		if (originStr == "Launcher") Origin = VectorOrigin::Launcher;
		else if (originStr == "Target") Origin = VectorOrigin::Target;
		else if (originStr == "Source") Origin = VectorOrigin::Source;
		else Origin = VectorOrigin::FLH;

		OriginFLH = reader->Get(title + "OriginFLH", OriginFLH);
		OriginNoUpdate = reader->Get(title + "OriginNoUpdate", OriginNoUpdate);
		Force = reader->Get(title + "Force", Force);
		Freeze = reader->Get(title + "Freeze", Freeze);

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
		CircleMaxAngle = reader->Get(title + "CircleMaxAngle", 0.0);
		CircleMinAngle = reader->Get(title + "CircleMinAngle", 0.0);
		CircleRadiusGrow = reader->Get(title + "CircleRadiusGrow", 0);
		CircleMaxRadius = reader->Get(title + "CircleMaxRadius", 0);
		CircleMinRadius = reader->Get(title + "CircleMinRadius", 0);
		CircleEndOnMaxRadius = reader->Get(title + "CircleEndOnMaxRadius", false);
		CircleEndOnMinRadius = reader->Get(title + "CircleEndOnMinRadius", false);

		// --- Speed / ReachTarget ---
		TargetFLH = reader->Get(title + "TargetFLH", TargetFLH);
		std::string targetOffsetFStr = reader->Get(title + "TargetOffsetF", std::string{ "" });
		std::string targetOffsetLStr = reader->Get(title + "TargetOffsetL", std::string{ "" });
		std::string targetOffsetHStr = reader->Get(title + "TargetOffsetH", std::string{ "" });
		ParseMinMax(targetOffsetFStr, TargetOffsetFMin, TargetOffsetFMax);
		ParseMinMax(targetOffsetLStr, TargetOffsetLMin, TargetOffsetLMax);
		ParseMinMax(targetOffsetHStr, TargetOffsetHMin, TargetOffsetHMax);
		ReachTarget = reader->Get(title + "ReachTarget", ReachTarget);
		ArcHeight = reader->Get(title + "ArcHeight", 0);
		AllowFallingDestroy = reader->Get(title + "AllowFallingDestroy", AllowFallingDestroy);
		FallingDestroyHeight = reader->Get(title + "FallingDestroyHeight", FallingDestroyHeight);

		// --- 速度 ---
		InitialSpeed = reader->Get(title + "InitialSpeed", -1);
		std::string randomSpeedStr = reader->Get(title + "RandomSpeed", std::string{ "" });
		ParseMinMax(randomSpeedStr, RandomSpeedMin, RandomSpeedMax);
		MaxSpeed = reader->Get(title + "MaxSpeed", -1);
		MinSpeed = reader->Get(title + "MinSpeed", -1);
		Acceleration = reader->Get(title + "Acceleration", Acceleration);

		Enable = !MoveTo.IsEmpty() || !TargetFLH.IsEmpty() || Force || Freeze
			|| (CircleRadius > 0) || (CircleSpeed > 0) || (CircleAnglePerStep > 0.0)
			|| (CircleRandomRadiusMax > CircleRandomRadiusMin)
			|| (CircleRandomAngleMax > CircleRandomAngleMin);
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
			.Process(this->TimeStep)
			.Process(this->Origin)
			.Process(this->OriginFLH)
			.Process(this->OriginNoUpdate)
			.Process(this->Force)
			.Process(this->Freeze)

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
			.Process(this->CircleRandomRadiusMin)
			.Process(this->CircleRandomRadiusMax)
			.Process(this->CircleRandomAngleMin)
			.Process(this->CircleRandomAngleMax)
			.Process(this->CircleRadiusGrow)
			.Process(this->CircleMaxRadius)
			.Process(this->CircleMinRadius)
			.Process(this->CircleEndOnMaxRadius)
			.Process(this->CircleEndOnMinRadius)

			.Process(this->TargetFLH)
			.Process(this->TargetOffsetFMin)
			.Process(this->TargetOffsetFMax)
			.Process(this->TargetOffsetLMin)
			.Process(this->TargetOffsetLMax)
			.Process(this->TargetOffsetHMin)
			.Process(this->TargetOffsetHMax)
			.Process(this->ReachTarget)
			.Process(this->ArcHeight)
			.Process(this->AllowFallingDestroy)
			.Process(this->FallingDestroyHeight)

			.Process(this->InitialSpeed)
			.Process(this->RandomSpeedMin)
			.Process(this->RandomSpeedMax)
			.Process(this->MaxSpeed)
			.Process(this->MinSpeed)
			.Process(this->Acceleration);
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
