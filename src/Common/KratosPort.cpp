#include <Common/KratosPort.h>

#include <Common/KratosPortDetail.h>
#include <Common/KratosUpdateUrl.h>
#include <Common/KratosVersion.h>
#include <Common.h>
#include <Version.h>
#include <Utilities/Debug.h>

#include <Windows.h>

// 静态库链接：只并入 KratosPPLib.lib，不引入额外 DLL 或文件。
#pragma comment(lib, "KratosPPLib.lib")

// ---- 端口符号引入 --------------------------------------------------------
// 本工程为 /Gz（stdcall），C 符号实际修饰为 _KL_Xxx@N。
// 端口层把全部入口逐个引入，避免"当前代码恰好没用到的入口"在链接期被丢弃，
// 也便于符号缺失时立刻在链接期暴露（LNK2001/LNK2019），而不是等到运行时。
// 与 KratosLib.h 的签名表（KratosLibApi::CheckSignatures）配合：那边管声明，
// 这边管实现。新增/删除入口时两份清单同步。
#pragma comment(linker, "/include:_KL_Initialize@8")
#pragma comment(linker, "/include:_KL_AcquireSession@0")
#pragma comment(linker, "/include:_KL_GetEntitlements@8")
#pragma comment(linker, "/include:_KL_CheckIniName@12")
#pragma comment(linker, "/include:_KL_OpenContent@28")
#pragma comment(linker, "/include:_KL_GetTable@20")
#pragma comment(linker, "/include:_KL_Update@16")
#pragma comment(linker, "/include:_KL_UpdateAsync@8")
#pragma comment(linker, "/include:_KL_GetVersion@16")
#pragma comment(linker, "/include:_KL_GetDownloadedVersion@16")
#pragma comment(linker, "/include:_KL_Tick@12")
#pragma comment(linker, "/include:_KL_Attest@20")
#pragma comment(linker, "/include:_KL_Shutdown@8")

// ---- ABI 签名比对 --------------------------------------------------------
// 编译期比对本机 <KratosLib.h> 与链接用的 KratosPPLib.lib 是否同一版 ABI。
// CheckSignatures() 是 constexpr 且返回 bool，可直接用作 static_assert 的条件。
//
// 必须直接写进 static_assert，不要先存进变量：
//   `const bool f = CheckSignatures(); static_assert(f, ...);` 在 C++20 下 f
//   不是常量表达式，MSVC 报 C2131。
static_assert(KratosLibApi::CheckSignatures(), "KratosPort: KratosLib 签名表比对未通过");

const char* KratosPort::ReasonName(KL_Reason reason)
{
	switch (reason)
	{
	case KL_Reason_None:
		return "None";
	case KL_Reason_ConflictModule:
		return "ConflictModule";
	case KL_Reason_ConflictIni:
		return "ConflictIni";
	case KL_Reason_EnvIncomplete:
		return "EnvIncomplete";
	case KL_Reason_UpdateRequired:
		return "UpdateRequired";
	case KL_Reason_AbiMismatch:
		return "AbiMismatch";
	default:
		return "Unknown";
	}
}

void KratosPort::Disable(KL_Reason reason)
{
	if (_reason == KL_Reason_None)
	{
		_reason = reason;
	}
	if (_session != 0)
	{
		// 停用后立即让会话失效，后续 KL_* 调用一律 fail-closed。
		KL_Shutdown(_session);
		_session = 0;
	}

	Debug::Log("[KratosPort] Kratos side disabled, reason = %s(%d)\n", ReasonName(_reason), (int)_reason);
}

uint64_t KratosPort::SelfHash()
{
	// 以本模块映像基址与大小派生一个稳定的自校验哈希（Windows 自动重定位 ⇒ 每次不同）。
	HMODULE self = Common::hInstance ? (HMODULE)Common::hInstance : GetModuleHandleA(INTERNAL_NAME);
	if (!self)
	{
		return 0;
	}

	const auto* base = reinterpret_cast<const uint8_t*>(self);
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
	{
		return 0;
	}
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE)
	{
		return 0;
	}

	uint64_t hash = 1469598103934665603ull; // FNV-1a 64 偏移基
	hash = (hash ^ reinterpret_cast<uintptr_t>(base)) * 1099511628211ull;
	hash = (hash ^ nt->OptionalHeader.SizeOfImage) * 1099511628211ull;
	hash = (hash ^ nt->OptionalHeader.AddressOfEntryPoint) * 1099511628211ull;
	return hash;
}

