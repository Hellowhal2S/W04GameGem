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
    Root->AssignAllDrawRanges();
    Root->BuildBatchBuffers(FEngineLoop::renderer);
    //Root->BuildBatchBuffers(FEngineLoop::renderer);
    Root->ClearBatchDatas();
    Root->ClearKDDatas(MaxDepthKD);

    FStatRegistry::RegisterResult(Timer);
}

void FRenderBatchData::CreateBuffersIfNeeded(FRenderer& Renderer)
{
    if (!VertexBuffer && !Vertices.IsEmpty())
    {
        UE_LOG(LogLevel::Display, "CreateVBuffer %d", Vertices.Num() * sizeof(FVertexCompact));
        VertexBuffer = Renderer.CreateVertexBuffer(Vertices, Vertices.Num() * sizeof(FVertexCompact));
    }

    if (!IndexBuffer && !Indices.IsEmpty())
    {
        UE_LOG(LogLevel::Display, "CreateIBuffer %d", Indices.Num() * sizeof(UINT));
        IndexBuffer = Renderer.CreateIndexBuffer(Indices, Indices.Num() * sizeof(UINT));
        IndicesNum = Indices.Num();
    }

    // Lazy 전략: CPU 메모리는 그대로 두거나, 아래처럼 제거할 수도 있음
    // Vertices.Empty();
    // Indices.Empty();
    // Vertices.ShrinkToFit();
    // Indices.ShrinkToFit();
}

FOctreeNode::FOctreeNode(const FBoundingBox& InBounds, int InDepth)
    : Bounds(InBounds)
    , Depth(InDepth)
{
    BoundingSphere = Bounds.GetBoundingSphere(true);
}

