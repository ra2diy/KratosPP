#include "PreOccupancyManager.h"

#include <FootClass.h>
#include <MapClass.h>
#include <Unsorted.h>
#include <Utilities/Debug.h>

bool SubCellOccupancy::IsSubCellOccupied(int subIdx) const
{
	return occupiedSubCells.find(subIdx) != occupiedSubCells.end();
}

bool SubCellOccupancy::IsFullyOccupied() const
{
	// 步兵一格最多3个（SUB_CELL_INFANTRY_COUNT），达到即视为满格
	return occupiedSubCells.size() >= SUB_CELL_INFANTRY_COUNT;
}

int SubCellOccupancy::GetFirstFreeSubCell() const
{
	// 步兵只占引擎的 3 个子格位：子格 2/3/4（East/South/Southeast），
	// 掩码 0x1C = 1<<2|1<<3|1<<4，见 sub_4810A0 与 InitList_0644 的表（0x89E9F0）。
	// 以前用 {中心, 北, 东} 选子格，映射到引擎坐标后中心和北会落在同一个
	// 中心子格（bit 0x01），导致多个乘客踩在同一位置——必须用引擎的这三位。
	static const int ENGINE_INFANTRY_SLOTS[SUB_CELL_INFANTRY_COUNT] =
	{
		SUB_CELL_EAST,      // 2
		SUB_CELL_SOUTH,     // 3
		SUB_CELL_SOUTHEAST, // 4
	};
	for (int i = 0; i < SUB_CELL_INFANTRY_COUNT; i++)
	{
		int slot = ENGINE_INFANTRY_SLOTS[i];
		if (!IsSubCellOccupied(slot))
		{
			return slot;
		}
	}
	return -1;
}

void PreOccupancyManager::Clear()
{
	m_occupancy.clear();
	m_unitToCell.clear();
}

bool PreOccupancyManager::IsPositionAvailable(TechnoClass* pTechno, const CellStruct& cell)
{
	auto it = m_occupancy.find(cell);
	if (it == m_occupancy.end())
	{
		return true; // 该格无预占用
	}

	// 判断单位类型
	bool isInfantry = (pTechno->WhatAmI() == AbstractType::Infantry);

	if (!isInfantry)
	{
		// 非步兵（坦克、飞行器等）：整格占用即不可用
		return false;
	}

	// 步兵：检查是否有空闲子格
	return !it->second.IsFullyOccupied();
}

bool PreOccupancyManager::IsSubCellAvailable(const CellStruct& cell, int subIdx)
{
	auto it = m_occupancy.find(cell);
	if (it == m_occupancy.end())
	{
		return true;
	}
	return !it->second.IsSubCellOccupied(subIdx);
}

void PreOccupancyManager::MarkOccupied(TechnoClass* pTechno, const CoordStruct& pos)
{
	CellClass* pCell = MapClass::Instance->TryGetCellAt(pos);
	if (!pCell)
	{
		return;
	}

	CellStruct cell = pCell->MapCoords;
	bool isInfantry = (pTechno->WhatAmI() == AbstractType::Infantry);
	int subIdx = -1;

	if (!isInfantry)
	{
		// 非步兵：占用所有子格
		SubCellOccupancy& occ = m_occupancy[cell];
		for (int i = 0; i < SUB_CELL_COUNT; i++)
		{
			occ.occupiedSubCells.insert(i);
		}
	}
	else
	{
		// 步兵：只占用对应的子格
		subIdx = GetSubCellIndexFromPos(pos, pCell);
		if (subIdx >= 0)
		{
			m_occupancy[cell].occupiedSubCells.insert(subIdx);
		}
	}
	// 诊断日志（排查预占登记时取消注释）：
	// Debug::Log("PreOcc Mark: cell{%d,%d} sub=%d\n", cell.X, cell.Y, subIdx);

	// 记录单位与格子的映射（用于后续清理）
	m_unitToCell[pTechno] = { cell, subIdx };
}

