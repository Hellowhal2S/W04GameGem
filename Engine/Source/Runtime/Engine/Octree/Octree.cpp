// OctreeSystem.cpp

#include "Octree.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Math/Frustum.h"
#include "Math/JungleMath.h"
#include "Profiling/PlatformTime.h"
#include "Profiling/StatRegistry.h"
#include "UnrealEd/PrimitiveBatch.h"
#include "UObject/Casts.h"
#include "UObject/UObjectIterator.h"
int GCurrentFrame = 0;
TMap<size_t, FRenderBatchData*> GGlobalMergedBatchPool;
size_t GenerateBatchHash(const TArray<FVertexCompact>& Vertices, const TArray<uint32>& Indices)
{
    size_t Hash = 0;
    for (const auto& V : Vertices)
    {
        Hash ^= std::hash<float>()(V.x) ^ std::hash<float>()(V.y) ^ std::hash<float>()(V.z);
        Hash ^= std::hash<float>()(V.u) ^ std::hash<float>()(V.v);
    }
    for (uint32 i : Indices)
    {
        Hash ^= std::hash<uint32>()(i);
    }
    return Hash;
}
TMap<FString, FRenderBatchData> GBatchRegistry;

FRenderBatchData* FindOrAddGlobalBatch(const FString& Key)
{
    if (FRenderBatchData* Found = GBatchRegistry.Find(Key))
    {
        return Found;
    }
    GBatchRegistry.Add(Key, FRenderBatchData{});
    return GBatchRegistry.Find(Key);
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

FOctreeNode::FOctreeNode(const FBoundingBox& InBounds, int InDepth)
    : Bounds(InBounds)
    , Depth(InDepth)
{
}

FOctreeNode::~FOctreeNode()
{
    for (int i = 0; i < 8; ++i)
        delete Children[i];
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
/*
void FOctreeNode::BuildBatchRenderData()
{
    FScopeCycleCounter Timer("BuildBatchRenderData");
    VertexBufferSizeInBytes = 0;
    IndexBufferSizeInBytes = 0;
    
    FVector NodeCenter = (Bounds.min + Bounds.max) * 0.5f;
    // Step 1. 자식 먼저 처리
    for (int i = 0; i < 8; ++i)
    {
        if (Children[i])
        {
            Children[i]->BuildBatchRenderData();
            if (Depth >= GVertexBufferCutoffDepth)
                for (const auto& Pair : Children[i]->CachedBatchData)
                {
                    const FString& MaterialName = Pair.Key;
                    const FRenderBatchData& ChildData = Pair.Value;

                    FRenderBatchData& CurrentData = CachedBatchData.FindOrAdd(MaterialName);
                    CurrentData.MaterialInfo = ChildData.MaterialInfo;

                    UINT VertexOffset = (UINT)CurrentData.Vertices.Num();
                    FVector ChildCenter = (Children[i]->Bounds.min + Children[i]->Bounds.max) * 0.5f;
                    FVector Offset = ChildCenter - NodeCenter;
                    for (const FVertexCompact& Vtx : ChildData.Vertices)
                    {
                        FVertexCompact Adjusted = Vtx;
                        Adjusted.x += Offset.x;
                        Adjusted.y += Offset.y;
                        Adjusted.z += Offset.z;
                        CurrentData.Vertices.Add(Adjusted);
                    }

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

            FString UniqueKey = StaticMeshComp->GetStaticMesh()->GetName() + TEXT("_mat") + FString::FromInt(Subset.MaterialIndex);
            FRenderBatchData* GlobalBatch = FindOrAddGlobalBatch(UniqueKey);
            GlobalBatch->MaterialInfo = MatInfo;

            FRenderBatchData& LocalEntry = CachedBatchData.FindOrAdd(MatInfo.MTLName);
            LocalEntry.MaterialInfo = MatInfo;
            
            UINT VertexStart = (UINT)LocalEntry.Vertices.Num();
            TMap<UINT, UINT> IndexMap;

            for (UINT j = 0; j < Subset.IndexCount; ++j)
            {
                UINT oldIndex = MeshIndices[Subset.IndexStart + j];
                if (!IndexMap.Contains(oldIndex))
                {
                    FVertexCompact TransformedVertex = ConvertToCompact(MeshVertices[oldIndex]);
                    
                    FVector LocalPos = { TransformedVertex.x, TransformedVertex.y, TransformedVertex.z };
                    FVector WorldPos = ModelMatrix.TransformPosition(LocalPos);
                    FVector RelativePos = WorldPos - NodeCenter;
                    TransformedVertex.x = RelativePos.x;
                    TransformedVertex.y = RelativePos.y;
                    TransformedVertex.z = RelativePos.z;
                    
                    LocalEntry.Vertices.Add(TransformedVertex);
                    GlobalBatch->Vertices.Add(TransformedVertex);

                    IndexMap.Add(oldIndex, VertexStart++);
                }
                LocalEntry.Indices.Add(IndexMap[oldIndex]);
                GlobalBatch->Indices.Add(IndexMap[oldIndex]);
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
*/
void FOctreeNode::BuildBatchRenderData()
{
    FScopeCycleCounter Timer("BuildBatchRenderData");
    VertexBufferSizeInBytes = 0;
    IndexBufferSizeInBytes = 0;

    for (int i = 0; i < 8; ++i)
    {
        if (Children[i])
            Children[i]->BuildBatchRenderData();
    }

    ProcessLocalComponents();
    MergeChildBatchData();

    FStatRegistry::RegisterResult(Timer);
}
void FOctreeNode::ProcessLocalComponents()
{
    FVector NodeCenter = (Bounds.min + Bounds.max) * 0.5f;

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

            TArray<FVertexCompact> TempVertices;
            TArray<uint32> TempIndices;
            TMap<uint32, uint32> IndexRemap;
            uint32 TempStart = 0;

            for (uint32 j = 0; j < Subset.IndexCount; ++j)
            {
                uint32 oldIndex = MeshIndices[Subset.IndexStart + j];
                if (!IndexRemap.Contains(oldIndex))
                {
                    FVertexCompact V = ConvertToCompact(MeshVertices[oldIndex]);
                    FVector LocalPos = { V.x, V.y, V.z };
                    FVector WorldPos = ModelMatrix.TransformPosition(LocalPos);
                    FVector RelativePos = WorldPos - NodeCenter;
                    V.x = RelativePos.x;
                    V.y = RelativePos.y;
                    V.z = RelativePos.z;

                    IndexRemap.Add(oldIndex, TempStart++);
                    TempVertices.Add(V);
                }
                TempIndices.Add(IndexRemap[oldIndex]);
            }

            size_t HashKey = GenerateBatchHash(TempVertices, TempIndices);
            FRenderBatchData* Shared = nullptr;

            if (FRenderBatchData** Found = GGlobalMergedBatchPool.Find(HashKey))
            {
                Shared = *Found;
            }
            else
            {
                Shared = new FRenderBatchData();
                Shared->Vertices = TempVertices;
                Shared->Indices = TempIndices;
                Shared->MaterialInfo = MatInfo;
                GGlobalMergedBatchPool.Add(HashKey, Shared);
            }

            CachedBatchData.Add(MatInfo.MTLName, Shared);
            VertexBufferSizeInBytes += Shared->Vertices.Num() * sizeof(FVertexCompact);
            IndexBufferSizeInBytes += Shared->Indices.Num() * sizeof(uint32);
        }
    }
}

