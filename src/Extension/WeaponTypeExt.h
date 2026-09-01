#pragma once

#include <WeaponTypeClass.h>

#include "Ext/ObjectType/AttachFireData.h"

#include "TypeExtension.h"

class WeaponTypeExt : public TypeExtension<WeaponTypeClass, WeaponTypeExt>
{
public:
	/// @brief 储存一些通用设置或者其他平台的设置
	class TypeData : public AttachFireData
	{
	public:
		// Ares
		int Ammo = 1;

		int LaserThickness = 0;
		bool LaserFade = false;
		bool IsSupported = false;

		// Phobos
		int BoltArcCount = 8;
		ColorStruct BoltColor1 = Colors::Empty;
		ColorStruct BoltColor2 = Colors::Empty;
		ColorStruct BoltColor3 = Colors::Empty;
		bool BoltDisable1 = false;
		bool BoltDisable2 = false;
		bool BoltDisable3 = false;
		bool BoltDisableParticle = false;
		bool IsSingleColor = false; // 单色激光
		bool VisualScatter = false; // 视觉散射
		double visualScatterMin = 0.03;
		double visualScatterMax = 0.13;
		int VisualScatterMin = Game::F2I(visualScatterMin * Unsorted::LeptonsPerCell);
		int VisualScatterMax = Game::F2I(visualScatterMax * Unsorted::LeptonsPerCell);
		AffectedTarget CanTarget = AffectedTarget::All;
		AffectedHouse CanTargetHouses = AffectedHouse::All;
		double CanTargetMinHealth = 0.0; // Phobos对低血量目标的筛选
		double CanTargetMaxHealth = 1.0; // Phobos对高血量目标的筛选
		bool SkipWeaponPicking = true; // 跳过强制武器选择

		// Kratos
		float RockerPitch = 0;
		bool SelfLaunch = false;
		bool SelfLaunchDash = false;
		bool PumpAction = false;
		Sequence PumpInfSequence = Sequence::Crawl;
		int HumanCannon = -1;

		int NoMoneyNoTalk = 0;
		bool DontNeedMoney = false;

		bool LaserRandomColor = false;

		virtual void Read(INIBufferReader* reader) override
		{
			AttachFireData::Read(reader);

			Ammo = reader->Get("Ammo", Ammo);

			LaserThickness = reader->Get("LaserThickness", LaserThickness);
			LaserFade = reader->Get("LaserFade", LaserFade);
			IsSupported = reader->Get("IsSupported", IsSupported);
			LaserRandomColor = reader->Get("LaserRandomColor", LaserRandomColor);

			BoltArcCount = reader->Get("Bolt.Arcs", BoltArcCount);

			BoltColor1 = reader->Get("Bolt.Color1", BoltColor1);
			BoltColor2 = reader->Get("Bolt.Color2", BoltColor2);
			BoltColor3 = reader->Get("Bolt.Color3", BoltColor3);

			BoltDisable1 = reader->Get("Bolt.Disable1", BoltDisable1);
			BoltDisable2 = reader->Get("Bolt.Disable2", BoltDisable2);
			BoltDisable3 = reader->Get("Bolt.Disable3", BoltDisable3);
			BoltDisableParticle = reader->Get("Bolt.DisableParticle", BoltDisableParticle);

			IsSingleColor = reader->Get("IsSingleColor", IsSingleColor);
			VisualScatter = reader->Get("VisualScatter", VisualScatter);
			// 读取全局设置
			INIBufferReader* avReader = INI::GetSection(INI::Rules, INI::SectionAudioVisual);
			visualScatterMin = avReader->Get("VisualScatter.Min", visualScatterMin);
			visualScatterMax = avReader->Get("VisualScatter.Max", visualScatterMax);
			// 读取个体设置
			visualScatterMin = reader->Get("VisualScatter.Min", visualScatterMin);
			visualScatterMax = reader->Get("VisualScatter.Max", visualScatterMax);
			VisualScatterMin = Game::F2I(visualScatterMin * Unsorted::LeptonsPerCell);
			VisualScatterMax = Game::F2I(visualScatterMax * Unsorted::LeptonsPerCell);

			CanTarget = reader->Get("CanTarget", CanTarget);
			CanTargetHouses = reader->Get("CanTargetHouses", CanTargetHouses);
			CanTargetMinHealth = reader->GetPercent("CanTarget.MinHealth", CanTargetMinHealth);
			CanTargetMaxHealth = reader->GetPercent("CanTarget.MaxHealth", CanTargetMaxHealth);
			if (CanTarget != AffectedTarget::All || CanTargetHouses != AffectedHouse::All
				|| CanTargetMinHealth > 0.0 || CanTargetMaxHealth < 1.0)
			{
				// 至少启用了一种目标类型筛选，则不跳过武器选择
				SkipWeaponPicking = false;
			}

			RockerPitch = reader->Get("RockerPitch", RockerPitch);
			SelfLaunch = reader->Get("SelfLaunch", SelfLaunch);
			SelfLaunchDash = reader->Get("SelfLaunch.Dash", SelfLaunchDash);
			PumpAction = reader->Get("PumpAction", PumpAction);
			PumpInfSequence = reader->Get("PumpAction.InfSequence", PumpInfSequence);
			HumanCannon = reader->Get("HumanCannon", HumanCannon);

			NoMoneyNoTalk = reader->Get("NoMoneyNoTalk", NoMoneyNoTalk);
			DontNeedMoney = reader->Get("DontNeedMoney", DontNeedMoney);
		}

#pragma region save/load
		template <typename T>
		bool Serialize(T& stream)
		{
			return stream
				.Process(this->Ammo)

				.Process(this->LaserThickness)
				.Process(this->LaserFade)
				.Process(this->IsSupported)
				.Process(this->LaserRandomColor)

				.Process(this->BoltArcCount)
				.Process(this->BoltColor1)
				.Process(this->BoltColor2)
				.Process(this->BoltColor3)
				.Process(this->BoltDisable1)
				.Process(this->BoltDisable2)
				.Process(this->BoltDisable3)
				.Process(this->BoltDisableParticle)
				.Process(this->IsSingleColor)
				.Process(this->VisualScatter)
				.Process(this->visualScatterMin)
				.Process(this->visualScatterMax)
				.Process(this->VisualScatterMin)
				.Process(this->VisualScatterMax)
				.Process(this->CanTarget)
				.Process(this->CanTargetHouses)

				.Process(this->RockerPitch)
				.Process(this->SelfLaunch)
				.Process(this->SelfLaunchDash)
				.Process(this->PumpAction)
				.Process(this->PumpInfSequence)
				.Process(this->HumanCannon)

				.Process(this->NoMoneyNoTalk)
				.Process(this->DontNeedMoney)
				.Success();
		};

		virtual bool Load(ExStreamReader& stream, bool registerForChange) override
		{
			AttachFireData::Load(stream, registerForChange);
			return this->Serialize(stream);
		}
		virtual bool Save(ExStreamWriter& stream) const override
		{
			AttachFireData::Save(stream);
			return const_cast<TypeData*>(this)->Serialize(stream);
		}
#pragma endregion
	};

	static constexpr DWORD Canary = 0x22222222;
	// static constexpr size_t ExtPointerOffset = 0x18;

	static WeaponTypeExt::ExtContainer ExtMap;
};
