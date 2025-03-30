#include "KDTree.h"

#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Math/Ray.h"
#include "UObject/Casts.h"
#include "UObject/UObjectIterator.h"
FKDTree::FKDTree()
{
    Root = nullptr;
}
FKDTree::~FKDTree()
{
	delete Root;
}

FKDTreeNode::~FKDTreeNode()
{
	delete Left;
	delete Right;
}

void FKDTree::Build()
{
	if (!PendingComponents.IsEmpty())
	{
		Root = new FKDTreeNode();
		Root->Build(PendingComponents);
	}
}

static bool CompareAxis(UStaticMeshComponent* A, UStaticMeshComponent* B, EKDAxis Axis)
{
	return A->WorldAABB.GetCenter()[static_cast<int>(Axis)] <
		   B->WorldAABB.GetCenter()[static_cast<int>(Axis)];
}
void FKDTreeNode::Build(TArray<UStaticMeshComponent*>& InComponents, int Depth)
{
	if (InComponents.Num() == 1)
	{
		Component = InComponents[0];
		Bounds = Component->WorldAABB;
		bIsLeaf = true;
		return;
	}

	SplitAxis = static_cast<EKDAxis>(Depth % 3);
    InComponents.Sort([this](UStaticMeshComponent* A, UStaticMeshComponent* B) {
        return CompareAxis(A, B, SplitAxis);
    });
	int Mid = InComponents.Num() / 2;
    TArray<UStaticMeshComponent*> LeftComps  = Slice(InComponents, 0, Mid);                     // 0 ~ Mid-1
    TArray<UStaticMeshComponent*> RightComps = Slice(InComponents, Mid, InComponents.Num() - Mid); // Mid ~ End
    
	Left = new FKDTreeNode();
	Left->Build(LeftComps, Depth + 1);

	Right = new FKDTreeNode();
	Right->Build(RightComps, Depth + 1);

    Bounds = FBoundingBox::Union(Right->Bounds, Left->Bounds);
}

UStaticMeshComponent* FKDTree::Raycast(const FRay& Ray, float& OutDistance) const
{
	if (!Root) return nullptr;
	return Root->Raycast(Ray, OutDistance);
}

UStaticMeshComponent* FKDTreeNode::Raycast(const FRay& Ray, float& OutDistance) const
{
	float NodeHitDist;
	if (!RayIntersectsAABB(Ray, Bounds, NodeHitDist))
		return nullptr;

	if (bIsLeaf)
	{
		float HitDist = 0;
		if (Component->CheckRayIntersection(Ray.Origin, Ray.Direction, HitDist))
		{
			OutDistance = HitDist;
			return Component;
		}
		return nullptr;
	}

	UStaticMeshComponent* HitA = nullptr;
	UStaticMeshComponent* HitB = nullptr;
	float DistA = FLT_MAX, DistB = FLT_MAX;

	if (Left)  HitA = Left->Raycast(Ray, DistA);
	if (Right) HitB = Right->Raycast(Ray, DistB);

	if (HitA && HitB)
	{
		if (DistA < DistB)
		{
			OutDistance = DistA;
			return HitA;
		}
		else
		{
			OutDistance = DistB;
			return HitB;
		}
	}
	else if (HitA)
	{
		OutDistance = DistA;
		return HitA;
	}
	else if (HitB)
	{
		OutDistance = DistB;
		return HitB;
	}
	return nullptr;
}
