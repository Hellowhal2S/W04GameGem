// Profiling/FStatRegistry.h
#pragma once
#include "Core/Container/Array.h"
#include "Core/Container/Map.h"

extern int GCurrentFrame; // 글로벌 프레임 카운터 선언

// 프레임 업데이트 매크로 (렌더링 루프에서 매 프레임 증가 필요)
#define ADVANCE_FRAME() (++GCurrentFrame)

class FScopeCycleCounter;
struct TStatId;
struct FStatFPSRecord
{
	TArray<double> FrameHistory;  // ms 기록들 (최대 5초치)
	double LastUpdateTime = 0.0;  // 마지막 기록 시간
	double MinMs = DBL_MAX;
	double MaxMs = 0.0;
};
class FStatRegistry
{
public:
	static void RegisterResult(const TStatId& StatId, double InMilliseconds);
    static double RegisterResult(FScopeCycleCounter& Timer);
	static double GetLastMilliseconds(const TStatId& StatId);
	struct FFPSStats
	{
		double FPS_1Sec = 0.0;
		double FPS_5Sec = 0.0;
		double FPS_Min = 0.0;
		double FPS_Max = 0.0;
	    double ms = 0.0;
	};
	static double GetFPS(const TStatId& StatId);
    static const TMap<uint32, double>& GetStatMap() { return StatMap; }
	
	static FFPSStats GetFPSStats(const TStatId& StatId);
	static void SetMainFrameStat(const TStatId& StatId);
private:
    inline static TMap<uint32, double> StatMap; // ← GetDisplayIndex 기반으로 저장
	inline static FStatFPSRecord MainFrameRecord;
	inline static uint32 MainFrameKey = 0; // 최초 설정 이후 고정
};
