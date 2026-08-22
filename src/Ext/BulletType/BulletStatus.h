#pragma once

#include <string>
#include <format>
#include <vector>
#include <map>

#include <BulletClass.h>
#include <TechnoClass.h>

#include <Utilities/Macro.h>
#include <Utilities/Debug.h>

#include <Common/Components/ScriptComponent.h>

#include <Common/INI/INIConfig.h>

#include <Ext/Helper/CastEx.h>
#include <Ext/Helper/MathEx.h>

// TODO Add new State
#include <Ext/StateType/State/BlackHoleState.h>
#include <Ext/StateType/State/DestroySelfState.h>
#include <Ext/StateType/State/ECMState.h>
#include <Ext/StateType/State/GiftBoxState.h>
#include <Ext/StateType/State/PaintballState.h>

#include "BulletStatusData.h"
#include "Status/ProximityData.h"
#include "Trajectory/TrajectoryData.h"

class AttachEffect;

/// @brief base compoment, save the Techno status
class BulletStatus : public BulletScript
{
public:
	BULLET_SCRIPT(BulletStatus);

	void TakeDamage(int damage = 0, bool eliminate = true, bool harmless = false, bool checkInterceptable = false);

	void TakeDamage(BulletDamage damageData, bool checkInterceptable = false);

	void ResetTarget(AbstractClass* pNewTarget, CoordStruct targetPos);

	void DrawVXL_Paintball(REGISTERS* R);

	void OnTechnoDelete(EventSystem* sender, Event e, void* args);

	void ActiveProximity();

	void SetFakeTarget(ObjectClass* pFakeTarget);

	void BlackHoleCapture(ObjectClass* pBlackHole, BlackHoleData data);
	void BlackHoleCancel();

	virtual void Clean() override
	{
		BulletScript::Clean();

		pSource = nullptr;
		pSourceHouse = nullptr;

		// 生命值和伤害值
		life = {};
		damage = {};

		// 染色
		MyPaintData = {};

		// 碰触地面会炸
		SubjectToGround = false;
		// 正在被黑洞吸引
		CaptureByBlackHole = false;
		// 正在被Vector强制位移
		VectorForced = false;

		SpeedChanged = false; // 改变抛射体的速度
		LocationLocked = false; // 锁定抛射体的位置

		_initFlag = false;

		_recordStatus = {};

		_pFakeTarget = nullptr;

		// 黑洞
		_pBlackHole = nullptr;
		_blackHoleData = {};
		_blackHoleDamageDelay = {};

		// 碰撞引信配置
		_proximityData = nullptr;
		// 碰撞引信
		_proximity = {};
		_activeProximity = false;
		// 近炸引信
		_proximityRange = -1;

		// 发射射手
		_selfLaunch = false;
		_limboFlag = false;
		_shooterIsSelected = false;
		_movingSelfInitFlag = false;

		// 状态机
		_BlackHole = nullptr;
		_ECM = nullptr;
		_GiftBox = nullptr;
		_DestroySelf = nullptr;
		_Paintball = nullptr;
	}

	virtual void Awake() override;

	virtual void Destroy() override;

	virtual void OnPut(CoordStruct* pLocation, DirType dir) override;

	virtual void OnUpdate() override;

	virtual void OnUpdateEnd() override;

	virtual void OnDetonate(CoordStruct* pCoords, bool& skip) override;

	// TODO Add new State
	// 状态机
	STATE_VAR_DEFINE(BlackHole);
	STATE_VAR_DEFINE(ECM);
	STATE_VAR_DEFINE(GiftBox);
	STATE_VAR_DEFINE(DestroySelf);
	STATE_VAR_DEFINE(Paintball);

	void AttachState()
	{
		STATE_VAR_INIT(BlackHole);
		STATE_VAR_INIT(ECM);
		STATE_VAR_INIT(GiftBox);
		STATE_VAR_INIT(DestroySelf);
		STATE_VAR_INIT(Paintball);
	}

	template <typename T>
	bool TryGetState(IStateScript*& state)
	{
		if (false) {}
		STATE_VAR_TRYGET(BlackHole)
			STATE_VAR_TRYGET(ECM)
			STATE_VAR_TRYGET(GiftBox)
			STATE_VAR_TRYGET(DestroySelf)
			STATE_VAR_TRYGET(Paintball)
			return state != nullptr;
	}

	TechnoClass* pSource = nullptr;
	HouseClass* pSourceHouse = nullptr;

	// 生命值和伤害值
	BulletLife life{};
	BulletDamage damage{};

	// 染色
	PaintData MyPaintData{};

