#include "CopyEffect.h"

#include <algorithm>

#include "CopyShared.h" // CopyAEInfo / CopyCollectSnapshot / CopyClueAllowed / CopyCollectSources / CopyResolveSource

// 发放组：一个粘贴对象 + 它要收到的清单项（按 list 索引）
struct CopyDispatch
{
	TechnoClass* obj = nullptr;
	std::vector<size_t> items;
};

void CopyEffect::OnStart()
{
	// AE 激活当帧立刻执行第 1 次（t=0 即第一次执行）
	ExecuteOnce();
	// 若还没到次数上限（或组件已被自己移除），启动周期等待下一轮
	if (IsActive())
	{
		_cycleTimer.Start(GetDelayFrame());
	}
}

void CopyEffect::OnUpdate()
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

void CopyEffect::ExecuteOnce()
{
	AttachEffectScript* myAE = AE;        // 本效果器所在 AE
	TechnoClass* host = pTechno;          // 宿主（EffectScript::pTechno，宿主非 Techno 时为 null）
	if (!host)
	{
		return; // 宿主须是 Techno
	}
	TechnoClass* copySource = myAE->pSource; // CopyAE 的来源（Source 位 / 回退链 Source 位）

	// Copy.From：观察谁身上的 AE（Target=宿主，默认；Source=CopyAE 来源）
	// 观察源与 AttachTo（贴给谁）、AttachFrom（来源记谁）各自独立
	TechnoClass* fromObject = Data->From == CopyFrom::Source ? copySource : host;
	if (CopyIsDead(fromObject))
	{
		return; // 观察源无效：本次作废、不计次数
	}
	AttachEffect* fromAEM = nullptr;
	if (!TryGetAEManager<TechnoExt>(fromObject, fromAEM))
	{
		return; // 观察源无 AE 管理器：本次作废（fromObject==host 时必然成功）
	}

	// ---- 1. 收集快照：观察源身上全部正在生效的 AE（收集完才操作，防遍历中增删）----
	std::vector<CopyAEInfo> infos;
	CopyCollectSnapshot(fromAEM, infos);

	// ---- 2. Watch 过滤 -> 候选清单（Ignore 黑名单优先 -> Watch 白名单，空=全量）----
	std::vector<CopyAEInfo> list;
	for (const CopyAEInfo& info : infos)
	{
		if (CopyClueAllowed(Data->Watch, info))
		{
			list.push_back(info);
		}
	}

	// ---- 3. DiscardOnInitialSourceDead：死源清单级移除（不回退）----
	if (Data->DiscardOnInitialSourceDead)
	{
		// yes：来源单位已死的命中 AE 一律移除
		auto it = std::remove_if(list.begin(), list.end(), [](const CopyAEInfo& info) {
			return CopyIsDead(info.source);
		});
		list.erase(it, list.end());
	}
	// no：保留（是否贴由 AttachTo 判定；来源解析走回退链）

	// 清单为空：本次无动作、不计次数
	if (list.empty())
	{
		return;
	}

	// ---- 4. Cut：最终清单确定后立即移除观察源身上属于清单名字的源 AE----
	// 跳过 Next 链；与 AttachTo 无关；执行在一切附加之前
	if (Data->Cut && fromAEM)
	{
		std::vector<std::string> names;
		for (const CopyAEInfo& info : list)
		{
			if (info.data.Copy.Enable)
			{
				continue; // 与观察过滤同规则：带 Copy 效果器的 AE 不参与 Cut
			}
			if (!CopyContains(names, info.name))
			{
				names.push_back(info.name);
			}
		}
		fromAEM->DetachByName(names, true); // 现有基建：名字级移除，skipNext=true 跳过 Next 链
	}

	// ---- 5. 按 AttachTo 解析发放对象（目标死亡只影响是否贴）----
	std::vector<CopyDispatch> dispatches;
	bool targetOk = true;
	switch (Data->AttachTo)
	{
	case CopyAttachTo::Source:
		// 整份清单 -> CopyAE 来源
		if (!CopyIsDead(copySource))
		{
			dispatches.push_back({ copySource, std::vector<size_t>{} });
			for (size_t i = 0; i < list.size(); i++)
			{
				dispatches[0].items.push_back(i);
			}
		}
		else
		{
			targetOk = false; // 粘贴对象死亡：本次作废
		}
		break;
	case CopyAttachTo::Target:
		// 整份清单 -> 宿主自己
		dispatches.push_back({ host, std::vector<size_t>{} });
		for (size_t i = 0; i < list.size(); i++)
		{
			dispatches[0].items.push_back(i);
		}
		break;
	case CopyAttachTo::InitialSource:
	{
		// 广播：先出清单内各 AE 初始来源的去重清单（死源不发），每个来源都收整份清单
		std::vector<TechnoClass*> sources;
		CopyCollectSources(list, sources);
		if (sources.empty())
		{
			targetOk = false;
			break;
		}
		std::vector<size_t> all;
		for (size_t i = 0; i < list.size(); i++)
		{
			all.push_back(i);
		}
		for (TechnoClass* src : sources)
		{
			dispatches.push_back({ src, all });
		}
		break;
	}
	case CopyAttachTo::Return:
		// 逐条返还：每条命中 AE 只发回它自己的初始来源
		for (size_t i = 0; i < list.size(); i++)
		{
			TechnoClass* src = list[i].source;
			if (CopyIsDead(src))
			{
				continue; // 返还对象已死：该条不贴
			}
			CopyDispatch* found = nullptr;
			for (CopyDispatch& d : dispatches)
			{
				if (d.obj == src)
				{
					found = &d;
					break;
				}
			}
			if (!found)
			{
				dispatches.push_back({ src, std::vector<size_t>{} });
				found = &dispatches.back();
			}
			found->items.push_back(i);
		}
		if (dispatches.empty())
		{
			targetOk = false; // 清单内没有任何一条的来源存活：本次作废
		}
		break;
	}

	// ---- 6. 逐对象逐条标准附加（来源=AttachFrom 独立解析；发放过滤=基类 FilterData）----
	bool hasAttached = false;
	if (targetOk)
	{
		for (const CopyDispatch& d : dispatches)
		{
			// AffectTypes/NotAffectTypes（基类 FilterData 字段）：发放对象类型检查——
			// 只有名单内的对象才允许被贴（直接复用现有 CanAffectType(TechnoClass*)）
			if (!Data->CanAffectType(d.obj))
			{
				continue; // 该对象不在类型名单内：不贴给它
			}
			AttachEffect* targetAEM = nullptr;
			if (!TryGetAEManager<TechnoExt>(d.obj, targetAEM))
			{
				continue; // 对象无 AE 管理器（极端）：跳过该对象
			}
			// OnlyAffectMarks/NotAffectMarks（基类 FilterData 字段）：发放对象标记检查
			if (!Data->OnMark(targetAEM->GetMarks()))
			{
				continue; // 对象标记未过名单：不贴给它
			}
			for (size_t i : d.items)
			{
				const CopyAEInfo& info = list[i];
				TechnoClass* pSource = CopyResolveSource(Data->AttachFrom, info.source, copySource, host);
				// 直接调用现有基建 AttachEffect::Attach：Enable/铁幕/排斥/金钱/叠加/分组/
				// Delay/Stack 等全套校验都在内部，Copy 不做任何预检、不干预结果
				targetAEM->Attach(info.data, pSource, nullptr, CoordStruct::Empty, -1, false);
				hasAttached = true;
			}
		}
	}

	// ---- 7. 成功判定与次数管理：有实际附加才算本次成功 ----
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
