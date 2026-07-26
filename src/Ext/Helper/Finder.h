#pragma once

#include <cmath>
#include <functional>
#include <string>
#include <vector>
#include <map>
#include <set>

#include <GeneralDefinitions.h>
#include <HouseClass.h>
#include <ObjectClass.h>
#include <BuildingClass.h>
#include <BulletClass.h>
#include <TechnoClass.h>
#include <WarheadTypeClass.h>

#include <Ext/ObjectType/FilterData.h>

// 指针解引用比较器，按对象值（UniqueID）排序，保证多人同步安全
struct DereferenceLess {
	using is_transparent = void;
	template <typename T, typename U>
	bool operator()(T&& lhs, U&& rhs) const {
		return std::less<>()(*lhs, *rhs);
	}
};

// std::set 别名，指针按解引用后的值排序
template<typename T>
using DistinctCollector = std::set<T, DereferenceLess>;

class AttachEffect;

namespace Finder
{
	/**
	 *@brief 计算两点之间的距离
	 *
	 * @param sourcePos 起始位置
	 * @param targetPos 终点位置
	 * @param fullAirspace 无视高度
	 * @return double
	 */
	double DistanceFrom(CoordStruct sourcePos, CoordStruct targetPos, bool fullAirspace);

	/**
	 *@brief 检查目标位置是否在指定位置的范围内
	 *
	 * @param location 中心位置
	 * @param targetLocation 目标位置
	 * @param maxRange 最大范围
	 * @param minRange 最小范围
	 * @param fullAirspace 全空域检查
	 * @param isInAir 目标是否在天上
	 * @param abstractType 目标类型
	 * @return true
	 * @return false
	 */
	bool InRange(CoordStruct location, CoordStruct targetLocation,
		double maxRange, double minRange, bool fullAirspace,
		bool isInAir, AbstractType abstractType);

	/**
	 *@brief 检查目标对象是否在指定位置的范围内
	 *
	 * @param pObject 目标对象
	 * @param location 中心
	 * @param maxRange 最大范围
	 * @param minRange 最小范围
	 * @param fullAirspace 全空域检查
	 * @param pHouse 比对所属阵营
	 * @param owner 自己人
	 * @param allied 友军
	 * @param enemies 敌军
	 * @param civilian 平民
	 * @return true
	 * @return false
	 */
	bool Hit(ObjectClass* pObject,
		CoordStruct location, double maxRange, double minRange, bool fullAirspace,
		HouseClass* pHouse,
		bool owner, bool allied, bool enemies, bool civilian);
}

/**
 *@brief 从列表中检索一个符合条件的对象，目标对象为ObjectClass的派生
 *
 * @tparam TBase 目标类型
 * @param array 列表
 * @param func 查找到后的回调函数
 * @param location 查找的位置
 * @param maxSpread 最大范围
 * @param minSpread 最小范围
 * @param fullAirspace 忽略高度，圆柱形判定
 * @param pHouse 比较的阵营
 * @param owner 包括自己人
 * @param allied 包括友军
 * @param enemies 包括敌军
 * @param civilian 包括平民
 */
template <typename TBase>
void FindObject(DynamicVectorClass<TBase*>* array, std::function<bool(TBase*)> func,
	CoordStruct location, double maxSpread, double minSpread = 0, bool fullAirspace = false,
	HouseClass* pHouse = nullptr,
	bool owner = true, bool allied = true, bool enemies = true, bool civilian = true)
{
	// 最大搜索范围小于0，搜索全部，等于0，搜索1格范围
	double maxRange = (maxSpread > 0 ? maxSpread : (maxSpread == 0 ? 1 : 0)) * Unsorted::LeptonsPerCell;
	double minRange = (minSpread <= 0 ? 0 : minSpread) * Unsorted::LeptonsPerCell;
	for (int i = array->Count - 1; i >= 0; i--)
	{
		TBase* pBase = array->GetItem(i);
		// 分离类型
		ObjectClass* pObject = static_cast<ObjectClass*>(pBase);
		if (pObject && Finder::Hit(pObject, location, maxRange, minRange, fullAirspace, pHouse, owner, allied, enemies, civilian))
		{
			if (func(pBase))
			{
				break;
			}
		}
	}
}

template <typename TBase>
void FindAllObject(DynamicVectorClass<TBase*>* array, std::function<bool(TBase*)> func,
	HouseClass* pHouse = nullptr,
	bool owner = true, bool allied = true, bool enemies = true, bool civilian = true)
{
	FindObject<TBase>(array, func, CoordStruct::Empty, -1, 0, false, pHouse, owner, allied, enemies, civilian);
}

/**
 *@brief 从列表中检索一个符合条件的对象，目标对象为ObjectClass的派生
 *
 * @tparam TBase 目标类型
 * @param array 列表
 * @param func 查找到后的回调函数
 * @param location 查找的位置
 * @param maxSpread 最大范围
 * @param minSpread 最小范围
 * @param fullAirspace 忽略高度，圆柱形判定
 * @param pHouse 比较的阵营
 * @param owner 包括自己人
 * @param allied 包括友军
 * @param enemies 包括敌军
 * @param civilian 包括平民
 */
