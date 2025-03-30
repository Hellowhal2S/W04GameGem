// OctreeSystem.cpp

#include "Octree.h"

#include <sstream>

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Math/Frustum.h"
#include "Math/JungleMath.h"
#include "Profiling/PlatformTime.h"
#include "Profiling/StatRegistry.h"
#include "UnrealEd/PrimitiveBatch.h"
#include "UObject/Casts.h"
#include "UObject/UObjectIterator.h"
#include "OcclusionQuerySystem.h"

int GCurrentFrame = 0;



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
/*
void FRenderBatchData::ReleaseBuffersIfUnused(int CurrentFrame, int ThresholdFrames)
{
    if ((CurrentFrame - LastUsedFrame) > ThresholdFrames)
    {
        if (VertexBuffer)
        {
            VertexBuffer->Release();
            VertexBuffer = nullptr;
        }

        if (IndexBuffer)
        {
            IndexBuffer->Release();
            IndexBuffer = nullptr;
        }
    }
}
*/
FOctreeNode::FOctreeNode(const FBoundingBox& InBounds, int InDepth)
    : Bounds(InBounds)
    , Depth(InDepth)
{
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

void FOctreeNode::Query(const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutResults) const
{
    if (!Frustum.Intersect(Bounds)) return;

    if (!bIsLeaf)
    {
        for (int i = 0; i < 8; ++i)
        {
            if (Children[i])
                Children[i]->Query(Frustum, OutResults);
        }
    }

    for (UPrimitiveComponent* Comp : Components)
    {
        if (Frustum.Intersect(Comp->WorldAABB))
            OutResults.Add(Comp);
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
        }
    }
}