// -----------------------------------------------------------------------------
// 远程更新 URL：**编译内置在宿主侧**（不再从 rulesmd.ini / rules.ini 读取）
//
//   KratosPP/src/Common/KratosUpdateUrl.h 的 KratosUpdate::kUpdateUrl
//
// 语义（三层，逐级收紧）：
//   * 常量非空 ⇒ 原样交给 lib 的 KL_HostInfo.updateUrl，由 lib 在 KL_Initialize
//     的"启动先下载"里做**一次** http(s) 拉取（有界总预算 ~4s，同步）；
//   * 常量留空（""） ⇒ 交给 lib 的是空串 = **显式关闭远程更新**：lib 在发请求之前
//     就早退，零 WinHTTP 调用、完全不触碰网络，只用内置离线载荷；
//   * 非法地址 ⇒ lib 同样在发请求之前拒绝（零网络）。
//
// 更新地址由 KratosUpdate::kUpdateUrl 决定，不从任何 ini/配置文件读取：
// 是否联网只由这一个常量决定，构建产物即事实。本函数只取常量 + 记日志，
// 不做文件 IO、不读配置。
// -----------------------------------------------------------------------------
const char* KratosPortDetail::BuiltinUpdateUrl()
{
#ifdef KRATOS_PORT_TEST_HOOKS
	// 取证专用：测试二进制可就地覆盖为本地地址（Kratos.dll 不含这段代码）
	if (KratosUpdate::kSimulatedUpdateUrlActive)
	{
		return KratosUpdate::kSimulatedUpdateUrl;
	}
#endif
	return KratosUpdate::kUpdateUrl;
}

#ifdef KRATOS_PORT_TEST_HOOKS
void KratosPortDetail::SimulateUpdateUrl(const char* url)
{
	KratosUpdate::kSimulatedUpdateUrl[0] = '\0';
	KratosUpdate::kSimulatedUpdateUrlActive = false;
	if (!url)
	{
		return; // nullptr = 恢复"使用编译内置常量"
	}

	size_t i = 0;
	for (; url[i] != '\0' && i + 1 < KratosUpdate::kSimulatedUrlCapacity; ++i)
	{
		KratosUpdate::kSimulatedUpdateUrl[i] = url[i];
	}
	KratosUpdate::kSimulatedUpdateUrl[i] = '\0';
	KratosUpdate::kSimulatedUpdateUrlActive = true;
}
#endif

// 宿主自己的版本串（**无前缀**纯版本号），用于日志/显示。
// 直接取 Version.h 的 VERSION_PLAIN_WSTR。只用于显示/日志，不参与比较。
std::wstring KratosPort::HostVersionString()
{
	return VERSION_PLAIN_WSTR;
}

// 宿主自己的版本号（4 个数字），版本比较的唯一依据。
// 直接从 Version.h 的 VERSION_MAJOR / MINOR / REVISION / PATCH 构造。
KratosVersion::Components KratosPort::HostVersionComponents()
{
	KratosVersion::Components out{};
	out.major = static_cast<uint64_t>(VERSION_MAJOR);
	out.minor = static_cast<uint64_t>(VERSION_MINOR);
	out.revision = static_cast<uint64_t>(VERSION_REVISION);
	out.patch = static_cast<uint64_t>(VERSION_PATCH);
	return out;
}

// 取"**下载到的**"版本号（lib 侧锁内快照）。
// 来源是 lib 接口 KL_GetDownloadedVersion，即本次运行远程下载并通过验签 + 解密
// 的那份载荷里的 hostVersion。下载失败 ⇒ lib 返回空串 ⇒ 本函数返回空串。
// 两段式：先探长度，再取值（缓冲区不足时 lib 返回需要的字符数）。
std::string KratosPort::AvailableHostVersion()
{
	if (Disabled())
	{
		return {};
	}

	const uint32_t needed = KL_GetDownloadedVersion(_session, nullptr, 0);
	if (needed == 0)
	{
		return {}; // 没有"下载到的版本"：调用方按"不提示"处理
	}

	std::string out(static_cast<size_t>(needed), '\0');
	const uint32_t written = KL_GetDownloadedVersion(_session, out.data(), needed + 1u);
	if (written != needed)
	{
		return {};
	}
	out.resize(written);
	return out;
}

