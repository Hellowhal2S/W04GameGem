// OctreeSystem.h
#pragma once

#include "Define.h"
#include "../../Core/Container/Map.h"
#include "Math.h"

class FRenderer;
class UPrimitiveBatch;
class FFrustum;
class UPrimitiveComponent;

// 템플릿 배열 정의
struct FRenderBatchData
{
    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;
    TArray<FVertexCompact> Vertices;
    TArray<UINT> Indices;
    FObjMaterialInfo MaterialInfo;
    uint32 IndicesNum = 0;
    int32 LastUsedFrame = -1;

    void CreateBuffersIfNeeded(FRenderer& Renderer);
    //void ReleaseBuffersIfUnused(int CurrentFrame, int ThresholdFrames);
};


class FOctreeNode
{
public:
    uint64 VertexBufferSizeInBytes = 0;
    uint64 IndexBufferSizeInBytes = 0;
    FBoundingBox Bounds;
    TArray<UPrimitiveComponent*> Components;
    FOctreeNode* Children[8] = {nullptr};
    bool bIsLeaf = true;
    int Depth = 0;

    TMap<FString, FRenderBatchData> CachedBatchData;

    FOctreeNode(const FBoundingBox& InBounds, int InDepth);
    ~FOctreeNode();

    void BuildBatchRenderData();
    void BuildBatchBuffers(FRenderer& Renderer);
    void Insert(UPrimitiveComponent* Component, int MaxDepth = 5);
    void Query(const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutResults) const;
    void TickBuffers(int CurrentFrame, int FrameThreshold);
    void CollectRenderNodes(const FFrustum& Frustum, TMap<FString, TArray<FRenderBatchData*>>& OutRenderMap);

    void RenderBatches(FRenderer& Renderer,const FFrustum& Frustum,const FMatrix& VP);
};

inline int GRenderDepthMin = 1; // 최소 깊이 (이보다 얕으면 스킵)
inline int GRenderDepthMax = 2; // 최대 깊이 (이보다 깊으면 스킵)
class FOctree
{
public:
    FOctree(const FBoundingBox& InBounds);
    ~FOctree();

    void Build();
    void QueryVisible(const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutResults) const;
    void DebugRenderOctreeNode(UPrimitiveBatch* PrimitiveBatch, const FOctreeNode* Node);
    FOctreeNode* GetRoot() { return Root; };

private:
    FOctreeNode* Root;
};

void DebugRenderOctreeNode(UPrimitiveBatch* PrimitiveBatch, const FOctreeNode* Node, int MaxDepth);
//FRenderer::RenderStaticMeshe에서 사용
const int FrameThreshold = 2; // 프레임 이상 사용 안 한 버퍼 제거

void RenderCollectedBatches(FRenderer& Renderer,const FMatrix& VP,const TMap<FString, TArray<FRenderBatchData*>>& RenderMap);