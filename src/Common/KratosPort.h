#pragma once

#include <KratosLib.h>

#include <Common/KratosVersion.h>

#include <cstdint>
#include <string>

class EventSystem;
class Event;

/// <summary>
/// 宿主侧的端口适配层：Kratos 把 KratosPPLib 当作一个"服务端口"使用。
///
/// 契约：
///  - 只有 KL_Initialize 返回 Proceed 且拿到会话，端口才算可用；
///  - 端口不可用（Refuse / 无会话）时 fail-closed：停用 Kratos 侧对应功能，
///    绝不让宿主游戏读盘失败，也绝不触碰存档或用户数据；
///  - 取不到表就不使用任何硬编码默认表，直接停用依赖它的功能。
///
/// 本层不提供任何"lib 不可用时的替代实现"。
/// </summary>
class KratosPort
{
public:
	/// <summary>
	/// 初始化端口并发起一次会话。DLL 装载完成后（ExeRun）调用。
	/// 本步内 KL_Initialize 先做**一次**有界的远程下载（总预算 ~4s，同步）：
	/// 成功 ⇒ 用下载到的名单/版本做一次性环境检测；
	/// 失败（离线/超时/非 200/验签失败/解密失败）⇒ 用内置离线载荷。
	/// 返回 true 表示端口可用。
	/// </summary>
	static bool Initialize();

	/// <summary>
	/// 端口是否已被停用（refuse 或运行时撤销）。
	/// </summary>
	static bool Disabled();

	/// <summary>
	/// 停用原因（KL_Reason）。端口正常时为 KL_Reason_None。
	/// </summary>
	static KL_Reason LastReason();

	/// <summary>
	/// 该 INI 名是否允许装载。false ⇒ 停用 Kratos 侧功能并记录原因。
	/// </summary>
	static bool CheckIniName(const char* iniName);

	/// <summary>
	/// 环境检测是一次性的：完整检测（已加载模块名 + 游戏目录内名单 INI 文件名）
	/// 只在 Initialize() 里的 KL_Initialize 做一次，结论缓存在端口会话里。
	/// 因此本层不再有"逻辑帧心跳"：原先注册在 LogicUpdateEvent 上的
	/// KratosPort::Tick 已整体移除，逻辑帧路径不再调用端口层任何函数。
	/// </summary>

	/// <summary>
	/// 一次性的在线名单更新调用点：让端口层执行"下载 → 按第一个 '|' 切分 → RSA 验签
	/// → AES-GCM 解密 → 写入变量/列表"（**不做版本比较、没有单调守卫、不做提示**）。
	///
	/// 真正的联网发生在 KratosPort::Initialize() → KL_Initialize() 的"启动先下载"里，
	/// 所以本函数在正常路径上返回 false：端口层写一行
	/// "single fetch already happened at startup (one-shot)" 就直接返回，
	/// 不发第二次请求、不创建任何线程。
	///
	/// URL **编译内置在 KratosPP 侧**（KratosPP/src/Common/KratosUpdateUrl.h 的
	/// KratosUpdate::kUpdateUrl），**不再从 rulesmd.ini 读取**：
	///   常量留空 ⇒ 传给端口层的是空串 ⇒ 初始化时**完全不做远程下载**
	///   （不发 WinHTTP 请求、不触碰网络），此时本函数也返回 false。
	/// 由 Kratos::ExeRun 在端口初始化成功后调用一次。
	/// </summary>
	static bool TryUpdateAsync();

	/// <summary>
	/// 宿主自己的版本串（**无前缀**纯版本号，形如 L"0.2.4" / L"0.2.4p1"）。
	/// 单一来源：Version.h 的 VERSION_PLAIN_WSTR。
	/// ⚠ **只用于显示/日志**；版本比较请用 HostVersionComponents()。
	/// </summary>
	static std::wstring HostVersionString();

	/// <summary>
	/// 宿主自己的版本号：**4 个数字**（主.次.修订.补丁），直接从 Version.h 的
	/// VERSION_MAJOR / MINOR / REVISION / PATCH 构造，不经过任何字符串。
	/// 版本比较的唯一依据 —— 与载荷侧解析出的 4 个数字逐位比。
	/// 因为是数字而非字符串，**不区分 DEBUG / Release**（DEBUG 只影响显示前缀）。
	/// </summary>
	static KratosVersion::Components HostVersionComponents();

	/// <summary>
	/// **下载到的**版本号（lib 侧锁内快照，纯 ASCII）—— 调用 lib 接口
	/// KL_GetDownloadedVersion 得到。
	/// 只有"本次运行远程下载 + 验签 + AES-GCM 解密全部成功"才有值；
	/// 下载失败（离线/超时/非 200/验签失败/解密失败）/ 端口停用 ⇒ 空串 ⇒ 不提示（fail-quiet）。
	/// </summary>
	static std::string AvailableHostVersion();

	/// <summary>
	/// "下载到的版本"是否比宿主版本新 ⇒ 该提示玩家"有更新版本"。
	/// 判据是**4 个数字逐位比较**（见 KratosVersion.h 的 ParseComponents）：
	///   * 只有**严格更高**才提示；**更低或相同一律不提示**
	///     （lib 完全可能提供一个比当前更低的版本号，那不是"有更新"）；
	///   * 任一侧无值/非法 ⇒ false（**不提示**，fail-quiet）；
	///   * **不区分 DEBUG / Release** —— 两侧数字同源于 Version.h 的 4 个宏，
	///     DEBUG 只改变显示前缀，不改变数字。
	/// 只在结论定稿时被调用一次（结论缓存在 PollUpdateNotice 里），
	/// 因此它的证据日志每次运行最多一行，不会刷屏。
	/// </summary>
	static bool UpdateAvailable();