void FOctreeNode::MergeChildBatchData()
{
    if (Depth < GVertexBufferCutoffDepth) return;

    FVector NodeCenter = (Bounds.min + Bounds.max) * 0.5f;

    for (int i = 0; i < 8; ++i)
    {
        if (!Children[i]) continue;

        FOctreeNode* Child = Children[i];
        FVector ChildCenter = (Child->Bounds.min + Child->Bounds.max) * 0.5f;
        FVector Offset = ChildCenter - NodeCenter;

        for (const auto& Pair : Child->CachedBatchData)
        {
            const FString& MatName = Pair.Key;
            const FRenderBatchData* ChildData = Pair.Value;

            //FRenderBatchData* Merged = CachedBatchData.FindOrAdd(MatName);
            FRenderBatchData*& MergedRef = CachedBatchData.FindOrAdd(MatName);
            if (MergedRef == nullptr)
            {
                MergedRef = new FRenderBatchData(); // 초기화 누락 방지
            }
            FRenderBatchData* Merged = MergedRef;
            
            Merged->MaterialInfo = ChildData->MaterialInfo;

            uint32 VertexOffset = Merged->Vertices.Num();
            for (const FVertexCompact& Vtx : ChildData->Vertices)
            {
                FVertexCompact Adjusted = Vtx;
                Adjusted.x += Offset.x;
                Adjusted.y += Offset.y;
                Adjusted.z += Offset.z;
                Merged->Vertices.Add(Adjusted);
            }

            for (uint32 Idx : ChildData->Indices)
            {
                Merged->Indices.Add(Idx + VertexOffset);
            }

            VertexBufferSizeInBytes += ChildData->Vertices.Num() * sizeof(FVertexCompact);
            IndexBufferSizeInBytes += ChildData->Indices.Num() * sizeof(uint32);
        }
    }
}

