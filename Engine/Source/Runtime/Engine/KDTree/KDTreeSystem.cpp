#include "KDTreeSystem.h"
#include "UObject/UObjectIterator.h"
#include "Components/StaticMeshComponent.h"
#include "Math/Ray.h"
#include "UObject/Casts.h"

FKDTreeSystem::FKDTreeSystem()
{
    for (int i = 0; i < TreeCount; ++i)
        Trees[i] = nullptr;
}

FKDTreeSystem::~FKDTreeSystem()
{
    for (int i = 0; i < TreeCount; ++i)
        delete Trees[i];
}

void FKDTreeSystem::Build(const FBoundingBox& RootBounds)
{
    FVector Center = (RootBounds.min + RootBounds.max) * 0.5f;

    // 8분할 BoundingBox 생성
    for (int i = 0; i < TreeCount; ++i)
    {
        FVector Min{
            (i & 1) ? Center.x : RootBounds.min.x,
            (i & 2) ? Center.y : RootBounds.min.y,
            (i & 4) ? Center.z : RootBounds.min.z
        };
        FVector Max{
            (i & 1) ? RootBounds.max.x : Center.x,
            (i & 2) ? RootBounds.max.y : Center.y,
            (i & 4) ? RootBounds.max.z : Center.z
        };
        SubBounds[i] = FBoundingBox(Min, Max);
        Trees[i] = new FKDTree();
    }

    // 컴포넌트 분배
    for (USceneComponent* SceneComp : TObjectRange<USceneComponent>())
    {
        UStaticMeshComponent* Comp = Cast<UStaticMeshComponent>(SceneComp);
        if (!Comp) continue;

        Comp->UpdateWorldAABB();

        for (int i = 0; i < 8; ++i)
        {
            if (SubBounds[i].Contains(Comp->WorldAABB.GetCenter()))
            {
                Trees[i]->PendingComponents.Add(Comp);
                break;
            }
        }
    }

    // 트리 빌드
    for (int i = 0; i < 8; ++i)
    {
        Trees[i]->Build();
    }
}

UStaticMeshComponent* FKDTreeSystem::Raycast(const FRay& Ray, float& OutDistance) const
{
    UStaticMeshComponent* Closest = nullptr;
    float MinDistance = FLT_MAX;

    for (int i = 0; i < TreeCount; ++i)
    {
        float Dist;
        if (!RayIntersectsAABB(Ray, SubBounds[i], Dist))
            continue;

        UStaticMeshComponent* Hit = Trees[i]->Raycast(Ray, Dist);
        if (Hit && Dist < MinDistance)
        {
            Closest = Hit;
            MinDistance = Dist;
        }
    }

    OutDistance = MinDistance;
    return Closest;
}