// 是否该提示"可更新"：载荷的 4 个数字 > 宿主本机的 4 个数字 ⇒ true。
// 任一侧无值/非法 ⇒ false（不提示）。逐位比较，高位优先，只有严格更高才提示。
bool KratosPort::UpdateAvailable()
{
	const std::string available = AvailableHostVersion();
	if (available.empty())
	{
		return false;
	}

	// 载荷侧的版本串 → 4 个数字；宿主侧直接用 4 个宏构造。
	KratosVersion::Components payload{};
	if (!KratosVersion::ParseComponents(available, &payload))
	{
		Debug::Log("[KratosPort] version check: downloaded version \"%s\" -> unparsable, no notice\n",
			available.c_str());
		return false;
	}
	const KratosVersion::Components host = HostVersionComponents();

	// 逐位比较（高位优先）。
	int cmp = 0;
	if (payload.major != host.major) { cmp = payload.major > host.major ? 1 : -1; }
	else if (payload.minor != host.minor) { cmp = payload.minor > host.minor ? 1 : -1; }
	else if (payload.revision != host.revision) { cmp = payload.revision > host.revision ? 1 : -1; }
	else if (payload.patch != host.patch) { cmp = payload.patch > host.patch ? 1 : -1; }

	if (cmp <= 0)
	{
		// 载荷不高于本机（含"更低"与"相同"）⇒ 不提示。
		Debug::Log("[KratosPort] version check: payload %llu.%llu.%llu.%llu vs host %llu.%llu.%llu.%llu"
			" -> %s, no notice\n",
			(unsigned long long)payload.major, (unsigned long long)payload.minor,
			(unsigned long long)payload.revision, (unsigned long long)payload.patch,
			(unsigned long long)host.major, (unsigned long long)host.minor,
			(unsigned long long)host.revision, (unsigned long long)host.patch,
			cmp < 0 ? "LOWER than host" : "equal to host");
		return false;
	}

	Debug::Log("[KratosPort] version check: payload %llu.%llu.%llu.%llu > host %llu.%llu.%llu.%llu"
		" -> NEWER, notice is due\n",
		(unsigned long long)payload.major, (unsigned long long)payload.minor,
		(unsigned long long)payload.revision, (unsigned long long)payload.patch,
		(unsigned long long)host.major, (unsigned long long)host.minor,
		(unsigned long long)host.revision, (unsigned long long)host.patch);
	return true;
}