bool PreOccupancyManager::GetAvailableSubCell(TechnoClass* pTechno, const CoordStruct& basePos, CoordStruct& outPos)
{
	CellClass* pCell = MapClass::Instance->TryGetCellAt(basePos);
	if (!pCell)
	{
		return false;
	}

	CellStruct cell = pCell->MapCoords;
	auto it = m_occupancy.find(cell);

	// 如果格子没有被预占用，使用引擎默认选择
	if (it == m_occupancy.end())
	{
		return MapClass::PickInfantrySublocation(outPos, basePos, false);
	}

	// 尝试找一个空闲子格
	int freeSubIdx = it->second.GetFirstFreeSubCell();
	if (freeSubIdx < 0)
	{
		return false;
	}

	// 将子格索引转换为坐标偏移
	CoordStruct offset = GetSubCellOffset(freeSubIdx);
	outPos = basePos;
	outPos.X += offset.X;
	outPos.Y += offset.Y;

	// 诊断日志（排查子格分配时取消注释）：
	// Debug::Log("PreOcc GetAvail: cell{%d,%d} sub=%d out{%d,%d}\n", cell.X, cell.Y, freeSubIdx, outPos.X / 256, outPos.Y / 256);
	return true;
}

void PreOccupancyManager::ReleaseOccupancy(TechnoClass* pTechno)
{
	auto it = m_unitToCell.find(pTechno);
	if (it == m_unitToCell.end())
	{
		return;
	}

	CellStruct cell = it->second.first;
	int subIdx = it->second.second;
	auto occIt = m_occupancy.find(cell);
	if (occIt != m_occupancy.end())
	{
		if (subIdx < 0)
		{
			// 非步兵：整格占用，全部释放
			occIt->second.occupiedSubCells.clear();
		}
		else
		{
			// 步兵：只释放该单位占用的子格，不影响同一格其他单位的预占用
			occIt->second.occupiedSubCells.erase(subIdx);
		}
		if (occIt->second.occupiedSubCells.empty())
		{
			m_occupancy.erase(occIt);
		}
	}

	m_unitToCell.erase(it);
}

size_t PreOccupancyManager::GetOccupiedCellCount() const
{
	return m_occupancy.size();
}

int PreOccupancyManager::GetOccupiedSubCellCount(const CellStruct& cell) const
{
	auto it = m_occupancy.find(cell);
	if (it == m_occupancy.end())
	{
		return 0;
	}
	return (int)it->second.occupiedSubCells.size();
}

int PreOccupancyManager::GetSubCellIndexFromPos(const CoordStruct& pos, CellClass* pCell)
{
	CoordStruct cellCenter = pCell->GetCoordsWithBridge();
	int dx = pos.X - cellCenter.X;
	int dy = pos.Y - cellCenter.Y;

	// 镜像引擎 sub_4810A0（坐标→子格索引）：
	//   距中心 60 leptons 以内 → 0（中心）
	//   X>0 且 Y<=0 → 2（East，格子内 (192,64)）
	//   X<=0 且 Y>0 → 3（South，格子内 (64,192)）
	//   X>0 且 Y>0  → 4（Southeast，格子内 (192,192)）
	//   其余（X<=0 且 Y<=0 的外圈）引擎也返回 0，与中心同格。
	if (dx * dx + dy * dy < 60 * 60)
	{
		return SUB_CELL_CENTER;
	}
	if (dx > 0 && dy <= 0)
	{
		return SUB_CELL_EAST;
	}
	if (dx <= 0 && dy > 0)
	{
		return SUB_CELL_SOUTH;
	}
	if (dx > 0 && dy > 0)
	{
		return SUB_CELL_SOUTHEAST;
	}
	return SUB_CELL_CENTER;
}

CoordStruct PreOccupancyManager::GetSubCellOffset(int subIdx)
{
	// 引擎子格偏移（Cell_Subcells_Infantry_Locations，InitList_0644 填充）：
	// 子格 2 = (192,64) / 3 = (64,192) / 4 = (192,192)，格子中心 128,128，
	// 即相对中心偏移 64 leptons：East(+64,-64) / South(-64,+64) / Southeast(+64,+64)。
	constexpr int ENGINE_SUBCELL_OFFSET = 64;

	CoordStruct offset = { 0, 0, 0 };

	switch (subIdx)
	{
	case SUB_CELL_CENTER:    offset = { 0, 0, 0 }; break;
	// 引擎实际使用的三个步兵位只有 2/3/4，其余子格仅保留枚举定义不再用于占位
	case SUB_CELL_EAST:      offset = { ENGINE_SUBCELL_OFFSET, -ENGINE_SUBCELL_OFFSET, 0 }; break;
	case SUB_CELL_SOUTH:     offset = { -ENGINE_SUBCELL_OFFSET, ENGINE_SUBCELL_OFFSET, 0 }; break;
	case SUB_CELL_SOUTHEAST: offset = { ENGINE_SUBCELL_OFFSET, ENGINE_SUBCELL_OFFSET, 0 }; break;
	default: break;
	}

	return offset;
}
