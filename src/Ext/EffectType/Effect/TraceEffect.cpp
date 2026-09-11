#include "TraceEffect.h"

#include "CopyShared.h" // CopyCollectSnapshot / CopyClueAllowed / CopyCollectSources / CopyResolveSource

// 从指定单位身上按名/按标记移除 AE（调用现成基建 DetachByName / DetachByMarks，
// skipNext 由配置决定是否跳过被移除 AE 的 Next 链）
static void TraceRemoveFrom(TechnoClass* obj,
	const std::vector<std::string>& names, const std::vector<std::string>& marks, bool skipNext)
{
	if (!obj || (names.empty() && marks.empty()))
	{
		return;
	}
	AttachEffect* aem = nullptr;
	if (!TryGetAEManager<TechnoExt>(obj, aem))
	{
		return; // 该单位无 AE 管理器（极端/已死亡）：无处可删
	}
	if (!names.empty())
	{
		aem->DetachByName(names, skipNext);
	}
	if (!marks.empty())
	{
		aem->DetachByMarks(marks, skipNext);
	}
}

void TraceEffect::OnStart()
{
	// AE 激活当帧立刻执行第 1 次（t=0 即第一次执行）
	ExecuteOnce();
	// 若还没到次数上限（或组件已被自己移除），启动周期等待下一轮
	if (IsActive())
	{
		_cycleTimer.Start(GetDelayFrame());
	}
}

void TraceEffect::OnUpdate()
{
	// Start 尚未发生（_started=false）或周期未到，不做任何事
	if (!_started || _cycleTimer.Expired() == false)
	{
		return;
	}
	ExecuteOnce();
	if (IsActive())
	{
		_cycleTimer.Start(GetDelayFrame()); // 周期推进：每 Delay 帧一次
	}
}

