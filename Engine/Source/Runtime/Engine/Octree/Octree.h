// OctreeSystem.h
#pragma once

#include "Define.h"
#include "../../Core/Container/Map.h"
#include "Math.h"

class UStaticMeshComponent;
class FKDTreeNode;
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
    int NodeId = 0;

    TMap<FString, FRenderBatchData> CachedBatchData;
    TMap<FString, FRenderBatchData> CachedBatchDataX5;
    TMap<FString, FRenderBatchData> CachedBatchDataX1;
    FKDTreeNode* KDTree = nullptr;
    
    FOctreeNode(const FBoundingBox& InBounds, int InDepth);
    ~FOctreeNode();

    //각 노드의 CachedBatchData 설정
    void BuildBatchRenderData();
    //사용할 노드들의 Vertex,Index 버퍼를 미리 생성
    void BuildBatchBuffers(FRenderer& Renderer);
    //CachedBatchData 전부 할당 해제. 현재 버퍼 생성 후 자동 실행
    void ClearBatchDatas();
    void ClearKDDatas(int MaxDepthKD);
    //재귀적으로 Components를 적절한 노드에 추가
    void Insert(UPrimitiveComponent* Component, int MaxDepth = 5);
    void BuildOverlappingRecursive(UPrimitiveComponent* Component);
    void BuildKDTreeRecursive();
    
    //Lazy Segtree에서 사용. FrameThreshold프레임만큼 사용하지 않은 버퍼 할당 해제. 현재 사용 X
    void TickBuffers(int CurrentFrame, int FrameThreshold);
    //현재 렌더할 노드를 결정해서 FRenderBatchData를 반환
    void CollectRenderNodes(const FFrustum& Frustum, TMap<FString, TArray<FRenderBatchData*>>& OutRenderMap);
    
    void QueryOcclusion(FRenderer& Renderer, ID3D11DeviceContext* Context, const FFrustum& Frustum);

    const int MaxQueriesPerFrame = 2000;

    UPrimitiveComponent* Raycast(const FRay& Ray, float& OutDistance) const;
    UPrimitiveComponent* RaycastWithKD(const FRay& Ray, float& OutDistance,int MaxDepthKD) const;

};
//현재 (GRenderDepthMax-GRenderDepthMin+1)*2GB만큼의 VRam 사용
inline int GRenderDepthMin = 1; // 최소 깊이 (이보다 얕으면 스킵)
inline int GRenderDepthMax = 2; // 최대 깊이 (이보다 깊으면 스킵) 2~3이 적절
class FOctree
{
public:
    FOctree(const FBoundingBox& InBounds);
    ~FOctree();
    void BuildFull();

    //모든 UPrimComp를 순회하며 Root에 삽입+WorldAABB Update
    void Build();

    FOctreeNode* GetRoot() { return Root; };

    UPrimitiveComponent* Raycast(const FRay& Ray, float& OutHitDistance) const;
    int MaxDepthKD=4;
    bool bUseKD=true;
private:
    FOctreeNode* Root;
};
//각 노드 AABB 출력
void DebugRenderOctreeNode(UPrimitiveBatch* PrimitiveBatch, const FOctreeNode* Node, int MaxDepth);
//FRenderer::RenderStaticMesh에서 사용(현재 사용 X)
const int FrameThreshold = 2; // 프레임 이상 사용 안 한 버퍼 제거


// 간단한 NodeId 생성기 (Bounds 기반 해시)
inline int MakeNodeId(const FBoundingBox& Bounds)
{
    int x = static_cast<int>(Bounds.min.x * 100); // 1cm 단위 정규화
    int y = static_cast<int>(Bounds.min.y * 100);
    int z = static_cast<int>(Bounds.min.z * 100);

    return x * 73856093 ^ y * 19349663 ^ z * 83492791;
}

//CollectRenderNodes를 통해 선별한 노드의 데이터를 렌더.
void RenderCollectedBatches(FRenderer& Renderer,const FMatrix& VP,const TMap<FString, TArray<FRenderBatchData*>>& RenderMap);