	// 碰触地面会炸
	bool SubjectToGround = false;
	// 正在被黑洞吸引
	bool CaptureByBlackHole = false;
	// 正在被Vector强制位移
	bool VectorForced = false;

	// 弹体是否仍被 Vector 接管（复用 OnUpdate_Vector 每帧 MarginVectorOffset 的现成结果）
	// Force=yes 的 Vector 必置 VectorForced；Force=no 的 Vector 以实际位移/冻结为准。
	// 供 BulletClass_AI_PreDetonation_Vector hook 判断是否跳过引爆流程。
	bool HasActiveVector()
	{
		return VectorForced || !_vectorResult.MoveDisp.IsEmpty() || _vectorResult.Freeze;
	}

	bool SpeedChanged = false; // 改变抛射体的速度
	bool LocationLocked = false; // 锁定抛射体的位置

	virtual void OwnerIsRelease(void* ptr) override
	{
		pSource = nullptr;
	};

#pragma region Save/Load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->pSource)
			.Process(this->pSourceHouse)
			.Process(this->life)
			.Process(this->damage)

			.Process(this->MyPaintData)

			.Process(this->SubjectToGround)

			.Process(this->SpeedChanged)
			.Process(this->LocationLocked)

			.Process(this->_initFlag)

			.Process(this->_recordStatus)

			.Process(this->_pFakeTarget)
			// 黑洞
			.Process(this->CaptureByBlackHole)
			.Process(this->_pBlackHole)
			.Process(this->_blackHoleData)
			.Process(this->_blackHoleDamageDelay)
			// 位移向量
			.Process(this->VectorForced)
			.Process(this->_vectorResult)
			.Process(this->_vectorStartPos)
			// 碰撞引信
			.Process(this->_proximity)
			.Process(this->_activeProximity)
			.Process(this->_proximityRange)
			// 发射射手
			.Process(this->_selfLaunch)
			.Process(this->_limboFlag)
			.Process(this->_shooterIsSelected)
			.Process(this->_movingSelfInitFlag)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		Component::Load(stream, registerForChange);
		bool res = this->Serialize(stream);
		EventSystems::General.AddHandler(Events::ObjectUnInitEvent, this, &BulletStatus::OnTechnoDelete);
		return res;
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		Component::Save(stream);
		return const_cast<BulletStatus*>(this)->Serialize(stream);
	}
#pragma endregion

private:
	AttachEffect* AEManager();

	void InitState();

	// 礼物盒
	bool IsOnMark_GiftBox();
	void ReleaseGift(std::vector<std::string> gifts, GiftBoxData data);

	// 反抛射体
	void CanAffectAndDamageBullet(BulletClass* pTarget, WarheadTypeClass* pWH);

	// 手动引爆抛射体
	bool ManualDetonation(CoordStruct sourcePos, bool KABOOM = true, AbstractClass* pTarget = nullptr, CoordStruct detonatePos = CoordStruct::Empty);

	void InitState_BlackHole();
	void InitState_ECM();
	void InitState_Proximity();

	void OnUpdate_DestroySelf();

	void OnUpdate_BlackHole();
	void OnUpdate_Vector();
	void OnUpdate_GiftBox();
	void OnUpdate_Paintball();
	void OnUpdate_RecalculateStatus();
	void OnUpdate_SelfLaunchOrPumpAction();

	void OnUpdateEnd_BlackHole(CoordStruct& sourcePos);
	void OnUpdateEnd_Vector(CoordStruct& sourcePos);
	void OnUpdateEnd_Proximity(CoordStruct& sourcePos);

	bool OnDetonate_AntiBullet(CoordStruct* pCoords);
	bool OnDetonate_GiftBox(CoordStruct* pCoords);
	bool OnDetonate_SelfLaunch(CoordStruct* pCoords);

	bool _initFlag = false;

	RecordBulletStatus _recordStatus{};

	ObjectClass* _pFakeTarget = nullptr;

	// 黑洞
	ObjectClass* _pBlackHole = nullptr;
	BlackHoleData _blackHoleData{};
	CDTimerClass _blackHoleDamageDelay{};

	// 向量位移
	VectorResult _vectorResult{};
	CoordStruct _vectorStartPos{};       // Force 模式起始位置快照

	// 碰撞引信配置
	ProximityData* _proximityData = nullptr;
	ProximityData* GetProximityData();
	// 碰撞引信
	Proximity _proximity{};
	bool _activeProximity = false;
	// 近炸引信
	int _proximityRange = -1;

	// 发射射手
	bool _selfLaunch = false;
	bool _limboFlag = false;
	bool _shooterIsSelected = false;
	bool _movingSelfInitFlag = false;
};
