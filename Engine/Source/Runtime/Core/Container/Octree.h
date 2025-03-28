// OctreeSystem.h
#pragma once

#include "Define.h"
#include "Map.h"
#include "Math.h"

class FRenderer;
class UPrimitiveBatch;
class FFrustum;
class UPrimitiveComponent;

struct FRenderBatchData
{
    FObjMaterialInfo MaterialInfo;
    TArray<FVertexSimple> Vertices;
    TArray<UINT> Indices;
    int IndicesNum;
    
    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;
};

class FOctreeNode
{
public:
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

    void RenderBatches(
        FRenderer& Renderer,
        const FFrustum& Frustum,
        const FMatrix& VP
    ) const;
};

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