	/// <summary>
	/// "有更新版本"提示的门闸：**一次性判定 + 一次性放行**（三态）。
	///   Pending   = 结论还没出来（兼容保留；当前路径下下载已在 KL_Initialize 里
	///               同步完成，所以第一次调用就会给出最终结论）；
	///   Available = 结论是"下载到的版本比本机新"，**且这是唯一一次返回 Available**
	///               （下一次调用即变成 Quiet ⇒ 每次运行最多提示一次）；
	///   Quiet     = 不会有提示：端口停用 / 已放行过 / 版本不更新 / **没有下载到版本**
	///               （下载失败 ⇒ fail-quiet，绝不用内置兜底载荷的版本号去提示）。
	/// 结论与证据日志都只产生一次（缓存），所以调用方"每渲染帧问一次"既不刷日志也不重复提示。
	/// 判定完全在宿主侧完成（lib 只给版本串快照），不参与任何同步/逻辑帧行为。
	/// </summary>
	enum class UpdateNoticeState { Pending, Available, Quiet };
	static UpdateNoticeState PollUpdateNotice();

	/// <summary>
	/// 取端口提供的表。取不到（含端口停用）⇒ 返回 false，调用方必须停用对应功能，
	/// 不得回退到硬编码表或默认值。
	/// *outN 是表的字节长度（含 16 字节表头），布局见 KratosLib.h。
	/// </summary>
	static bool GetTable(KL_TableId id, const void** out, uint32_t* outN);

	/// <summary>
	/// 表可用性闸门（结果缓存）。首次取不到或布局校验不过即永久停用该表对应的功能：
	/// 源码中不再保留任何硬编码表或默认值可供回退。
	/// </summary>
	static bool UseTable(KL_TableId id, const void** out, uint32_t* outN);

	/// <summary>
	/// 已通过严格校验的端口表视图（布局见 KratosLib.h）。
	/// 全部按字节偏移读取，不依赖结构体对齐/填充，与 lib 侧布局严格一致。
	/// </summary>
	struct TableView
	{
		const uint8_t* base = nullptr;
		uint32_t bytes = 0;    // 表的总字节长度（含表头），来自 KL_GetTable 的 outN
		uint32_t magic = 0;    // 校验通过的表头 magic
		uint32_t version = 0;  // 校验通过的表头 version
		uint32_t count = 0;    // 元素个数
		uint32_t elemSize = 0; // 元素定长尺寸
	};

	/// <summary>
	/// 取表并做完整校验：bytes ≥ 表头、magic、version、elemSize、
	/// count×elemSize 边界、以及每个字符串 [nameOff, nameOff+nameLen) 落在表内。
	/// 任一不满足 ⇒ 返回 false（调用方按"取不到表"处理，fail-closed）。
	/// </summary>
	static bool OpenTable(KL_TableId id, uint32_t expectMagic, uint32_t expectElemSize, TableView* out);

	/// <summary>
	/// 在已校验的表中按 key 取 u32 元素（AeTable：{ key, value }）。找不到 ⇒ false。
	/// </summary>
	static bool TableValue(const TableView& view, uint32_t key, uint32_t* outValue);

	/// <summary>
	/// 在已校验的表中取第 index 个字段元素（IniFieldMap：{ nameOff, nameLen, value, flags }）。
	/// 名称池是定长字节串（非 0 结尾），按 nameLen 取。
	/// </summary>
	static bool TableField(const TableView& view, uint32_t index,
		std::string* outName, uint32_t* outValue, uint32_t* outFlags);

	/// <summary>
	/// 宿主进程内的自身校验哈希（用于 KL_Attest 的入参）。
	/// </summary>
	static uint64_t SelfHash();

	/// <summary>
	/// ExeTerminate 时释放会话。
	/// </summary>
	static void Shutdown(EventSystem* sender, Event e, void* args);

private:
	static void Disable(KL_Reason reason);
	static const char* ReasonName(KL_Reason reason);

	// 远程更新 URL 的**唯一来源**（Common/KratosUpdateUrl.h 的编译内置常量，
	// 空串 = 关闭远程更新）与取证覆盖入口，都声明在 Common/KratosPortDetail.h
	// （只属于 KratosPort.cpp）。本层不读任何 ini/配置文件、不做任何文件 IO。

	static bool ProbeTable(KL_TableId id);
	static bool ExpectedLayout(KL_TableId id, uint32_t* magic, uint32_t* elemSize);
	static bool ValidateFetched(KL_TableId id, uint32_t expectMagic, uint32_t expectElemSize, TableView* out);
	static uint32_t ReadU32(const uint8_t* p);
	static void LogTable(const TableView& view);

	// 每张表只探一次：结果（含已校验的视图）永久缓存
	struct TableSlot
	{
		bool probed = false;
		bool usable = false;
		TableView view{};
	};

	inline static KL_Session _session = 0;
	inline static KL_Reason _reason = KL_Reason_None;
	inline static uint64_t _buildId = 0;
	inline static bool _updateStarted = false;

	// ---- "有更新版本"一次性判定/放行的状态（见 PollUpdateNotice）----
	// 结论缓存：0 = 还没定稿（可能只是后台还没拉到），1 = 可更新，2 = 不提示
	enum : int { _verdictUnknown = 0, _verdictAvailable = 1, _verdictQuiet = 2 };
	inline static int _updateNoticeVerdict = _verdictUnknown;
	// 已经放行过 ⇒ 每次运行最多提示一次
	inline static bool _updateNoticeShown = false;

	inline static TableSlot _tables[2]{};
};
