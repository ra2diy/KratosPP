#pragma once

#include <algorithm>
#include <string>
#include <format>
#include <vector>
#include <set>
#include <map>

#include <TechnoClass.h>

#include <Utilities/Macro.h>
#include <Utilities/Debug.h>

#include <Common/Components/ScriptComponent.h>
#include <Common/EventSystems/EventSystem.h>

#include <Common/INI/INIConfig.h>

#include <Ext/Common/PrintTextData.h>
#include <Ext/EffectType/Effect/CrateBuffData.h>
#include <Ext/EffectType/Effect/StandData.h>

// TODO Add new State
#include <Ext/StateType/State/AntiBulletState.h>
#include <Ext/StateType/State/BlackHoleState.h>
#include <Ext/StateType/State/DamageReactionState.h>
#include <Ext/StateType/State/DeselectState.h>
#include <Ext/StateType/State/DestroyAnimState.h>
#include <Ext/StateType/State/DestroySelfState.h>
#include <Ext/StateType/State/DisableWeaponState.h>
#include <Ext/StateType/State/FreezeState.h>
#include <Ext/StateType/State/GiftBoxState.h>
#include <Ext/StateType/State/NoMoneyNoTalkState.h>
#include <Ext/StateType/State/OverrideWeaponState.h>
#include <Ext/StateType/State/PaintballState.h>
#include <Ext/StateType/State/PumpState.h>
#include <Ext/StateType/State/ScatterState.h>
#include <Ext/StateType/State/TeleportState.h>
#include <Ext/StateType/State/TransformState.h>

#include <Ext/WeaponType/TargetLaserData.h>

#include "AirstrikeData.h"
#include "FireFLHData.h"
#include "PassengersData.h"


class AttachEffect;

/// @brief base compoment, save the Techno status
class TechnoStatus : public TechnoScript
{
public:
	TECHNO_SCRIPT(TechnoStatus);

	void SetupStand(StandData data, TechnoClass* pMaster);
	bool AmIStand();

	void OnTechnoDelete(EventSystem* sender, Event e, void* args)
	{
		if (args == pMyMaster)
		{
			pMyMaster = nullptr;
		}
	}

	void RockerPitch(WeaponTypeClass* pWeapon);

	void OnAirstrikeDetach(EventSystem* sender, Event e, void* args)
	{
		auto const& argsArray = reinterpret_cast<void**>(args);
		AbstractClass* pInvalid = (AbstractClass*)argsArray[0];
		if (AirstrikeClass* pAirstrike = dynamic_cast<AirstrikeClass*>(pInvalid))
		{
			CancelAirstrike(pAirstrike);
		}
	}

	/**
	 *@brief 被空袭
	 *
	 * @param airstrike
	 */
	void SetAirstrike(AirstrikeClass* airstrike);
	void CancelAirstrike(AirstrikeClass* airstrike);
	bool AnyAirstrike();

	/**
	 *@brief 我有一只激光笔
	 *
	 * @param sender
	 * @param e
	 * @param args
	 */
	void StartTargetLaser(AbstractClass* pTarget, WeaponTypeClass* pWeapon, TargetLaserData data, CoordStruct flh, bool isOnTurret = true);
	void CloseTargetLaser(AbstractClass* pTarget);
	void CloseTargetLaser(WeaponTypeClass* pWeapon);
	bool OutOfTargetLaserRange(TargetLaser laser);
	void OnGScreenRender(EventSystem* sender, Event e, void* args);

	void OnLaserTargetDetach(EventSystem* sender, Event e, void* args)
	{
		auto const& argsArray = reinterpret_cast<void**>(args);
		AbstractClass* pInvalid = (AbstractClass*)argsArray[0];
		CloseTargetLaser(pInvalid);
	}

	/**
	 *@brief 踩箱子获得的buff
	 *
	 */
	void RecalculateStatus();
	bool CanICloakByDefault();

	// 破隐相关
	bool IsForceDecloakActive() const { return ForceDecloakActive; }
	void UpdateForceDecloakState(bool active);

	bool PlayDestroyAnims();

	void SetExtraSparkleAnim(AnimClass* pAnim);

	bool IsFLHOnBody(int weaponIdx);
	bool IsFLHOnTarget();

	void DrawSHP_Paintball(REGISTERS* R);
	void DrawSHP_Paintball_BuildingAnim(REGISTERS* R);
	void DrawSHP_Colour(REGISTERS* R);
	void DrawVXL_Paintball(REGISTERS* R, bool isBuilding);

