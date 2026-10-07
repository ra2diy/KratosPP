#pragma once
#include <stdint.h>
extern "C" {
#define KL_ABI_VERSION 2u
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

KL_Verdict KL_Initialize(const KL_HostInfo* info, KL_Reason* outReason);

KL_Session KL_AcquireSession(void);
uint64_t   KL_GetEntitlements(KL_Session s);

bool       KL_CheckIniName(KL_Session s, const char* iniName);
bool       KL_OpenContent(KL_Session s, uint32_t contentId, const void* in, uint32_t n, void* out, uint32_t* outN);

bool       KL_GetTable(KL_Session s, KL_TableId id, const void** out, uint32_t* outN);
bool       KL_Update(KL_Session s, const void* signedBlob, uint32_t n);
bool       KL_UpdateAsync(KL_Session s);
uint32_t   KL_GetVersion(KL_Session s, char* outBuf, uint32_t bufSize);

uint32_t   KL_GetDownloadedVersion(KL_Session s, char* outBuf, uint32_t bufSize);
bool       KL_Tick(KL_Session s, KL_Reason* outReason);
bool       KL_Attest(KL_Session s, uint64_t hostHash, uint8_t outMac[32]);

void       KL_Shutdown(KL_Session s);
}