bool KratosPort::Initialize()
{
	if (_session != 0)
	{
		return !Disabled();
	}

	HMODULE self = Common::hInstance ? (HMODULE)Common::hInstance : GetModuleHandleA(INTERNAL_NAME);

	_buildId = SelfHash();

	// hostVer：主版本.次版本.修订.补丁 各 8 位
	const uint32_t hostVer = (uint32_t(VERSION_MAJOR) << 24) | (uint32_t(VERSION_MINOR) << 16)
		| (uint32_t(VERSION_REVISION) << 8) | uint32_t(VERSION_PATCH);

	KL_HostInfo info{};
	info.abiVer = KL_ABI_VERSION;
	info.buildId = _buildId;
	info.hostVer = hostVer;
	info.modBase = self;
	info.modSize = 0;
	info.accountToken = nullptr;

	// 远程更新 URL：**编译内置常量**（不再读任何 ini/配置文件）。
	// 必须在 KL_Initialize 之前定稿：URL 随 KL_HostInfo 一次性交给端口层，
	// 由端口层在 KL_Initialize 内部执行"启动先下载"（同步、有界 ~4s）。
	// 常量留空 ⇒ 传空串 ⇒ lib 在发请求之前早退（零 WinHTTP、零网络）。
	const char* const builtinUrl = KratosPortDetail::BuiltinUpdateUrl();
	info.updateUrl = builtinUrl;

	if (builtinUrl == nullptr || *builtinUrl == '\0')
	{
		Debug::Log("[KratosPort] update url (builtin): EMPTY -> remote download disabled (no network); builtin offline list only\n");
	}
	else
	{
		Debug::Log("[KratosPort] update url (builtin): \"%s\" -> passed to KL_HostInfo.updateUrl (download-first inside KL_Initialize)\n", builtinUrl);
	}
	Debug::Log("[KratosPort] host version = \"%ls\" (Version.h)\n",
		HostVersionString().c_str());

	if (self)
	{
		const auto* base = reinterpret_cast<const uint8_t*>(self);
		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
		if (dos->e_magic == IMAGE_DOS_SIGNATURE)
		{
			const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
			if (nt->Signature == IMAGE_NT_SIGNATURE)
			{
				info.modSize = nt->OptionalHeader.SizeOfImage;
			}
		}
	}

	KL_Reason reason = KL_Reason_None;
	const KL_Verdict verdict = KL_Initialize(&info, &reason);
	if (verdict != KL_Verdict_Proceed)
	{
		Disable(reason == KL_Reason_None ? KL_Reason_EnvIncomplete : reason);
		return false;
	}

	const KL_Session session = KL_AcquireSession();
	if (session == 0)
	{
		Disable(KL_Reason_EnvIncomplete);
		return false;
	}

	_session = session;
	_reason = KL_Reason_None;

	// 强制引用其余端口符号，保证多 TU 依赖与符号存在性在链接期可验证。
	void* table = nullptr;
	uint32_t tableBytes = 0; // 表字节长度（含表头），布局见 KratosLib.h
	KL_GetTable(_session, KL_Table_IniFieldMap, const_cast<const void**>(&table), &tableBytes);

	// 宿主自身校验值 + 一次端口证明（仅记录前 4 字节，不用于任何本地判定）。
	uint8_t mac[32]{};
	const bool attested = KL_Attest(_session, _buildId, mac);
	(void)attested;
	(void)KL_OpenContent; // 内容通道的调用点由用户在业务侧接入

	Debug::Log("[KratosPort] session = %llu, entitlements = %llu, buildId = 0x%llX, attest = %02X%02X%02X%02X\n",
		static_cast<unsigned long long>(_session),
		static_cast<unsigned long long>(KL_GetEntitlements(_session)),
		static_cast<unsigned long long>(_buildId),
		mac[0], mac[1], mac[2], mac[3]);

	return true;
}

bool KratosPort::Disabled()
{
	return _session == 0 || _reason != KL_Reason_None;
}

KL_Reason KratosPort::LastReason()
{
	return _reason;
}

bool KratosPort::CheckIniName(const char* iniName)
{
	if (Disabled())
	{
		return false;
	}
	if (!KL_CheckIniName(_session, iniName))
	{
		Disable(KL_Reason_ConflictIni);
		return false;
	}
	return true;
}

bool KratosPort::TryUpdateAsync()
{
	if (Disabled())
	{
		return false;
	}
	if (_updateStarted)
	{
		// 一次性：已经启动过就不再启动第二次
		return false;
	}
	_updateStarted = true;

	// 传会话给端口层，由端口层执行"下载 → 按第一个 '|' 切分 → RSA 验签 →
	// AES-GCM 解密 → 写入变量/列表"（无版本比较、无守卫、无提示）。
	//
	// 真正的联网发生在 KratosPort::Initialize() → KL_Initialize()（"启动先下载"，
	// 有界总预算 ~4s；此后再无网络动作）。所以本函数在正常路径上只拿到 false ——
	// 端口层写一行 "this run's single fetch already happened at startup (one-shot)"
	// 并直接返回，不发第二次请求、不创建线程。
	//
	// 版本提示不在这里做：由 Kratos::DrawVersionText -> PollUpdateNotice() 负责。
	// 这里只复位"本次运行"的判定状态（每次运行最多提示一次）。
	_updateNoticeShown = false;
	_updateNoticeVerdict = _verdictUnknown;

	const bool started = KL_UpdateAsync(_session);
	if (!started)
	{
		// 正常路径：本次运行唯一的那次拉取已经在上面的 Initialize（KL_Initialize
		// 的"先下载"）里做完了；URL 为空/非法时同样在这里返回 false（零网络）。
		// 两种情况端口层都保持着"当前生效内容"（下载到的或内置兜底那份），Kratos 继续正常服务。
		Debug::Log("[KratosPort] online update: no new fetch issued (already downloaded at startup / url empty or invalid);"
			" contents kept, port stays usable\n");
	}
	else
	{
		Debug::Log("[KratosPort] online update: first fetch of this run performed synchronously by the lib (bounded ~4s)\n");
	}
	return started;
}

