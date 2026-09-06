#include "../TechnoStatus.h"

#include <FootClass.h>
#include <JumpjetLocomotionClass.h>

#include <Common/INI/INI.h>

#include <Ext/Helper/Gift.h>
#include <Ext/Helper/Scripts.h>

#include <Ext/EffectType/AttachEffectScript.h>
#include <Ext/ObjectType/AttachEffect.h>


DeployToTransformData* TechnoStatus::GetTransformData()
{
	if (!_transformData)
	{
		_transformData = INI::GetConfig<DeployToTransformData>(INI::Rules, pTechno->GetTechnoType()->ID)->Data;
	}
	return _transformData;
}

DeployToAttachData* TechnoStatus::GetDeployAttachData()
{
	if (!_deployAttachData)
	{
		_deployAttachData = INI::GetConfig<DeployToAttachData>(INI::Rules, pTechno->GetTechnoType()->ID)->Data;
	}
	return _deployAttachData;
}

DeploysIntoData* TechnoStatus::GetDeploysIntoData()
{
	if (!_deploysIntoData)
	{
		_deploysIntoData = INI::GetConfig<DeploysIntoData>(INI::Rules, pTechno->GetTechnoType()->ID)->Data;
	}
	return _deploysIntoData;
}

void TechnoStatus::OnUpdate_DeployTo()
{
	bool transform = GetTransformData()->Enable;
	bool attachEffect = GetDeployAttachData()->Enable;

	if (!transform && !attachEffect)
	{
		return;
	}

	switch (DeployState)
	{
	case DeployState::None:
		if (IsInfantry())
		{
			// 步兵序列为Deploy和Undeploy时，即是开始部署
			Sequence sequence = abstract_cast<InfantryClass*, true>(pTechno)->SequenceAnim;
			if (sequence == Sequence::Deploy)
			{
				DeployState = DeployState::Deploying;
			}
			else if (sequence == Sequence::Undeploy)
			{
				DeployState = DeployState::Undeploying;
			}
		}
		else if (IsUnit())
		{
			// 载具任务处于unload时，即是开始部署
			if (pTechno->CurrentMission == Mission::Unload)
			{
				DeployState = DeployState::Deploying;
				// 如果是IsSimpleDeploy， 在部署完成会设置Deployed，否则会设置为false
				if (abstract_cast<UnitClass*, true>(pTechno)->Deployed)
				{
					DeployState = DeployState::Undeploying;
				}
			}
		}
		break;
	case DeployState::Deploying:
		if (IsInfantry())
		{
			// 步兵序列为Deployed时，即是部署完成
			Sequence sequence = abstract_cast<InfantryClass*, true>(pTechno)->SequenceAnim;
			if (sequence == Sequence::Deployed)
			{
				DeployState = DeployState::Deployed;
			}
		}
		else if (IsUnit())
		{
			// 如果是IsSimpleDeploy， 在部署完成会设置Deployed，否则会设置为false
			if (abstract_cast<UnitClass*, true>(pTechno)->Deployed)
			{
				DeployState = DeployState::Deployed;
				break;
			}
			// 如果是其他类型，DeployFire，需要在Hook中判断是否部署完成，此处不处理
			if (pTechno->CurrentMission != Mission::Unload)
			{
				DeployState = DeployState::None;
			}
		}
		break;
	case DeployState::Undeploying:
		if (IsInfantry())
		{
			// 步兵序列不为Undeploy时，即是部署完成
			Sequence sequence = abstract_cast<InfantryClass*, true>(pTechno)->SequenceAnim;
			if (sequence != Sequence::Undeploy)
			{
				DeployState = DeployState::Undeployed;
			}
		}
		else if (IsUnit())
		{
			// 如果是IsSimpleDeploy， 在部署完成会设置Deployed，否则会设置为false
			if (abstract_cast<UnitClass*, true>(pTechno)->Deployed)
			{
				DeployState = DeployState::Undeployed;
				break;
			}
			// 如果是其他类型，DeployFire，需要在Hook中判断是否部署完成，此处不处理
			if (pTechno->CurrentMission != Mission::Unload)
			{
				DeployState = DeployState::None;
			}
		}
		break;
	}

	// 部署触发变形或附加AE
	if (DeployState == DeployState::Deployed)
	{
		DeployState = DeployState::None;

		if (GetTransformData()->DeployTo)
		{
			// 部署变形，在单位部署完成后触发
			GiftBox->Start(&GetTransformData()->DeployToTransform);
		}
		else if (GetDeployAttachData()->DeployTo)
		{
			// 部署附加AE
			AttachEffect* aeManager = AEManager();
			if (aeManager)
			{
				// 附加AE
				aeManager->Attach(GetDeployAttachData()->DeployToAttachEffects, GetDeployAttachData()->DeployToAttachChances, false);
			}
		}
	}
	else if (DeployState == DeployState::Undeployed)
	{
		DeployState = DeployState::None;

		if (GetTransformData()->UndeployTo)
		{
			// 卸载变形，在单位卸载完成后触发
			GiftBox->Start(&GetTransformData()->UndeployToTransform);
		}
		else if (GetDeployAttachData()->UndeployTo)
		{
			// 卸载附加AE
			AttachEffect* aeManager = AEManager();
			if (aeManager)
			{
				// 附加AE
				aeManager->Attach(GetDeployAttachData()->UndeployToAttachEffects, GetDeployAttachData()->UndeployToAttachChances, false);
			}
		}
	}
}

void TechnoStatus::DeploysInto(TechnoClass* pGift, bool isDeploying)
{
	if (!pGift)
	{
		return;
	}

	DeploysIntoData* config = GetDeploysIntoData();
	DeployToTransformEntity* data = nullptr;

	bool inherit = false;

	if (isDeploying)
	{
		// 部署变形，载具部署成建筑时触发
		data = &config->DeploysInto;
		inherit = config->DeployTo;
	}
	else
	{
		// 卸载变形，建筑收起为载具时触发
		data = &config->UndeploysInto;
		inherit = config->UndeployTo;
	}

	if (inherit)
	{
		TechnoTypeClass* pGiftType = pGift->GetTechnoType();
		// 继承等级
		if (data->InheritExperience && pGiftType->Trainable)
		{
			pGift->Veterancy = pTechno->Veterancy;
		}

		// 继承ROF
		if (data->InheritROF && pTechno->ROFTimer.InProgress())
		{
			pGift->ROFTimer.Start(pTechno->ROFTimer.GetTimeLeft());
		}

		// 继承弹药
		if (data->InheritAmmo && pGiftType->Ammo > 1 && pTechno->GetTechnoType()->Ammo > 1)
		{
			int ammo = pTechno->Ammo;
			if (ammo >= 0)
			{
				pGift->Ammo = ammo;
			}
		}

		AttachEffect* giftAEM = GetAEManager<TechnoExt>(pGift);
		// 继承AE管理器
		if (data->InheritAE)
		{
			TechnoStatus* pGiftStatus = GetStatus<TechnoExt, TechnoStatus>(pGift);
			// 继承AE并修改变量
			giftAEM = InheritAE(this, pGiftStatus);
		}

		// 调整AE
		if (giftAEM)
		{
			// 移除失效的AE
			giftAEM->DetachByName(data->RemoveEffects, false);
			// 附加新的AE
			giftAEM->Attach(data->AttachEffects, data->AttachChances);
		}

	}
}
