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
	// AE 激活当帧立刻执行第 1 次拷贝（拍板：激活帧 t=0 执行第一次）
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
			for (const CopyAEInfo& info : list)
			{
				if (CopyIsDead(info.source))
				{
					continue;
				}
				if (std::find(sources.begin(), sources.end(), info.source) == sources.end())
				{
					sources.push_back(info.source);
				}
			}
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
					TechnoClass* pSource = ResolveSource(info.source, copySource, host);
					// 直接调用现有基建 AttachEffect::Attach：Enable/铁幕/CanAffectType/排斥/金钱/
					// 叠加/分组/Delay/Stack 等全套校验都在内部，Copy 不做任何预检、不干预结果
					targetAEM->Attach(info.data, pSource, nullptr, CoordStruct::Empty, -1, false);
					hasMain = true;
				}
			}
		}
	}

	// ---- 5. Additional 附加通道（与主清单并行）----
	bool hasAdditional = false;
	if (!Data->AdditionalAttachEffects.empty())
	{
		hasAdditional = ExecuteAdditional(host, copySource, fromAEM);
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

bool CopyEffect::ExecuteAdditional(TechnoClass* host, TechnoClass* copySource, AttachEffect* fromAEM)
{
	// "贴给"或"来源"任一侧写了 InitialSource 才需要来源名单
	bool needList = Data->AdditionalAttachTo == CopyAdditionalAttachTo::InitialSource
		|| Data->AdditionalAttachFrom == CopyAttachFrom::InitialSource;

	// 用户拍板：涉及来源名单时必须显式给出 AllowTypes 或 AllowMarks（至少一个非空），
	// 不允许"白名单空 = 把复制源身上全部 AE 都当线索"（用户拍板）
	if (needList && !Data->NeedAdditionalSourceList())
	{
		return false; // 无显式白名单：不读全部 AE，该通道本次不执行
	}

	// 来源名单：从复制源身上生效 AE 中筛线索（名单语义同主通道），收来源去重、死源剔除
	std::vector<TechnoClass*> sources;
	if (needList)
	{
		fromAEM->ForeachChild([&](Component* c) {
			AttachEffectScript* ae = dynamic_cast<AttachEffectScript*>(c);
			if (!ae || !ae->IsAlive())
			{
				return;
			}
			std::vector<std::string> marks;
			ae->GetMarks(marks);
			CopyAEInfo info{ ae->AEData, ae->pSource, ae->AEData.Name, marks };
			if (!CopyClueAllowed(Data, info))
			{
				return; // 不在名单（或带 Copy 类）：其来源直接丢弃
			}
			if (CopyIsDead(info.source))
			{
				return; // 来源已死：当不了目标也当不了来源
			}
			if (std::find(sources.begin(), sources.end(), info.source) == sources.end())
			{
				sources.push_back(info.source);
			}
		});
	}

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
		// 本轮来源记谁（AdditionalAttachFrom）
		TechnoClass* attachFrom = nullptr;
		switch (Data->AdditionalAttachFrom)
		{
		case CopyAttachFrom::Source:
			attachFrom = copySource;
			break;
		case CopyAttachFrom::Target:
			attachFrom = host;
			break;
		case CopyAttachFrom::InitialSource:
		default:
			attachFrom = sources[i]; // needList 为 true 时必可达
			break;
		}

		// 死亡只影响"贴不贴"与"来源可不可用"：对象死/来源死 -> 本轮跳过
		if (CopyIsDead(obj) || CopyIsDead(attachFrom))
		{
			continue;
		}
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

TechnoClass* CopyEffect::ResolveSource(TechnoClass* initialSource, TechnoClass* copySource, TechnoClass* host)
{
	// AttachFrom + 死亡回退链（只决定"来源记谁"，与 AttachTo/贴判定无关）
	TechnoClass* s = nullptr;
	switch (Data->AttachFrom)
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
