#pragma once
// ============================================================================
// CopyShared — Copy 与 Trace 共用的执行工具（同源族）
//
// 两个效果器共享：候选 AE 快照结构、快照收集、线索过滤判定、来源去重、死亡回退解析。
// 观察名单语义（Watch/Ignore）与附加分发（AttachTo/AttachFrom）的差异在各自效果器内处理，
// 本文件只放两边写法完全一致的部分。
//
// 本头只被两个效果器的 .h/.cpp 引用；CopyData.h / TraceData.h（会被
// AttachEffectData.h 统一收集）不引用本头，避免与 AttachEffectData.h 形成包含环。
// ============================================================================

#include <string>
#include <vector>
#include <algorithm>

#include <GeneralStructures.h>

#include <Ext/Helper/Status.h>  // IsDeadOrInvisible
#include <Ext/Helper/Scripts.h> // TryGetAEManager

#include "CopyData.h"
#include "../EffectScript.h"    // AttachEffectData / AttachEffectScript（快照需要完整类型）

// 候选 AE 快照：完整数据 + 来源 + 名字 + 自带标记
struct CopyAEInfo
{
	AttachEffectData data;
	TechnoClass* source = nullptr;
	std::string name;
	std::vector<std::string> marks;
};

static inline bool CopyContains(const std::vector<std::string>& list, const std::string& key)
{
	return std::find(list.begin(), list.end(), key) != list.end();
}

static inline bool CopyIsDead(TechnoClass* p)
{
	return !p || IsDeadOrInvisible(p);
}

// 收集快照：对象 AE 管理器上全部正在生效的 AE（ForeachChild 只回调已激活组件；
// 快照收集完才允许后续增删操作，防遍历中增删）
static inline bool CopyCollectSnapshot(AttachEffect* aem, std::vector<CopyAEInfo>& out)
{
	if (!aem)
	{
		return false;
	}
	aem->ForeachChild([&](Component* c) {
		AttachEffectScript* ae = dynamic_cast<AttachEffectScript*>(c);
		if (!ae || !ae->IsAlive())
		{
			return; // 只收正在生效的
		}
		std::vector<std::string> marks;
		ae->GetMarks(marks); // AttachEffectScript::GetMarks（单条 AE 自带标记）
		out.push_back({ ae->AEData, ae->pSource, ae->AEData.Name, marks });
	});
	return true;
}

// 线索过滤判定：这条 AE 能否当观察线索
// 剔除带 Copy 效果器的 AE（防嵌套）-> Ignore 黑名单优先 -> Watch 白名单（空=全量）。
static inline bool CopyClueAllowed(const CopyWatchConfig& cfg, const CopyAEInfo& info)
{
	if (info.data.Copy.Enable)
	{
		return false; // 剔除带 Copy 效果器的 AE，防嵌套
	}
	if (!cfg.IgnoreTypes.empty() && CopyContains(cfg.IgnoreTypes, info.name))
	{
		return false; // 黑名单优先（AE 名）
	}
	if (!cfg.IgnoreMarks.empty() && CheckOnMarks(cfg.IgnoreMarks, info.marks))
	{
		return false; // 黑名单优先（自带标记）
	}
	if (!cfg.Watch.empty() && !CopyContains(cfg.Watch, info.name))
	{
		return false; // 白名单（AE 名）
	}
	if (!cfg.WatchMarks.empty() && !CheckOnMarks(cfg.WatchMarks, info.marks))
	{
		return false; // 白名单（自带标记）
	}
	return true;
}

// 从一份 AE 清单收集各 AE 初始来源的去重存活列表；死源剔除——死人当不了目标也当不了来源
static inline void CopyCollectSources(const std::vector<CopyAEInfo>& items, std::vector<TechnoClass*>& out)
{
	for (const CopyAEInfo& info : items)
	{
		if (CopyIsDead(info.source))
		{
			continue;
		}
		if (std::find(out.begin(), out.end(), info.source) == out.end())
		{
			out.push_back(info.source);
		}
	}
}

// AttachFrom 来源解析 + 死亡回退（只决定"来源记谁"，与贴判定无关）：
//   Source        死 -> Target 直退（不回退到 InitialSource）
//   InitialSource 死 -> Source -> 再死 -> Target
//   Target        宿主恒活，无回退
static inline TechnoClass* CopyResolveSource(
	CopyAttachFrom mode, TechnoClass* initialSource, TechnoClass* copySource, TechnoClass* host)
{
	TechnoClass* s = nullptr;
	switch (mode)
	{
	case CopyAttachFrom::Source:
		s = copySource;
		if (CopyIsDead(s))
		{
			s = host;
		}
		break;
	case CopyAttachFrom::Target:
		s = host; // 宿主恒活
		break;
	case CopyAttachFrom::InitialSource:
	default:
		s = initialSource;
		if (CopyIsDead(s))
		{
			s = copySource;
		}
		if (CopyIsDead(s))
		{
			s = host;
		}
		break;
	}
	return s;
}
