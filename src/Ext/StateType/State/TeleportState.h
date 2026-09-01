#pragma once

#include <GeneralDefinitions.h>
#include <HouseClass.h>

#include "../StateScript.h"
#include "TeleportData.h"

#include <Ext/TechnoType/DamageText.h>

class TeleportState : public StateScript<TeleportData>
{
public:
	enum class TeleportStep
	{
		NONE = 0, READY = 1, TELEPORTED = 2, FREEZING = 3, MOVEFORWARD = 4
	};

	STATE_SCRIPT(Teleport);

	bool Teleport(CoordStruct* pLocation, WarheadTypeClass* pWH);

	bool IsFreezing();

	bool IsReadyToMoveWarp();

	virtual void Clean() override
	{
		StateScript<TeleportData>::Clean();

		_count = 0;
		_delay = 0;
		_delayTimer = {};

		// 状态机一直处于激活状态，额外开关控制是否可以进行传送
		_canWarp = false;
		_step = TeleportStep::READY;

		_warpTo; // 弹头传进来的坐标
		_teleportTimer = {}; // 传送冰冻时间
		_jumpTo; // 传送的坐标

		isInfantry = false;
		isAircraft = false;
		isJumpJet = false;

		pDest = nullptr;
		pFocus = nullptr;
	}

	virtual void Deactivate() override
	{
		// 永久激活，不可关闭
	}

	virtual void OnStart() override;

	virtual void OnEnd() override;

	virtual void OnUpdate() override;

	virtual void OnWarpUpdate() override;

	TeleportState& operator=(const TeleportState& other)
	{
		if (this != &other)
		{
			StateScript<TeleportData>::operator=(other);
			_count = other._count;
			_delay = other._delay;
			_delayTimer = other._delayTimer;
		}
		return *this;
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->_count)
			.Process(this->_delay)
			.Process(this->_delayTimer)

			.Process(this->_canWarp)
			.Process(this->_step)
			.Process(this->_warpTo)
			.Process(this->_teleportTimer)
			.Process(this->_jumpTo)

			.Process(this->isInfantry)
			.Process(this->isAircraft)
			.Process(this->isJumpJet)
			.Process(this->pDest)
			.Process(this->pFocus)
			.Process(this->pTarget)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange)
	{
		StateScript<TeleportData>::Load(stream, registerForChange);
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const
	{
		StateScript<TeleportData>::Save(stream);
		return const_cast<TeleportState*>(this)->Serialize(stream);
	}
#pragma endregion
private:
	CoordStruct GetAndMarkDestination(CoordStruct location);

	void Reload();

	bool IsReady();

	bool Timeup();

	bool IsDone();

	int _count = 0;
	int _delay = 0;
	CDTimerClass _delayTimer{};

	// 状态机一直处于激活状态，额外开关控制是否可以进行传送
	bool _canWarp = false;
	TeleportStep _step = TeleportStep::READY;

	CoordStruct _warpTo; // 弹头传进来的坐标
	CDTimerClass _teleportTimer{}; // 传送冰冻时间

	CoordStruct _jumpTo; // 传送的坐标，用来判断是否已经到达指定位置，需要清除Jumpjet的目标，否则JJ会在爬升时来回跳跃

	bool isInfantry = false;
	bool isAircraft = false;
	bool isJumpJet = false;

	AbstractClass* pDest = nullptr;
	AbstractClass* pFocus = nullptr;
	AbstractClass* pTarget = nullptr;
};
