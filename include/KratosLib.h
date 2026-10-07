#pragma once
#include <cstdint>
extern "C" {

#define KL_ABI_VERSION 2u

#define KL_ABI_ENTRY_COUNT 13u
typedef uint64_t KL_Session;
typedef enum { KL_Verdict_Proceed = 0, KL_Verdict_Refuse = 1 } KL_Verdict;
typedef enum { KL_Reason_None = 0, KL_Reason_ConflictModule, KL_Reason_ConflictIni,
               KL_Reason_EnvIncomplete, KL_Reason_UpdateRequired, KL_Reason_AbiMismatch } KL_Reason;
typedef enum { KL_Table_IniFieldMap = 0, KL_Table_AeTable = 1 } KL_TableId;

typedef struct { uint32_t abiVer; uint64_t buildId; uint32_t hostVer;
                 void* modBase; uint32_t modSize; const char* accountToken;
                 const char* updateUrl; } KL_HostInfo;

#define KL_TABLE_VERSION 1u
#define KL_TABLE_HEADER_SIZE 16u

#define KL_TABLE_MAGIC_INI_FIELD_MAP 0x46494C4Bu
#define KL_TABLE_MAGIC_AE            0x45414C4Bu

#define KL_TABLE_ELEM_AE             8u
#define KL_TABLE_ELEM_INI_FIELD      16u

typedef struct { uint32_t magic; uint32_t version; uint32_t count; uint32_t elemSize; } KL_TableHeader;

typedef struct { uint32_t key; uint32_t value; } KL_TableU32Entry;

typedef struct { uint32_t nameOff; uint32_t nameLen; uint32_t value; uint32_t flags; } KL_TableFieldEntry;

#define KL_AE_KEY_GROUP_LIMIT 1u

#define KL_FIELD_FLAG_ONE_BASED 0x1u

/*------------------------------------------------------------------*/
/* ---- 端口入口 ----------------------------------------------------------
 * 下列函数构成 Kratos 端口层的完整接口。宿主侧统一通过
 * KratosPP/src/Common/KratosPort.* 调用；导入库在链接期按符号解析。
 * ---------------------------------------------------------------------- */
/* 初始化。Refuse 时 outReason 给出原因；abiVer 不匹配直接 Refuse。 */
KL_Verdict KL_Initialize(const KL_HostInfo* info, KL_Reason* outReason);
/* 取会话句柄。0 表示无有效会话。 */
KL_Session KL_AcquireSession(void);
/* 端口能力位。0 表示无可用能力。 */
uint64_t   KL_GetEntitlements(KL_Session s);
/* 单条名称判定；false ⇒ 调用方停用依赖该名称的功能。 */
bool       KL_CheckIniName(KL_Session s, const char* iniName);
bool       KL_OpenContent(KL_Session s, uint32_t contentId, const void* in, uint32_t n, void* out, uint32_t* outN);
/* 取端口表。成功时 *out = 表首字节、*outN = 表总字节数（含 16 字节表头）；
   失败（无有效会话 / 无数据 / 自检不过）⇒ false 且 *outN = 0。表内偏移相对表首。 */
bool       KL_GetTable(KL_Session s, KL_TableId id, const void** out, uint32_t* outN);
/* 投喂一份更新载荷。signedBlob 非空 ⇒ 用该缓冲区；为空 ⇒ 走会话 URL。
   返回 false 表示本次调用没有改变任何内容。 */
bool       KL_Update(KL_Session s, const void* signedBlob, uint32_t n);
/* 触发一次有界的在线更新。返回 false 表示本次调用没有改变任何内容。 */
bool       KL_UpdateAsync(KL_Session s);
/* 版本号快照（当前生效的那份）。返回所需字符数（不含结尾 NUL）；
   传 outBuf = nullptr / bufSize = 0 仅探长度，此时不写缓冲区。 */
uint32_t   KL_GetHostVersion(KL_Session s, char* outBuf, uint32_t bufSize);
/* 版本号快照（本次运行成功取得的那份）。缓冲区语义同上：nullptr/0 仅探长度。 */
uint32_t   KL_GetDownloadedHostVersion(KL_Session s, char* outBuf, uint32_t bufSize);
/* 环境检测结果回读（结论在 Initialize 时一次性算出，本函数只回读缓存）。 */
bool       KL_Tick(KL_Session s, KL_Reason* outReason);
/* 以 hostHash 派生一份会话证明，写入 outMac[32]。 */
bool       KL_Attest(KL_Session s, uint64_t hostHash, uint8_t outMac[32]);
/* 释放会话。调用后该会话上的一切入口一律 fail-closed。 */
void       KL_Shutdown(KL_Session s);
/*------------------------------------------------------------------*/

#ifdef __cplusplus

typedef KL_Verdict(*KL_Fn_Initialize)(const KL_HostInfo*, KL_Reason*);
typedef KL_Session(*KL_Fn_AcquireSession)(void);
typedef uint64_t(*KL_Fn_GetEntitlements)(KL_Session);
typedef bool(*KL_Fn_CheckIniName)(KL_Session, const char*);
typedef bool(*KL_Fn_OpenContent)(KL_Session, uint32_t, const void*, uint32_t, void*, uint32_t*);
typedef bool(*KL_Fn_GetTable)(KL_Session, KL_TableId, const void**, uint32_t*);
typedef bool(*KL_Fn_Update)(KL_Session, const void*, uint32_t);
typedef bool(*KL_Fn_UpdateAsync)(KL_Session);
typedef uint32_t(*KL_Fn_GetHostVersion)(KL_Session, char*, uint32_t);
typedef uint32_t(*KL_Fn_GetDownloadedHostVersion)(KL_Session, char*, uint32_t);
typedef bool(*KL_Fn_Tick)(KL_Session, KL_Reason*);
typedef bool(*KL_Fn_Attest)(KL_Session, uint64_t, uint8_t*);
typedef void(*KL_Fn_Shutdown)(KL_Session);

namespace KratosLibApi
{

	constexpr bool CheckSignatures() noexcept
	{

		[[maybe_unused]] const KL_Fn_Initialize               pin0  = &KL_Initialize;
		[[maybe_unused]] const KL_Fn_AcquireSession           pin1  = &KL_AcquireSession;
		[[maybe_unused]] const KL_Fn_GetEntitlements          pin2  = &KL_GetEntitlements;
		[[maybe_unused]] const KL_Fn_CheckIniName             pin3  = &KL_CheckIniName;
		[[maybe_unused]] const KL_Fn_OpenContent              pin4  = &KL_OpenContent;
		[[maybe_unused]] const KL_Fn_GetTable                 pin5  = &KL_GetTable;
		[[maybe_unused]] const KL_Fn_Update                   pin6  = &KL_Update;
		[[maybe_unused]] const KL_Fn_UpdateAsync              pin7  = &KL_UpdateAsync;
		[[maybe_unused]] const KL_Fn_GetHostVersion           pin8  = &KL_GetHostVersion;
		[[maybe_unused]] const KL_Fn_GetDownloadedHostVersion pin9  = &KL_GetDownloadedHostVersion;
		[[maybe_unused]] const KL_Fn_Tick                     pin10 = &KL_Tick;
		[[maybe_unused]] const KL_Fn_Attest                   pin11 = &KL_Attest;
		[[maybe_unused]] const KL_Fn_Shutdown                 pin12 = &KL_Shutdown;

		static_assert(KL_ABI_ENTRY_COUNT == 13,
			"KratosLib: 入口数与签名表不符 —— 新增/删除入口时请同步更新签名表。");
		return true;
	}
}
#endif
}
