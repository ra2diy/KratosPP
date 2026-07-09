#pragma once

#include <string>
#include <vector>
#include <map>
#include <type_traits>

#include <GeneralStructures.h>

#include <Utilities/Debug.h>

#include <Common/Components/ScriptComponent.h>
#include <Common/EventSystems/EventSystem.h>

#include <Extension/TechnoExt.h>
#include <Extension/BulletExt.h>
#include <Extension/BulletTypeExt.h>
#include <Extension/WarheadTypeExt.h>
#include <Extension/WeaponTypeExt.h>

#include <Ext/Helper/FLH.h>
#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Weapon.h>

#include <Ext/BulletType/Trajectory/TrajectoryData.h>
#include "AttachFireData.h"

struct DelayFire
{
public:
	AbstractClass* pTarget = nullptr;
	CoordStruct FLH = CoordStruct::Empty;
	bool FireOwnWeapon = false;
	int WeaponIndex = -1;
	WeaponTypeClass* pWeapon = nullptr;
	WeaponTypeExt::TypeData WeaponTypeData{};

	bool Invalid = true;

	DelayFire() {};

	DelayFire(AbstractClass* pTarget, int delay = 0, int count = 1)
	{
		this->pTarget = pTarget;
		this->_delay = delay;
		this->_timer.Start(delay);
		this->_count = count;

		this->Invalid = pTarget == nullptr;
	}

	DelayFire(int weaponIndex, AbstractClass* pTarget, int delay = 0, int count = 1) : DelayFire(pTarget, delay, count)
	{
		this->FireOwnWeapon = true;
		this->WeaponIndex = weaponIndex;
		this->pWeapon = nullptr;
	}

	DelayFire(WeaponTypeClass* pWeapon, WeaponTypeExt::TypeData weaponTypeData, AbstractClass* pTarget, int delay = 0, int count = 1) : DelayFire(pTarget, delay, count)
	{
		this->FireOwnWeapon = false;
		this->WeaponIndex = -1;
		this->pWeapon = pWeapon;
		this->WeaponTypeData = weaponTypeData;
	}

	bool TimesUp()
	{
		return _timer.Expired();
	}

	void RecuceOnce()
	{
		_count--;
		_timer.Start(_delay);
	}

	void Done()
	{
		_count = -1;
	}

	bool NotDone()
	{
		return _count > 0;
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->pTarget)
			.Process(this->FLH)
			.Process(this->FireOwnWeapon)
			.Process(this->WeaponIndex)
			.Process(this->pWeapon)
			.Process(this->WeaponTypeData)
			.Process(this->Invalid)

			.Process(this->_delay)
			.Process(this->_timer)
			.Process(this->_count)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange)
	{
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const
	{
		return const_cast<DelayFire*>(this)->Serialize(stream);
	}
#pragma endregion
private:
	int _delay = 0;
	CDTimerClass _timer{};
	int _count = 0;
};

struct SimulateBurst
{
public:
	AbstractClass* pTarget = nullptr;

	TechnoClass* pAttacker = nullptr;
	HouseClass* pAttackingHouse = nullptr;

	WeaponTypeClass* pWeapon = nullptr;
	WeaponTypeExt::TypeData WeaponTypeData{};
	int MinRange = 0;
	int MaxRange = 0;

	CoordStruct FLH = CoordStruct::Empty;
	bool IsOnBody = false;
	bool IsOnTarget = false;
	FireBulletToTarget Callback = nullptr;

	// burst 控制
	int Burst = 1; // 总数
	int Index = 0; // 当前发射的序号
	int FlipY = 1; // 翻转FLH

	bool Invalid = true;

	SimulateBurst() {}

	SimulateBurst(AbstractClass* pTarget, TechnoClass* pAttacker, HouseClass* pAttackingHouse,
		WeaponTypeClass* pWeapon, WeaponTypeExt::TypeData weaponTypeData, int minRange = -1, int maxRange = -1,
		CoordStruct flh = CoordStruct::Empty, bool isOnBody = false, bool isOnTarget = false,
		int burst = 1, int flipY = 1, FireBulletToTarget callback = nullptr)
	{
		this->pTarget = pTarget;
		this->pAttacker = pAttacker;
		this->pAttackingHouse = pAttackingHouse;

		this->pWeapon = pWeapon;
		this->WeaponTypeData = weaponTypeData;
		this->MinRange = minRange;
		this->MaxRange = maxRange;

		this->FLH = flh;
		this->IsOnBody = isOnBody;
		this->IsOnTarget = isOnTarget;

		this->Burst = burst;
		this->FlipY = flipY;
		this->Callback = callback;

		this->_delay = weaponTypeData.SimulateBurstDelay;
		this->_timer.Start(_delay);

		this->Invalid = false;
	}

	bool CanFire()
	{
		if (_timer.Expired())
		{
			_timer.Start(_delay);
			return true;
		}
		return false;
	}

	void CountOne()
	{
		Index++;
		switch (WeaponTypeData.SimulateBurstMode)
		{
		case 1:
			// 左右切换
			FlipY *= -1;
			break;
		case 2:
			// 左一半右一半
			FlipY = (Index < Burst / 2.0f) ? _flag : -_flag;
			break;
		default:
			break;
		}
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->pTarget)
			.Process(this->pAttacker)
			.Process(this->pAttackingHouse)

			.Process(this->pWeapon)
			.Process(this->WeaponTypeData)
			.Process(this->MinRange)
			.Process(this->MaxRange)