void FOctree::QueryVisible(const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutResults) const
{
    Root->Query(Frustum, OutResults);
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
    FScopeCycleCounter Timer("BuildBatchRenderData");
    VertexBufferSizeInBytes = 0;
    IndexBufferSizeInBytes = 0;
    // Step 1. 자식 먼저 처리
    for (int i = 0; i < 8; ++i)
    {
        if (Children[i])
        {
            Children[i]->BuildBatchRenderData();
            if (Depth >= GRenderDepthMin)
                for (const auto& Pair : Children[i]->CachedBatchData)
                {
                    const FString& MaterialName = Pair.Key;
                    const FRenderBatchData& ChildData = Pair.Value;

                    FRenderBatchData& CurrentData = CachedBatchData.FindOrAdd(MaterialName);
                    CurrentData.MaterialInfo = ChildData.MaterialInfo;

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
                    FVertexCompact TransformedVertex=MeshVertices[oldIndex];
                    // 월드 위치 변환
                    FVector LocalPosition{TransformedVertex.x, TransformedVertex.y, TransformedVertex.z};
                    FVector WorldPosition = ModelMatrix.TransformPosition(LocalPosition);
                    TransformedVertex.x = WorldPosition.x;
                    TransformedVertex.y = WorldPosition.y;
                    TransformedVertex.z = WorldPosition.z;

                    // 노멀 변환 (정규화 포함)
                    /*
                    FVector LocalNormal{TransformedVertex.nx, TransformedVertex.ny, TransformedVertex.nz};
                    FVector WorldNormal = FMatrix::TransformVector(LocalNormal, ModelMatrix).Normalize();
                    TransformedVertex.nx = WorldNormal.x;
                    TransformedVertex.ny = WorldNormal.y;
                    TransformedVertex.nz = WorldNormal.z;*/
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
    FStatRegistry::RegisterResult(Timer);
}

void FOctreeNode::BuildBatchBuffers(FRenderer& Renderer)
{
    FScopeCycleCounter Timer("BuildBatchBuffers");
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
        RenderData.Vertices.Empty();
        RenderData.Indices.Empty();
    }

    for (int i = 0; i < 8; ++i)
    {
        if (Children[i])
            Children[i]->BuildBatchBuffers(Renderer);
    }
    FStatRegistry::RegisterResult(Timer);
}

/*
void FOctreeNode::RenderBatches(
    FRenderer& Renderer,
    const FFrustum& Frustum,
    const FMatrix& VP
) const
{
    EFrustumContainment Containment = Frustum.CheckContainment(Bounds);

    if (Containment == EFrustumContainment::Contains)
    {
        UE_LOG(LogLevel::Display, "[OctreeRender] Rendered Node at Depth: %d | Batches: %d",
               Depth, CachedBatchData.Num());
        for (const auto& Pair : CachedBatchData)
        {
            const FRenderBatchData& RenderData = Pair.Value;
            if (!RenderData.VertexBuffer || !RenderData.IndexBuffer)
                continue;
            // Material 설정
            Renderer.UpdateMaterial(RenderData.MaterialInfo);

            // 버퍼 설정
            UINT offset = 0;
            //Renderer.Graphics->DeviceContext->IASetVertexBuffers(0, 1, &CachedData->VertexBuffer, &Renderer.Stride, &offset);
            //Renderer.Graphics->DeviceContext->IASetIndexBuffer(CachedData->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
            Renderer.Graphics->DeviceContext->IASetVertexBuffers(0, 1, &RenderData.VertexBuffer, &Renderer.Stride, &offset);
            Renderer.Graphics->DeviceContext->IASetIndexBuffer(RenderData.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

            // 상수 버퍼 설정 (Model은 Identity)
            FMatrix Model = FMatrix::Identity;
            FMatrix MVP = Model * VP;
            FMatrix NormalMatrix = FMatrix::Transpose(FMatrix::Inverse(Model));
            Renderer.UpdateConstant(MVP, NormalMatrix, FVector4(0, 0, 0, 0), false);

            // 렌더링
            Renderer.Graphics->DeviceContext->DrawIndexed(RenderData.IndicesNum, 0, 0);
        }

        return;
    }

    // 재귀 탐색
    for (int i = 0; i < 8; ++i)
    {
        if (Children[i])
        {
            Children[i]->RenderBatches(Renderer, Frustum, VP);
        }
    }

}*/
int cnt1 = 0;
int cnt2 = 0;
void FOctreeNode::RenderBatches(FRenderer& Renderer, const FFrustum& Frustum, const FMatrix& VP)
{
    EFrustumContainment Containment = Frustum.CheckContainment(Bounds);
    if (Containment == EFrustumContainment::Contains||Containment == EFrustumContainment::Intersects&&Depth==GRenderDepthMax)
    {
        if (!GOcclusionSystem->IsRegionVisible(NodeId))
        {
            cnt2++;
            return;
        }
        cnt1++;

        if (Depth >= GRenderDepthMin)
        {
            UE_LOG(LogLevel::Display, "[OctreeRender] Rendered Node at Depth: %d | Batches: %d",
                   Depth, CachedBatchData.Num());

            for (auto& Pair : CachedBatchData) // ← 수정: const 제거
            {
                FRenderBatchData& RenderData = Pair.Value;

                // Lazy 생성: 필요한 경우에만 생성
                FScopeCycleCounter Timer("CreateBuffers");
                RenderData.CreateBuffersIfNeeded(Renderer);
                FStatRegistry::RegisterResult(Timer);

                if (!RenderData.VertexBuffer || !RenderData.IndexBuffer)
                    continue;

                // 사용 시점 기록
                RenderData.LastUsedFrame = GCurrentFrame;

                // 머티리얼 설정
                Renderer.UpdateMaterial(RenderData.MaterialInfo);

                // 버퍼 설정
                UINT offset = 0;
                Renderer.Graphics->DeviceContext->IASetVertexBuffers(0, 1, &RenderData.VertexBuffer, &Renderer.Stride, &offset);
                Renderer.Graphics->DeviceContext->IASetIndexBuffer(RenderData.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

                // 상수 버퍼 설정
                FMatrix MVP = FMatrix::Identity * VP;
                FMatrix NormalMatrix = FMatrix::Transpose(FMatrix::Inverse(FMatrix::Identity));
                Renderer.UpdateConstant(MVP, NormalMatrix, FVector4(0, 0, 0, 0), false);

                Renderer.Graphics->DeviceContext->DrawIndexed(RenderData.IndicesNum, 0, 0);
            }
            return;
        }
    }

    for (int i = 0; i < 8; ++i)
    {
        if (Children[i])
            Children[i]->RenderBatches(Renderer, Frustum, VP);
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

void FOctreeNode::QueryOcclusion(FRenderer& Renderer, ID3D11DeviceContext* Context, const FFrustum& Frustum, bool bParentVisible)
{
    if (!bParentVisible)
        return;

    EFrustumContainment Containment = Frustum.CheckContainment(Bounds);
    if (Containment == EFrustumContainment::Contains || (Containment == EFrustumContainment::Intersects && Depth == GRenderDepthMax))
    {
        if (NodeId == 0)
            NodeId = MakeNodeId(Bounds);

        if (GOcclusionSystem->QueriesThisFrame >= MaxQueriesPerFrame)
            return;

        // Z-Depth 기반 Occlusion Query 등록
        GOcclusionSystem->QueryRegion(NodeId, Bounds, Context, [&](const FBoundingBox& Box) {
            Renderer.RenderOcclusionBox(Box);
            });

        GOcclusionSystem->QueriesThisFrame++;

        if (!GOcclusionSystem->IsRegionVisible(NodeId))
            return;

        bool bVisible = GOcclusionSystem->IsRegionVisible(NodeId);

        // 재귀 처리
        for (int i = 0; i < 8; ++i)
        {
            if (Children[i])
                Children[i]->QueryOcclusion(Renderer, Context, Frustum, bVisible);
        }
    }
}