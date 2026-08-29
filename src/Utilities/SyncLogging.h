#pragma once

#include <AbstractClass.h>
#include <GeneralDefinitions.h>
#include <Randomizer.h>
#include <TeamClass.h>

#include <algorithm>
#include <vector>

static constexpr unsigned int RNGCalls_Size = 4096;
static constexpr unsigned int FacingChanges_Size = 1024;
static constexpr unsigned int TargetChanges_Size = 1024;
static constexpr unsigned int DestinationChanges_Size = 1024;
static constexpr unsigned int MissionOverrides_Size = 256;
static constexpr unsigned int RangeStatus_Size = 2048;

template <typename T, unsigned int size>
class SyncLogEventBuffer
{
private:
	std::vector<T> Data;
	int LastWritePosition;
	int LastReadPosition;
	bool HasBeenFilled;
public:
	SyncLogEventBuffer() : Data(size), LastWritePosition(0), LastReadPosition(-1), HasBeenFilled(false) { };

	bool Add(T item)
	{
		Data[LastWritePosition] = item;
		LastWritePosition++;

		if (static_cast<size_t>(LastWritePosition) >= Data.size())
		{
			HasBeenFilled = true;
			LastWritePosition = 0;
			return true;
		}

		return false;
	}

	T Get()
	{
		if (!LastWritePosition && LastReadPosition == -1 && !HasBeenFilled)
			return T();

		if (LastReadPosition == -1 && HasBeenFilled)
			LastReadPosition = LastWritePosition;
		else if (LastReadPosition == -1 || static_cast<size_t>(LastReadPosition) >= Data.size())
			LastReadPosition = 0;

		return Data[LastReadPosition++];
	}

	size_t Size() { return Data.size(); }

	/// <summary>
	/// 清空环形缓冲，使下一次写入只包含新事件。
	/// </summary>
	void Clear()
	{
		LastWritePosition = 0;
		LastReadPosition = -1;
		HasBeenFilled = false;
		std::fill(Data.begin(), Data.end(), T());
	}
};

struct SyncLogEvent
{
	bool Initialized;
	unsigned int Caller;
	unsigned int Frame;

	SyncLogEvent() : Initialized(false), Caller(0), Frame(0) { }

	SyncLogEvent(unsigned int Caller, unsigned int Frame)
		: Caller(Caller), Frame(Frame)
	{
		Initialized = true;
	}
};

struct RNGCallSyncLogEvent : SyncLogEvent
{
	int Type;
	bool IsCritical;
	unsigned int Index1;
	unsigned int Index2;
	int Min;
	int Max;

	RNGCallSyncLogEvent() : SyncLogEvent() { }

	RNGCallSyncLogEvent(int Type, bool IsCritical, unsigned int Index1, unsigned int Index2, unsigned int Caller, unsigned int Frame, int Min, int Max)
		: Type(Type), IsCritical(IsCritical), Index1(Index1), Index2(Index2), Min(Min), Max(Max), SyncLogEvent(Caller, Frame)
	{ }
};

struct FacingChangeSyncLogEvent : SyncLogEvent
{
	unsigned short Facing;

	FacingChangeSyncLogEvent() : SyncLogEvent() { }

	FacingChangeSyncLogEvent(unsigned short Facing, unsigned int Caller, unsigned int Frame)
		: Facing(Facing), SyncLogEvent(Caller, Frame)
	{ }
};

struct TargetChangeSyncLogEvent : SyncLogEvent
{
	AbstractType Type;
	DWORD ID;
	AbstractType TargetType;
	DWORD TargetID;

	TargetChangeSyncLogEvent() = default;

	TargetChangeSyncLogEvent(const AbstractType& Type, const DWORD& ID, const AbstractType& TargetType, const DWORD& TargetID, unsigned int Caller, unsigned int Frame)
		: Type(Type), ID(ID), TargetType(TargetType), TargetID(TargetID), SyncLogEvent(Caller, Frame)
	{ }
};

struct MissionOverrideSyncLogEvent : SyncLogEvent
{
	AbstractType Type;
	DWORD ID;
	int Mission;

	MissionOverrideSyncLogEvent() : SyncLogEvent() { }

	MissionOverrideSyncLogEvent(const AbstractType& Type, const DWORD& ID, int Mission, unsigned int Caller, unsigned int Frame)
		: Type(Type), ID(ID), Mission(Mission), SyncLogEvent(Caller, Frame)
	{ }
};

struct RangeStatusSyncLogEvent : SyncLogEvent
{
	AbstractType Type;
	DWORD ID;
	int BaseRange;
	double RangeMultiplier;
	int RangeCell;
	int FinalRange;
	AbstractType TargetRTTI;
	DWORD TargetID;

	RangeStatusSyncLogEvent() : SyncLogEvent() { }

	RangeStatusSyncLogEvent(AbstractType Type, DWORD ID, int BaseRange, double RangeMultiplier, int RangeCell, int FinalRange, AbstractType TargetRTTI, DWORD TargetID, unsigned int Caller, unsigned int Frame)
		: Type(Type), ID(ID), BaseRange(BaseRange), RangeMultiplier(RangeMultiplier), RangeCell(RangeCell), FinalRange(FinalRange), TargetRTTI(TargetRTTI), TargetID(TargetID), SyncLogEvent(Caller, Frame)
	{ }
};

class SyncLogger
{
private:
	static SyncLogEventBuffer<RNGCallSyncLogEvent, RNGCalls_Size> RNGCalls;
	static SyncLogEventBuffer<FacingChangeSyncLogEvent, FacingChanges_Size> FacingChanges;
	static SyncLogEventBuffer<TargetChangeSyncLogEvent, TargetChanges_Size> TargetChanges;
	static SyncLogEventBuffer<TargetChangeSyncLogEvent, DestinationChanges_Size> DestinationChanges;
	static SyncLogEventBuffer<MissionOverrideSyncLogEvent, MissionOverrides_Size> MissionOverrides;
	static SyncLogEventBuffer<RangeStatusSyncLogEvent, RangeStatus_Size> RangeStatus;

	static void WriteRNGCalls(FILE* const pLogFile, int frameDigits);
	static void WriteFacingChanges(FILE* const pLogFile, int frameDigits);
	static void WriteTargetChanges(FILE* const pLogFile, int frameDigits);
	static void WriteDestinationChanges(FILE* const pLogFile, int frameDigits);
	static void WriteMissionOverrides(FILE* const pLogFile, int frameDigits);
	static void WriteRangeStatus(FILE* const pLogFile, int frameDigits);
	static void WriteBuildings(FILE* const pLogFile);

public:
	static bool Enabled;

	static void AddRNGCallSyncLogEvent(Randomizer* pRandomizer, int type, unsigned int callerAddress, int min = 0, int max = 0);
	static void AddFacingChangeSyncLogEvent(unsigned short facing, unsigned int callerAddress);
	static void AddTargetChangeSyncLogEvent(AbstractClass* pObject, AbstractClass* pTarget, unsigned int callerAddress);
	static void AddDestinationChangeSyncLogEvent(AbstractClass* pObject, AbstractClass* pTarget, unsigned int callerAddress);
	static void AddMissionOverrideSyncLogEvent(AbstractClass* pObject, int mission, unsigned int callerAddress);
	static void AddRangeStatusSyncLogEvent(AbstractClass* pObject, int baseRange, double rangeMultiplier, int rangeCell, int finalRange, AbstractClass* pTarget, unsigned int callerAddress);
	static void WriteSyncLog(const char* logFilename);
};