	// 黑洞
	void BlackHoleCapture(ObjectClass* pBlackHole, BlackHoleData data);
	void BlackHoleCancel();

	// 气泵
	bool PumpAction(CoordStruct targetPos, bool isLobber, Sequence flySequence); // 爆炸冲击
	void HumanCannon(CoordStruct sourcePos, CoordStruct targetPos, int height, bool isLobber, Sequence flySequence); // 人间大炮

	virtual void Clean() override
	{
		TechnoScript::Clean();

		// 踩箱子获得的buff
		CrateBuff = {};

		// 破隐状态
		ForceDecloakActive = false;
		ExternalCloakable = false;

		// 替身的配置
		MyStandData = {};
		pMyMaster = nullptr;
		MyMasterIsSpawned = false;
		MyMasterIsAnim = false;
		StandIsMoving = false;

		// 染色配置
		MyPaintData = {};

		drivingState = DrivingState::Moving;

		DisableVoxelCache = false;

		DisableSelectVoice = false;

		// 攻击攻击信标
		AttackBeaconRecruited = false;

		CaptureByBlackHole = false;
		Jumping = false;

		// 冻结
		Freezing = false;
		// 冻结JJ时记录下他的面向
		JJMark = false;
		JJFacing.SetValue(0);

		// 气球强制降落
		BalloonFall = false;
		// Jumpjet强制降落，接管移动
		CarryallLanding = false;

		// 虚单位
		VirtualUnit = false;
		Disappear = false;

		FLHIndex = 0;

		// 阿伟死了，DestroySelfState干的
		_isDead = false;

		_lastMission = Mission::Guard;

		_location = CoordStruct::Empty;
		_isMoving = false;

		//
		_isVoxel = false;
		// 选择状态
		_disableSelectable = false;
		// 冻结状态
		_cantMoveFlag = false;

		// 我最后打的目标
		_lastTarget = nullptr;

		// 我是残骸
		_destroyAnimData = nullptr;
		// 乖巧乘客
		_passengersData = nullptr;

		// 部署变形
		_transformData = nullptr;

		// 我有一只激光笔
		_targetLasers.clear();

		// 被空袭管理器
		_airstrikes.clear();

		// 染色状态
		_deactivateDimEMP = 0.8f;
		_deactivateDimPowered = 0.5f;
		_berserkColor2 = 0;
		_buildingWasBerzerk = false;
		_buildingWasEMP = false;
		_buildingWasColor = false;

		// 变形
		pSourceType = nullptr;
		pTargetType = nullptr;
		_changeToType = { "" };
		_hasBeenChanged = false; // 处于变形状态
		_transformLocked = false; // 处于变形状态时锁定不允许再变形

		// EMP动画
		pExtraSparkleAnim = nullptr;

		// FLH
		_fireFLHData = nullptr;

		// 黑洞
		_pBlackHole = nullptr;
		_blackHoleData = {};
		_blackHoleDamageDelay = {};
		_lostControl = false;

		// 状态机
		_AntiBullet = nullptr;
		_BlackHole = nullptr;
		_DamageReaction = nullptr;
		_Deselect = nullptr;
		_DestroyAnim = nullptr;
		_DestroySelf = nullptr;
		_DisableWeapon = nullptr;
		_Freeze = nullptr;
		_GiftBox = nullptr;
		_NoMoneyNoTalk = nullptr;
		_OverrideWeapon = nullptr;
		_Paintball = nullptr;
		_Pump = nullptr;
		_Scatter = nullptr;
		_Teleport = nullptr;
		_Transform = nullptr;
	}

	virtual void Awake() override;

	virtual void Destroy() override;

	virtual void ExtChanged() override;

	virtual void OnUpdate() override;

	void OnUpdate_DeployToTransform(); // call by hook
	void OnUpdate_DestroySelf(); // call by Stand

	virtual void OnUpdateEnd() override;

	virtual void OnWarpUpdate() override;

	virtual void OnTemporalUpdate(TemporalClass* pTemporal) override;

	virtual void OnPut(CoordStruct* pCoord, DirType dirType) override;

	virtual void OnRemove() override;

	virtual void OnReceiveDamage(args_ReceiveDamage* args) override;

