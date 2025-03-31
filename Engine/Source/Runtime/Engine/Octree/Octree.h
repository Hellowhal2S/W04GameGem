// OctreeSystem.h
#pragma once

#include "Define.h"
#include "../../Core/Container/Map.h"
#include "Math.h"

struct FDrawRange;
class FOctreeNode;
class UStaticMeshComponent;
class FKDTreeNode;
struct FRay;
class FRenderer;
class UPrimitiveBatch;
class FFrustum;
class UPrimitiveComponent;
enum class ELODLevel : uint8
{
    LOD0 = 0,
    LOD1,
    LOD2,
};
/*
FString MakeBatchKey(const FString& MatName, ELODLevel LOD)
{
    return MatName + TEXT("_LOD") + FString::FromInt(static_cast<int32>(LOD));
}*/
// 템플릿 배열 정의
struct FRenderBatchRootData
{
    TMap<ELODLevel, ID3D11Buffer*> VertexBuffers;
    TMap<ELODLevel, ID3D11Buffer*> IndexBuffers;
    TMap<ELODLevel,TArray<FVertexCompact>> Vertices;
    TMap<ELODLevel,TArray<UINT>> Indices;
    int32 LastUsedFrame = -1;

    //void CreateBuffersIfNeeded(FRenderer& Renderer);
};

struct FRenderBatchNodeData
{
    FObjMaterialInfo MaterialInfo;
    uint32 IndicesNum = 0;             // 노드의 전체 인덱스 수
    FOctreeNode* OwnerNode = nullptr;  // 이 데이터를 소유한 노드
    TMap<ELODLevel, FDrawRange> LODDrawRanges;
};

struct FDrawRange
{
    uint32 IndexStart = 0;
    uint32 IndexCount = 0;
};


class FOctreeNode
{
public:
    FOctreeNode* Parent = nullptr;
    int ChildIndex = -1; // 0~7 중 본인이 부모에서 몇 번째인지
    uint64 VertexBufferSizeInBytes = 0;
    uint64 IndexBufferSizeInBytes = 0;
    FBoundingBox Bounds;
    FSphere BoundingSphere;

    TArray<UPrimitiveComponent*> Components;
    TArray<UPrimitiveComponent*> OverlappingComponents;
    FOctreeNode* Children[8] = {nullptr};
    bool bIsLeaf = true;
    int Depth = 0;

    TMap<FString, FRenderBatchRootData> CachedBatchRootData;
    TMap<FString, FRenderBatchNodeData> CachedBatchNodeData;
    FKDTreeNode* KDTree = nullptr;

    FOctreeNode(const FBoundingBox& InBounds, int InDepth);
    ~FOctreeNode();

    //각 노드의 CachedBatchData 설정
    void BuildBatchRenderData(FOctreeNode* RootNode=nullptr);
    //사용할 노드들의 Vertex,Index 버퍼를 미리 생성
    void BuildBatchBuffers(FRenderer& Renderer);
    //CachedBatchData 전부 할당 해제. 현재 버퍼 생성 후 자동 실행
    //void ClearBatchDatas();
    void ClearKDDatas(int MaxDepthKD);
    //재귀적으로 Components를 적절한 노드에 추가
    void Insert(UPrimitiveComponent* Component, int MaxDepth = 5);
    void BuildOverlappingRecursive(UPrimitiveComponent* Component);
    void BuildKDTreeRecursive();

    //Lazy Segtree에서 사용. FrameThreshold프레임만큼 사용하지 않은 버퍼 할당 해제. 현재 사용 X
    //void TickBuffers(int CurrentFrame, int FrameThreshold);
    //현재 렌더할 노드를 결정해서 FRenderBatchData를 반환
    void CollectRenderNodes(const FFrustum& Frustum, TArray<FOctreeNode*>& OutNodes);

    UPrimitiveComponent* Raycast(const FRay& Ray, float& OutDistance) const;
    UPrimitiveComponent* RaycastWithKD(const FRay& Ray, float& OutDistance, int MaxDepthKD) const;
    void AssignAllDrawRangesLODWrapped();
    void ComputeDrawRangesFromParentLODWrapped(
        const FString& MatName,
        const TMap<ELODLevel, FDrawRange>& InRanges);


    std::string DumpLODRangeRecursive(int MaxDepth, int IndentLevel = 0) const;


    //TMap<FString, FDrawRange> DrawRanges; // 루트 기준 범위 정보 저장
};

//현재 (GRenderDepthMax-GRenderDepthMin+1)*2GB만큼의 VRam 사용
inline int GRenderDepthMin = 1; // 최소 깊이 (이보다 얕으면 스킵)
inline int GRenderDepthMax = 3; // 최대 깊이 (이보다 깊으면 스킵) 2~3이 적절
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
    int MaxDepthKD = 4;
    bool bUseKD = true;

private:
    FOctreeNode* Root;
};

//각 노드 AABB 출력
void DebugRenderOctreeNode(UPrimitiveBatch* PrimitiveBatch, const FOctreeNode* Node, int MaxDepth);
//FRenderer::RenderStaticMesh에서 사용(현재 사용 X)
const int FrameThreshold = 2; // 프레임 이상 사용 안 한 버퍼 제거
//CollectRenderNodes를 통해 선별한 노드의 데이터를 렌더.
void RenderCollectedBatches(FRenderer& Renderer, const FMatrix& VP, const TArray<FOctreeNode*>& RenderNodes, const FOctreeNode* RootNode, ELODLevel LODLevel);
