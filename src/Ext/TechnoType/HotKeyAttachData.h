#pragma once

#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include <Common/INI/INI.h>
#include <Ext/ObjectType/FilterData.h>
#include <Ext/Helper/StringEx.h>

class HotKeyAttachData : public FilterData
{
public:
	std::vector<std::string> AttachEffects{};
	std::vector<double> AttachChances{};
	std::vector<int> Keys{};
	bool IsEnable = false;

	HotKeyAttachData()
	{
		AffectsOwner = true;
		AffectsAllies = false;
		AffectsEnemies = false;
		AffectsCivilian = false;
	}

	virtual void Read(INIBufferReader* reader) override
	{
		Read(reader, "");
	}

	virtual void Read(INIBufferReader* reader, std::string title)
	{
		FilterData::Read(reader, title);
		AttachEffects = reader->GetList<std::string>(title + "AttachEffects", AttachEffects);
		ClearIfGetNone(AttachEffects);
		AttachChances = reader->GetChanceList(title + "AttachChances", AttachChances);
		Keys = reader->GetList<int>(title + "Keys", Keys);
		IsEnable = !AttachEffects.empty() && AffectTechno;
	}

	/// <summary>
	/// 本组配置是否响应第 group 个快捷键。
	/// 与 DPKratos 的 IsOnKey 一致：Keys 为空（或不写）⇒ 响应全部；group &lt; 1 ⇒ 全部（对应 Keys=0）。
	/// </summary>
	bool IsOnKey(int group) const
	{
		return Keys.empty() || group < 1 || std::find(Keys.begin(), Keys.end(), group) != Keys.end();
	}
};

class HotKeyAttachTypeData : public INIConfig
{
public:
	bool Enable = false;
	std::map<int, HotKeyAttachData> Datas{};

	virtual void Read(INIBufferReader* reader) override
	{
		Datas.clear();
		INIBufferReader* general = INI::GetSection(INI::Rules, INI::SectionCombatDamage);

		ReadAndAdd(0, "HotKey.", general, reader);
		for (int i = 0; i <= 127; i++)
		{
			ReadAndAdd(i, "HotKey" + std::to_string(i) + ".", general, reader);
		}

		Enable = !Datas.empty();
	}

	/// <summary>按 section 名（TechnoType 的 ID）取合并后的配置，带缓存。</summary>
	static HotKeyAttachTypeData* Get(const char* section)
	{
		return INI::GetConfig<HotKeyAttachTypeData>(INI::Rules, section)->Data;
	}

private:
	void ReadAndAdd(int index, std::string title, INIBufferReader* general, INIBufferReader* privateReader)
	{
		HotKeyAttachData data{};
		data.Read(general, title);
		if (data.IsEnable)
		{
			Datas[index] = data;
		}
		// 个体覆盖全局（与 DPKratos 的 FindAndAdd 覆盖语义一致）
		HotKeyAttachData privateData{};
		privateData.Read(privateReader, title);
		if (privateData.IsEnable)
		{
			Datas[index] = privateData;
		}
	}
};
