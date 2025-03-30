#include "Engine/Source/Runtime/Engine/World.h"

#include "Actors/Player.h"
#include "BaseGizmos/TransformGizmo.h"
#include "Camera/CameraComponent.h"
#include "LevelEditor/SLevelEditor.h"
#include "Engine/FLoaderOBJ.h"
#include "Classes/Components/StaticMeshComponent.h"
#include "Components/CubeComp.h"
#include "Engine/StaticMeshActor.h"
#include "Components/SkySphereComponent.h"
#include "Components/SphereComp.h"
#include "KDTree/KDTree.h"
#include "Octree/Octree.h"
#include "Math/Frustum.h"
#include "Math/JungleMath.h"
#include "Profiling/PlatformTime.h"
#include "Profiling/StatRegistry.h"
#include "UnrealEd/PrimitiveBatch.h"
#include "UObject/UObjectIterator.h"


void UWorld::Initialize()
{
    // TODO: Load Scene
    FScopeCycleCounter Timer("CreateBaseObject");
    CreateBaseObject();
    FStatRegistry::RegisterResult(Timer); 
    BuildOctree();
    SceneKDTree = new FKDTree();
    SceneKDTree->Build();
}

void UWorld::CreateBaseObject()
{
    if (EditorPlayer == nullptr)
    {
        EditorPlayer = FObjectFactory::ConstructObject<AEditorPlayer>();;
    }
    FManagerOBJ::CreateStaticMesh("Data/JungleApples/apple_mid.obj");


    for (int i = 0; i < 10; i++)
    {
        for (int j = 0; j < 10; j++)
        {
            for (int k = 0; k < 10; k++)
            {
                AActor* SpawnedActor = SpawnActor<AActor>();
                UStaticMeshComponent* apple = SpawnedActor->AddComponent<UStaticMeshComponent>();
                apple->SetStaticMesh(FManagerOBJ::GetStaticMesh(L"apple_mid.obj"));
                FVector newPos = FVector(i, j, k);
                SpawnedActor->SetActorLocation(newPos);
                apple->UpdateWorldAABB();
            }
        }
    }
    // if (LocalGizmo == nullptr)
    // {
    //     LocalGizmo = FObjectFactory::ConstructObject<UTransformGizmo>();
    // }
}

void UWorld::ReleaseBaseObject()
{
    if (LocalGizmo)
    {
        delete LocalGizmo;
        LocalGizmo = nullptr;
    }

    if (worldGizmo)
    {
        delete worldGizmo;
        worldGizmo = nullptr;
    }

    if (EditorPlayer)
    {
        delete EditorPlayer;
        EditorPlayer = nullptr;
    }
}

void UWorld::Tick(float DeltaTime)
{
	// camera->TickComponent(DeltaTime);
	EditorPlayer->Tick(DeltaTime);
	// LocalGizmo->Tick(DeltaTime);
    // SpawnActor()에 의해 Actor가 생성된 경우, 여기서 BeginPlay 호출
    for (AActor* Actor : PendingBeginPlayActors)
    {
        Actor->BeginPlay();
    }
    PendingBeginPlayActors.Empty();

    // 매 틱마다 Actor->Tick(...) 호출
    for (AActor* Actor : ActorsArray)
    {
        //Actor->Tick(DeltaTime);
    }
}

void UWorld::Release()
{
    for (AActor* Actor : ActorsArray)
    {
        Actor->EndPlay(EEndPlayReason::WorldTransition);
        TSet<UActorComponent*> Components = Actor->GetComponents();
        for (UActorComponent* Component : Components)
        {
            GUObjectArray.MarkRemoveObject(Component);
        }
        GUObjectArray.MarkRemoveObject(Actor);
    }
    ActorsArray.Empty();

    pickingGizmo = nullptr;
    ReleaseBaseObject();

    GUObjectArray.ProcessPendingDestroyObjects();
}

bool UWorld::DestroyActor(AActor* ThisActor)
{
    if (ThisActor->GetWorld() == nullptr)
    {
        return false;
    }

    if (ThisActor->IsActorBeingDestroyed())
    {
        return true;
    }

    // 액터의 Destroyed 호출
    ThisActor->Destroyed();

    if (ThisActor->GetOwner())
    {
        ThisActor->SetOwner(nullptr);
    }

    TSet<UActorComponent*> Components = ThisActor->GetComponents();
    for (UActorComponent* Component : Components)
    {
        Component->DestroyComponent();
    }

    // World에서 제거
    ActorsArray.Remove(ThisActor);

    // 제거 대기열에 추가
    GUObjectArray.MarkRemoveObject(ThisActor);
    return true;
}