// "有更新版本"提示的门闸：**一次性判定 + 一次性放行**（三态，语义见 KratosPort.h）。
//
// 判据：调用 lib 接口读取"下载到的"版本号，在宿主侧用 Common/KratosVersion.h
// 的规则与本机版本比较；无值/解析失败 ⇒ 不提示。
//
// 时机：下载同步跑在 KL_Initialize 里（游戏主循环开始之前），所以本门闸第一次被
// 问到时结论已经定稿。Pending 分支保留只为 UI/状态机兼容。
// 判定与日志都只发生一次（缓存），调用方可以每渲染帧问一次而不刷日志。
KratosPort::UpdateNoticeState KratosPort::PollUpdateNotice()
{
	if (Disabled())
	{
		return UpdateNoticeState::Quiet;
	}
	if (_updateNoticeShown)
	{
		return UpdateNoticeState::Quiet; // 本次运行已经提示过 ⇒ 永不再提示
	}
	if (_updateNoticeVerdict == _verdictQuiet)
	{
		return UpdateNoticeState::Quiet;
	}
	if (_updateNoticeVerdict == _verdictAvailable)
	{
		_updateNoticeShown = true;
		const std::string available = AvailableHostVersion();
		const std::wstring current = HostVersionString();
		Debug::Log("[KratosPort] update available: downloaded version \"%s\" > host \"%ls\""
			" -> one-time notice granted to UI (this run only)\n",
			available.c_str(), current.c_str());
		return UpdateNoticeState::Available;
	}

	// ---- 结论定稿（每次运行只在这一处做比较与证据日志）----
	const std::string available = AvailableHostVersion();
	if (available.empty())
	{
		// 下载失败（离线/超时/非 200/验签失败/解密失败）⇒ 没有任何"下载到的版本"
		// ⇒ fail-quiet：不提示。内置兜底载荷的版本号不参与这里。
		_updateNoticeVerdict = _verdictQuiet;
		Debug::Log("[KratosPort] version check: no downloaded version (download failed/absent)"
			" -> no notice (fail-quiet); builtin offline list is in use\n");
		return UpdateNoticeState::Quiet;
	}

	const bool newer = UpdateAvailable();
	_updateNoticeVerdict = newer ? _verdictAvailable : _verdictQuiet;
	if (!newer)
	{
		return UpdateNoticeState::Quiet;
	}

	_updateNoticeShown = true;
	const std::wstring current = HostVersionString();
	Debug::Log("[KratosPort] update available: downloaded version \"%s\" > host \"%ls\""
		" -> one-time notice granted to UI (this run only)\n",
		available.c_str(), current.c_str());
	return UpdateNoticeState::Available;
}

bool KratosPort::GetTable(KL_TableId id, const void** out, uint32_t* outN)
{
	if (out)
	{
		*out = nullptr;
	}
	if (outN)
	{
		*outN = 0;
	}
	if (Disabled())
	{
		return false;
	}
	return KL_GetTable(_session, id, out, outN);
}

