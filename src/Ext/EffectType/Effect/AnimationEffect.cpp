#include "AnimationEffect.h"

#include <AnimTypeClass.h>

#include <Extension/AnimExt.h>

#include <Ext/Helper/FLH.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>

#include <Ext/AnimType/AnimStatus.h>

void AnimationEffect::UpdateLocationOffset(CoordStruct offset)
{
	AnimStatus* animStatus = nullptr;
	if (pIdleAnim && TryGetStatus<AnimExt, AnimStatus>(pIdleAnim, animStatus))
	{
		animStatus->Offset = offset;
	}

}

void AnimationEffect::Awake()
{
	// 监听IdleAnim自身的销毁，避免pIdleAnim悬挂
	EventSystems::General.AddHandler(Events::ObjectUnInitEvent, this, &AnimationEffect::OnAnimUnInit);
}

void AnimationEffect::Destroy()
{
	EventSystems::General.RemoveHandler(Events::ObjectUnInitEvent, this, &AnimationEffect::OnAnimUnInit);
	KillIdleAnim();
}

void AnimationEffect::OnAnimUnInit(EventSystem* sender, Event e, void* args)
{
	if (args == pIdleAnim)
	{
		pIdleAnim = nullptr;
	}
}

void AnimationEffect::CreateIdleAnim(bool force, CoordStruct location)
{
	if (pIdleAnim)
	{
		KillIdleAnim();
	}
	if (!force && (!Data->IdleAnim.Enable || IsDeadOrInvisible(pObject) || (Data->IdleAnim.RemoveInCloak && ownerIsCloak)))
	{
		return;
	}
	// 创建动画
	AnimTypeClass* pAnimType = AnimTypeClass::Find(Data->IdleAnim.Type.c_str());
	if (pAnimType)
	{
		OffsetData offsetData = Data->IdleAnim.Offset;
		if (location.IsEmpty())
		{
			location = GetRelativeLocation(pObject, offsetData).Location;
		}
		pIdleAnim = GameCreate<AnimClass>(pAnimType, location);
		pIdleAnim->RemainingIterations = 0xFF; // Loops
		if (pBullet)
		{
			SetAnimOwner(pIdleAnim, pBullet);
			SetAnimCreater(pIdleAnim, pBullet);
		}
		else if (pTechno)
		{
			SetAnimOwner(pIdleAnim, pTechno);
			SetAnimCreater(pIdleAnim, pTechno);
		}
		ShowAnim(pIdleAnim, Data->IdleAnim.Visibility);
		// 记录动画的渲染参数
		animFlags = pIdleAnim->AnimFlags;
		// 设置动画的附着对象，由动画自身去位移
		AnimStatus* status = GetStatus<AnimExt, AnimStatus>(pIdleAnim);
		status->AttachToObject(pObject, offsetData);
	}
}

void AnimationEffect::KillIdleAnim()
{
	if (pIdleAnim)
	{
		// 不将动画附着于单位上，动画就不会自行注销，需要手动注销
		pIdleAnim->TimeToDie = true;
		pIdleAnim->UnInit(); // 包含了SetOwnerObject(0) 0x4255B0
		pIdleAnim = nullptr;
	}
}

void AnimationEffect::OnStart() {
	// 激活动画
	if (Data->ActiveAnim.Enable)
	{
		AnimTypeClass* pAnimType = AnimTypeClass::Find(Data->ActiveAnim.Type.c_str());
		if (pAnimType)
		{
			OffsetData offsetData = Data->ActiveAnim.Offset;
			CoordStruct location = GetRelativeLocation(pObject, offsetData).Location;
			AnimClass* pAnim = GameCreate<AnimClass>(pAnimType, location);
			if (pBullet)
			{
				SetAnimOwner(pAnim, pBullet);
				SetAnimCreater(pAnim, pBullet);
			}
			else if (pTechno)
			{
				SetAnimOwner(pAnim, pTechno);
				SetAnimCreater(pAnim, pTechno);
			}
		}
	}
	// 持续动画
	CreateIdleAnim();
}