	virtual void OnReceiveDamageEnd(int* pRealDamage, WarheadTypeClass* pWH, DamageState damageState, TechnoClass* pAttacker, HouseClass* pAttackingHouse) override;

	virtual void OnReceiveDamageDestroy() override;

	virtual void OnRegisterDestruction(TechnoClass* pKiller, int cost, bool& skip) override;

	virtual void CanFire(AbstractClass* pTarget, WeaponTypeClass* pWeapon, bool& ceaseFire) override;

	virtual void OnFire(AbstractClass* pTarget, int weaponIdx) override;

	virtual void OnSelect(bool& selectable) override;

	// TODO Add new State
	// 状态机
	STATE_VAR_DEFINE(AntiBullet);
	STATE_VAR_DEFINE(BlackHole);
	STATE_VAR_DEFINE(DamageReaction);
	STATE_VAR_DEFINE(Deselect);
	STATE_VAR_DEFINE(DestroyAnim);
	STATE_VAR_DEFINE(DestroySelf);
	STATE_VAR_DEFINE(DisableWeapon);
	STATE_VAR_DEFINE(Freeze);
	STATE_VAR_DEFINE(GiftBox);
	STATE_VAR_DEFINE(NoMoneyNoTalk);
	STATE_VAR_DEFINE(OverrideWeapon);
	STATE_VAR_DEFINE(Paintball);
	STATE_VAR_DEFINE(Pump);
	STATE_VAR_DEFINE(Scatter);
	STATE_VAR_DEFINE(Teleport);
	STATE_VAR_DEFINE(Transform);

	void AttachState()
	{
		STATE_VAR_INIT(AntiBullet);
		STATE_VAR_INIT(BlackHole);
		STATE_VAR_INIT(DamageReaction);
		STATE_VAR_INIT(Deselect);
		STATE_VAR_INIT(DestroyAnim);
		STATE_VAR_INIT(DestroySelf);
		STATE_VAR_INIT(DisableWeapon);
		STATE_VAR_INIT(Freeze);
		STATE_VAR_INIT(GiftBox);
		STATE_VAR_INIT(NoMoneyNoTalk);
		STATE_VAR_INIT(OverrideWeapon);
		STATE_VAR_INIT(Paintball);
		STATE_VAR_INIT(Pump);
		STATE_VAR_INIT(Scatter);
		STATE_VAR_INIT(Teleport);
		STATE_VAR_INIT(Transform);
	}

	void InheritedStatsTo(TechnoStatus*& heir)
	{
		STATE_VAR_INHERITED(AntiBullet);
		STATE_VAR_INHERITED(BlackHole);
		STATE_VAR_INHERITED(DamageReaction);
		STATE_VAR_INHERITED(Deselect);
		STATE_VAR_INHERITED(DestroyAnim);
		STATE_VAR_INHERITED(DestroySelf);
		STATE_VAR_INHERITED(DisableWeapon);
		STATE_VAR_INHERITED(Freeze);
		// STATE_VAR_INHERITED(GiftBox);
		STATE_VAR_INHERITED(NoMoneyNoTalk);
		STATE_VAR_INHERITED(OverrideWeapon);
		STATE_VAR_INHERITED(Paintball);
		STATE_VAR_INHERITED(Pump);
		STATE_VAR_INHERITED(Scatter);
		STATE_VAR_INHERITED(Teleport);
		// STATE_VAR_INHERITED(Transform);
	}

	template <typename T>
	bool TryGetState(IStateScript*& state)
	{
		if (false) {}
		STATE_VAR_TRYGET(AntiBullet)
			STATE_VAR_TRYGET(BlackHole)
			STATE_VAR_TRYGET(DamageReaction)
			STATE_VAR_TRYGET(Deselect)
			STATE_VAR_TRYGET(DestroyAnim)
			STATE_VAR_TRYGET(DestroySelf)
			STATE_VAR_TRYGET(DisableWeapon)
			STATE_VAR_TRYGET(Freeze)
			STATE_VAR_TRYGET(GiftBox)
			STATE_VAR_TRYGET(NoMoneyNoTalk)
			STATE_VAR_TRYGET(OverrideWeapon)
			STATE_VAR_TRYGET(Paintball)
			STATE_VAR_TRYGET(Pump)
			STATE_VAR_TRYGET(Scatter)
			STATE_VAR_TRYGET(Teleport)
			STATE_VAR_TRYGET(Transform)
			return state != nullptr;
	}

