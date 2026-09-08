#include "CopyEffect.h"

#include <algorithm>

#include <Ext/Helper/Scripts.h> // TryGetAEManager
#include <Ext/Helper/Status.h>  // IsDeadOrInvisible

// 候选 AE 快照：配置原值 + 来源 + 名字 + 自带标记
struct CopyAEInfo
{
	AttachEffectData data;
	TechnoClass* source = nullptr;
	std::string name;
	std::vector<std::string> marks;
};

// 发放组：一个粘贴对象 + 它要收到的清单项（按 list 索引）
struct CopyDispatch
{
	TechnoClass* obj = nullptr;
	std::vector<size_t> items;
};

static bool CopyContains(const std::vector<std::string>& list, const std::string& key)
{
	return std::find(list.begin(), list.end(), key) != list.end();
}

static bool CopyIsDead(TechnoClass* p)
{
	return !p || IsDeadOrInvisible(p);
}

// 从一份 AE 清单收集各 AE 初始来源的去重存活列表。
// 主通道 AttachTo=InitialSource 的广播目标与 Additional 的来源名单共用（同一名单过滤
// 结果 list 的两种投影）；死源剔除——死人当不了目标也当不了来源
static void CollectSources(const std::vector<CopyAEInfo>& items, std::vector<TechnoClass*>& out)
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

// 名单语义（主通道过滤与 Additional 来源名单共用，只判"这条 AE 能否当线索/进清单"）：
// 剔除 Copy 类（含 CopyAE 自己）→ Disallow 黑名单优先 → Allow 白名单（空=放行）
static bool CopyClueAllowed(const CopyData* data, const CopyAEInfo& info)
{
	if (info.data.Copy.Enable)
	{
		return false; // 剔除带 Copy 效果器的 AE，防嵌套
	}
	if (!data->DisallowTypes.empty() && CopyContains(data->DisallowTypes, info.name))
	{
		return false; // 黑名单优先
	}
	if (!data->DisallowMarks.empty() && CheckOnMarks(data->DisallowMarks, info.marks))
	{
		return false; // 黑名单优先（命中自带标记）
	}
	if (!data->AllowTypes.empty() && !CopyContains(data->AllowTypes, info.name))
	{
		return false; // 白名单
	}
	if (!data->AllowMarks.empty() && !CheckOnMarks(data->AllowMarks, info.marks))
	{
		return false; // 白名单（自带标记）
	}
	return true;
}

