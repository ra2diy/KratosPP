#pragma once

#include <set>
#include <map>
#include <vector>
#include <algorithm>

#include <GeneralStructures.h>

class TechnoClass;
class CellClass;

// 子格索引（与RA2引擎一致）
enum SubCellIndex : int
{
	SUB_CELL_CENTER = 0,
	SUB_CELL_NORTH,
	SUB_CELL_EAST,
	SUB_CELL_SOUTH,
	SUB_CELL_WEST,
	SUB_CELL_NORTHEAST,
	SUB_CELL_SOUTHEAST,
	SUB_CELL_SOUTHWEST,
	SUB_CELL_NORTHWEST,
	SUB_CELL_COUNT
};

// 一个格子里步兵最多可占的子格数（与RA2一致：一格最多3个步兵，或1个载具）
constexpr int SUB_CELL_INFANTRY_COUNT = 3;

// 格子的子格占用信息
struct SubCellOccupancy
{
	// 被占用的子格集合（0-8）
	std::set<int> occupiedSubCells;

	// 检查某个子格是否被占用
	bool IsSubCellOccupied(int subIdx) const;

	// 检查是否所有子格都被占用（步兵按3个子格计，达到3个即视为满格）
	bool IsFullyOccupied() const;

	// 获取第一个空闲子格（用于步兵落点选择，只在前3个子格中找）
	int GetFirstFreeSubCell() const;
};

// 预占用管理器，用于处理单位落地前的格子占用检查
class PreOccupancyManager
{
private:
	// 格子 -> 子格占用信息
	std::map<CellStruct, SubCellOccupancy> m_occupancy;

	// 用于快速查找：单位ID -> (占用的格子, 子格索引)；子格索引为 -1 表示整格占用（非步兵）
	std::map<void*, std::pair<CellStruct, int>> m_unitToCell;

public:
	// 清空所有预占用
	void Clear();

	// 检查一个位置是否可用（用于 canLand）
	bool IsPositionAvailable(TechnoClass* pTechno, const CellStruct& cell);

	// 检查特定子格是否可用
	bool IsSubCellAvailable(const CellStruct& cell, int subIdx);

	// 标记一个位置被预占用
	void MarkOccupied(TechnoClass* pTechno, const CoordStruct& pos);

	// 获取一个步兵可用的子格位置（配合 PickInfantrySublocation）
	bool GetAvailableSubCell(TechnoClass* pTechno, const CoordStruct& basePos, CoordStruct& outPos);

	// 释放某个单位预占用的格子（落地后调用）
	void ReleaseOccupancy(TechnoClass* pTechno);

	// 获取所有预占用格子的数量（用于调试）
	size_t GetOccupiedCellCount() const;

private:
	// 根据坐标计算子格索引
	int GetSubCellIndexFromPos(const CoordStruct& pos, CellClass* pCell);

	// 根据子格索引获取坐标偏移
	CoordStruct GetSubCellOffset(int subIdx);
};