	// 踩箱子获得的buff
	CrateBuffData CrateBuff{};

	// 替身的配置
	StandData MyStandData{};
	TechnoClass* pMyMaster = nullptr;
	bool MyMasterIsSpawned = false;
	bool MyMasterIsAnim = false;
	bool StandIsMoving = false;

	// 染色配置
	PaintData MyPaintData{};

	DrivingState drivingState = DrivingState::Moving;

	bool DisableVoxelCache = false;

	bool DisableSelectVoice = false;

	// 攻击攻击信标
	bool AttackBeaconRecruited = false;

	bool CaptureByBlackHole = false;
	bool Jumping = false;

	// 冻结
	bool Freezing = false;
	// 冻结JJ时记录下他的面向
	bool JJMark = false;
	DirStruct JJFacing{};

	// 气球强制降落
	bool BalloonFall = false;
	// Jumpjet强制降落，接管移动
	bool CarryallLanding = false;

	// 虚单位
	bool VirtualUnit = false;
	bool Disappear = false;

	int FLHIndex = 0;

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->CrateBuff)
			.Process(this->InnateCloakable)
			.Process(this->ForceDecloakActive)
			.Process(this->ExternalCloakable)
			.Process(this->MyStandData)
			.Process(this->pMyMaster)
			.Process(this->MyMasterIsSpawned)

			.Process(this->MyPaintData)

			.Process(this->DisableVoxelCache)

			.Process(this->DisableSelectVoice)

			.Process(this->Freezing)
			.Process(this->JJMark)
			.Process(this->JJFacing)

			.Process(this->VirtualUnit)
			.Process(this->Disappear)

			.Process(this->_disableSelectable)
			.Process(this->_cantMoveFlag)

			.Process(this->_airstrikes)

			.Process(this->_targetLasers)

			.Process(this->_deactivateDimEMP)
			.Process(this->_deactivateDimPowered)
			.Process(this->_berserkColor2)
			.Process(this->_buildingWasBerzerk)
			.Process(this->_buildingWasEMP)
			.Process(this->_buildingWasColor)

			.Process(this->pSourceType)
			.Process(this->pTargetType)
			.Process(this->_changeToType)
			.Process(this->_hasBeenChanged)
			.Process(this->_transformLocked)

			.Process(this->pExtraSparkleAnim)

			// 黑洞
			.Process(this->CaptureByBlackHole)
			.Process(this->_pBlackHole)
			.Process(this->_blackHoleData)
			.Process(this->_blackHoleDamageDelay)
			.Process(this->_lostControl)
			.Success();
	};
	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		Component::Load(stream, registerForChange);
		bool res = this->Serialize(stream);
		if (MyStandData.Enable)
		{
			EventSystems::General.AddHandler(Events::ObjectUnInitEvent, this, &TechnoStatus::OnTechnoDelete);
		}
		if (!_airstrikes.empty())
		{
			EventSystems::General.AddHandler(Events::DetachAll, this, &TechnoStatus::OnAirstrikeDetach);
		}
		if (!_targetLasers.empty())
		{
			EventSystems::General.AddHandler(Events::DetachAll, this, &TechnoStatus::OnLaserTargetDetach);
			EventSystems::Render.AddHandler(Events::GScreenRenderEvent, this, &TechnoStatus::OnGScreenRender);
		}
		return res;
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		Component::Save(stream);
		return const_cast<TechnoStatus*>(this)->Serialize(stream);
	}
#pragma endregion