void CopyEffect::OnStart()
{
	// AE 激活当帧立刻执行第 1 次拷贝（t=0 即第一次执行）
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

	// Copy.From：复制谁身上的 AE（Target=宿主，默认；Source=CopyAE 来源）
	// 复制源与 AttachTo（贴给谁）、AttachFrom（来源记谁）各自独立
	TechnoClass* fromObject = Data->From == CopyFrom::Source ? copySource : host;
	if (CopyIsDead(fromObject))
	{
		return; // 复制源无效：两个通道都无法读取，本次作废、不计次数
	}
	AttachEffect* fromAEM = nullptr;
	if (!TryGetAEManager<TechnoExt>(fromObject, fromAEM))
	{
		return; // 复制源无 AE 管理器：本次作废（fromObject==host 时必然成功）
	}

	// ---- 1. 收集快照：复制源（From 对象）身上全部正在生效的 AE ----
	// 直接调用 fromAEM->ForeachChild（Component 公开方法，只回调已激活组件）
	std::vector<CopyAEInfo> infos;
	fromAEM->ForeachChild([&](Component* c) {
		AttachEffectScript* ae = dynamic_cast<AttachEffectScript*>(c);
		if (!ae || !ae->IsAlive())
		{
			return; // 只收正在生效的
		}
		std::vector<std::string> marks;
		ae->GetMarks(marks); // 直接调用 AttachEffectScript::GetMarks（单条 AE 自带标记）
		infos.push_back({ ae->AEData, ae->pSource, ae->AEData.Name, marks });
	});
	// 快照收集完成后才做后续操作（防遍历中增删）

	// ---- 2. 过滤 -> 候选清单----
	std::vector<CopyAEInfo> list;
	for (const CopyAEInfo& info : infos)
	{
		if (CopyClueAllowed(Data, info))
		{
			list.push_back(info);
		}
	}

	// ---- 3. 判定来源死活----
	if (Data->DiscardOnInitialSourceDead)
	{
		// yes：清单级移除——来源单位已死的，所有来自它的 AE 一律移除
		auto it = std::remove_if(list.begin(), list.end(), [](const CopyAEInfo& info) {
			return CopyIsDead(info.source);
		});
		list.erase(it, list.end());
	}
	// no：保留（是否贴由 AttachTo 判定；来源解析走 ResolveSource 回退链）

	// ---- 3.5 Additional 决策（先于 Cut）----
	// 与主通道共用同一份快照与同一套名单：主清单 list 就是名单命中的 AE，
	// Additional 来源名单 = list 各 AE 的存活去重来源（CollectSources），不单独重复过滤。
	// 名单在 Cut 前已定，Cut 移除主清单源 AE 不影响这里
	bool addNeedList = Data->AdditionalAttachTo == CopyAdditionalAttachTo::InitialSource
		|| Data->AdditionalAttachFrom == CopyAttachFrom::InitialSource;
	bool addReady = !Data->AdditionalAttachEffects.empty();
	std::vector<TechnoClass*> addSources; // 仅 addNeedList 为 true 时使用
	if (addReady && addNeedList)
	{
		// 涉及来源名单时的名单来源条件：默认要求 AllowTypes/AllowMarks 至少一个非空，
		// 避免"空 = 把复制源身上全部 AE 都当线索"；Copy.AdditionalCollectAll=yes
		// 豁免该要求——名单空也放行全量检索（此时主清单 list 本就是全量，CollectSources 照常收集）。
		// 已写名单时本标签无效（名单语义优先）
		if (!Data->NeedAdditionalSourceList())
		{
			addReady = false; // 该通道本次不执行
		}
		else
		{
			CollectSources(list, addSources); // 主清单命中 AE 的来源去重（剔死源）
		}
	}

	// ---- 4. 主清单通道（清单为空则该通道跳过，不影响 Additional）----
	bool hasMain = false;
	if (!list.empty())
	{
		// 4.1 Cut：最终清单确定后立即移除复制源身上的源 AE
		if (Data->Cut)
		{
			std::vector<std::string> names;
			for (const CopyAEInfo& info : list)
			{
				if (!CopyContains(names, info.name))
				{
					names.push_back(info.name);
				}
			}
			fromAEM->DetachByName(names, true); // 现有基建：名字级移除，skipNext=true 跳过 Next 链
		}

		// 4.2 按 AttachTo 解析发放对象（死亡只影响是否贴）
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
				targetOk = false; // 粘贴对象死亡：主通道作废（本次不计数）
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
			CollectSources(list, sources); // 共享收集（与 Additional 来源名单同一逻辑）
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
			// 逐条返还：每条 AE 只发回它自己的初始来源
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
				targetOk = false; // 清单内没有任何一条的来源存活：主通道作废
			}
			break;
		}

		// 4.3 逐对象逐条标准附加（AttachFrom 独立解析来源）
		if (targetOk)
		{
			for (const CopyDispatch& d : dispatches)
			{
				// AffectTypes/NotAffectTypes（基类 FilterData 字段）：发放对象类型检查——
				// 只有名单内的对象才允许被 AttachTo（直接复用现有 CanAffectType(TechnoClass*)）
				if (!Data->CanAffectType(d.obj))
				{
					continue; // 该对象不在类型名单内：不贴给它（黑名单命中 / 白名单未命中）
				}
				AttachEffect* targetAEM = nullptr;
				if (!TryGetAEManager<TechnoExt>(d.obj, targetAEM))
				{
					continue; // 对象无 AE 管理器（极端）：跳过该对象
				}
				// OnlyAffectMarks/NotAffectMarks（基类 FilterData 字段）：发放对象标记检查——
				// 目标对象身上汇总标记（AttachEffect::GetMarks）必须通过 OnMark 白/黑名单才允许贴
				if (!Data->OnMark(targetAEM->GetMarks()))
				{
					continue; // 对象标记未过名单：不贴给它
				}
				for (size_t i : d.items)
				{
					const CopyAEInfo& info = list[i];
					TechnoClass* pSource = ResolveSource(info.source, copySource, host, Data->AttachFrom);
					// 直接调用现有基建 AttachEffect::Attach：Enable/铁幕/CanAffectType/排斥/金钱/
					// 叠加/分组/Delay/Stack 等全套校验都在内部，Copy 不做任何预检、不干预结果
					targetAEM->Attach(info.data, pSource, nullptr, CoordStruct::Empty, -1, false);
					hasMain = true;
				}
			}
		}
	}

	// ---- 5. Additional 附加（决策在 3.5 已完成：来源名单基于 Cut 前的快照；
	// 附加动作在 Cut 之后执行，Cut 不可能波及本通道刚附加的 AE）----
	bool hasAdditional = false;
	if (addReady)
	{
		hasAdditional = ExecuteAdditional(host, copySource, addNeedList, addSources);
	}

	// ---- 6. 成功判定与次数管理：任一通道有实际附加即算本次成功 ----
	if (!hasMain && !hasAdditional)
	{
		return; // 两个通道都无动作：本次作废、不计次数
	}
	_count++;
	if (Data->TriggeredTimes > 0 && _count >= Data->TriggeredTimes)
	{
		Deactivate();    // 组件公开方法：标记停用
		AE->TimeToDie(); // AttachEffectScript 公开方法：停寿命计时，由 AE 管理器帧检查统一移除
	}
}

