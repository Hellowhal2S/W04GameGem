// OctreeSystem.cpp

#include "Octree.h"

#include <sstream>

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "KDTree/KDTree.h"
#include "Math/Frustum.h"
#include "Math/JungleMath.h"
#include "Math/Ray.h"
#include "Profiling/PlatformTime.h"
#include "Profiling/StatRegistry.h"
#include "UnrealEd/PrimitiveBatch.h"
#include "UObject/Casts.h"
#include "UObject/UObjectIterator.h"
int GCurrentFrame = 0;

void FOctree::BuildFull()
{
    FScopeCycleCounter Timer("BuildFullOctree");

    FVector MinBound(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector MaxBound(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    // Step 1. 전체 Bounds 계산
    for (const auto* SceneComp : TObjectRange<USceneComponent>())
    {
        if (const auto* MeshComp = Cast<UMeshComponent>(SceneComp))
        {
            auto* PrimComp = Cast<UPrimitiveComponent>(MeshComp);
            PrimComp->UpdateWorldAABB();
            const FBoundingBox& AABB = MeshComp->WorldAABB;

            MinBound = FVector::Min(MinBound, AABB.min);
            MaxBound = FVector::Max(MaxBound, AABB.max);
        }
    }

    delete Root;
    Root = new FOctreeNode(FBoundingBox(MinBound, MaxBound), 0);

    // Step 2. 노드 삽입
    Build();

    // Step 3. KDTree 및 렌더링 데이터 구축
    Root->BuildKDTreeRecursive();
    Root->BuildBatchRenderData();
    Root->AssignAllDrawRangesLODWrapped();
    Root->BuildBatchBuffers(FEngineLoop::renderer);
    //Root->BuildBatchBuffers(FEngineLoop::renderer);
    //Root->ClearBatchDatas();
    Root->ClearKDDatas(MaxDepthKD);

    FStatRegistry::RegisterResult(Timer);
}


FOctreeNode::FOctreeNode(const FBoundingBox& InBounds, int InDepth)
    : Bounds(InBounds)
    , Depth(InDepth)
{
    BoundingSphere = Bounds.GetBoundingSphere(true);
}

FOctree::~FOctree()
{
    delete Root;
}

FOctreeNode::~FOctreeNode()
{
    // 자식 해제
    for (int i = 0; i < 8; ++i)
        delete Children[i];

    // Root 버퍼 해제
    for (auto& Pair : CachedBatchRootData)
    {
        FRenderBatchRootData& Batch = Pair.Value;

        // LOD별 해제
        for (auto& VBPair : Batch.VertexBuffers)
        {
            if (VBPair.Value)
            {
                VBPair.Value->Release();
                VBPair.Value = nullptr;
            }
        }
        for (auto& IBPair : Batch.IndexBuffers)
        {
            if (IBPair.Value)
            {
                IBPair.Value->Release();
                IBPair.Value = nullptr;
            }
        }

        // 정점/인덱스 배열 비우기
        for (auto& VerticesPair : Batch.Vertices)
        {
            VerticesPair.Value.Empty();
            VerticesPair.Value.ShrinkToFit();
        }

        for (auto& IndicesPair : Batch.Indices)
        {
            IndicesPair.Value.Empty();
            IndicesPair.Value.ShrinkToFit();
        }

        Batch.VertexBuffers.Empty();
        Batch.IndexBuffers.Empty();
        Batch.Vertices.Empty();
        Batch.Indices.Empty();
    }
}


void FOctreeNode::Insert(UPrimitiveComponent* Component, int MaxDepth)
{
    if (Depth >= MaxDepth)
    {
        Components.Add(Component);
        return;
    }

    if (bIsLeaf)
    {
        FVector Center = (Bounds.min + Bounds.max) * 0.5f;
        for (int i = 0; i < 8; ++i)
        {
            FVector Min = {
                (i & 1) ? Center.x : Bounds.min.x,
                (i & 2) ? Center.y : Bounds.min.y,
                (i & 4) ? Center.z : Bounds.min.z
            };
            FVector Max = {
                (i & 1) ? Bounds.max.x : Center.x,
                (i & 2) ? Bounds.max.y : Center.y,
                (i & 4) ? Bounds.max.z : Center.z
            };

            // 정확히 잘린 조각이므로 보정(Epsilon)은 오히려 왜곡을 일으킬 수 있음
            Children[i] = new FOctreeNode(FBoundingBox(Min, Max), Depth + 1);
            Children[i]->Parent = this;
            Children[i]->ChildIndex = i;
        }
        bIsLeaf = false;

        for (UPrimitiveComponent* Comp : Components)
            Insert(Comp, MaxDepth);

        Components.Empty();
    }

    for (int i = 0; i < 8; ++i)
    {
        //if (Children[i]->Bounds.Contains(Component->WorldAABB))
        //if (Children[i]->Bounds.Overlaps(Component->AABB))
        if (Children[i]->Bounds.Contains(Component->WorldAABB.GetCenter()))
        {
            Children[i]->Insert(Component, MaxDepth);
            return;
        }
    }

    Components.Add(Component);
}

void FOctreeNode::BuildOverlappingRecursive(UPrimitiveComponent* Component)
{
    if (!Bounds.Overlaps(Component->WorldAABB))
        return;

    OverlappingComponents.Add(Component);
    for (int i = 0; i < 8; ++i)
    {
        if (Children[i])
            Children[i]->BuildOverlappingRecursive(Component);
    }
}

void FOctreeNode::BuildKDTreeRecursive()
{
    // 리프 + 조건 만족 시 KDTree 생성
    if (/*Depth == MaxDepthKD && */!KDTree)
    {
        TArray<UStaticMeshComponent*> StaticMeshComps;
        //CollectStaticMeshesRecursive(StaticMeshComps);
        for (auto i : OverlappingComponents)
        {
            StaticMeshComps.Add(dynamic_cast<UStaticMeshComponent*>(i));
        }
        if (!StaticMeshComps.IsEmpty())
        {
            KDTree = new FKDTreeNode();
            KDTree->Build(StaticMeshComps);
        }
    }

    for (int i = 0; i < 8; ++i)
    {
        if (Children[i])
            Children[i]->BuildKDTreeRecursive();
    }
}

FOctree::FOctree(const FBoundingBox& InBounds)
    : Root(new FOctreeNode(InBounds, 0))
{
}


void FOctree::Build()
{
    for (const auto* SceneComp : TObjectRange<USceneComponent>())
    {
        if (auto* PrimComp = Cast<UPrimitiveComponent>(SceneComp))
        {
            PrimComp->UpdateWorldAABB();
            Root->Insert(PrimComp);
            Root->BuildOverlappingRecursive(PrimComp);
        }
    }
}

UPrimitiveComponent* FOctree::Raycast(const FRay& Ray, float& OutHitDistance) const
{
    if (!Root) return nullptr;
    if (bUseKD) return Root->RaycastWithKD(Ray, OutHitDistance, MaxDepthKD);
    else return Root->Raycast(Ray, OutHitDistance);
}

//불용
UPrimitiveComponent* FOctreeNode::Raycast(const FRay& Ray, float& OutDistance) const
{
    float NodeHitDist;
    if (!RayIntersectsAABB(Ray, Bounds, NodeHitDist)) return nullptr;
    //if (!IntersectRaySphere(Ray.Origin, Ray.Direction, BoundingSphere, NodeHitDist)) return nullptr;

    UPrimitiveComponent* ClosestComponent = nullptr;
    float ClosestDistance = FLT_MAX;

    // 1. Leaf Node → OverlappingComponents 검사
    if (bIsLeaf)
    {
        for (UPrimitiveComponent* Comp : OverlappingComponents)
        {
            float HitDist = 0.0f;
            if (IntersectRaySphere(Ray.Origin, Ray.Direction, Comp->BoundingSphere, HitDist))
            {
                if (HitDist < ClosestDistance)
                {
                    ClosestDistance = HitDist;
                    ClosestComponent = Comp;
                }
            }
        }
    }
    else
    {
        for (int i = 0; i < 8; ++i)
        {
            if (Children[i])
            {
                float ChildHitDist = FLT_MAX;
                UPrimitiveComponent* HitComp = Children[i]->Raycast(Ray, ChildHitDist);
                if (HitComp && ChildHitDist < ClosestDistance)
                {
                    ClosestComponent = HitComp;
                    ClosestDistance = ChildHitDist;
                }
            }
        }
    }

    OutDistance = ClosestDistance;
    return ClosestComponent;
}

UPrimitiveComponent* FOctreeNode::RaycastWithKD(const FRay& Ray, float& OutDistance, int MaxDepthKD) const
{
    float NodeHitDist;
    if (!RayIntersectsAABB(Ray, Bounds, NodeHitDist))
        return nullptr;

    // 리프 노드: KD 트리 사용
    if (Depth == MaxDepthKD)
    {
        if (KDTree)
        {
            float HitDist = FLT_MAX;
            UPrimitiveComponent* KDHit = KDTree->Raycast(Ray, HitDist);
            if (KDHit)
            {
                OutDistance = HitDist;
                return KDHit; // ✅ 바로 종료
            }
        }

        return nullptr;
    }

    // 내부 노드: 자식 노드를 정렬하여 거리순으로 검사
    struct FChildAndDist
    {
        const FOctreeNode* Node;
        float Dist;

        bool operator<(const FChildAndDist& Other) const { return Dist < Other.Dist; }
    };

    TArray<FChildAndDist> SortedChildren;
    for (int i = 0; i < 8; ++i)
    {
        if (Children[i])
        {
            const FVector Center = Children[i]->Bounds.GetCenter();
            float Dist = (Center - Ray.Origin).Magnitude(); // 또는 Dot으로 방향성 포함
            SortedChildren.Add({Children[i], Dist});
        }
    }

    SortedChildren.Sort();

    // 거리순으로 탐색 → 가장 가까운 곳에서 Hit 되면 종료
    for (const FChildAndDist& Entry : SortedChildren)
    {
        float ChildHitDist = FLT_MAX;
        UPrimitiveComponent* HitComp = Entry.Node->RaycastWithKD(Ray, ChildHitDist, MaxDepthKD);
        if (HitComp)
        {
            OutDistance = ChildHitDist;
            return HitComp; // ✅ 가장 가까운 곳에서 Hit되면 바로 반환
        }
    }

    return nullptr;
}


void DebugRenderOctreeNode(UPrimitiveBatch* PrimitiveBatch, const FOctreeNode* Node, int MaxDepth)
{
    if (!Node) return;

    const FVector Center = (Node->Bounds.min + Node->Bounds.max) * 0.5f;
    const FMatrix Identity = FMatrix::Identity;

    PrimitiveBatch->RenderAABB(Node->Bounds, FVector::ZeroVector, Identity);
    if (Node->Depth == MaxDepth)return;
    if (!Node->bIsLeaf)
    {
        for (int i = 0; i < 8; ++i)
        {
            DebugRenderOctreeNode(PrimitiveBatch, Node->Children[i], MaxDepth);
        }
    }
}

/*
void FOctreeNode::BuildBatchRenderData(FOctreeNode* RootNode)
{
    if (!RootNode)
        RootNode = this;

    VertexBufferSizeInBytes = 0;
    IndexBufferSizeInBytes = 0;

    if (bIsLeaf)
    {
        for (UPrimitiveComponent* Comp : Components)
        {
            UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Comp);
            if (!StaticMeshComp || !StaticMeshComp->GetStaticMesh()) continue;

            // LOD 순회
            for (ELODLevel LODLevel : {ELODLevel::LOD0, ELODLevel::LOD1, ELODLevel::LOD2})
            {
                OBJ::FStaticMeshRenderData* RenderData= nullptr;
                RenderData = StaticMeshComp->GetStaticMesh()->GetRenderData(LODLevel);
                if (!RenderData) continue;

                const TArray<FVertexCompact>& MeshVertices = RenderData->Vertices;
                const TArray<unsigned>& MeshIndices = RenderData->Indices;
                const TArray<FObjMaterialInfo>& Materials = RenderData->Materials;
                const TArray<FMaterialSubset>& Subsets = RenderData->MaterialSubsets;

                const FMatrix ModelMatrix = JungleMath::CreateModelMatrix(
                    StaticMeshComp->GetWorldLocation(),
                    StaticMeshComp->GetWorldRotation(),
                    StaticMeshComp->GetWorldScale()
                );
                for (int i = 0; i < Subsets.Num(); ++i)
                {
                    const FMaterialSubset& Subset = Subsets[i];
                    const FObjMaterialInfo& MatInfo = Materials[Subset.MaterialIndex];
                    const FString& MatName = MatInfo.MTLName;

                    // 이미 Key는 LOD 포함된 형태라고 가정 (예: AppleMat_LOD0)
                    const FString& Key = MatName; // MakeBatchKey 제거

                    // 루트에 버퍼 추가
                    FRenderBatchRootData& RootBatch = RootNode->CachedBatchRootData.FindOrAdd(Key);
                    TArray<FVertexCompact>& LODVerts = RootBatch.Vertices.FindOrAdd(LODLevel);
                    TArray<UINT>& LODIndices = RootBatch.Indices.FindOrAdd(LODLevel);
                    UINT VertexStart = (UINT)LODVerts.Num();

                    TMap<UINT, UINT> IndexMap;

                    for (UINT j = 0; j < Subset.IndexCount; ++j)
                    {
                        UINT oldIndex = MeshIndices[Subset.IndexStart + j];
                        if (!IndexMap.Contains(oldIndex))
                        {
                            FVertexCompact V = MeshVertices[oldIndex];
                            FVector WorldPos = ModelMatrix.TransformPosition(FVector(V.x, V.y, V.z));
                            V.x = WorldPos.x; V.y = WorldPos.y; V.z = WorldPos.z;

                            LODVerts.Add(V);
                            IndexMap.Add(oldIndex, VertexStart++);
                        }
                        LODIndices.Add(IndexMap[oldIndex]);
                    }

                    // 노드 정보 저장
                    FRenderBatchNodeData& NodeData = CachedBatchNodeData.FindOrAdd(Key);
                    NodeData.MaterialInfo = MatInfo;
                    NodeData.OwnerNode = this;
                    NodeData.IndicesNum += Subset.IndexCount;

                    FDrawRange DrawRange;
                    DrawRange.IndexStart = LODIndices.Num() - Subset.IndexCount;
                    DrawRange.IndexCount = Subset.IndexCount;

                    NodeData.LODDrawRanges.FindOrAdd(LODLevel) = DrawRange;
                }
            }
        }
    }

    // 자식 재귀 호출
    for (int i = 0; i < 8; ++i)
    {
        if (Children[i])
            Children[i]->BuildBatchRenderData(RootNode);
    }

    // 자식 노드 병합
    for (int i = 0; i < 8; ++i)
    {
        if (!Children[i]) continue;

        for (const auto& ChildPair : Children[i]->CachedBatchNodeData)
        {
            const FString& Key = ChildPair.Key;
            const FRenderBatchNodeData& ChildBatch = ChildPair.Value;

            if (ChildBatch.IndicesNum == 0) continue;

            FRenderBatchNodeData& MyBatch = CachedBatchNodeData.FindOrAdd(Key);
            MyBatch.MaterialInfo = ChildBatch.MaterialInfo;
            MyBatch.IndicesNum += ChildBatch.IndicesNum;
            MyBatch.OwnerNode = this;

            // LOD별 DrawRange 병합
            for (const auto& LODPair : ChildBatch.LODDrawRanges)
            {
                ELODLevel LOD = LODPair.Key;
                const FDrawRange& ChildRange = LODPair.Value;

                FDrawRange& MyRange = MyBatch.LODDrawRanges.FindOrAdd(LOD);
                if (MyRange.IndexCount == 0)
                {
                    MyRange = ChildRange;
                }
                else
                {
                    // 여러 자식 노드일 경우 DrawRange 이어붙이지 않고 일단 전체 범위 추적
                    // → 이후 렌더링에서 StartIndex, IndexCount로 Draw 호출
                    MyRange.IndexCount += ChildRange.IndexCount;
                }
            }
        }
    }
}
*/
void FOctreeNode::BuildBatchRenderData(FOctreeNode* RootNode)
{
    if (!RootNode)
        RootNode = this;

    VertexBufferSizeInBytes = 0;
    IndexBufferSizeInBytes = 0;

    // LEAF 처리
    if (bIsLeaf)
    {
        for (UPrimitiveComponent* Comp : Components)
        {
            UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Comp);
            if (!StaticMeshComp || !StaticMeshComp->GetStaticMesh()) continue;

            for (int LOD = (int)ELODLevel::LOD0; LOD <= (int)ELODLevel::LOD2; ++LOD)
            {
                ELODLevel LODLevel = static_cast<ELODLevel>(LOD);
                OBJ::FStaticMeshRenderData* RenderData = StaticMeshComp->GetStaticMesh()->GetRenderData(LODLevel);
                if (!RenderData) continue;

                const auto& MeshVertices = RenderData->Vertices;
                const auto& MeshIndices = RenderData->Indices;
                const auto& Materials = RenderData->Materials;
                const auto& Subsets = RenderData->MaterialSubsets;

                const FMatrix ModelMatrix = JungleMath::CreateModelMatrix(
                    StaticMeshComp->GetWorldLocation(),
                    StaticMeshComp->GetWorldRotation(),
                    StaticMeshComp->GetWorldScale()
                );

                for (int i = 0; i < Subsets.Num(); ++i)
                {
                    const auto& Subset = Subsets[i];
                    const auto& MatInfo = Materials[Subset.MaterialIndex];
                    const FString& MatName = MatInfo.MTLName;

                    // 루트에 버퍼 추가
                    FRenderBatchRootData& RootBatch = RootNode->CachedBatchRootData.FindOrAdd(MatName);
                    TArray<FVertexCompact>& Vertices = RootBatch.Vertices.FindOrAdd(LODLevel);
                    TArray<UINT>& Indices = RootBatch.Indices.FindOrAdd(LODLevel);

                    UINT VertexStart = (UINT)Vertices.Num();
                    TMap<UINT, UINT> IndexMap;

                    for (UINT j = 0; j < Subset.IndexCount; ++j)
                    {
                        UINT oldIndex = MeshIndices[Subset.IndexStart + j];
                        if (!IndexMap.Contains(oldIndex))
                        {
                            FVertexCompact V = MeshVertices[oldIndex];
                            FVector WorldPos = ModelMatrix.TransformPosition(FVector(V.x, V.y, V.z));
                            V.x = WorldPos.x; V.y = WorldPos.y; V.z = WorldPos.z;

                            Vertices.Add(V);
                            IndexMap.Add(oldIndex, VertexStart++);
                        }
                        Indices.Add(IndexMap[oldIndex]);
                    }

                    // 이 노드에는 인덱스 수만 기록
                    FRenderBatchNodeData& NodeData = CachedBatchNodeData.FindOrAdd(MatName);
                    NodeData.MaterialInfo = MatInfo;
                    NodeData.OwnerNode = this;

                    FDrawRange& MyRange = NodeData.LODDrawRanges.FindOrAdd(LODLevel);
                    MyRange.IndexCount += Subset.IndexCount;
                }
            }
        }
    }

    // 자식 재귀
    for (int i = 0; i < 8; ++i)
    {
        if (Children[i])
            Children[i]->BuildBatchRenderData(RootNode);
    }

    // 자식 인덱스 정보 병합
    for (int i = 0; i < 8; ++i)
    {
        if (!Children[i]) continue;

        for (const auto& ChildPair : Children[i]->CachedBatchNodeData)
        {
            const FString& MatName = ChildPair.Key;
            const FRenderBatchNodeData& ChildBatch = ChildPair.Value;

            FRenderBatchNodeData& MyBatch = CachedBatchNodeData.FindOrAdd(MatName);
            MyBatch.MaterialInfo = ChildBatch.MaterialInfo;
            MyBatch.OwnerNode = this;

            for (const auto& LODPair : ChildBatch.LODDrawRanges)
            {
                ELODLevel LOD = LODPair.Key;
                MyBatch.LODDrawRanges.FindOrAdd(LOD).IndexCount += LODPair.Value.IndexCount;
            }
        }
    }
}




void FOctreeNode::BuildBatchBuffers(FRenderer& Renderer)
{
    FScopeCycleCounter Timer("BuildBatchBuffers");

    // 루트 노드만 버퍼를 생성
    if (Depth == 0)
    {
        for (auto& Pair : CachedBatchRootData)
        {
            FRenderBatchRootData& RenderData = Pair.Value;

            for (const auto& LODPair : RenderData.Vertices)
            {
                ELODLevel LOD = LODPair.Key;
                const TArray<FVertexCompact>& Vertices = LODPair.Value;

                if (!Vertices.IsEmpty())
                {
                    ID3D11Buffer* VB = Renderer.CreateVertexBuffer(
                        Vertices, Vertices.Num() * sizeof(FVertexCompact));
                    RenderData.VertexBuffers.Add(LOD, VB);
                }
            }

            for (const auto& LODPair : RenderData.Indices)
            {
                ELODLevel LOD = LODPair.Key;
                const TArray<UINT>& Indices = LODPair.Value;

                if (!Indices.IsEmpty())
                {
                    ID3D11Buffer* IB = Renderer.CreateIndexBuffer(
                        Indices, Indices.Num() * sizeof(UINT));
                    RenderData.IndexBuffers.Add(LOD, IB);
                }
            }
        }
    }

    FStatRegistry::RegisterResult(Timer);
}


void FOctreeNode::ClearKDDatas(int MaxDepthKD)
{
    if (Depth != MaxDepthKD)
    {
        delete KDTree;
        KDTree = nullptr;
        OverlappingComponents.Empty();
        OverlappingComponents.ShrinkToFit();
    }
    Components.Empty();
    Components.ShrinkToFit();
    for (int i = 0; i < 8; ++i)
    {
        if (Children[i])
            Children[i]->ClearKDDatas(MaxDepthKD);
    }
}

void FOctreeNode::CollectRenderNodes(const FFrustum& Frustum, TArray<FOctreeNode*>& OutNodes)
{
    EFrustumContainment Containment = Frustum.CheckContainment(Bounds);
    if (Containment == EFrustumContainment::Contains ||
        (Containment == EFrustumContainment::Intersects && Depth == GRenderDepthMax))
    {
        if (Depth >= GRenderDepthMin)
        {
            OutNodes.Add(this);
            return;
        }
    }

    for (int i = 0; i < 8; ++i)
    {
        if (Children[i])
            Children[i]->CollectRenderNodes(Frustum, OutNodes);
    }
}
void RenderCollectedBatches(FRenderer& Renderer, const FMatrix& VP, const TArray<FOctreeNode*>& RenderNodes, const FOctreeNode* RootNode, ELODLevel LODLevel)
{
    std::string str = RootNode->DumpLODRangeRecursive(3);
    if (!RootNode) return;

    FMatrix MVP = FMatrix::Identity * VP;
    FMatrix NormalMatrix = FMatrix::Transpose(FMatrix::Inverse(FMatrix::Identity));
    Renderer.UpdateConstant(MVP, NormalMatrix, FVector4(0, 0, 0, 0), false);

    const TMap<FString, FRenderBatchRootData>& RootBatches = RootNode->CachedBatchRootData;

    // 머티리얼+LOD 단위로 묶기
    TMap<FString, TArray<const FOctreeNode*>> MaterialNodeMap;
    for (const FOctreeNode* Node : RenderNodes)
    {
        for (const auto& Pair : Node->CachedBatchNodeData)
        {
            const FString& Key = Pair.Key; // Material_LODx
            const FRenderBatchNodeData& NodeBatch = Pair.Value;

            if (!NodeBatch.LODDrawRanges.Contains(LODLevel))
                continue;

            const FDrawRange& Range = NodeBatch.LODDrawRanges[LODLevel];
            if (Range.IndexCount > 0)
                MaterialNodeMap.FindOrAdd(Key).Add(Node);
        }
    }

    // 머티리얼 단위 렌더링
    for (const auto& Pair : MaterialNodeMap)
    {
        const FString& Key = Pair.Key; // Material_LODx
        const TArray<const FOctreeNode*>& Nodes = Pair.Value;

        const FRenderBatchRootData* RootBatch = RootBatches.Find(Key);
        if (!RootBatch)
            continue;

        ID3D11Buffer* VB = RootBatch->VertexBuffers[LODLevel];
        ID3D11Buffer* IB = RootBatch->IndexBuffers[LODLevel];
        if (!VB || !IB) continue;

        // 머티리얼 설정
        const FRenderBatchNodeData* BatchData = Nodes[0]->CachedBatchNodeData.Find(Key);
        if (BatchData)
            Renderer.UpdateMaterial(BatchData->MaterialInfo);

        // 루트 버퍼 바인딩
        UINT offset = 0;
        Renderer.Graphics->DeviceContext->IASetVertexBuffers(0, 1, &VB, &Renderer.Stride, &offset);
        Renderer.Graphics->DeviceContext->IASetIndexBuffer(IB, DXGI_FORMAT_R32_UINT, 0);

        // Draw
        for (const FOctreeNode* Node : Nodes)
        {
            const FRenderBatchNodeData* NodeBatch = Node->CachedBatchNodeData.Find(Key);
            if (!NodeBatch) continue;

            const FDrawRange* Range = NodeBatch->LODDrawRanges.Find(LODLevel);
            if (!Range || Range->IndexCount == 0)
                continue;

            Renderer.Graphics->DeviceContext->DrawIndexed(Range->IndexCount, Range->IndexStart, 0);
        }
    }
}


/*
void FOctreeNode::TickBuffers(int CurrentFrame, int FrameThreshold)
{
    for (auto& Pair : CachedBatchRootData)
    {
        FRenderBatchRootData& Data = Pair.Value;

        // 사용된 지 오래된 경우 메모리 해제
        if (Data.VertexBuffer && (CurrentFrame - Data.LastUsedFrame > FrameThreshold))
        {
            Data.VertexBuffer->Release();
            Data.VertexBuffer = nullptr;
        }

        if (Data.IndexBuffer && (CurrentFrame - Data.LastUsedFrame > FrameThreshold))
        {
            Data.IndexBuffer->Release();
            Data.IndexBuffer = nullptr;
        }
    }

    // 재귀적으로 자식 노드에도 적용
    for (int i = 0; i < 8; ++i)
    {
        if (Children[i])
        {
            Children[i]->TickBuffers(CurrentFrame, FrameThreshold);
        }
    }
}
*/
// FOctreeNode.cpp


/*
void FOctreeNode::ComputeDrawRangesFromParentLOD(const FString& MatName, const TMap<ELODLevel, FDrawRange>& InRanges)
{
    // 1. 본인 설정
    FRenderBatchNodeData* MyBatch = CachedBatchNodeData.Find(MatName);
    if (!MyBatch) return;

    for (const auto& Pair : InRanges)
    {
        ELODLevel LOD = Pair.Key;
        const FDrawRange& ParentRange = Pair.Value;

        uint32 Count = MyBatch->LODDrawRanges[LOD].IndexCount;

        FDrawRange MyRange;
        MyRange.IndexStart = ParentRange.IndexStart;
        MyRange.IndexCount = Count;

        MyBatch->LODDrawRanges[LOD] = MyRange; // 저장
    }

    // 2. 자식에게 분배
    TMap<ELODLevel, uint32> RunningStart;
    for (const auto& Pair : InRanges)
    {
        RunningStart.Add(Pair.Key, Pair.Value.IndexStart);
    }

    for (int i = 0; i < 8; ++i)
    {
        if (!Children[i]) continue;

        FRenderBatchNodeData* ChildBatch = Children[i]->CachedBatchNodeData.Find(MatName);
        if (!ChildBatch) continue;

        TMap<ELODLevel, FDrawRange> ChildRangeMap;

        for (const auto& LODPair : ChildBatch->LODDrawRanges)
        {
            ELODLevel LOD = LODPair.Key;
            uint32 Count = LODPair.Value.IndexCount;
            if (Count == 0) continue;

            uint32 Start = RunningStart.FindOrAdd(LOD);
            ChildRangeMap.Add(LOD, FDrawRange{ Start, Count });

            RunningStart[LOD] += Count;
        }

        Children[i]->ComputeDrawRangesFromParentLOD(MatName, ChildRangeMap);
    }
}



*/
std::string FOctreeNode::DumpLODRangeRecursive(int MaxDepth, int IndentLevel) const
{
    std::ostringstream oss;
    std::string indent(IndentLevel * 2, ' ');

    // 노드 경로 출력
    if (Depth == 0)
    {
        oss << indent << "Root Node:\n";
    }
    else
    {
        oss << indent << "Child";
        const FOctreeNode* Curr = this;
        std::vector<int> Path;
        while (Curr->Parent)
        {
            Path.push_back(Curr->ChildIndex);
            Curr = Curr->Parent;
        }
        std::reverse(Path.begin(), Path.end());
        for (int idx : Path)
            oss << "[" << idx << "]";
        oss << ":\n";
    }

    // Material + LOD + DrawRange 출력
    for (const auto& pair : CachedBatchNodeData)
    {
        const FString& MatName = pair.Key;
        const FRenderBatchNodeData& Batch = pair.Value;

        for (const auto& lodPair : Batch.LODDrawRanges)
        {
            ELODLevel lod = lodPair.Key;
            const FDrawRange& range = lodPair.Value;
            uint32_t end = range.IndexStart + range.IndexCount;

            oss << indent << "  [" << (*MatName) << "][LOD" << static_cast<int>(lod) << "] : "
                << range.IndexStart << " ~ " << end << " (Count: " << range.IndexCount << ")\n";
        }
    }

    // 자식 재귀 호출
    if (Depth < MaxDepth)
    {
        for (int i = 0; i < 8; ++i)
        {
            if (Children[i])
                oss << Children[i]->DumpLODRangeRecursive(MaxDepth, IndentLevel + 1);
        }
    }

    return oss.str();
}

void FOctreeNode::AssignAllDrawRangesLODWrapped()
{
    for (const auto& Pair : CachedBatchNodeData)
    {
        const FString& MatName = Pair.Key;
        const FRenderBatchNodeData& Batch = Pair.Value;

        // LOD별로 반복
        for (const auto& LODPair : Batch.LODDrawRanges)
        {
            ELODLevel LOD = LODPair.Key;
            const FDrawRange& ThisLODRange = LODPair.Value;

            // 루트 노드의 시작점은 항상 0
            FDrawRange MyRange;
            MyRange.IndexStart = 0;
            MyRange.IndexCount = ThisLODRange.IndexCount;

            FRenderBatchNodeData& MyBatch = CachedBatchNodeData[MatName];
            MyBatch.LODDrawRanges[LOD] = MyRange;
        }
    }

    // 자식에게 분배
    for (const auto& Pair : CachedBatchNodeData)
    {
        const FString& MatName = Pair.Key;
        const FRenderBatchNodeData& Batch = Pair.Value;

        TMap<ELODLevel, uint32> RunningStart;

        for (int i = 0; i < 8; ++i)
        {
            if (!Children[i]) continue;

            auto* ChildBatch = Children[i]->CachedBatchNodeData.Find(MatName);
            if (!ChildBatch) continue;

            TMap<ELODLevel, FDrawRange> LODRangeMap;

            for (const auto& LODPair : ChildBatch->LODDrawRanges)
            {
                ELODLevel LOD = LODPair.Key;
                uint32 Count = LODPair.Value.IndexCount;
                if (Count == 0) continue;

                uint32 Start = RunningStart.FindOrAdd(LOD);
                LODRangeMap.Add(LOD, FDrawRange{ Start, Count });

                RunningStart[LOD] += Count;
            }

            Children[i]->ComputeDrawRangesFromParentLODWrapped(MatName, LODRangeMap);
        }
    }
}
void FOctreeNode::ComputeDrawRangesFromParentLODWrapped(
    const FString& MatName,
    const TMap<ELODLevel, FDrawRange>& InRanges)
{
    FRenderBatchNodeData* MyBatch = CachedBatchNodeData.Find(MatName);
    if (!MyBatch) return;

    // 본인 설정
    for (const auto& LODPair : InRanges)
    {
        ELODLevel LOD = LODPair.Key;
        const FDrawRange& ParentRange = LODPair.Value;

        uint32 Count = 0;
        if (const FDrawRange* Found = MyBatch->LODDrawRanges.Find(LOD))
        {
            Count = Found->IndexCount;
        }

        FDrawRange MyRange;
        MyRange.IndexStart = ParentRange.IndexStart;
        MyRange.IndexCount = Count;

        MyBatch->LODDrawRanges[LOD] = MyRange;
    }

    // 자식 분배
    TMap<ELODLevel, uint32> RunningStart;
    for (const auto& LODPair : InRanges)
    {
        RunningStart.Add(LODPair.Key, LODPair.Value.IndexStart);
    }

    for (int i = 0; i < 8; ++i)
    {
        if (!Children[i]) continue;

        FRenderBatchNodeData* ChildBatch = Children[i]->CachedBatchNodeData.Find(MatName);
        if (!ChildBatch) continue;

        TMap<ELODLevel, FDrawRange> ChildRanges;
        for (const auto& LODPair : ChildBatch->LODDrawRanges)
        {
            ELODLevel LOD = LODPair.Key;
            uint32 Count = LODPair.Value.IndexCount;
            if (Count == 0) continue;

            uint32 Start = RunningStart.FindOrAdd(LOD);
            ChildRanges.Add(LOD, FDrawRange{ Start, Count });

            RunningStart[LOD] += Count;
        }

        Children[i]->ComputeDrawRangesFromParentLODWrapped(MatName, ChildRanges);
    }
}