FOctreeNode::~FOctreeNode()
{
    for (int i = 0; i < 8; ++i)
        delete Children[i];
    for (auto& Pair : CachedBatchData)
    {
        FRenderBatchData& Batch = Pair.Value;
        if (Batch.VertexBuffer)
        {
            Batch.VertexBuffer->Release();
            Batch.VertexBuffer = nullptr;
        }
        if (Batch.IndexBuffer)
        {
            Batch.IndexBuffer->Release();
            Batch.IndexBuffer = nullptr;
        }
        Batch.Vertices.Empty();
        Batch.Indices.Empty();
        Batch.Vertices.ShrinkToFit();
        Batch.Indices.ShrinkToFit();
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

FOctree::~FOctree()
{
    delete Root;
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

void FOctreeNode::BuildBatchRenderData()
{
    VertexBufferSizeInBytes = 0;
    IndexBufferSizeInBytes = 0;
    // Step 1. 자식 먼저 처리
    for (int i = 0; i < 8; ++i)
    {
        if (Children[i])
        {
            Children[i]->BuildBatchRenderData();
            //if (Depth >= GRenderDepthMin)
            for (const auto& Pair : Children[i]->CachedBatchData)
            {
                const FString& MaterialName = Pair.Key;
                const FRenderBatchData& ChildData = Pair.Value;

                FRenderBatchData& CurrentData = CachedBatchData.FindOrAdd(MaterialName);
                CurrentData.MaterialInfo = ChildData.MaterialInfo;
                CurrentData.OwnerNode = this;
                UINT VertexOffset = (UINT)CurrentData.Vertices.Num();
                CurrentData.Vertices.Append(ChildData.Vertices);

                for (UINT Index : ChildData.Indices)
                    CurrentData.Indices.Add(Index + VertexOffset);
            }
        }
    }
    // Step 2. 본인 노드의 Components 처리
    for (UPrimitiveComponent* Comp : Components)
    {
        UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Comp);
        if (!StaticMeshComp || !StaticMeshComp->GetStaticMesh()) continue;

        OBJ::FStaticMeshRenderData* RenderData = StaticMeshComp->GetStaticMesh()->GetRenderData();
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

            FRenderBatchData& Entry = CachedBatchData.FindOrAdd(MatInfo.MTLName);
            Entry.MaterialInfo = MatInfo;
            UINT VertexStart = (UINT)Entry.Vertices.Num();
            TMap<UINT, UINT> IndexMap;

            for (UINT j = 0; j < Subset.IndexCount; ++j)
            {
                UINT oldIndex = MeshIndices[Subset.IndexStart + j];
                if (!IndexMap.Contains(oldIndex))
                {
                    //FVertexCompact TransformedVertex = ConvertToCompact(MeshVertices[oldIndex]);
                    FVertexCompact TransformedVertex = MeshVertices[oldIndex];
                    // 월드 위치 변환
                    FVector LocalPosition{TransformedVertex.x, TransformedVertex.y, TransformedVertex.z};
                    FVector WorldPosition = ModelMatrix.TransformPosition(LocalPosition);
                    TransformedVertex.x = WorldPosition.x;
                    TransformedVertex.y = WorldPosition.y;
                    TransformedVertex.z = WorldPosition.z;

                    Entry.Vertices.Add(TransformedVertex);

                    IndexMap.Add(oldIndex, VertexStart++);
                }
                Entry.Indices.Add(IndexMap[oldIndex]);
            }
        }
    }
    // Step 3. 최종 버퍼 크기 계산 (현재 Vertex/Index는 FVertexCompact, uint32 기준)
    for (const auto& Pair : CachedBatchData)
    {
        const FRenderBatchData& Batch = Pair.Value;
        VertexBufferSizeInBytes += Batch.Vertices.Num() * sizeof(FVertexCompact);
        IndexBufferSizeInBytes += Batch.Indices.Num() * sizeof(uint32);
    }
}

void FOctreeNode::BuildBatchBuffers(FRenderer& Renderer)
{
    FScopeCycleCounter Timer("BuildBatchBuffers");
    if (Depth == 0)
    {
        for (auto& Pair : CachedBatchData)
        {
            FRenderBatchData& RenderData = Pair.Value;

            if (!RenderData.Vertices.IsEmpty())
            {
                RenderData.VertexBuffer = Renderer.CreateVertexBuffer(
                    RenderData.Vertices, RenderData.Vertices.Num() * sizeof(FVertexCompact));
            }

            if (!RenderData.Indices.IsEmpty())
            {
                RenderData.IndexBuffer = Renderer.CreateIndexBuffer(
                    RenderData.Indices, RenderData.Indices.Num() * sizeof(UINT));
            }

            RenderData.IndicesNum = RenderData.Indices.Num();
        }
    }
    /*
    if (Depth >= GRenderDepthMin && Depth <= GRenderDepthMax)
    {
        for (auto& Pair : CachedBatchData)
        {
            FRenderBatchData& RenderData = Pair.Value;

            if (!RenderData.Vertices.IsEmpty())
            {
                RenderData.VertexBuffer = Renderer.CreateVertexBuffer(
                    RenderData.Vertices, RenderData.Vertices.Num() * sizeof(FVertexCompact));
            }

            if (!RenderData.Indices.IsEmpty())
            {
                RenderData.IndexBuffer = Renderer.CreateIndexBuffer(
                    RenderData.Indices, RenderData.Indices.Num() * sizeof(UINT));
            }

            RenderData.IndicesNum = RenderData.Indices.Num();
        }
    }
    for (int i = 0; i < 8; ++i)
    {
        if (Children[i])
            Children[i]->BuildBatchBuffers(Renderer);
    }*/
    FStatRegistry::RegisterResult(Timer);
}

void FOctreeNode::ClearBatchDatas()
{
    for (auto& Pair : CachedBatchData)
    {
        FRenderBatchData& RenderData = Pair.Value;
        RenderData.Vertices.Empty();
        RenderData.Vertices.ShrinkToFit();
        RenderData.Indices.Empty();
        RenderData.Indices.ShrinkToFit();
    }

    for (int i = 0; i < 8; ++i)
    {
        if (Children[i])
            Children[i]->ClearBatchDatas();
    }
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

void FOctreeNode::CollectRenderNodes(const FFrustum& Frustum, TMap<FString, TArray<FRenderBatchData*>>& OutRenderMap)
{
    EFrustumContainment Containment = Frustum.CheckContainment(Bounds);
    if (Containment == EFrustumContainment::Contains ||
        (Containment == EFrustumContainment::Intersects && Depth == GRenderDepthMax))
    {
        if (Depth >= GRenderDepthMin)
        {
            for (auto& Pair : CachedBatchData)
            {
                const FString& MatName = Pair.Key;
                const FDrawRange* Range = DrawRanges.Find(MatName);
                if (Range && Range->IndexCount > 0)
                    OutRenderMap.FindOrAdd(MatName).Add(&Pair.Value);
            }
            return;
        }
    }

    for (int i = 0; i < 8; ++i)
    {
        if (Children[i])
            Children[i]->CollectRenderNodes(Frustum, OutRenderMap);
    }
}


/*void RenderCollectedBatches(FRenderer& Renderer, const FMatrix& VP, const TMap<FString, TArray<FRenderBatchData*>>& RenderMap)
{
    FMatrix MVP = FMatrix::Identity * VP;
    FMatrix NormalMatrix = FMatrix::Transpose(FMatrix::Inverse(FMatrix::Identity));

    Renderer.UpdateConstant(MVP, NormalMatrix, FVector4(0, 0, 0, 0), false);
    for (const auto& Pair : RenderMap)
    {
        const TArray<FRenderBatchData*>& Batches = Pair.Value;
        if (Batches.IsEmpty()) continue;

        // 머티리얼 설정 (한 번만)
        FRenderBatchData* First = Batches[0];
        Renderer.UpdateMaterial(First->MaterialInfo);

        for (FRenderBatchData* Batch : Batches)
        {
            Batch->CreateBuffersIfNeeded(Renderer);
            if (!Batch->VertexBuffer || !Batch->IndexBuffer)
                continue;

            Batch->LastUsedFrame = GCurrentFrame;

            UINT offset = 0;
            Renderer.Graphics->DeviceContext->IASetVertexBuffers(0, 1, &Batch->VertexBuffer, &Renderer.Stride, &offset);
            Renderer.Graphics->DeviceContext->IASetIndexBuffer(Batch->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
            Renderer.Graphics->DeviceContext->DrawIndexed(Batch->IndicesNum, 0, 0);
        }
    }
}*/
void RenderCollectedBatches(FRenderer& Renderer, const FMatrix& VP, const TMap<FString, TArray<FRenderBatchData*>>& RenderMap,
                            const FOctreeNode* RootNode)
{
    //std::string DebugText = RootNode->DumpDrawRangesRecursive(3);
    
    if (!RootNode) return;

    const TMap<FString, FRenderBatchData>& RootBatches = RootNode->CachedBatchData;

    FMatrix MVP = FMatrix::Identity * VP;
    FMatrix NormalMatrix = FMatrix::Transpose(FMatrix::Inverse(FMatrix::Identity));
    Renderer.UpdateConstant(MVP, NormalMatrix, FVector4(0, 0, 0, 0), false);

    for (const auto& Pair : RenderMap)
    {
        const FString& MaterialName = Pair.Key;
        const TArray<FRenderBatchData*>& Batches = Pair.Value;

        const FRenderBatchData* RootBatch = RootBatches.Find(MaterialName);
        if (!RootBatch || !RootBatch->VertexBuffer || !RootBatch->IndexBuffer || RootBatch->IndicesNum == 0)
            continue;

        // 머티리얼 한 번 설정
        Renderer.UpdateMaterial(RootBatch->MaterialInfo);

        // 루트의 버퍼 바인딩 (한 번만)
        UINT offset = 0;
        Renderer.Graphics->DeviceContext->IASetVertexBuffers(0, 1, &RootBatch->VertexBuffer, &Renderer.Stride, &offset);
        Renderer.Graphics->DeviceContext->IASetIndexBuffer(RootBatch->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

        // 각 Batch → OwnerNode → DrawRange 기반으로 Draw
        for (FRenderBatchData* Batch : Batches)
        {
            if (!Batch || !Batch->OwnerNode)
                continue;

            const FDrawRange* Range = Batch->OwnerNode->DrawRanges.Find(MaterialName);
            if (!Range || Range->IndexCount == 0)
                continue;

            Batch->LastUsedFrame = GCurrentFrame;
            Renderer.Graphics->DeviceContext->DrawIndexed(Range->IndexCount, Range->IndexStart, 0);
        }
    }
}

void FOctreeNode::TickBuffers(int CurrentFrame, int FrameThreshold)
{
    for (auto& Pair : CachedBatchData)
    {
        FRenderBatchData& Data = Pair.Value;

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
/*
void FOctreeNode::AssignAllDrawRanges()
{
    TMap<FString, FDrawRange> RootRanges;

    for (const auto& Pair : CachedBatchData)
    {
        const FString& MatName = Pair.Key;
        const FRenderBatchData& Batch = Pair.Value;

        FDrawRange Range;
        Range.IndexStart = 0;
        Range.IndexCount = Batch.Indices.Num();

        DrawRanges.Add(MatName, Range); // 루트 자신의 범위
        RootRanges.Add(MatName, Range); // 자식 분배용
    }

    // 재귀 시작
    for (int i = 0; i < 8; ++i)
    {
        if (Children[i])
            Children[i]->ComputeDrawRangesFromParent(RootRanges);
    }
}
*/
void FOctreeNode::AssignAllDrawRanges()
{
    for (const auto& Pair : CachedBatchData)
    {
        const FString& MatName = Pair.Key;
        const FRenderBatchData& Batch = Pair.Value;

        FDrawRange Range;
        Range.IndexStart = 0;
        Range.IndexCount = Batch.Indices.Num();

        DrawRanges.Add(MatName, Range);
    }

    // 각 머티리얼에 대해 자식들에게 분배
    for (const auto& Pair : CachedBatchData)
    {
        const FString& MatName = Pair.Key;
        const FRenderBatchData& Batch = Pair.Value;

        uint32 RunningStart = 0;

        for (int i = 0; i < 8; ++i)
        {
            if (!Children[i]) continue;
            auto* ChildBatch = Children[i]->CachedBatchData.Find(MatName);
            if (!ChildBatch) continue;

            uint32 Count = ChildBatch->Indices.Num();
            if (Count == 0) continue;

            FDrawRange ChildRange { RunningStart, Count };

            TMap<FString, FDrawRange> MatOnly;
            MatOnly.Add(MatName, ChildRange);
            Children[i]->ComputeDrawRangesFromParent(MatOnly);

            RunningStart += Count;
        }
    }
}
void FOctreeNode::ComputeDrawRangesFromParent(const TMap<FString, FDrawRange>& InRanges)
{
    // 1. 본인 DrawRange 설정
    for (auto& Pair : CachedBatchData)
    {
        const FString& MatName = Pair.Key;
        const FRenderBatchData& MyBatch = Pair.Value;

        const FDrawRange* RangeFromParent = InRanges.Find(MatName);
        if (!RangeFromParent) continue;

        FDrawRange& MyRange = DrawRanges.FindOrAdd(MatName);
        MyRange.IndexStart = RangeFromParent->IndexStart;
        MyRange.IndexCount = MyBatch.Indices.Num();
    }

    // 2. 자식에게 머티리얼별로 정확히 나눠서 전달
    for (const auto& ParentPair : InRanges)
    {
        const FString& MatName = ParentPair.Key;
        const FDrawRange& ParentRange = ParentPair.Value;

        uint32 RunningStart = ParentRange.IndexStart;

        for (int i = 0; i < 8; ++i)
        {
            if (!Children[i]) continue;

            FRenderBatchData* ChildBatch = Children[i]->CachedBatchData.Find(MatName);
            if (!ChildBatch) continue;

            uint32 Count = ChildBatch->Indices.Num();
            if (Count == 0) continue;

            FDrawRange ChildRange;
            ChildRange.IndexStart = RunningStart;
            ChildRange.IndexCount = Count;

            TMap<FString, FDrawRange> ChildRanges;
            ChildRanges.Add(MatName, ChildRange);

            RunningStart += Count;

            Children[i]->ComputeDrawRangesFromParent(ChildRanges);
        }
    }
}


std::string FOctreeNode::DumpDrawRangesRecursive(int MaxDepth, int IndentLevel) const
{
    std::ostringstream oss;

    // 들여쓰기 설정
    std::string Indent(IndentLevel * 2, ' ');

    // 현재 노드 헤더 출력
    if (Depth == 0)
        oss << Indent << "Root Range:\n";
    else
        oss << Indent << "Child";

    // 경로 출력 (Child[0][2][1]...)
    FOctreeNode const* Curr = this;
    std::vector<int> Path;
    while (Curr->Parent)
    {
        Path.push_back(Curr->ChildIndex);
        Curr = Curr->Parent;
    }
    std::reverse(Path.begin(), Path.end());
    for (int idx : Path)
        oss << "[" << idx << "]";
    if (Depth != 0)
        oss << " Range:\n";

    // DrawRange 출력
    for (const auto& Pair : DrawRanges)
    {
        const FString& MatName = Pair.Key;
        const FDrawRange& Range = Pair.Value;
        uint32 End = Range.IndexStart + Range.IndexCount;

        oss << Indent << "  [" << (*MatName) << "] : "
            << Range.IndexStart << " ~ " << End << " (Count: " << Range.IndexCount << ")\n";
    }

    // 재귀
    if (Depth < MaxDepth)
    {
        for (int i = 0; i < 8; ++i)
        {
            if (Children[i])
                oss << Children[i]->DumpDrawRangesRecursive(MaxDepth, IndentLevel + 1);
        }
    }

    return oss.str();
}
