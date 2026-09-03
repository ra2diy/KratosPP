#include "BulletStatus.h"

#include <Extension/WarheadTypeExt.h>

#include <Ext/Helper/Scripts.h>

#include <Ext/BulletType/Trajectory/ArcingTrajectory.h>
#include <Ext/BulletType/Trajectory/MissileTrajectory.h>
#include <Ext/BulletType/Trajectory/StraightTrajectory.h>

#include "Bounce.h"

#include <Ext/ObjectType/AttachEffect.h>
#include <Ext/TechnoType/TechnoStatus.h>
#include <Ext/TechnoType/DecoyMissile.h>

void BulletStatus::OnTechnoDelete(EventSystem* sender, Event e, void* args)
{
	if (args == pSource)
	{
		pSource = nullptr;
	}
	if (args == _pFakeTarget)
	{
		_pFakeTarget = nullptr;
	}
	if (args == _pBlackHole)
	{
		BlackHoleCancel();
	}
}

AttachEffect* BulletStatus::AEManager()
{
	AttachEffect* aeManager = nullptr;
	if (_parent)
	{
		aeManager = _parent->GetComponent<AttachEffect>();
	}
	return aeManager;
}

void BulletStatus::InitState()
{
	AttachState();

	// 根据类型分配弹道控制
	switch (GetBulletType())
	{
	case BulletType::ARCING:
		FindOrAttach<ArcingTrajectory>();
		FindOrAttach<Bounce>();
		break;
	case BulletType::MISSILE:
		FindOrAttach<MissileTrajectory>();
		break;
	case BulletType::ROCKET:
		FindOrAttach<StraightTrajectory>();
		break;
	}
}

void BulletStatus::SetFakeTarget(ObjectClass* pFakeTarget)
{
	_pFakeTarget = pFakeTarget;
}

void BulletStatus::Awake()
{
	EventSystems::General.AddHandler(Events::ObjectUnInitEvent, this, &BulletStatus::OnTechnoDelete);

	Tag = pBullet->GetType()->Name;

	pSource = pBullet->Owner;
	if (pSource)
	{
		pSourceHouse = pSource->Owner;
	}
	// 读取生命值
	int health = pBullet->Health;
	// 抛射体武器伤害为复述或者零时需要处理
	if (health < 0)
	{
		health = -health;
	}
	else if (health == 0)
	{
		health = 1; // 武器伤害为0，如[NukeCarrier]
	}

	INIBufferReader* reader = INI::GetSection(INI::Rules, pBullet->GetType()->ID);
	// 初始化生命
	this->life.Health = health;
	this->life.Read(reader);
	// 初始化伤害
	this->damage.Damage = health;
	if (TechnoStatus* sourceStatue = GetStatus<TechnoExt, TechnoStatus>(pSource))
	{
		if (sourceStatue->AntiBullet->IsAlive())
		{
			damage.Eliminate = sourceStatue->AntiBullet->Data.OneShotOneKill;
			damage.Harmless = sourceStatue->AntiBullet->Data.Harmless;
		}
	}

	// 初始化地面碰撞属性
	switch (trajectoryData->SubjectToGround)
	{
	case SubjectToGroundType::YES:
		this->SubjectToGround = true;
		break;
	case SubjectToGroundType::NO:
		this->SubjectToGround = false;
		break;
	default:
		this->SubjectToGround = !IsArcing() && !IsRocket() && !trajectoryData->IsStraight();
		break;
	}

	// 初始化状态机
	InitState();
}

void BulletStatus::Destroy()
{
	EventSystems::General.RemoveHandler(Events::ObjectUnInitEvent, this, &BulletStatus::OnTechnoDelete);
}

void BulletStatus::TakeDamage(int damage, bool eliminate, bool harmless, bool checkInterceptable)
{
	if (!checkInterceptable || life.Interceptable)
	{
		if (eliminate)
		{
			life.Detonate(harmless);
		}
		else
		{
			life.TakeDamage(damage, harmless);
		}
	}
}

void BulletStatus::TakeDamage(BulletDamage damageData, bool checkInterceptable)
{
	TakeDamage(damageData.Damage, damageData.Eliminate, damageData.Harmless, checkInterceptable);
}