bool CopyEffect::ExecuteAdditional(TechnoClass* host, TechnoClass* copySource, bool needList, const std::vector<TechnoClass*>& sources)
{
	// 本函数只做迭代附加：是否执行与来源名单（needList + sources）已由 ExecuteOnce 第 3.5 步
	// 基于 Cut 前的快照决策完毕，这里不再读取复制源身上的 AE
	// 附加轮数：涉及 InitialSource = 来源名单人数；否则只附加 1 次
	int rounds = needList ? static_cast<int>(sources.size()) : 1;
	bool hasAttached = false;
	for (int i = 0; i < rounds; i++)
	{
		// 本轮贴给谁（AdditionalAttachTo）
		TechnoClass* obj = nullptr;
		switch (Data->AdditionalAttachTo)
		{
		case CopyAdditionalAttachTo::Source:
			obj = copySource;
			break;
		case CopyAdditionalAttachTo::Target:
			obj = host;
			break;
		case CopyAdditionalAttachTo::InitialSource:
		default:
			obj = sources[i];
			break;
		}
		// 本轮目标死亡：不贴（目标死只影响贴不贴，与来源解析无关）
		if (CopyIsDead(obj))
		{
			continue;
		}
		// 本轮来源记谁（AdditionalAttachFrom）：与主通道共用同一套回退链——
		// Source 死 -> Target 直退；InitialSource（=本轮分发对象自己）死 -> Source -> 再死 -> Target；
		// 直接调用 ResolveSource（模式参数传 Data->AdditionalAttachFrom），返回恒为存活单位。
		// 仅当来源模式为 InitialSource 时才取 sources[i]，否则传 nullptr（该参数不会被使用）
		TechnoClass* attachFrom = ResolveSource(
			Data->AdditionalAttachFrom == CopyAttachFrom::InitialSource ? sources[i] : nullptr,
			copySource,
			host,
			Data->AdditionalAttachFrom);
		// 发放对象过滤（与主通道同套）
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
		targetAEM->Attach(Data->AdditionalAttachEffects, {}, false, attachFrom, nullptr);
		hasAttached = true;
	}
	return hasAttached;
}

TechnoClass* CopyEffect::ResolveSource(TechnoClass* initialSource, TechnoClass* copySource, TechnoClass* host, CopyAttachFrom mode)
{
	// 来源解析 + 死亡回退（只决定"来源记谁"，与贴判定无关）：
	//   Source      死 -> Target 直退（不回退到 InitialSource）
	//   InitialSource 死 -> Source -> 再死 -> Target
	//   Target     宿主恒活，无回退
	// mode 由调用方传入：主通道用 Data->AttachFrom，Additional 通道用 Data->AdditionalAttachFrom，
	// 两通道共用这一份实现，回退规则一致
	TechnoClass* s = nullptr;
	switch (mode)
	{
	case CopyAttachFrom::Source:
		s = copySource;
		if (CopyIsDead(s))
		{
			s = host; // Source -> Target 直退（不回退到 InitialSource）
		}
		break;
	case CopyAttachFrom::Target:
		s = host; // 宿主恒活，无回退
		break;
	case CopyAttachFrom::InitialSource:
	default:
		s = initialSource;
		if (CopyIsDead(s))
		{
			s = copySource; // InitialSource -> Source
		}
		if (CopyIsDead(s))
		{
			s = host; // -> Target（宿主恒活）
		}
		break;
	}
	return s;
}
