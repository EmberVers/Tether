#include "Shared/TetherCallLog.h"
#include "Shared/TetherCompat.h"

FTetherCallLog& FTetherCallLog::Get()
{
	static FTetherCallLog Instance;
	return Instance;
}

void FTetherCallLog::Append(FTetherCallRecord&& Record)
{
	FScopeLock L(&Lock);
	Entries.Add(MoveTemp(Record));
	while (Entries.Num() > Capacity)
	{
		Entries.RemoveAt(0, /*Count*/ 1, /*EAllowShrinking*/ EAllowShrinking::No);
		++TotalDropped;
	}
}

int32 FTetherCallLog::Clear()
{
	FScopeLock L(&Lock);
	const int32 N = Entries.Num();
	Entries.Reset();
	return N;
}

int32 FTetherCallLog::GetCapacity() const
{
	FScopeLock L(&Lock);
	return Capacity;
}

int32 FTetherCallLog::SetCapacity(int32 NewCap)
{
	FScopeLock L(&Lock);
	Capacity = NewCap;
	while (Entries.Num() > Capacity)
	{
		Entries.RemoveAt(0, /*Count*/ 1, /*EAllowShrinking*/ EAllowShrinking::No);
		++TotalDropped;
	}
	return Capacity;
}

int32 FTetherCallLog::GetTotalDropped() const
{
	FScopeLock L(&Lock);
	return TotalDropped;
}

TArray<FTetherCallRecord> FTetherCallLog::Snapshot(int32 MaxEntries) const
{
	FScopeLock L(&Lock);
	if (MaxEntries <= 0 || MaxEntries >= Entries.Num())
	{
		return Entries;
	}
	TArray<FTetherCallRecord> Out;
	Out.Reserve(MaxEntries);
	const int32 Start = Entries.Num() - MaxEntries;
	for (int32 i = Start; i < Entries.Num(); ++i)
	{
		Out.Add(Entries[i]);
	}
	return Out;
}