// 端口表布局（见 KratosLib.h）：按字节小端读取，不依赖结构体对齐/填充。
uint32_t KratosPort::ReadU32(const uint8_t* p)
{
	return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

bool KratosPort::ExpectedLayout(KL_TableId id, uint32_t* magic, uint32_t* elemSize)
{
	switch (id)
	{
	case KL_Table_IniFieldMap:
		*magic = KL_TABLE_MAGIC_INI_FIELD_MAP;
		*elemSize = KL_TABLE_ELEM_INI_FIELD;
		return true;
	case KL_Table_AeTable:
		*magic = KL_TABLE_MAGIC_AE;
		*elemSize = KL_TABLE_ELEM_AE;
		return true;
	default:
		return false;
	}
}

bool KratosPort::OpenTable(KL_TableId id, uint32_t expectMagic, uint32_t expectElemSize, TableView* out)
{
	if (out)
	{
		*out = TableView{};
	}

	uint32_t expectMagicOwn = 0;
	uint32_t expectElemSizeOwn = 0;
	// 调用方必须按 ABI 里该表的布局来取，避免"取了 A 表却按 B 表解析"
	if (!ExpectedLayout(id, &expectMagicOwn, &expectElemSizeOwn)
		|| expectMagic != expectMagicOwn || expectElemSize != expectElemSizeOwn)
	{
		return false;
	}

	const int index = static_cast<int>(id);
	if (index < 0 || index >= 2)
	{
		return false;
	}

	// 首次调用时取表 + 严格校验，结果永久缓存；失败 ⇒ fail-closed，不回退硬编码值。
	if (!ProbeTable(id))
	{
		return false;
	}

	if (out)
	{
		*out = _tables[index].view;
	}
	return true;
}

// 取表并按 ABI 布局严格校验（长度 / magic / version / elemSize / 元素区与名称池边界）
bool KratosPort::ValidateFetched(KL_TableId id, uint32_t expectMagic, uint32_t expectElemSize, TableView* out)
{
	if (out)
	{
		*out = TableView{};
	}

	const void* table = nullptr;
	uint32_t bytes = 0;
	if (!GetTable(id, &table, &bytes))
	{
		return false;
	}

	// ---- 长度 / magic / version / elemSize / 元素区边界 ----
	if (table == nullptr || bytes < KL_TABLE_HEADER_SIZE)
	{
		return false;
	}
	const uint8_t* base = static_cast<const uint8_t*>(table);
	const uint32_t magic = ReadU32(base + 0);
	const uint32_t version = ReadU32(base + 4);
	const uint32_t count = ReadU32(base + 8);
	const uint32_t elemSize = ReadU32(base + 12);

	if (magic != expectMagic || version != KL_TABLE_VERSION || elemSize != expectElemSize)
	{
		Debug::Log("[KratosPort] table %d header mismatch (magic=%08X ver=%u elem=%u)\n",
			(int)id, magic, version, elemSize);
		return false;
	}
	if (count == 0)
	{
		return false;
	}
	const uint64_t elementsEnd = uint64_t(KL_TABLE_HEADER_SIZE) + uint64_t(count) * uint64_t(elemSize);
	if (elementsEnd > uint64_t(bytes))
	{
		Debug::Log("[KratosPort] table %d truncated (count=%u elem=%u bytes=%u)\n",
			(int)id, count, elemSize, bytes);
		return false;
	}

	// ---- 字符串池边界（仅带名字的表） ----
	if (elemSize == KL_TABLE_ELEM_INI_FIELD)
	{
		for (uint32_t i = 0; i < count; ++i)
		{
			const uint8_t* elem = base + KL_TABLE_HEADER_SIZE + size_t(i) * elemSize;
			const uint32_t nameOff = ReadU32(elem + 0);
			const uint32_t nameLen = ReadU32(elem + 4);
			if (nameLen == 0 || nameLen > 128
				|| uint64_t(nameOff) + uint64_t(nameLen) > uint64_t(bytes))
			{
				Debug::Log("[KratosPort] table %d field %u name out of range\n", (int)id, i);
				return false;
			}
		}
	}

	if (out)
	{
		out->base = base;
		out->bytes = bytes;
		out->magic = magic;
		out->version = version;
		out->count = count;
		out->elemSize = elemSize;
	}
	return true;
}

bool KratosPort::TableValue(const TableView& view, uint32_t key, uint32_t* outValue)
{
	if (view.base == nullptr || view.elemSize != KL_TABLE_ELEM_AE)
	{
		return false;
	}
	for (uint32_t i = 0; i < view.count; ++i)
	{
		const uint8_t* elem = view.base + KL_TABLE_HEADER_SIZE + size_t(i) * view.elemSize;
		if (ReadU32(elem + 0) == key)
		{
			if (outValue)
			{
				*outValue = ReadU32(elem + 4);
			}
			return true;
		}
	}
	return false;
}

bool KratosPort::TableField(const TableView& view, uint32_t index,
	std::string* outName, uint32_t* outValue, uint32_t* outFlags)
{
	if (view.base == nullptr || view.elemSize != KL_TABLE_ELEM_INI_FIELD || index >= view.count)
	{
		return false;
	}
	const uint8_t* elem = view.base + KL_TABLE_HEADER_SIZE + size_t(index) * view.elemSize;
	const uint32_t nameOff = ReadU32(elem + 0);
	const uint32_t nameLen = ReadU32(elem + 4);
	if (uint64_t(nameOff) + uint64_t(nameLen) > uint64_t(view.bytes))
	{
		return false;
	}
	if (outName)
	{
		outName->assign(reinterpret_cast<const char*>(view.base + nameOff), nameLen);
	}
	if (outValue)
	{
		*outValue = ReadU32(elem + 8);
	}
	if (outFlags)
	{
		*outFlags = ReadU32(elem + 12);
	}
	return true;
}

// 表一到手就记一次证据：magic/version/count/elemSize + 首条/关键条目，
// 供"功能是否真的取到表"在运行日志里核对（不影响任何功能语义）。
void KratosPort::LogTable(const TableView& view)
{
	// magic 按字节还原成 4 个可见字符，便于与 KratosLib.h 里的定义对照
	const char m[5] = {
		char(view.magic & 0xFF), char((view.magic >> 8) & 0xFF),
		char((view.magic >> 16) & 0xFF), char((view.magic >> 24) & 0xFF), '\0'
	};

	uint32_t aeGroupLimit = 0;
	if (TableValue(view, KL_AE_KEY_GROUP_LIMIT, &aeGroupLimit))
	{
		Debug::Log("[KratosPort] AeTable magic=\"%s\" ver=%u count=%u elemSize=%u -> AeGroupLimit=%u\n",
			m, view.version, view.count, view.elemSize, aeGroupLimit);
		return;
	}

	std::string firstName;
	uint32_t firstValue = 0;
	uint32_t firstFlags = 0;
	TableField(view, 0, &firstName, &firstValue, &firstFlags);
	Debug::Log("[KratosPort] IniFieldMap magic=\"%s\" ver=%u count=%u elemSize=%u first=\"%s\" value=%u flags=%u\n",
		m, view.version, view.count, view.elemSize, firstName.c_str(), firstValue, firstFlags);
}

bool KratosPort::ProbeTable(KL_TableId id)
{
	const int index = static_cast<int>(id);
	if (index < 0 || index >= 2)
	{
		return false;
	}
	if (_tables[index].probed)
	{
		return _tables[index].usable;
	}
	_tables[index].probed = true;

	uint32_t magic = 0;
	uint32_t elemSize = 0;
	TableView view{};
	// 只有"取到且按 ABI 布局校验通过且非空"才算可用；否则功能停用，源码不留任何硬编码回退。
	_tables[index].usable = ExpectedLayout(id, &magic, &elemSize)
		&& ValidateFetched(id, magic, elemSize, &view);
	_tables[index].view = view;

	if (_tables[index].usable)
	{
		LogTable(view);
	}
	else
	{
		Debug::Log("[KratosPort] table %d unavailable, dependent feature disabled\n", index);
	}
	return _tables[index].usable;
}

bool KratosPort::UseTable(KL_TableId id, const void** out, uint32_t* outN)
{
	if (out)
	{
		*out = nullptr;
	}
	if (outN)
	{
		*outN = 0;
	}
	if (Disabled())
	{
		return false;
	}
	if (!ProbeTable(id))
	{
		return false;
	}
	return GetTable(id, out, outN);
}

void KratosPort::Shutdown(EventSystem* sender, Event e, void* args)
{
	// 处理器签名由事件系统规定（三个形参），本函数一个都不用：
	// 只做"释放会话"。显式 (void) 掉，避免 /W4 的 C4100。
	(void)sender;
	(void)e;
	(void)args;

	if (_session != 0)
	{
		KL_Shutdown(_session);
		_session = 0;
	}
}
