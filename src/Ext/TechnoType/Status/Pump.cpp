#include "../TechnoStatus.h"

#include <JumpjetLocomotionClass.h>

#include <Utilities/Debug.h>
#include <Ext/Helper/Physics.h>
#include <Ext/Helper/Weapon.h>
#include <Ext/Helper/Scripts.h>

#include <Extension/WarheadTypeExt.h>

bool TechnoStatus::PumpAction(CoordStruct targetPos, bool isLobber, Sequence flySequence)
{
	if (!IsBuilding() && !pTechno->IsFallingDown && !AmIStand())
	{
		return Pump->Jump(targetPos, isLobber, flySequence);
	}
	return false;
}

void TechnoStatus::HumanCannon(CoordStruct sourcePos, CoordStruct targetPos, int height, bool isLobber, Sequence flySequence)
{
	if (pTechno->Passengers.NumPassengers > 0)
	{
		// 人间大炮一级准备
		FootClass* pPassenger = pTechno->Passengers.RemoveFirstPassenger();
		pPassenger->Transporter = nullptr;
		DirStruct facing = pTechno->GetRealFacing().Current();
		++Unsorted::IKnowWhatImDoing;
		pPassenger->Unlimbo(sourcePos, ToDirType(facing));
		--Unsorted::IKnowWhatImDoing;
		// 人间大炮二级准备
		if (TechnoStatus* status = GetStatus<TechnoExt, TechnoStatus>(abstract_cast<TechnoClass*, true>(pPassenger)))
		{
			// 诊断日志（排查落点选择时取消注释）：
			// Debug::Log("HumanCannon: passenger=%08X src{%d,%d} tgt{%d,%d}\n",
			// 	(unsigned)pPassenger, sourcePos.X / 256, sourcePos.Y / 256, targetPos.X / 256, targetPos.Y / 256);
			// 获取最近的可着陆位置（强制地面搜索：乘客的 IsInAir 可能被误置，
			// 若走空中早退会直接返回目标格，导致落在建筑上；
			// 不应用路径优先，让落点围绕目标形成包围圈：
			// 路径优先会沿"车→目标"连线从目标往车方向找第一个可落脚点，
			// 目标格被占满后所有乘客都会散落在车前的连线上；关掉后改为
			// 以目标为中心逐圈扩散、以连线为轴向两侧分布）
			CoordStruct landingPos;
			bool ok = TryGetLandingPoint(pPassenger, targetPos, landingPos, true, false, &TechnoExt::HumanConnonPreOcc);
			// 诊断日志（排查落点选择时取消注释）：
			// Debug::Log("HumanCannon: land{%d,%d} ok=%d\n", landingPos.X / 256, landingPos.Y / 256, ok);
			if (!ok)
			{
				// 兜底：不能无脑用原始目标点——目标点可能在建筑中心
				// （之前会导致乘客全部堆在建筑格上，撞建筑后高空摔死）。
				// 目标格本身可通行才用它，否则落回发射点附近保命。
				CellClass* pFallbackCell = MapClass::Instance->TryGetCellAt(targetPos);
				TechnoTypeClass* pType = pPassenger->GetTechnoType();
				// 诊断日志（排查兜底落点时取消注释）：
				// Debug::Log("HumanCannon: fallback building=%d clear=%d\n",
				// 	pFallbackCell && pFallbackCell->GetBuilding() != nullptr,
				// 	pFallbackCell && pFallbackCell->IsClearToMove(pType->SpeedType, pType->MovementZone, true, true));
				if (pFallbackCell
					&& !pFallbackCell->GetBuilding()
					&& pFallbackCell->IsClearToMove(pType->SpeedType, pType->MovementZone, true, true))
				{
					landingPos = targetPos;
				}
				else
				{
					landingPos = sourcePos;
				}
			}
			// 预占用：无论是否精确找到落点，都把选中的落点登记给预占管理器，
			// 后续乘客的落点搜索会避开它，实现分散投送（包围圈）
			TechnoExt::HumanConnonPreOcc.MarkOccupied(pPassenger, landingPos);
			// 人间大炮发射
			landingPos += CoordStruct{ 0, 0, height };
			// 提前占据落点在 PumpState::Jump 内完成（会记录占位，死亡/结束时释放）
			status->Pump->Jump(landingPos, isLobber, flySequence, true);
		}
	}
}