			.Process(this->FLH)
			.Process(this->IsOnBody)
			.Process(this->IsOnTarget)

			.Process(this->Callback)

			.Process(this->Burst)
			.Process(this->Index)
			.Process(this->FlipY)

			.Process(this->Invalid)

			.Process(this->_delay)
			.Process(this->_timer)
			.Process(this->_flag)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange)
	{
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const
	{
		return const_cast<SimulateBurst*>(this)->Serialize(stream);
	}
#pragma endregion
private:
	int _delay = 0; // burst延迟
	CDTimerClass _timer; // 延迟计时器
	int _flag = 1; // 翻转FLH的记录
};

class AttachFire : public ObjectScript
{
public:
	OBJECT_SCRIPT(AttachFire);

	virtual void Clean() override
	{
		ObjectScript::Clean();

		SpawnerBurstFLH.clear();
		_delayFires.clear();
		_simulateBurst.clear();
	}

	virtual void Awake() override
	{
		EventSystems::General.AddHandler(Events::DetachAll, this, &AttachFire::OnTargetDetach);
	}

	virtual void Destroy() override
	{
		EventSystems::General.RemoveHandler(Events::DetachAll, this, &AttachFire::OnTargetDetach);
	}

	void OnTargetDetach(EventSystem* sender, Event e, void* args)
	{
		auto const& argsArray = reinterpret_cast<void**>(args);
		AbstractClass* pInvalid = (AbstractClass*)argsArray[0];
		for (DelayFire& delayFire : _delayFires)
		{
			if (delayFire.pTarget == pInvalid)
			{
				delayFire.pTarget = nullptr;
				delayFire.Invalid = true;
			}
		}
		for (SimulateBurst& burst : _simulateBurst)
		{
			if (burst.pTarget == pInvalid)
			{
				burst.pTarget = nullptr;
				burst.Invalid = true;
			}
		}
	}

	/**
	 *@brief 发射任务结束，清除所有未发射任务
	 *
	 */
	void FireMissionDone();

	/**
	 *@brief 发射单位自身的武器
	 *
	 * @param weaponIdx 武器序号
	 * @param pTarget 目标
	 * @param delay 延时
	 * @param count 次数
	 */
	void FireOwnWeapon(int weaponIdx, AbstractClass* pTarget, int delay = 0, int count = 1);

	/**
	 *@brief 发射自定义武器
	 *
	 * @param pAttacker 攻击者
	 * @param pTarget 目标
	 * @param pAttackingHouse 攻击阵营
	 * @param pWeapon 武器指针
	 * @param weaponTypeData 武器通用配置
	 * @param flh FLH
	 * @param isOnBody 按发射者计算FLH
	 * @param isOnTarget 按目标计算FLH
	 * @param callback 抛射体发射后回调
	 * @return true
	 * @return false
	 */
	bool FireCustomWeapon(TechnoClass* pAttacker, AbstractClass* pTarget, HouseClass* pAttackingHouse,
		WeaponTypeClass* pWeapon, WeaponTypeExt::TypeData weaponTypeData,
		CoordStruct flh, bool isOnBody = false, bool isOnTarget = false, FireBulletToTarget callback = nullptr, CoordStruct sourcePos = CoordStruct::Empty);

	/**
	 *@brief 发射自定义武器
	 *
	 * @param pAttacker 攻击者
	 * @param pTarget 目标
	 * @param pAttackingHouse 攻击阵营
	 * @param weaponId 武器ID
	 * @param flh FLH
	 * @param isOnBody 按发射者计算FLH
	 * @param isOnTarget 按目标计算FLH
	 * @param callback 抛射体发射后回调
	 * @return true
	 * @return false
	 */
	bool FireCustomWeapon(TechnoClass* pAttacker, AbstractClass* pTarget, HouseClass* pAttackingHouse,
		std::string weaponId,
		CoordStruct flh, bool isOnBody = false, bool isOnTarget = false, FireBulletToTarget callback = nullptr, CoordStruct sourcePos = CoordStruct::Empty);

	virtual void OnUpdate() override;

	// 发射过的子机发射器的开火坐标
	std::map<int, CoordStruct> SpawnerBurstFLH{};

#pragma region Save/Load
	template <typename T>
	bool Serialize(T& stream) {
		return stream
			.Process(this->_delayFires)
			.Process(this->_simulateBurst)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		Component::Load(stream, registerForChange);
		EventSystems::General.AddHandler(Events::DetachAll, this, &AttachFire::OnTargetDetach);
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		Component::Save(stream);
		return const_cast<AttachFire*>(this)->Serialize(stream);
	}
#pragma endregion
private:
	CoordStruct GetSourcePos(CoordStruct flh, DirStruct& facingDir, bool isOnTurret = true, int flipY = 1);

	CoordStruct GetSourcePosOnTarget(CoordStruct sourcePos, CoordStruct targetPos, CoordStruct flh, DirStruct& facingDir, int flipY = 1);

	bool IsInRange(AbstractClass* pTarget, SimulateBurst burst);

	void SimulateBurstFire(SimulateBurst& burst);

	/**
	 *@brief 发射一次武器
	 *
	 * @param burst
	 */
	void SimulateBurstFireOnce(SimulateBurst& burst);

	// 发射自身武器的待发射队列
	std::vector<DelayFire> _delayFires{};
	// Burst模式下剩余待发射队列
	std::vector<SimulateBurst> _simulateBurst{};
};
