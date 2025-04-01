#include "OcclusionQuerySystem.h"
#include <d3d11.h>

OcclusionQuerySystem* GOcclusionSystem = nullptr;

OcclusionQuerySystem::OcclusionQuerySystem(ID3D11Device* device)
    : m_device(device)
{
}

OcclusionQuerySystem::~OcclusionQuerySystem()
{
    for (auto& [_, region] : m_regions)
    {
        if (region.Query)
            region.Query->Release();
    }
}

void OcclusionQuerySystem::BeginFrame()
{
    QueriesThisFrame = 0;
}

void OcclusionQuerySystem::QueryRegion(
    int id,
    const FBoundingBox& bounds,
    ID3D11DeviceContext* context,
    std::function<void(const FBoundingBox&)> DrawFunc)
{
    RegionData& region = m_regions[id];
    region.Bounds = bounds;

    if (!region.Query)
    {
        D3D11_QUERY_DESC desc = { D3D11_QUERY_OCCLUSION, 0 };
        m_device->CreateQuery(&desc, &region.Query);
    }

    if ((m_currentFrame - region.LastQueryFrame) >= kQueryInterval)
    {
        if (id % 4 == m_currentFrame % 4)
        {
            context->Begin(region.Query);
            DrawFunc(bounds);
            context->End(region.Query);
            region.LastQueryFrame = m_currentFrame;
        }
        //context->Begin(region.Query);
        //DrawFunc(bounds);
        //context->End(region.Query);
        //region.LastQueryFrame = m_currentFrame;
    }
}

void OcclusionQuerySystem::EndFrame(ID3D11DeviceContext* context)
{
    const int QueryGroupMod = 4;

    for (auto& [id, region] : m_regions)
    {
        if ((id % QueryGroupMod) != (m_currentFrame % QueryGroupMod)) continue;

        if (!region.Query) continue;

        UINT64 samples = 0;
        HRESULT hr = context->GetData(
            region.Query,
            &samples,
            sizeof(samples),
            D3D11_ASYNC_GETDATA_DONOTFLUSH);

        if (hr == S_OK)
        {
            region.Visible = samples > 10;
            region.LastValidFrame = m_currentFrame;
        }
    }

    m_currentFrame++;
}



bool OcclusionQuerySystem::IsRegionVisible(int id) const
{
    auto it = m_regions.find(id);
    if (it == m_regions.end())
    {
        //UE_LOG(LogLevel::Display, "First Time %d", id);
        return true; // 처음 보는 박스는 보이는 것으로 간주
    }


    const RegionData& region = it->second;
    if (((m_currentFrame - region.LastValidFrame) > kFallbackMaxAge))
    {
        //UE_LOG(LogLevel::Display, "Old Frame %d", id);
        //UE_LOG(LogLevel::Display, "Old Frame CurrentFrame %d", m_currentFrame);
        //UE_LOG(LogLevel::Display, "Old Frame LastValidFrame %d", region.LastValidFrame);
        return true; // 오래된 프레임은 보이는 것으로 간주
    }

    //UE_LOG(LogLevel::Display, "Visible %s", region.Visible ? "true" : "false");

    return region.Visible;
}