private:
	bool ForceDecloakActive = false;	// 是否破隐
	bool InnateCloakable = false;		// 原本能否隐身
	bool ExternalCloakable = false;	// 是否隐身

	AttachEffect* AEManager();

	/**
	 *@brief 初始化所需的状态机
	 *
	 */
	void InitState();

	/**
	 *@brief 从TechnoType中读取扩展的ini标签，并初始化相应的component
	 *
	 */
	void InitExt();

	// 礼物盒
	bool IsOnMark_GiftBox();
	void ReleaseGift(std::vector<std::string> gifts, GiftBoxData data);

	// 反抛射体
	bool WeaponNoAA(int weaponIdx);

	// 变形
	bool ChangeTechnoTypeTo(TechnoTypeClass* pNewType);

	// 激光笔
	void LostTargetLaser();

	void OnPut_Stand(CoordStruct* pCoord, DirType dirType);

	void OnRemove_Stand();
	void OnRemove_TargetLaser();

	void OnUpdate_AntiBullet();
	void OnUpdate_BalloonTransporter();
	void OnUpdate_BlackHole();
	void OnUpdate_Deselect();
	void OnUpdate_Freeze();
	void OnUpdate_GiftBox();
	void OnUpdate_Paintball();
	void OnUpdate_Passenger();
	void OnUpdate_TargetLaser();
	void OnUpdate_Transform();

	void OnWarpUpdate_DestroySelf_Stand();

	void OnReceiveDamage_Stand(args_ReceiveDamage* args);

	void OnReceiveDamageEnd_DestroyAnim(int* pRealDamage, WarheadTypeClass* pWH, DamageState damageState, TechnoClass* pAttacker, HouseClass* pAttackingHouse);
	void OnReceiveDamageEnd_BlackHole(int* pRealDamage, WarheadTypeClass* pWH, DamageState damageState, TechnoClass* pAttacker, HouseClass* pAttackingHouse);
	void OnReceiveDamageEnd_GiftBox(int* pRealDamage, WarheadTypeClass* pWH, DamageState damageState, TechnoClass* pAttacker, HouseClass* pAttackingHouse);
	void OnReceiveDamageEnd_Vampire(int* pRealDamage, WarheadTypeClass* pWH, DamageState damageState, TechnoClass* pAttacker, HouseClass* pAttackingHouse);

	void OnReceiveDamageDestroy_GiftBox();
	void OnReceiveDamageDestroy_Transform();

	void OnRegisterDestruction_Stand(TechnoClass* pKiller, int cost, bool& skip);

	bool CanFire_DisableWeapon(AbstractClass* pTarget, WeaponTypeClass* pWeapon);
	bool CanFire_NoMoneyNoTalk(AbstractClass* pTarget, WeaponTypeClass* pWeapon);
	bool CanFire_Passenger(AbstractClass* pTarget, WeaponTypeClass* pWeapon);

	void OnFire_RockerPitch(AbstractClass* pTarget, int weaponIdx);
	void OnFire_AttackBeaconRecruit(AbstractClass* pTarget, int weaponIdx);
	void OnFire_TargetLaser(AbstractClass* pTarget, int weaponIdx);

	bool OnSelect_VirtualUnit();
	bool OnSelect_Deselect();

	// 阿伟死了，DestroySelfState干的
	bool _isDead = false;

	Mission _lastMission = Mission::Guard;

	CoordStruct _location{};
	bool _isMoving = false;

	//
	bool _isVoxel = false;
	// 选择状态
	bool _disableSelectable = false;
	// 冻结状态
	bool _cantMoveFlag = false;

	// 我最后打的目标
	AbstractClass* _lastTarget = nullptr;

	// 我是残骸
	DestroyAnimData* _destroyAnimData = nullptr;
	DestroyAnimData* GetDestroyAnimData();

	// 乖巧乘客
	PassengersData* _passengersData = nullptr;
	PassengersData* GetPassengersData();

	// 部署变形
	DeployToTransformData* _transformData = nullptr;
	DeployToTransformData* GetTransformData();

	// 我有一只激光笔
	std::vector<TargetLaser> _targetLasers{};

	// 被空袭管理器
	std::vector<AirstrikeClass*> _airstrikes;

	// 染色状态
	AirstrikeData* GetAirstrikeData(TechnoClass* pOwner);
	float _deactivateDimEMP = 0.8f;
	float _deactivateDimPowered = 0.5f;
	unsigned int _berserkColor2 = 0;
	unsigned int GetBerserkColor2();
	bool _buildingWasBerzerk = false;
	bool _buildingWasEMP = false;
	bool _buildingWasColor = false;

	// 变形
	TechnoTypeClass* pSourceType = nullptr;
	TechnoTypeClass* pTargetType = nullptr;
	std::string _changeToType{ "" };
	bool _hasBeenChanged = false; // 处于变形状态
	bool _transformLocked = false; // 处于变形状态时锁定不允许再变形

	// EMP动画
	AnimClass* pExtraSparkleAnim = nullptr;

	// FLH
	FireFLHData* _fireFLHData = nullptr;
	FireFLHData* GetFireFLHData();

	// 黑洞
	ObjectClass* _pBlackHole = nullptr;
	BlackHoleData _blackHoleData{};
	CDTimerClass _blackHoleDamageDelay{};
	bool _lostControl = false;
};
