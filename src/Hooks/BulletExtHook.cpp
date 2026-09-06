#include <exception>
#include <Windows.h>

#include <GeneralDefinitions.h>
#include <SpecificStructures.h>
#include <AnimClass.h>
#include <AnimTypeClass.h>

#include <Extension.h>
#include <Utilities/Macro.h>

#include <Extension/AnimExt.h>
#include <Extension/BulletExt.h>

#include <Ext/Helper/Scripts.h>

#include <Ext/BulletType/BulletStatus.h>
#include <Ext/Common/CommonStatus.h>
#include <Ext/Common/ExpandAnimsManager.h>


// ----------------
// Extension
// ----------------


DEFINE_HOOK(0x4664BA, BulletClass_CTOR, 0x5)
{
	// skip this Allocate just left BulletClass_Load_Suffix => LoadKey to Allocate
	// when is loading a save game.
	if (!Common::IsLoadGame)
	{
		GET(BulletClass*, pItem, ESI);

		BulletExt::ExtMap.TryAllocate(pItem);
	}
	return 0;
}

DEFINE_HOOK(0x4665E9, BulletClass_DTOR, 0xA)
{
	GET(BulletClass*, pItem, ECX);
	if (BulletExt::ExtData* ext = BulletExt::ExtMap.Find(pItem))
	{
		ext->SetExtStatus(nullptr);
		ext->_GameObject->Foreach([](Component* c)
			{if (auto cc = dynamic_cast<IBulletScript*>(c)) { cc->OnUnInit(); } });
	}
	BulletExt::ExtMap.Remove(pItem);

	return 0;
}

DEFINE_HOOK_AGAIN(0x46AFB0, BulletClass_SaveLoad_Prefix, 0x8)
DEFINE_HOOK(0x46AE70, BulletClass_SaveLoad_Prefix, 0x5)
{
	GET_STACK(BulletClass*, pItem, 0x4);
	GET_STACK(IStream*, pStm, 0x8);

	BulletExt::ExtMap.PrepareStream(pItem, pStm);

	return 0;
}

DEFINE_HOOK_AGAIN(0x46AF97, BulletClass_Load_Suffix, 0x7)
DEFINE_HOOK(0x46AF9E, BulletClass_Load_Suffix, 0x7)
{
	BulletExt::ExtMap.LoadStatic();

	return 0;
}

DEFINE_HOOK(0x46AFC4, BulletClass_Save_Suffix, 0x5)
{
	BulletExt::ExtMap.SaveStatic();

	return 0;
}

// ----------------
// Component
// ----------------

DEFINE_HOOK(0x466556, BulletClass_Init, 0x6)
{
	GET(BulletClass*, pThis, ECX);

	if (auto pExt = BulletExt::ExtMap.Find(pThis))
	{
		pExt->_GameObject->Foreach([](Component* c)
			{if (auto cc = dynamic_cast<IBulletScript*>(c)) { cc->OnInit(); } });
	}

	return 0;
}

DEFINE_HOOK(0x468B5D, BulletClass_Put, 0x6)
{
	GET(BulletClass*, pThis, EBX);
	GET_STACK(CoordStruct*, pCoord, -0x20);
	DirType faceDir = DirType::North;

	if (auto pExt = BulletExt::ExtMap.Find(pThis))
	{
		pExt->_GameObject->Foreach([pCoord, faceDir](Component* c)
			{ if (auto cc = dynamic_cast<IBulletScript*>(c)) { cc->OnPut(pCoord, faceDir); } });
	}

	return 0;
}

DEFINE_HOOK(0x4666F7, BulletClass_Update, 0x6)
{
	GET(BulletClass*, pThis, EBP);

	if (auto pExt = BulletExt::ExtMap.Find(pThis))
	{
		pExt->_GameObject->Foreach([](Component* c)
			{ c->OnUpdate(); });
	}

	return 0;
}

DEFINE_HOOK_AGAIN(0x467FEE, BulletClass_UpdateEnd, 0x6)
DEFINE_HOOK(0x466781, BulletClass_UpdateEnd, 0x6)
{
	GET(BulletClass*, pThis, EBP);

	if (auto pExt = BulletExt::ExtMap.Find(pThis))
	{
		pExt->_GameObject->Foreach([](Component* c)
			{ c->OnUpdateEnd(); });
	}

	return 0;
}

