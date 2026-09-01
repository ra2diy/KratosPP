#pragma once

#include <GeneralDefinitions.h>
#include <HouseClass.h>

#include "../StateScript.h"
#include "PumpData.h"

class PumpState : public StateScript<PumpData>
{
public:
	STATE_SCRIPT(Pump);

	bool Jump(CoordStruct targetPos, bool isLobber, Sequence flySequence, bool isHumanCannon = false);

	virtual void Clean() override
	{
		StateScript<PumpData>::Clean();

		_isHumanCannon = false; // 人间大炮
		_flyTimer = {}; // 飞行时间
		_jumpTarget = CoordStruct::Empty; // 目标落点

		_gravity = 0;
		_velocity = BulletVelocity::Empty; // 飞行向量

		_flySequence = Sequence::Ready; // 步兵飞行时的序列

		// 状态机一直处于激活状态，额外开关控制是否可以进行跳跃
		_canJump = false;
	}

	virtual void Deactivate() override
	{
		// 永久激活，不可关闭
	}

	virtual void OnStart() override;

	virtual void OnEnd() override;

	virtual void OnUpdate() override;

	PumpState& operator=(const PumpState& other)
	{
		if (this != &other)
		{
			StateScript<PumpData>::operator=(other);
			_isHumanCannon = other._isHumanCannon;
			_flyTimer = other._flyTimer;
			_jumpTarget = other._jumpTarget;
			_gravity = other._gravity;
			_velocity = other._velocity;
			_flySequence = other._flySequence;
			_canJump = other._canJump;
		}
		return *this;
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->_isHumanCannon)
			.Process(this->_flyTimer)
			.Process(this->_jumpTarget)

			.Process(this->_gravity)
			.Process(this->_velocity)

			.Process(this->_flySequence)

			.Process(this->_canJump)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange)
	{
		StateScript<PumpData>::Load(stream, registerForChange);
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const
	{
		StateScript<PumpData>::Save(stream);
		return const_cast<PumpState*>(this)->Serialize(stream);
	}
#pragma endregion
private:
	void SetupPump();

	bool ActionJump(BulletVelocity velocity, int gravity, double straightDistance);
	void CancelJump();

	bool _isHumanCannon = false; // 人间大炮
	CDTimerClass _flyTimer{}; // 飞行时间
	CoordStruct _jumpTarget = CoordStruct::Empty; // 人间大炮的目标落点（飞行收尾精确落回）

	int _gravity = 0;
	BulletVelocity _velocity = BulletVelocity::Empty; // 飞行向量

	Sequence _flySequence = Sequence::Ready; // 步兵飞行时的序列

	// 状态机一直处于激活状态，额外开关控制是否可以进行跳跃
	bool _canJump = false;
};