void TraceEffect::ExecuteOnce()
{
	AttachEffectScript* myAE = AE;        // 本效果器所在 AE
	TechnoClass* host = pTechno;          // 宿主（EffectScript::pTechno，宿主非 Techno 时为 null）
	if (!host)
	{
		return; // 宿主须是 Techno
	}
	TechnoClass* copySource = myAE->pSource; // 挂本 AE 的单位（Source 位 / 回退链 Source 位）

	// 有何事可做：附加（AttachEffects 非空）或任一位置的移除名单非空
	bool hasAttach = !Data->AttachEffects.empty();
	bool hasRemoveTarget = !Data->RemoveEffectsTarget.empty() || !Data->RemoveEffectsTargetMarks.empty();
	bool hasRemoveSource = !Data->RemoveEffectsSource.empty() || !Data->RemoveEffectsSourceMarks.empty();
	bool hasRemoveInit = !Data->RemoveEffectsInitialSource.empty()
		|| !Data->RemoveEffectsInitialSourceMarks.empty();
	if (!hasAttach && !hasRemoveTarget && !hasRemoveSource && !hasRemoveInit)
	{
		return; // 无事可做：本次无动作、不计次数
	}

	// ---- 观察快照：附加与 InitialSource 移除都需要追溯来源（From 定观察源）----
	bool needObserve = hasAttach || hasRemoveInit;
	std::vector<CopyAEInfo> infos;
	bool observed = false;
	if (needObserve)
	{
		TechnoClass* fromObject = Data->From == CopyFrom::Source ? copySource : host;
		if (!CopyIsDead(fromObject))
		{
			AttachEffect* fromAEM = nullptr;
			if (TryGetAEManager<TechnoExt>(fromObject, fromAEM))
			{
				CopyCollectSnapshot(fromAEM, infos);
				observed = true;
			}
		}
	}
	// 观察不可用（观察源死亡/无管理器）：依赖追溯的动作做不了
	if (needObserve && !observed)
	{
		if (!hasRemoveTarget && !hasRemoveSource)
		{
			return; // 只剩依赖观察的动作，全部不可执行
		}
		hasAttach = false;   // 附加需要线索：观察失败则不附加
		hasRemoveInit = false; // InitialSource 移除需要来源名单：观察失败则不做
	}

	// ---- 线索过滤与来源名单（观察成功才可用；附加与 InitialSource 移除共用一份）----
	std::vector<CopyAEInfo> clues;
	std::vector<TechnoClass*> sources;
	if (observed)
	{
		for (const CopyAEInfo& info : infos)
		{
			if (CopyClueAllowed(Data->Watch, info))
			{
				clues.push_back(info);
			}
		}
		bool needList = hasAttach
			&& (Data->AttachTo == CopyAttachTo::InitialSource
				|| Data->AttachFrom == CopyAttachFrom::InitialSource);
		if (hasRemoveInit || needList)
		{
			CopyCollectSources(clues, sources);
		}
	}

	// ---- 移除步骤（先于附加；三位置各自独立可同时执行）----
	if (hasRemoveTarget && !CopyIsDead(host))
	{
		TraceRemoveFrom(host, Data->RemoveEffectsTarget, Data->RemoveEffectsTargetMarks, Data->RemoveEffectsSkipNext);
	}
	if (hasRemoveSource && !CopyIsDead(copySource))
	{
		TraceRemoveFrom(copySource, Data->RemoveEffectsSource, Data->RemoveEffectsSourceMarks, Data->RemoveEffectsSkipNext);
	}
	bool hasRemoved = hasRemoveTarget || hasRemoveSource
		|| (hasRemoveInit && !sources.empty());
	if (hasRemoveInit && !sources.empty())
	{
		for (TechnoClass* s : sources)
		{
			if (CopyIsDead(s))
			{
				continue;
			}
			TraceRemoveFrom(s, Data->RemoveEffectsInitialSource, Data->RemoveEffectsInitialSourceMarks, Data->RemoveEffectsSkipNext);
		}
	}

	// ---- 附加步骤（目标 × 来源份额全组合）----
	bool hasAttached = false;
	if (hasAttach && observed)
	{
		bool needList = Data->AttachTo == CopyAttachTo::InitialSource
			|| Data->AttachFrom == CopyAttachFrom::InitialSource;
		if (needList && sources.empty())
		{
			hasAttach = false; // 无来源可发：附加不做
		}
	}
	if (hasAttach && observed)
	{
		bool needList = Data->AttachTo == CopyAttachTo::InitialSource
			|| Data->AttachFrom == CopyAttachFrom::InitialSource;

		// 目标集合：AttachTo 定贴给谁（Source=挂本 AE 的单位 / Target=宿主 / InitialSource=来源名单）
		std::vector<TechnoClass*> targets;
		switch (Data->AttachTo)
		{
		case CopyAttachTo::Source:
			if (!CopyIsDead(copySource))
			{
				targets.push_back(copySource);
			}
			break;
		case CopyAttachTo::Target:
			targets.push_back(host); // 宿主恒活
			break;
		case CopyAttachTo::InitialSource:
		default:
			targets = sources; // 名单收集时已剔死源
			break;
		}

		// 来源份额集合：AttachFrom=InitialSource 时每个名单单位一个份额，否则固定一个
		bool perSource = Data->AttachFrom == CopyAttachFrom::InitialSource;
		std::vector<TechnoClass*> froms;
		if (perSource)
		{
			froms = sources;
		}
		else
		{
			froms.push_back(CopyResolveSource(Data->AttachFrom, nullptr, copySource, host));
		}

		for (TechnoClass* obj : targets)
		{
			// 目标死亡：该目标的全部份额不贴
			if (CopyIsDead(obj))
			{
				continue;
			}
			// 发放对象过滤（基类 FilterData，读 Trace.* 键）
			if (!Data->CanAffectType(obj))
			{
				continue;
			}
			AttachEffect* targetAEM = nullptr;
			if (!TryGetAEManager<TechnoExt>(obj, targetAEM))
			{
				continue;
			}
			if (!Data->OnMark(targetAEM->GetMarks()))
			{
				continue;
			}
			for (TechnoClass* s : froms)
			{
				// Discard=yes 且该来源份额单位（名单单位）已死：该份额丢弃，走不到回退
				if (perSource && Data->DiscardOnInitialSourceDead && CopyIsDead(s))
				{
					continue;
				}
				TechnoClass* attachFrom = perSource
					? CopyResolveSource(CopyAttachFrom::InitialSource, s, copySource, host)
					: s; // 固定来源已在份额集合里解析好
				// 整串按名附加（现有基建 AttachEffect::Attach 按名列表重载，一次带全名单）
				targetAEM->Attach(Data->AttachEffects, {}, false, attachFrom, nullptr);
				hasAttached = true;
			}
		}
	}

	// ---- 成功判定与次数管理：移除或附加任一发生即算本次成功 ----
	if (!hasAttached && !hasRemoved)
	{
		return; // 本次无动作、不计次数
	}
	_count++;
	if (Data->TriggeredTimes > 0 && _count >= Data->TriggeredTimes)
	{
		Deactivate();    // 组件公开方法：标记停用
		AE->TimeToDie(); // AttachEffectScript 公开方法：停寿命计时，由 AE 管理器帧检查统一移除
	}
}
