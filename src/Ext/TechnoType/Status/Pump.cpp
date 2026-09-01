#include "../TechnoStatus.h"

#include <JumpjetLocomotionClass.h>

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
			// 获取最近的可着陆位置（强制地面搜索：乘客的 IsInAir 可能被误置，
			// 若走空中早退会直接返回目标格，导致落在建筑上；
			// 不应用路径优先，让落点围绕目标形成包围圈）
			CoordStruct landingPos;
			if (!TryGetLandingPoint(pPassenger, targetPos, landingPos, true, &TechnoExt::HumanConnonPreOcc))
			{
				landingPos = targetPos;
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

