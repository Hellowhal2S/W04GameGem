// OctreeSystem.h
#pragma once

#include "Define.h"
#include "../../Core/Container/Map.h"
#include "Math.h"

struct FRay;
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
    FSphere BoundingSphere;

    TArray<UPrimitiveComponent*> Components;
    TArray<UPrimitiveComponent*> OverlappingComponents;
    FOctreeNode* Children[8] = {nullptr};
    bool bIsLeaf = true;
    int Depth = 0;

    TMap<FString, FRenderBatchData> CachedBatchData;

    FOctreeNode(const FBoundingBox& InBounds, int InDepth);
    ~FOctreeNode();

    //각 노드의 CachedBatchData 설정
    void BuildBatchRenderData();
    //사용할 노드들의 Vertex,Index 버퍼를 미리 생성
    void BuildBatchBuffers(FRenderer& Renderer);
    //CachedBatchData 전부 할당 해제. 현재 버퍼 생성 후 자동 실행
    void ClearBatchDatas(FRenderer& Renderer);

    //재귀적으로 Components를 적절한 노드에 추가
    void Insert(UPrimitiveComponent* Component, int MaxDepth = 5);
    void InsertOverlapping(UPrimitiveComponent* Component, int MaxDepth=3);

    //Octree를 순회하면서 렌더할 오브젝트 결정. 하지만 현재 사용 X
    void Query(const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutResults) const;
    //Lazy Segtree에서 사용. FrameThreshold프레임만큼 사용하지 않은 버퍼 할당 해제. 현재 사용 X
    void TickBuffers(int CurrentFrame, int FrameThreshold);
    //현재 렌더할 노드를 결정해서 FRenderBatchData를 반환
    void CollectRenderNodes(const FFrustum& Frustum, TMap<FString, TArray<FRenderBatchData*>>& OutRenderMap);
    //현재 렌더할 노드를 결정해서 바로 렌더. mat sort가 안되서 현재 사용 X
    void RenderBatches(FRenderer& Renderer,const FFrustum& Frustum,const FMatrix& VP);

    UPrimitiveComponent* Raycast(const FRay& Ray, float& OutDistance) const;
};
//현재 (GRenderDepthMax-GRenderDepthMin+1)*2GB만큼의 VRam 사용
inline int GRenderDepthMin = 1; // 최소 깊이 (이보다 얕으면 스킵)
inline int GRenderDepthMax = 2; // 최대 깊이 (이보다 깊으면 스킵) 2~3이 적절
class FOctree
{
public:
    FOctree(const FBoundingBox& InBounds);
    ~FOctree();

    //모든 UPrimComp를 순회하며 Root에 삽입+WorldAABB Update
    void Build();
    //위에 Query함수 사용. 현재 사용 X
    void QueryVisible(const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutResults) const;
    
    FOctreeNode* GetRoot() { return Root; };

    UPrimitiveComponent* Raycast(const FRay& Ray, float& OutHitDistance) const;

private:
    FOctreeNode* Root;
};
//각 노드 AABB 출력
void DebugRenderOctreeNode(UPrimitiveBatch* PrimitiveBatch, const FOctreeNode* Node, int MaxDepth);
//FRenderer::RenderStaticMesh에서 사용(현재 사용 X)
const int FrameThreshold = 2; // 프레임 이상 사용 안 한 버퍼 제거
//CollectRenderNodes를 통해 선별한 노드의 데이터를 렌더.
void RenderCollectedBatches(FRenderer& Renderer,const FMatrix& VP,const TMap<FString, TArray<FRenderBatchData*>>& RenderMap);