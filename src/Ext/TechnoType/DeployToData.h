#pragma once

#include <GeneralStructures.h>

#include <Common/INI/INIConfig.h>

#include <Ext/StateType/State/GiftBoxData.h>


enum class DeployState : int
{
	None = 0,
	Deploying = 1,
	Deployed = 2,
	Undeploying = 3,
	Undeployed = 4,
};

class DeployToTransformEntity : public GiftBoxData
{
public:
	virtual void Read(INIBufferReader* reader, std::string title) override
	{
		ForceTransform = true;

		// 单个语句简写
		std::string type{ "" };
		type = reader->Get(title, type);
		if (IsNotNone(type))
		{
			Data.Gifts.emplace_back(type);
		}

		GiftBoxData::Read(reader, title + ".");
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		GiftBoxData::Load(stream, registerForChange);
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		GiftBoxData::Save(stream);
		return const_cast<DeployToTransformEntity*>(this)->Serialize(stream);
	}
#pragma endregion
private:
};

class DeployToTransformData : public INIConfig
{
public:
	bool DeployTo = false;
	DeployToTransformEntity DeployToTransform{};

	bool UndeployTo = false;
	DeployToTransformEntity UndeployToTransform{};

	virtual void Read(INIBufferReader* reader) override
	{
		DeployToTransform.Read(reader, "DeployToTransform");
		DeployTo = DeployToTransform.Enable;

		UndeployToTransform.Read(reader, "UndeployToTransform");
		UndeployTo = UndeployToTransform.Enable;

		Enable = DeployTo || UndeployTo;
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(DeployToTransform)
			.Process(DeployTo)
			.Process(UndeployToTransform)
			.Process(UndeployTo)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		INIConfig::Load(stream, registerForChange);
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		INIConfig::Save(stream);
		return const_cast<DeployToTransformData*>(this)->Serialize(stream);
	}
#pragma endregion
private:
};

class DeployToAttachData : public INIConfig
{
public:
	bool DeployTo = false;
	std::vector<std::string> DeployToAttachEffects{};
	std::vector<double> DeployToAttachChances{}; // 附加成功率，应该只对弹头有用

	bool UndeployTo = false;
	std::vector<std::string> UndeployToAttachEffects{};
	std::vector<double> UndeployToAttachChances{}; // 附加成功率，应该只对弹头有用

	virtual void Read(INIBufferReader* reader) override
	{
		DeployToAttachEffects = reader->GetList("DeployToAttachEffects", DeployToAttachEffects);
		DeployToAttachChances = reader->GetChanceList("DeployToAttachChances", DeployToAttachChances);
		DeployTo = !DeployToAttachEffects.empty();

		UndeployToAttachEffects = reader->GetList("UndeployToAttachEffects", UndeployToAttachEffects);
		UndeployToAttachChances = reader->GetChanceList("UndeployToAttachChances", UndeployToAttachChances);
		UndeployTo = !UndeployToAttachEffects.empty();

		Enable = DeployTo || UndeployTo;
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(DeployToAttachEffects)
			.Process(DeployToAttachChances)
			.Process(DeployTo)

			.Process(UndeployToAttachEffects)
			.Process(UndeployToAttachChances)
			.Process(UndeployTo)

			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		INIConfig::Load(stream, registerForChange);
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		INIConfig::Save(stream);
		return const_cast<DeployToAttachData*>(this)->Serialize(stream);
	}
#pragma endregion
private:
};


class DeploysIntoData : public INIConfig
{
public:
	bool DeployTo = false;
	DeployToTransformEntity DeploysInto{};

	bool UndeployTo = false;
	DeployToTransformEntity UndeploysInto{};

	virtual void Read(INIBufferReader* reader) override
	{
		DeploysInto.Read(reader, "DeploysInto");
		DeployTo = DeploysInto.Enable;

		UndeploysInto.Read(reader, "UndeploysInto");
		UndeployTo = UndeploysInto.Enable;

		Enable = DeployTo || UndeployTo;
	}

#pragma region save/load
	template <typename T>
	bool Serialize(T& stream)
	{
		return stream
			.Process(DeploysInto)
			.Process(DeployTo)
			.Process(UndeploysInto)
			.Process(UndeployTo)
			.Success();
	};

	virtual bool Load(ExStreamReader& stream, bool registerForChange) override
	{
		INIConfig::Load(stream, registerForChange);
		return this->Serialize(stream);
	}
	virtual bool Save(ExStreamWriter& stream) const override
	{
		INIConfig::Save(stream);
		return const_cast<DeploysIntoData*>(this)->Serialize(stream);
	}
#pragma endregion
private:
};