void AnimationEffect::End(CoordStruct location)
{
	KillIdleAnim();
	if (Data->DoneAnim.Enable)
	{
		AnimTypeClass* pAnimType = AnimTypeClass::Find(Data->DoneAnim.Type.c_str());
		if (pAnimType)
		{
			bool isDead = AE->OwnerIsDead();
			if (!isDead)
			{
				OffsetData offsetData = Data->DoneAnim.Offset;
				location = GetRelativeLocation(pObject, offsetData).Location;
			}
			AnimClass* pAnim = GameCreate<AnimClass>(pAnimType, location);
			if (!isDead)
			{
				if (pBullet)
				{
					SetAnimOwner(pAnim, pBullet);
					SetAnimCreater(pAnim, pBullet);
				}
				else if (pTechno)
				{
					SetAnimOwner(pAnim, pTechno);
					SetAnimCreater(pAnim, pTechno);
				}
			}
			else
			{
				SetAnimOwner(pAnim, AE->pSourceHouse);
			}
		}
	}
}

void AnimationEffect::OnPause()
{
	End(CoordStruct::Empty);
}

void AnimationEffect::OnRecover()
{
	OnStart();
}

void AnimationEffect::OnPut(CoordStruct* pCoords, DirType faceDir)
{
	CreateIdleAnim(true, *pCoords);
}

void AnimationEffect::ExtChanged()
{
	if (Data->IdleAnim.Enable)
	{
		if (!pIdleAnim)
		{
			// 挂载对象已换且旧动画已不存在，直接按新单位重建
			CreateIdleAnim();
		}
		else if (AnimStatus* status = GetStatus<AnimExt, AnimStatus>(pIdleAnim))
		{
			// 重新设置动画的附着对象，由动画自身去位移
			status->AttachToObject(pObject, Data->IdleAnim.Offset);
		}
	}
}

void AnimationEffect::OnUpdate()
{
	if (pTechno && AE && !AE->OwnerIsDead())
	{
		switch (pTechno->CloakState)
		{
		case CloakState::Uncloaked:
			// 显形状态
			if (ownerIsCloak)
			{
				ownerIsCloak = false;
				if (Data->IdleAnim.RemoveInCloak)
				{
					CreateIdleAnim();
				}
				else if (pIdleAnim && Data->IdleAnim.TranslucentInCloak)
				{
					// 恢复不透明
					pIdleAnim->AnimFlags = animFlags;
				}
			}
			break;
		default:
			// 进入隐形或处于正在隐\显形的过程中
			if (!ownerIsCloak)
			{
				ownerIsCloak = true;
				if (Data->IdleAnim.RemoveInCloak)
				{
					KillIdleAnim();
				}
				else if (pIdleAnim && Data->IdleAnim.TranslucentInCloak)
				{
					// 半透明
					pIdleAnim->AnimFlags |= BlitterFlags::TransLucent50;
				}
			}
			break;
		}
	}
}

void AnimationEffect::OnRemove()
{
	KillIdleAnim();
}

void AnimationEffect::OnReceiveDamage(args_ReceiveDamage* args)
{
	// 受击动画
	if (Data->HitAnim.Enable)
	{
		AnimTypeClass* pAnimType = AnimTypeClass::Find(Data->HitAnim.Type.c_str());
		if (pAnimType)
		{
			OffsetData offsetData = Data->HitAnim.Offset;
			CoordStruct location = GetRelativeLocation(pObject, offsetData).Location;
			AnimClass* pAnim = GameCreate<AnimClass>(pAnimType, location);
			if (pBullet)
			{
				SetAnimOwner(pAnim, pBullet);
				SetAnimCreater(pAnim, pBullet);
			}
			else if (pTechno)
			{
				SetAnimOwner(pAnim, pTechno);
				SetAnimCreater(pAnim, pTechno);
			}
			ShowAnim(pAnim, Data->IdleAnim.Visibility);
			// 设置动画的附着对象，由动画自身去位移
			AnimStatus* status = GetStatus<AnimExt, AnimStatus>(pAnim);
			status->AttachToObject(pObject, offsetData);
		}
	}
}
