#include "AddByExistingEffect.h"

#include "CopyShared.h" // CopyCollectSnapshot / CopyClueAllowed / CopyCollectSources / CopyResolveSource

void AddByExistingEffect::OnStart()
{
	// AE 激活当帧立刻执行第 1 次（t=0 即第一次执行）
	ExecuteOnce();
	// 若还没到次数上限（或组件已被自己移除），启动周期等待下一轮
	if (IsActive())
	{
		_cycleTimer.Start(GetDelayFrame());
	}
}

void AddByExistingEffect::OnUpdate()
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

void AddByExistingEffect::ExecuteOnce()
{
	AttachEffectScript* myAE = AE;        // 本效果器所在 AE
	TechnoClass* host = pTechno;          // 宿主（EffectScript::pTechno，宿主非 Techno 时为 null）
	if (!host)
	{
		return; // 宿主须是 Techno
	}
	TechnoClass* copySource = myAE->pSource; // 挂本 AE 的单位（Source 位 / 回退链 Source 位）

	// 附加名单为空：执行无动作、不计次数
	if (Data->AttachEffects.empty())
	{
		return;
	}

	// From：观察谁身上的 AE（Target=宿主，默认；Source=挂本 AE 的单位）
	TechnoClass* fromObject = Data->From == CopyFrom::Source ? copySource : host;
	if (CopyIsDead(fromObject))
	{
		return; // 观察源无效：本次作废、不计次数
	}
	AttachEffect* fromAEM = nullptr;
	if (!TryGetAEManager<TechnoExt>(fromObject, fromAEM))
	{
		return; // 观察源无 AE 管理器：本次作废
	}

	// ---- 1. 收集快照：观察源身上全部正在生效的 AE ----
	std::vector<CopyAEInfo> infos;
	CopyCollectSnapshot(fromAEM, infos);

	// ---- 2. Watch 过滤 -> 线索 AE（Ignore 黑名单优先 -> Watch 白名单，空=全量）----
	std::vector<CopyAEInfo> clues;
	for (const CopyAEInfo& info : infos)
	{
		if (CopyClueAllowed(Data->Watch, info))
		{
			clues.push_back(info);
		}
	}

	// ---- 3. 来源名单：AttachTo 或 AttachFrom 含 InitialSource 时需要 ----
	bool needList = Data->AttachTo == CopyAttachTo::InitialSource
		|| Data->AttachFrom == CopyAttachFrom::InitialSource;
	std::vector<TechnoClass*> sources; // 仅 needList 时使用（去重、死源剔除）
	if (needList)
	{
		CopyCollectSources(clues, sources);
		if (sources.empty())
		{
			return; // 无来源可发：本次无动作、不计次数
		}
	}

	// ---- 4. 逐轮附加：轮数 = 涉及来源名单 ? 名单人数 : 1 ----
	bool hasAttached = false;
	int rounds = needList ? static_cast<int>(sources.size()) : 1;
	for (int i = 0; i < rounds; i++)
	{
		// 本轮贴给谁（AttachTo：Source=挂本 AE 的单位 / Target=宿主 / InitialSource=名单[i]）
		TechnoClass* obj = nullptr;
		switch (Data->AttachTo)
		{
		case CopyAttachTo::Source:
			obj = copySource;
			break;
		case CopyAttachTo::Target:
			obj = host;
			break;
		case CopyAttachTo::InitialSource:
		default:
			obj = sources[i];
			break;
		}
		// 目标死亡：不贴（AttachTo=InitialSource 时名单单位死亡即由此拦下）
		if (CopyIsDead(obj))
		{
			continue;
		}
		// Discard=yes 且本轮名单单位（来源位）已死：丢弃该轮，走不到来源回退
		if (needList && Data->DiscardOnInitialSourceDead
			&& Data->AttachFrom == CopyAttachFrom::InitialSource
			&& CopyIsDead(sources[i]))
		{
			continue;
		}
		// 本轮来源记谁（AttachFrom + 死亡回退链；来源位=名单[i] 仅在来源模式为 InitialSource 时使用）
		TechnoClass* attachFrom = CopyResolveSource(
			Data->AttachFrom,
			Data->AttachFrom == CopyAttachFrom::InitialSource ? sources[i] : nullptr,
			copySource,
			host);
		// 发放对象过滤（基类 FilterData，读 AddByExisting.* 键）
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
		// 整串按名附加（现有基建 AttachEffect::Attach 按名列表重载，一次带全额外名单）
		targetAEM->Attach(Data->AttachEffects, {}, false, attachFrom, nullptr);
		hasAttached = true;
	}

	// ---- 5. 成功判定与次数管理 ----
	if (!hasAttached)
	{
		return; // 本次作废、不计次数
	}
	_count++;
	if (Data->TriggeredTimes > 0 && _count >= Data->TriggeredTimes)
	{
		Deactivate();    // 组件公开方法：标记停用
		AE->TimeToDie(); // AttachEffectScript 公开方法：停寿命计时，由 AE 管理器帧检查统一移除
	}
}
