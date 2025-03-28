#pragma once
#include "Core/HAL/PlatformType.h"
#include "UObject/NameTypes.h"

class FWindowsPlatformTime
{
public:
	static double GSecondsPerCycle; // 0
	static bool bInitialized; // false

	static void InitTiming()
	{
		if (!bInitialized)
		{
			bInitialized = true;

			double Frequency = (double)GetFrequency();
			if (Frequency <= 0.0)
			{
				Frequency = 1.0;
			}

			GSecondsPerCycle = 1.0 / Frequency;
		}
	}
	static float GetSecondsPerCycle()
	{
		if (!bInitialized)
		{
			InitTiming();
		}
		return (float)GSecondsPerCycle;
	}
	static uint64 GetFrequency()
	{
		LARGE_INTEGER Frequency;
		QueryPerformanceFrequency(&Frequency);
		return Frequency.QuadPart;
	}
	static double ToMilliseconds(uint64 CycleDiff)
	{
		double Ms = static_cast<double>(CycleDiff)
			* GetSecondsPerCycle()
			* 1000.0;

		return Ms;
	}

	static uint64 Cycles64()
	{
		LARGE_INTEGER CycleCount;
		QueryPerformanceCounter(&CycleCount);
		return (uint64)CycleCount.QuadPart;
	}
};

struct TStatId
{
	FName Name;
	// 기본 생성자
	TStatId() = default;
	// const char* → 자동 FName 변환
	TStatId(const char* InName)
		: Name(FName(InName))
	{
	}
	// FString → 자동 FName 변환
	TStatId(const FString& InName)
		: Name(FName(InName))
	{
	}
	bool operator==(const TStatId& Other) const
	{
		return Name == Other.Name;
	}

	bool operator!=(const TStatId& Other) const
	{
		return !(Name == Other.Name);
	}
	uint32 GetId() const
	{
		return Name.GetDisplayIndex(); // ← 이걸 해시 키로 사용할 것
	}
};

typedef FWindowsPlatformTime FPlatformTime;

class FScopeCycleCounter
{
public:
	FScopeCycleCounter(TStatId StatId)
		: StartCycles(FPlatformTime::Cycles64())
		, UsedStatId(StatId)
	{
	}

	~FScopeCycleCounter()
	{
		Finish();
	}

	uint64 Finish()
	{
		const uint64 EndCycles = FPlatformTime::Cycles64();
		const uint64 CycleDiff = EndCycles - StartCycles;

		// FThreadStats::AddMessage(UsedStatId, EStatOperation::Add, CycleDiff);

		return CycleDiff;
	}
	TStatId GetStatId() const{return UsedStatId;}

private:
	uint64 StartCycles;
	TStatId UsedStatId;
};