DEFINE_HOOK(0x4690C1, BulletClass_Detonate, 0x8)
{
	GET(BulletClass*, pThis, ECX);
	GET_BASE(CoordStruct*, pPos, 0x8);

	if (auto pExt = BulletExt::ExtMap.Find(pThis))
	{
		bool skip = false;
		pExt->_GameObject->Foreach([&](Component* c)
			{ if (auto cc = dynamic_cast<IBulletScript*>(c)) { cc->OnDetonate(pPos, skip); } });

		if (skip)
		{
			return 0x46A2FB;
		}
	}
	return 0;
}

// ----------------
// Feature
// ----------------

#pragma region Remap

// when shooter dead, project's house will be 0
DEFINE_HOOK(0x469A75, BulletClass_Detonate_GetHouse, 0x7)
{
	GET(BulletClass*, pBullet, ESI);
	GET(HouseClass*, pHouse, ECX);

	if (!pHouse)
	{
		// GetStatus save the House
		if (HouseClass* pSourceHouse = GetSourceHouse(pBullet))
		{
			R->ECX(pSourceHouse);
		}
	}
	return 0;
}

// Take over to create Warhead Anim
// Phobos Hook in 0x469C46, Skip Ares 0x469C4E, must skip Phobos
// SplashList of Phobos will be killed
DEFINE_HOOK(0x469C46, BulletClass_Detonate_WHAnim_Remap, 0x8)
{
	bool createdAnim = false;
	GET(AnimTypeClass*, pAnimType, EBX);
	if (pAnimType)
	{
		GET(BulletClass*, pBullet, ESI);
		GET_STACK(CoordStruct, pos, 0x64);
		if (pAnimType)
		{
			if (AnimClass* pAnim = GameCreate<AnimClass>(pAnimType, pos, 0, 1, 0x2600, -15, false))
			{
				createdAnim = true;
				SetAnimOwner(pAnim, pBullet);
				SetAnimCreater(pAnim, pBullet);
			}
		}
	}
	R->EAX(createdAnim);
	return 0x469C98;
}

// Take over to create Warhead VxlAnim
DEFINE_HOOK(0x469D5C, BulletClass_Detonate_WHVxlDebris_Remap, 0x6)
{
	if (AudioVisual::Data()->AllowMakeVoxelDebrisByKratos)
	{
		GET(BulletClass*, pBullet, ESI);
		GET(WarheadTypeClass*, pWH, EAX);
		GET(int, times, EBX);
		HouseClass* pHouse = GetSourceHouse(pBullet);
		TechnoClass* pCreate = pBullet->Owner;
		CoordStruct location = pBullet->GetCoords();
		ExpandAnimsManager::PlayExpandDebirs(pWH->DebrisTypes, pWH->DebrisMaximums, times, location, pHouse, pCreate);
		R->EBX(0);
		return 0x469E18;
	}
	return 0;
}

DEFINE_HOOK(0x469EB4, BulletClass_Detonate_WHDebris_Remap, 0x6)
{
	GET(BulletClass*, pBullet, ESI);
	GET(AnimClass*, pAnim, EDI);
	if (pAnim)
	{
		SetAnimOwner(pAnim, pBullet);
		SetAnimCreater(pAnim, pBullet);
	}
	GET(int, i, EBX);
	if (i > 0)
	{
		return 0x469E34;
	}
	return 0;
}

namespace BulletColour
{
	BulletClass* pBullet = nullptr;
	PaintData myData{};
}

DEFINE_HOOK(0x4683E2, BulletClass_DrawSHP_Colour, 0x5)
{
	GET(BulletClass*, pBullet, ESI);
	BulletColour::pBullet = nullptr;
	if (BulletStatus* status = GetStatus<BulletExt, BulletStatus>(pBullet))
	{
		PaintData data = status->MyPaintData;
		if (data.ChangeColor || data.ChangeBright)
		{
			BulletColour::pBullet = pBullet;
			BulletColour::myData = data;
			if (data.ChangeColor)
			{
				R->Stack(0, data.Data.Color2);
			}
		}
	}
	return 0;
}

DEFINE_HOOK(0x4683E7, BulletClass_DrawSHP_Bright, 0x9)
{
	GET(BulletClass*, pBullet, ESI);
	if (pBullet == BulletColour::pBullet && BulletColour::myData.ChangeBright)
	{
		R->Stack(0, GetBright(1000, BulletColour::myData.Data.BrightMultiplier));
	}
	BulletColour::pBullet = nullptr;
	return 0;
}

