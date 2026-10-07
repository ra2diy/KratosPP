#pragma once

#include <string>
#include <vector>

#include <Common/INI/INIConfig.h>
#include <Common/KratosSession.h>

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
		if (!KratosLib::AeGroupLimitLoaded)
		{
			KratosLib::AeGroupLimitLoaded = true;

			const void* table = nullptr;
			uint32_t bytes = 0;
			if (KratosLib::Session != 0
				&& KL_GetTable(KratosLib::Session, KL_Table_AeTable, &table, &bytes)
				&& table != nullptr && bytes >= KL_TABLE_HEADER_SIZE)
			{
				const uint8_t* base = static_cast<const uint8_t*>(table);
				const auto u32 = [base](uint32_t offset)
				{
					return uint32_t(base[offset]) | (uint32_t(base[offset + 1]) << 8)
						| (uint32_t(base[offset + 2]) << 16) | (uint32_t(base[offset + 3]) << 24);
				};

				const uint32_t count = u32(8);
				const uint32_t elemSize = u32(12);
				if (elemSize == KL_TABLE_ELEM_AE
					&& uint64_t(KL_TABLE_HEADER_SIZE) + uint64_t(count) * elemSize <= uint64_t(bytes))
				{
					for (uint32_t i = 0; i < count; ++i)
					{
						const uint32_t elem = KL_TABLE_HEADER_SIZE + i * elemSize;
						if (u32(elem) == KL_AE_KEY_GROUP_LIMIT)
						{
							KratosLib::AeGroupLimit = u32(elem + 4);
							break;
						}
					}
				}
			}
		}

		// 读取带序号的
		for (uint32_t i = 0; i < KratosLib::AeGroupLimit; i++)
		{
			AttachEffectTypeData data;
			data.Read(reader, static_cast<int>(i));
			if (data.Enable)
			{
				Datas[static_cast<int>(i)] = data;
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



