// OctreeSystem.h
#pragma once

#include "Define.h"
#include "Map.h"
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
    void ReleaseBuffersIfUnused(int CurrentFrame, int ThresholdFrames);
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

    void RenderBatches(
        FRenderer& Renderer,
        const FFrustum& Frustum,
        const FMatrix& VP
    );
};
inline int GVertexBufferCutoffDepth = 1;//해당 수보다 깊은 노드만 버퍼 보유

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