void UWorld::SetPickingGizmo(UObject* Object)
{
    pickingGizmo = Cast<USceneComponent>(Object);
}
void UWorld::SetHighlightedComponent(UStaticMeshComponent* OriginalMeshComp)
{
    if (!OriginalMeshComp) return;

    // 기존 하이라이트 사과 제거
    if (HighlightedMeshComp)
    {
        HighlightedMeshComp->GetOwner()->Destroy();
        HighlightedMeshComp->DestroyComponent();
        HighlightedMeshComp = nullptr;
    }
    AActor* SpawnedActor = SpawnActor<AActor>();
    UStaticMeshComponent* apple = SpawnedActor->AddComponent<UStaticMeshComponent>();
    
    apple->SetStaticMesh(OriginalMeshComp->GetStaticMesh());
    apple->SetLocation(OriginalMeshComp->GetLocalLocation());
    SpawnedActor->SetActorLocation(OriginalMeshComp->GetOwner()->GetActorLocation());
    HighlightedMeshComp=apple;
}
void UWorld::RenderHighlightedComponent(FRenderer& Renderer, const FMatrix& VP)
{
    if (!HighlightedMeshComp || !HighlightedMeshComp->GetStaticMesh()) return;

    const OBJ::FStaticMeshRenderData* RenderData = HighlightedMeshComp->GetStaticMesh()->GetRenderData();
    if (!RenderData || RenderData->Vertices.IsEmpty() || RenderData->Indices.IsEmpty()) return;

    const TArray<FVertexCompact>& Vertices = RenderData->Vertices;
    const TArray<UINT>& Indices = RenderData->Indices;
    const TArray<FObjMaterialInfo>& Materials = RenderData->Materials;
    const TArray<FMaterialSubset>& Subsets = RenderData->MaterialSubsets;

    // 버텍스/인덱스 버퍼 생성 (임시, 매 프레임 해도 될 정도로 가볍게)
    ID3D11Buffer* VB = Renderer.CreateVertexBuffer(Vertices, Vertices.Num() * sizeof(FVertexCompact));
    ID3D11Buffer* IB = Renderer.CreateIndexBuffer(Indices, Indices.Num() * sizeof(UINT));

    if (!VB || !IB) return;

    UINT stride = sizeof(FVertexCompact);
    UINT offset = 0;
    Renderer.Graphics->DeviceContext->IASetVertexBuffers(0, 1, &VB, &stride, &offset);
    Renderer.Graphics->DeviceContext->IASetIndexBuffer(IB, DXGI_FORMAT_R32_UINT, 0);

    FMatrix ModelMatrix = JungleMath::CreateModelMatrix(
        HighlightedMeshComp->GetWorldLocation(),
        HighlightedMeshComp->GetWorldRotation(),
        HighlightedMeshComp->GetWorldScale()*1.01f
    );

    FMatrix MVP = ModelMatrix * VP;
    FMatrix NormalMatrix = FMatrix::Transpose(FMatrix::Inverse(ModelMatrix));
    
    // 서브셋별 렌더링 (여러 머티리얼 처리)
    for (const FMaterialSubset& Subset : Subsets)
    {
        const FObjMaterialInfo& MatInfo = Materials[Subset.MaterialIndex];

        Renderer.UpdateMaterial(MatInfo);
        Renderer.UpdateConstant(MVP, NormalMatrix, FVector4(0, 0, 0, 0), true); // ⭐ Highlight On
        Renderer.Graphics->DeviceContext->DrawIndexed(Subset.IndexCount, Subset.IndexStart, 0);
    }
    Renderer.UpdateConstant(MVP, NormalMatrix, FVector4(0, 0, 0, 0), false);

    // 직접 만든 버퍼 해제
    VB->Release();
    IB->Release();
}
void UWorld::BuildOctree()
{
    FScopeCycleCounter Timer("BuildOctree");
    FVector MinBound = FVector(FLT_MAX,FLT_MAX,FLT_MAX);
    FVector MaxBound = FVector(-FLT_MAX,-FLT_MAX,-FLT_MAX);
    for (const auto* SceneComp : TObjectRange<USceneComponent>())
    {
        if (const auto* MeshComp = Cast<UMeshComponent>(SceneComp))
        {
            //const FBoundingBox& AABB = MeshComp->AABB;
            //FBoundingBox WorldAABB = JungleMath::TransformAABB(AABB, SceneComp->GetOwner()->GetModelMatrix());
            auto *PrimComp = Cast<UPrimitiveComponent>(MeshComp);
            PrimComp->UpdateWorldAABB();
            FBoundingBox WorldAABB = MeshComp->WorldAABB;
            MinBound = FVector::Min(MinBound, WorldAABB.min);
            MaxBound = FVector::Max(MaxBound, WorldAABB.max);
        }
    }
    FVector Center = (MinBound + MaxBound) * 0.5f;
    FVector Extents = (MaxBound - MinBound) * 0.5f;

    // 가장 긴 축의 길이 기준
    float MaxExtent = FMath::Max(Extents.x, FMath::Max(Extents.y, Extents.z));

    // 정육면체 범위
    FVector CubeExtent(MaxExtent, MaxExtent, MaxExtent);
    FVector CubeMin = Center - CubeExtent;
    FVector CubeMax = Center + CubeExtent;

    //FBoundingBox WorldBounds = FBoundingBox(CubeMin, CubeMax);
    FBoundingBox WorldBounds = FBoundingBox(MinBound, MaxBound);
    delete SceneOctree;
    SceneOctree = new FOctree(WorldBounds);
    SceneOctree->Build(); // 모든 컴포넌트 삽입
    SceneOctree->GetRoot()->BuildBatchRenderData();   // 2단계: 머티리얼 기준 정점 수집
    SceneOctree->GetRoot()->BuildBatchBuffers(FEngineLoop::renderer);
    SceneOctree->GetRoot()->ClearBatchDatas(FEngineLoop::renderer);
    FStatRegistry::RegisterResult(Timer); 
}
void UWorld::ClearScene()
{
    // 1. 모든 Actor Destroy
    for (UPrimitiveComponent* Prim : TObjectRange<UPrimitiveComponent>())
    {
        Prim->GetOwner()->Destroy();
        Prim->DestroyComponent();
        
    }
}