DEFINE_HOOK(0x46B201, BulletClass_DrawVXL_Colour, 0x7)
{
	R->EDI(BlitterFlags::None);
	R->Stack(0, Add2RGB565(ToColorAdd(Colors::Red)));
	GET_STACK(BulletClass*, pBullet, 0x10 - 0x4);
	if (BulletStatus* status = GetStatus<BulletExt, BulletStatus>(pBullet))
	{
		status->DrawVXL_Paintball(R);
	}
	return 0;
}
#pragma endregion

#pragma region Trajectory
// 除 ROT>0 和 Vertical 之外的抛射体会在此根据重力对储存的向量变量进行运算
// 对Arcing抛射体的重力进行削减
DEFINE_HOOK(0x46745C, BulletClass_Update_ChangeVelocity, 0x7)
{
	GET(BulletClass*, pBullet, EBP);
	if (BulletStatus* status = GetStatus<BulletExt, BulletStatus>(pBullet))
	{
		if (status->IsArcing() && status->SpeedChanged)
		{
			LEA_STACK(BulletVelocity*, pVelocity, 0x90);
			BulletVelocity velocity = pBullet->Velocity;
			pVelocity->X = velocity.X;
			pVelocity->Y = velocity.Y;
			pVelocity->Z = status->LocationLocked ? 0 : velocity.Z;
		}
	}
	return 0;
}

// 除 ROT>0 和 Vertical 之外的抛射体会在Label_158根据速度向量获取坐标
// Arcing抛射体即使向量非常小也会试图移动1点
DEFINE_HOOK(0x4677C7, BulletClass_Update_ChangeVelocity_Locked, 0x8)
{
	GET(BulletClass*, pBullet, EBP);
	if (BulletStatus* status = GetStatus<BulletExt, BulletStatus>(pBullet))
	{
		if (status->IsArcing() && status->SpeedChanged && status->LocationLocked)
		{
			CoordStruct location = pBullet->Location;
			R->ESI(location.X);
			R->EDI(location.Y);
			R->EAX(location.Z);
		}
	}
	return 0;
}

// 导弹类抛射体当高度低于地面高度时强制引爆
// 让直线导弹可以潜地
// Vector 接管 + Vector.SubjectToCliffs=no（默认，VectorIgnoresCliffs）同样跳过：Vector 强移
// 可能把弹体短暂摆到地形（悬崖顶）以下，属正常路径，不在此引爆——与 SubjectToGround=no 同效。
// SubjectToCliffs=yes 显式配置则恢复原逻辑，Vector 结束判据变假同样恢复。
DEFINE_HOOK(0x466E18, BulletClass_CheckHight_UnderGround, 0x6)
{
	GET(BulletClass*, pBullet, ECX);
	if (pBullet->GetHeight() <= 0)
	{
		if (BulletStatus* status = GetStatus<BulletExt, BulletStatus>(pBullet))
		{
			if (!status->SubjectToGround || status->VectorIgnoresCliffs())
			{
				R->Stack<bool>(0x18, false);
				R->Stack<unsigned int>(0x20, 0);
			}
		}
	}
	return 0;
}
#pragma endregion

#pragma region Vector KeepAlive

// Vector 存在期间暴力跳过"抛射体抵达目标进入引爆流程"
// 原流程：0x467E53 进入预爆动画 / Detonate / UnInit，弹体消失
// 新流程：弹体仍被 Vector 接管（复用 BulletStatus::OnUpdate_Vector 每帧
//         MarginVectorOffset 的现成结果，不重复遍历判定）→ 跳到 0x467FBA
//         （跳过引爆三段），弹体保持存活并继续被 Vector 位移控制；
//         Vector 结束 → 判据变假，走原版引爆流程。
// 原始字节：8B 8D 28 01 00 00（mov ecx, [ebp+0x128]）
DEFINE_HOOK(0x467E53, BulletClass_AI_PreDetonation_Vector, 0x6)
{
	GET(BulletClass*, pThis, EBP);

	if (BulletStatus* status = GetStatus<BulletExt, BulletStatus>(pThis))
	{
		if (status->HasActiveVector())
		{
			// 跳过预爆动画、Detonate 和 UnInit，继续帧尾记录以及 0x467FEE 的 OnUpdateEnd
			return 0x467FBA;
		}
	}

	// Vector 不存在或已结束，进入原版引爆流程
	return 0;
}
#pragma endregion
