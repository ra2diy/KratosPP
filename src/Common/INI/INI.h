#pragma once

#include "INIReaderManager.h"

#include <string>
#include <vector>

#include <Common/EventSystems/EventSystem.h>
#include <Common/KratosSession.h>

using Dependency = std::vector<std::string>;
using GetDependency = Dependency(*)();

/// <summary>
/// INI操作类，读取配置信息
/// INI.GetConfig<T>(INI.Rules, section);
/// </summary>
class INI
{
public:

#pragma region MakeDependency

	static bool ICaseCompare(std::string_view a, std::string_view b)
	{
		if (a.length() == b.length())
		{
			return std::equal(a.begin(), a.end(), b.begin(),
				[](char a, char b)
				{ return tolower(a) == tolower(b); });
		}
		return false;
	}

	/// <summary>
	/// 获取ini文件名列表，如：
	/// rules = [Map.ini, GameMode.ini, Rulesmd.ini]
	/// art = [artmd.ini]
	/// ai = [Map.ini, Aimd.ini]
	/// </summary>
	/// <param name="iniName"></param>
	/// <returns></returns>
	static Dependency MakeDependency(std::string_view iniFileName)
	{
		std::string fileName = iniFileName.data();
		auto it = s_Dependency.find(fileName);
		if (it != s_Dependency.end())
		{
			return CheckNames(it->second);
		}
		std::string rulesName = INIConstant::GetRulesName().data();
		if (ICaseCompare(fileName, rulesName))
		{
			if (SessionClass::Instance->GameMode == GameMode::Campaign)
			{
				// Map.ini, rulesmd.ini
				const std::vector<std::string> d{ INIConstant::GetMapName().data(), rulesName };
				s_Dependency[rulesName] = d;
				return CheckNames(d);
			}
			// Map.ini, GameMode.ini, rulesmd.ini
			const std::vector<std::string> d{ INIConstant::GetMapName().data(), INIConstant::GetGameModeName().data(), rulesName };
			s_Dependency[rulesName] = d;
			return CheckNames(d);
		}
		std::string artName = INIConstant::GetArtName().data();
		if (ICaseCompare(fileName, artName))
		{
			// artmd.ini
			const std::vector<std::string> d{ artName };
			s_Dependency[artName] = d;
			return CheckNames(d);
		}
		std::string aiName = INIConstant::GetAIName().data();
		if (ICaseCompare(fileName, aiName))
		{
			// Map.ini, aimd.ini
			const std::vector<std::string> d{ INIConstant::GetMapName().data(), aiName };
			s_Dependency[aiName] = d;
			return CheckNames(d);
		}
		const std::vector<std::string> d{ fileName };
		return CheckNames(d);
	}

	static Dependency CheckNames(const Dependency& dependency)
	{
		for (const std::string& name : dependency)
		{
			if (KratosLib::Session != 0 && !KL_CheckIniName(KratosLib::Session, name.c_str()))
			{
				Debug::Log("[Kratos] ini name rejected: \"%s\"\n", name.c_str());
				KL_Shutdown(KratosLib::Session);
				KratosLib::Session = 0;
			}
		}

		return dependency;
	}
#pragma endregion

#pragma region GetDependency
	static Dependency GetRules()
	{
		if (s_Rules.empty())
		{
			s_Rules = MakeDependency(INIConstant::GetRulesName());
		}
		return s_Rules;
	}

	static Dependency GetArt()
	{
		if (s_Art.empty())
		{
			s_Art = MakeDependency(INIConstant::GetArtName());
		}
		return s_Art;
	}

	static Dependency GetAI()
	{
		if (s_AI.empty())
		{
			s_AI = MakeDependency(INIConstant::GetAIName());
		}
		return s_AI;
	}
#pragma endregion

#pragma region Read INI
	static void ClearBuffer(EventSystem* sender, Event e, void* args)
	{
		s_Rules.clear();
		s_Art.clear();
		s_AI.clear();

		INIReaderManager::ClearBuffer(sender, e, args);
	}

	static INIBufferReader* GetSection(GetDependency dependency, const char* section)
	{
		return INIReaderManager::FindBufferReader(dependency(), section);
	}

	template <typename TConfig>
	static INIConfigReader<TConfig>* GetConfig(GetDependency dependency, const char* section)
	{
		return INIReaderManager::FindConfigReader<TConfig>(dependency(), section);
	}

	static bool HasSection(GetDependency dependency, const char* section)
	{
		for (std::string fileName : dependency())
		{
			if (INIBufferManager::FindFile(fileName)->HasSection(section))
			{
				return true;
			}
		}
		return false;
	}
#pragma endregion

	inline static GetDependency Rules = GetRules;
	inline static GetDependency Art = GetArt;
	inline static GetDependency AI = GetAI;

	inline static const char* SectionGeneral = "General";
	inline static const char* SectionCombatDamage = "CombatDamage";
	inline static const char* SectionAudioVisual = "AudioVisual";
	inline static const char* SectionAI = "AI";

private:
	// ini文件名，文件名列表
	inline static std::map<std::string, Dependency> s_Dependency{};

	inline static Dependency s_Rules{};
	inline static Dependency s_Art{};
	inline static Dependency s_AI{};

};
