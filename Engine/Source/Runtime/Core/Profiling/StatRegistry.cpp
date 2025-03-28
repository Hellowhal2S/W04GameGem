// Core/Profiling/FStatRegistry.cpp
#include "StatRegistry.h"

#include "PlatformTime.h"
void FStatRegistry::RegisterResult(const TStatId& StatId, double InMilliseconds)
{
	const uint32 Key = StatId.GetId();
	StatMap[Key] = InMilliseconds;

	// FPS 기록은 MainFrame만 추적
	if (Key == MainFrameKey)
	{
		FStatFPSRecord& Record = MainFrameRecord;
		double Now = FPlatformTime::ToMilliseconds(FPlatformTime::Cycles64()) / 1000.0; // 초 단위

		Record.FrameHistory.Add(InMilliseconds);
		Record.LastUpdateTime = Now;

		if (InMilliseconds < Record.MinMs)
			Record.MinMs = InMilliseconds;
		if (InMilliseconds > Record.MaxMs)
			Record.MaxMs = InMilliseconds;

		// 오래된 프레임 제거
		const double MaxKeepSeconds = 5.0;
		double AccumTime = 0.0;
		int RemoveCount = 0;

		for (int i = Record.FrameHistory.Num() - 1; i >= 0; --i)
		{
			AccumTime += Record.FrameHistory[i] / 1000.0;
			if (AccumTime > MaxKeepSeconds)
			{
				RemoveCount = i;
				break;
			}
		}
		if (RemoveCount > 0)
		{
			// Replace with STL-style erase if needed
			Record.FrameHistory.RemoveAt(0, RemoveCount);
		}
	}
}

void FStatRegistry::RegisterResult(FScopeCycleCounter& Timer)
{
	const TStatId StatId = Timer.GetStatId();
	double Ms = FPlatformTime::ToMilliseconds(Timer.Finish());
	RegisterResult(StatId, Ms);
}
double FStatRegistry::GetLastMilliseconds(const TStatId& StatId)
{
	const uint32 Key = StatId.GetId();
	if (double* Value = StatMap.Find(Key))
	{
		return *Value;
	}
	return 0.0f;
}
double FStatRegistry::GetFPS(const TStatId& StatId)
{
	static TStatId Stat_Frame(StatId);
	double Ms = GetLastMilliseconds(Stat_Frame);
	return Ms > 0.0 ? 1000.0 / Ms : 0.0;
}

FStatRegistry::FFPSStats FStatRegistry::GetFPSStats(const TStatId& StatId)
{
	if (StatId.GetId() != MainFrameKey)
	{
		return {}; // MainFrame이 아니면 빈 값
	}

	FFPSStats Result;
	const TArray<double>& Frames = MainFrameRecord.FrameHistory;

	// 1초 평균
	double Total1s = 0.0;
	int Count1s = 0;
	double AccumTime = 0.0;

	for (int i = Frames.Num() - 1; i >= 0; --i)
	{
		double ms = Frames[i];
		AccumTime += ms / 1000.0;
		if (AccumTime > 1.0)
			break;

		Total1s += ms;
		++Count1s;
	}

	if (Count1s > 0)
	{
		Result.FPS_1Sec = 1000.0 / (Total1s / Count1s);
	}

	// 5초 평균
	double Total5s = 0.0;
	for (double ms : Frames)
		Total5s += ms;

	if (Frames.Num() > 0)
		Result.FPS_5Sec = 1000.0 / (Total5s / Frames.Num());

	Result.FPS_Min = 1000.0 / MainFrameRecord.MaxMs;
	Result.FPS_Max = 1000.0 / MainFrameRecord.MinMs;

	return Result;
}
void FStatRegistry::SetMainFrameStat(const TStatId& StatId)
{
	MainFrameKey = StatId.GetId();
	MainFrameRecord = FStatFPSRecord(); // 초기화
}