void BulletStatus::ResetTarget(AbstractClass* pNewTarget, CoordStruct targetPos)
{
	pBullet->SetTarget(pNewTarget);
	if (targetPos == CoordStruct::Empty && pNewTarget)
	{
		targetPos = pNewTarget->GetCoords();
	}
	pBullet->TargetCoords = targetPos;
	// 重设弹道
	switch (GetBulletType())
	{
	case BulletType::ARCING:
		if (ArcingTrajectory* at = GetComponent<ArcingTrajectory>())
		{
			at->ResetTarget(pNewTarget, targetPos);
		}
		break;
	case BulletType::ROCKET:
		if (StraightTrajectory* st = GetComponent<StraightTrajectory>())
		{
			st->ResetTarget(pNewTarget, targetPos);
		}
	}
}

void BulletStatus::OnPut(CoordStruct* pLocation, DirType dir)
{
	if (!_initFlag)
	{
		_initFlag = true;
		InitState_BlackHole();
		// InitState_Bounce();
		InitState_Proximity();
		// // 弹道初始化
		// if (IsMissile())
		// {
		// 	InitState_ECM();
		// 	InitState_Trajectory_Missile();
		// }
		// else if (IsRocket())
		// {
		// 	InitState_Trajectory_Straight();
		// }
	}

}

void BulletStatus::InitState_BlackHole() {};
void BulletStatus::InitState_ECM() {};

void BulletStatus::OnUpdate()
{

	// 自毁
	OnUpdate_DestroySelf();

	CoordStruct location = pBullet->GetCoords();
	// 潜地
	// Vector 接管期间（HasActiveVector）弹体位置由 Vector 摆布，短暂入地（低空经过悬崖顶下方等）
	// 属正常路径，不在此手动引爆——与 SubjectToGround=no 同效；Vector 结束判据变假后恢复原逻辑。
	if (!life.IsDetonate && !HasPreImpactAnim(pBullet->WH))
	{
		if (!HasActiveVector() && (SubjectToGround || GetComponent<Bounce>()) && pBullet->GetHeight() < 0)
		{
			// 抛射体潜入地下，重新设置目标参数，并手动引爆
			CoordStruct targetPos = location;
			if (CellClass* pTargetCell = MapClass::Instance->TryGetCellAt(location))
			{
				targetPos.Z = pTargetCell->GetCoordsWithBridge().Z;
				pBullet->SetTarget(pTargetCell);
			}
			pBullet->TargetCoords = targetPos;
			life.Detonate();
		}
		if (!life.IsDetonate && IsArcing() && pBullet->GetHeight() <= 8)
		{
			// Arcing 近炸
			CoordStruct tempSourcePos = location;
			tempSourcePos.Z = 0;
			CoordStruct tempTargetPos = pBullet->TargetCoords;
			tempTargetPos.Z = 0;
			if (tempSourcePos.DistanceFrom(tempTargetPos) <= static_cast<double>(256 + pBullet->Type->Acceleration))
			{
				// 距离目标太近，强制爆炸
				life.Detonate();
			}
		}
	}

	// 生命检查
	if (life.IsDetonate)
	{
		// 延迟引爆
		if (!life.DetonateTimer.InProgress())
		{
			// 引爆
			if (!life.IsHarmless)
			{
				pBullet->Detonate(location);
			}
			pBullet->UnInit();
			//重要，击杀自己后中断所有后续循环
			Break();
			return;
		}
	}
	if (life.Health <= 0)
	{
		life.IsDetonate = true;
	}
	// 其他逻辑
	if (!IsDeadOrInvisible(pBullet, this) || life.DetonateTimer.InProgress())
	{
		OnUpdate_BlackHole();
		OnUpdate_Vector();
		OnUpdate_Paintball();
		OnUpdate_RecalculateStatus();
		OnUpdate_SelfLaunchOrPumpAction();
		OnUpdate_GiftBox(); // 礼盒会删除对象，所以要放最后
	}
}

void BulletStatus::OnUpdateEnd()
{
	if (!IsDeadOrInvisible(pBullet, this))
	{
		CoordStruct location = pBullet->GetCoords();
		OnUpdateEnd_BlackHole(location); // 黑洞会更新位置，要第一个执行
		OnUpdateEnd_Vector(location);
		OnUpdateEnd_Proximity(location);
	}
};


void BulletStatus::OnDetonate(CoordStruct* pCoords, bool& skip)
{
	ObjectClass* pTemp = _pFakeTarget;
	_pFakeTarget = nullptr;
	if (pTemp)
	{
		pTemp->UnInit();
	}
	if (!skip)
	{
		if ((skip = OnDetonate_AntiBullet(pCoords)) == true)
		{
			return;
		}
		if ((skip = OnDetonate_GiftBox(pCoords)) == true)
		{
			return;
		}
		if ((skip = OnDetonate_SelfLaunch(pCoords)) == true)
		{
			return;
		}
	}
};
