#pragma once
#include "Core/HAL/PlatformType.h" 
#include <d3d11.h>
#include <unordered_map>
#include <functional>
#include "Define.h"
#include "Math.h"

struct RegionData {
    FBoundingBox Bounds;
    ID3D11Query* Query = nullptr;
    bool Visible = false;
    int LastQueryFrame = -1000;
    int LastValidFrame = -1;
    bool HasResult = false;
};

class OcclusionQuerySystem {
public:
    OcclusionQuerySystem(ID3D11Device* device);
    ~OcclusionQuerySystem();

    void BeginFrame();

    void QueryRegion(
        int id,
        const FBoundingBox& bounds,
        ID3D11DeviceContext* context,
        std::function<void(const FBoundingBox&)> DrawFunc);

    void EndFrame(ID3D11DeviceContext* context);

    bool IsRegionVisible(int id) const;
    int QueriesThisFrame = 0;

private:
    ID3D11Device* m_device;
    std::unordered_map<int, RegionData> m_regions;
    int m_currentFrame = 0;

    const int kQueryInterval = 4;      // 쿼리 갱신 주기 (프레임 단위)
    const int kFallbackMaxAge = 10;    // 결과 무효화까지 허용되는 최대 프레임 수

    int successCnt = 0;
    int failureCnt = 0;

};

extern OcclusionQuerySystem* GOcclusionSystem; // 외부에서 전역 관리
extern int GCurrentFrame; // 현재 프레임