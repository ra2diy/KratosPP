#pragma once

#include <string>
#include <vector>

#include <Common/INI/INIConfig.h>

class AttachEffectTypeData : public INIConfig
{
public:
	std::vector<std::string> AttachEffectTypes{};
	std::vector<double> AttachEffectChances{}; // 附加成功率，应该只对弹头有用

	bool AttachToSource = false; // 用弹头附加时，反过来附加给攻击者

	std::vector<std::string> GetEffectTypes{};
	std::vector<double> GetEffectChances{};

	bool AttachFullAirspace = false; // 搜索圆柱体范围

	int StandTrainCabinLength = 512; // 火车替身间隔
	int AEMode = -1; // 作为乘客时的激活载具的组序号

	// 多组AE的赋予控制
	bool AttachByPassenger = true; // 仅由乘客的AEMode赋予
	int AEModeIndex = -1; // 组序号

	virtual void Read(INIBufferReader* reader) override
	{
		AttachEffectTypes = reader->GetList("AttachEffectTypes", AttachEffectTypes);
		AttachEffectChances = reader->GetChanceList("AttachEffectChances", AttachEffectChances);

		AttachToSource = reader->Get("AttachToSource", AttachToSource);

		GetEffectTypes = reader->GetList("GetEffectTypes", GetEffectTypes);
		GetEffectChances = reader->GetChanceList("GetEffectChances", GetEffectChances);

		AttachFullAirspace = reader->Get("AttachFullAirspace", AttachFullAirspace);

		StandTrainCabinLength = reader->Get("StandTrainCabinLength", StandTrainCabinLength);
		// 由乘客读取
		AEMode = reader->Get("AEMode", AEMode);

		Enable = !AttachEffectTypes.empty() || !GetEffectTypes.empty();
	}

	// 通过AEMode触发附加的多组AE的设置读取
	void Read(INIBufferReader* reader, int index)
	{
		std::string title = "AttachEffectTypes" + std::to_string(index);

		AttachEffectTypes = reader->GetList(title, AttachEffectTypes);
		AttachEffectChances = reader->GetChanceList(title + ".Chances", AttachEffectChances);

		AttachByPassenger = reader->Get(title + ".AttachByPassenger", AttachByPassenger);
		AEModeIndex = index;

		Enable = !AttachEffectTypes.empty();
	}

};

class AttachEffectGroupData : public INIConfig
{
public:
	std::map<int, AttachEffectTypeData> Datas{};

	virtual void Read(INIBufferReader* reader) override
	{
		// 读取带序号的
		for (int i = 0; i < 128; i++)
		{
			AttachEffectTypeData data;
			data.Read(reader, i);
			if (data.Enable)
			{
				Datas[i] = data;
			}
		}

		Enable = !Datas.empty();
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(this->Datas)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		return const_cast<AttachEffectGroupData*>(this)->Serialize(stream);
	}
#pragma endregion
};