void FOctreeNode::RenderBatches(FRenderer& Renderer, const FFrustum& Frustum, const FMatrix& VP)
{
    EFrustumContainment Containment = Frustum.CheckContainment(Bounds);
    if (Containment == EFrustumContainment::Contains)//Disjoint로 바꾸고 return?
    {
        if (Depth >= GVertexBufferCutoffDepth)
        {
            UE_LOG(LogLevel::Display, "[OctreeRender] Rendered Node at Depth: %d | Batches: %d",
                   Depth, CachedBatchData.Num());

            FVector NodeCenter = (Bounds.min + Bounds.max) * 0.5f;
            FMatrix ModelMatrix = FMatrix::CreateTranslationMatrix(NodeCenter);
            
            for (auto& Pair : CachedBatchData) // ← 수정: const 제거
            {
                FRenderBatchData* RenderData = Pair.Value;

                // 🟡 Lazy 생성: 필요한 경우에만 생성
                FScopeCycleCounter Timer("CreateBuffers");
                RenderData->CreateBuffersIfNeeded(Renderer);
                FStatRegistry::RegisterResult(Timer);

                if (!RenderData->VertexBuffer || !RenderData->IndexBuffer)
                    continue;

                // ✅ 사용 시점 기록
                RenderData->LastUsedFrame = GCurrentFrame;

                // 머티리얼 설정
                Renderer.UpdateMaterial(RenderData->MaterialInfo);

                // 버퍼 설정
                UINT offset = 0;
                Renderer.Graphics->DeviceContext->IASetVertexBuffers(0, 1, &RenderData->VertexBuffer, &Renderer.Stride, &offset);
                Renderer.Graphics->DeviceContext->IASetIndexBuffer(RenderData->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

                // 상수 버퍼 설정
                FMatrix MVP = ModelMatrix * VP;
                FMatrix NormalMatrix = FMatrix::Transpose(FMatrix::Inverse(ModelMatrix));
                Renderer.UpdateConstant(MVP, NormalMatrix, FVector4(0, 0, 0, 0), false);

                Renderer.Graphics->DeviceContext->DrawIndexed(RenderData->IndicesNum, 0, 0);
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
        FRenderBatchData* RenderData = Pair.Value;

        // 사용된 지 오래된 경우 메모리 해제
        if (RenderData->VertexBuffer && (CurrentFrame - RenderData->LastUsedFrame > FrameThreshold))
        {
            RenderData->VertexBuffer->Release();
            RenderData->VertexBuffer = nullptr;
        }

        if (RenderData->IndexBuffer && (CurrentFrame - RenderData->LastUsedFrame > FrameThreshold))
        {
            RenderData->IndexBuffer->Release();
            RenderData->IndexBuffer = nullptr;
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