template <typename TBase>
void FindObject(std::vector<TBase*> array, std::function<bool(TBase*)> func,
	CoordStruct location, double maxSpread, double minSpread = 0, bool fullAirspace = false,
	HouseClass* pHouse = nullptr,
	bool owner = true, bool allied = true, bool enemies = true, bool civilian = true)
{
	// 最大搜索范围小于0，搜索全部，等于0，搜索1格范围
	double maxRange = (maxSpread > 0 ? maxSpread : (maxSpread == 0 ? 1 : 0)) * 256;
	double minRange = (minSpread <= 0 ? 0 : minSpread) * 256;
	for (TBase* pBase : array)
	{
		// 分离类型
		ObjectClass* pObject = static_cast<ObjectClass*>(pBase);
		if (pObject && Finder::Hit(pObject, location, maxRange, minRange, fullAirspace, pHouse, owner, allied, enemies, civilian))
		{
			if (func(pBase))
			{
				break;
			}
		}
	}
}

template <typename TBase>
void FindAllObject(std::vector<TBase*> array, std::function<bool(TBase*)> func,
	HouseClass* pHouse = nullptr,
	bool owner = true, bool allied = true, bool enemies = true, bool civilian = true)
{
	FindObject<TBase>(array, func, CoordStruct::Empty, -1, 0, false, pHouse, owner, allied, enemies, civilian);
}

/**
 *@brief 随机搜索一个阵营里的单位
 *
 * @param pHouse 阵营
 * @return TechnoClass* 搜索到的单位
 */
TechnoClass* FindRandomTechno(HouseClass* pHouse);
/**
 *@brief 按照CellSpread搜索一个点附近的单位
 *
 * @param centerCell 中心的格子的坐标
 * @param location 比对的位置
 * @param spread 搜索范围
 * @param fullAirspace 圆柱形搜索
 * @param includeInAir 包含空中单位
 * @param ignoreBuildingOuter 忽略建筑的外扩格子
 * @param pHouse 所属
 * @param owner 包含自身阵营
 * @param allied 包含友军
 * @param enemies 包含敌人
 * @param civilian 包含平民
 * @return std::vector<TechnoClass*>
 */
std::vector<TechnoClass*> GetCellSpreadTechnos(CellStruct centerCell, CoordStruct location, double spread, bool fullAirspace, bool includeInAir, bool ignoreBuildingOuter,
	HouseClass* pHouse = nullptr,
	bool owner = true, bool allied = true, bool enemies = true, bool civilian = true);

/**
 *@brief 按照CellSpread搜索一个点附近的单位
 *
 * @param location 中心位置
 * @param spread 搜索范围
 * @param fullAirspace 圆柱形搜索
 * @param includeInAir 包含空中单位
 * @param ignoreBuildingOuter 忽略建筑的外扩格子
 * @param pHouse 所属
 * @param owner 包含自身阵营
 * @param allied 包含友军
 * @param enemies 包含敌人
 * @param civilian 包含平民
 * @return std::vector<TechnoClass*>
 */
std::vector<TechnoClass*> GetCellSpreadTechnos(CoordStruct location, double spread, bool fullAirspace, bool includeInAir, bool ignoreBuildingOuter,
	HouseClass* pHouse = nullptr,
	bool owner = true, bool allied = true, bool enemies = true, bool civilian = true);

void FindTechnoOnMark(std::function<bool(TechnoClass*, AttachEffect*)> func,
	CoordStruct location, double maxSpread, double minSpread, bool fullAirspace,
	HouseClass* pHouse, FilterData data, ObjectClass* exclude);

void FindBulletOnMark(std::function<bool(BulletClass*, AttachEffect*)> func,
	CoordStruct location, double maxSpread, double minSpread, bool fullAirspace,
	HouseClass* pHouse, FilterData data, ObjectClass* exclude);

/**
 *@brief 搜索爆炸位置物体并附加AE
 *
 * @param location 爆炸位置
 * @param damage 伤害
 * @param pWH 弹头
 * @param pAttacker 攻击者
 * @param pAttackingHouse 攻击者所属
 */
void FindAndAttachEffect(CoordStruct location, int damage, WarheadTypeClass* pWH, ObjectClass* pAttacker, HouseClass* pAttackingHouse);

struct DamageGroup
{
public:
	TechnoClass* pTarget;
	double Distance;
};

/**
 *@brief 查找爆炸位置的替身或者虚拟单位并对其造成伤害
 *
 * @param location 爆炸位置
 * @param damage 伤害
 * @param pWH 弹头
 * @param pAttacker 攻击者
 * @param pAttackingHouse 攻击者所属
 * @param exclude 排除
 */
void FindAndDamageStandOrVUnit(CoordStruct location, int damage,
	WarheadTypeClass* pWH, ObjectClass* pAttacker, HouseClass* pAttackingHouse, ObjectClass* exclude = nullptr);

bool CheckAndMarkTarget(TechnoClass* pTarget, double distance, CoordStruct location, int damage, ObjectClass* pAttacker,
	WarheadTypeClass* pWH, HouseClass* pAttackingHouse, DamageGroup& damageGroup);

/**
 *@brief 查找加载物一共加载了几个
 *
 * @param pUpgrade
 * @param pHouse
 * @return int
 */
int GetUpgradesAmount(BuildingTypeClass* pUpgrade, HouseClass* pHouse);
