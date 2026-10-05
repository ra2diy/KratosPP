#pragma once

#include <cmath>
#include <string>

#include <GeneralDefinitions.h>
#include <Matrix3D.h>
#include <ObjectClass.h>
#include <BulletClass.h>
#include <TechnoClass.h>

#include <Ext/EffectType/Effect/OffsetData.h>

class LocationMark
{
public:
	CoordStruct Location = CoordStruct::Empty;
	DirStruct Dir{};

	bool IsEmpty()
	{
		return Location.IsEmpty();
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->Location)
			.Process(this->Dir)
			.Success();
	};

	bool Load(ExStreamReader& stream, bool registerForChange)
	{
		return this->Serialize(stream);
	}
	bool Save(ExStreamWriter& stream) const
	{
		return const_cast<LocationMark*>(this)->Serialize(stream);
	}
#pragma endregion
};

// ----------------
// symmetric round up
// Bias: away from zero
template <typename FloatType>
FloatType ceil0(const FloatType& value)
{
	FloatType result = std::ceil(std::fabs(value));
	return (value < 0.0) ? -result : result;
}

// ----------------
// FLHHelper
// ----------------
#pragma region FLH
CoordStruct GetFLH(CoordStruct source, CoordStruct flh, DirStruct dir, bool flip = false);

// Step 1: get body transform matrix
Matrix3D GetMatrix3D(TechnoClass* pTechno, VoxelIndexKey* pKey = nullptr, bool isShadow = false);

// Step 2: move to turretOffset
Matrix3D MoveMatrix3DOnTurret(Matrix3D& mtx, CoordStruct turretOffset, double factor = 1.0);

// Step 4: apply FLH offset
Vector3D<float> GetFLHOffset(Matrix3D& mtx, CoordStruct flh);

CoordStruct GetFLHAbsoluteOffset(CoordStruct flh, DirStruct dir, CoordStruct turretOffset = CoordStruct::Empty);

CoordStruct GetFLHAbsoluteCoords(CoordStruct source, CoordStruct flh, DirStruct dir, CoordStruct turretOffset = CoordStruct::Empty);

// reversed from 6F3D60
CoordStruct GetFLHAbsoluteCoords(TechnoClass* pTechno, CoordStruct flh, bool isOnTurret, int flipY = 1, CoordStruct turretOffset = CoordStruct::Empty);

CoordStruct GetFLHAbsoluteCoords(BulletClass* pBullet, CoordStruct flh, int flipY = 1);

CoordStruct GetFLHAbsoluteCoords(ObjectClass* pObject, CoordStruct flh, bool isOnTurret = true, int flipY = 1, CoordStruct turretOffset = CoordStruct::Empty);
#pragma endregion

#pragma region RelativeLocation
DirStruct GetRelativeDir(TechnoClass* pMaster, int dir, bool isOnTurret);

/// @param predictBulletNextFrame 抛射体宿主是否按"下一帧位置"外推（原渲染期行为）。
///        替身定位路径（火车记录点 / 每帧替身定位）必须传 false —— 它们现在跑在逻辑期，
///        要的是**当帧**实际坐标，否则替身永远比弹体领先一帧。
LocationMark GetRelativeLocation(ObjectClass* pOwner, OffsetData data, CoordStruct offset = CoordStruct::Empty, bool predictBulletNextFrame = true);
DirStruct GetRelativeDirection(ObjectClass* pOwner, int dir = 0, bool isOnTurret = false, bool isOnWorld = false);
#pragma endregion

#pragma region ForwardFLH
/// @brief 获取向量上指定距离的坐标
/// @param sourceV 起始位置
/// @param targetV 结束位置
/// @param speed 速度
/// @param dist 距离
/// @return CoordStruct
CoordStruct GetForwardCoords(Vector3D<double> sourceV, Vector3D<double> targetV, double speed, double dist = 0);

CoordStruct GetForwardCoords(CoordStruct sourcePos, CoordStruct targetPos, double speed, double dist = 0);
#pragma endregion

#pragma region Random offset
CoordStruct GetRandomOffset(int min, int max);
/// @brief 最小1格范围内的随机坐标偏移
/// @param maxSpread 最大半径
/// @param minSpread 最小半径
/// @return offset
CoordStruct GetRandomOffset(double maxSpread, double minSpread = 0);
#pragma endregion

#pragma region Dir
const double BINARY_ANGLE_MAGIC = -(360.0 / (65535 - 1)) * (Math::Pi / 180);

/// @brief 获得一个指定朝向的DirStruct
/// @param index 序号
/// @param facing 面数
/// @return dir
DirStruct DirNormalized(int index, int facing);

/// @brief 8的倍数分割面向，顺时针，0为↗，Dir的0方向
/// @param dir 朝向
/// @param facing 面数
/// @return index
int Dir2FacingIndex(DirStruct dir, int facing);

/// @brief 0的方向是游戏中的北方，是↗，SHP素材0帧是朝向0点，是↑
/// @param dir 朝向
/// @param facing 面数
/// @return index
int Dir2FrameIndex(DirStruct dir, int facing);

DirType ToDirType(DirStruct dir);

DirStruct Radians2Dir(double radians);
DirStruct Point2Dir(CoordStruct source, CoordStruct target);

/// @brief 计算targetDir相对于bodyDir之间的夹角，以360度划分圆，以bodyDir为0点，顺时针旋转，targetDir在哪一度。
/// @param bodyDirIndex
/// @param targetDirIndex
/// @return angle
int IncludedAngle360(int bodyDirIndex, int targetDirIndex);

/// @brief 目标在死区范围内更靠近哪一边
/// @param targetBodyDelta 
/// @param min 
/// @param max 
/// @return 
int GetTurnAngle(int targetBodyDelta, int min, int max);
int GetTurnAngle(int targetBodyDelta, Point2D angleZone);
#pragma endregion

