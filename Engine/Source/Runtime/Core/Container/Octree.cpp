// OctreeSystem.cpp

#include "Octree.h"

#include "Components/PrimitiveComponent.h"
#include "Math/Frustum.h"
#include "UObject/Casts.h"
#include "UObject/UObjectIterator.h"

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
        constexpr float Epsilon = 1e-5f;
        for (int i = 0; i < 8; ++i)
        {
            FVector Min, Max;

            Min.x = (i & 1) ? Center.x : Bounds.min.x;
            Max.x = (i & 1) ? Bounds.max.x : Center.x;

            Min.y = (i & 2) ? Center.y : Bounds.min.y;
            Max.y = (i & 2) ? Bounds.max.y : Center.y;

            Min.z = (i & 4) ? Center.z : Bounds.min.z;
            Max.z = (i & 4) ? Bounds.max.z : Center.z;

            // 보정: 두께가 0인 경우 최소한의 두께 부여
            if (FMath::Abs(Max.x - Min.x) < Epsilon) Max.x = Min.x + Epsilon;
            if (FMath::Abs(Max.y - Min.y) < Epsilon) Max.y = Min.y + Epsilon;
            if (FMath::Abs(Max.z - Min.z) < Epsilon) Max.z = Min.z + Epsilon;

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
