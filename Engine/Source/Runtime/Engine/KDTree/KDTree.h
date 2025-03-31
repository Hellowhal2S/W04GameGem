#pragma once

#include "Define.h"

struct FRay;
class UStaticMeshComponent;

enum class EKDAxis : uint8
{
    X = 0,
    Y = 1,
    Z = 2
};

class FKDTreeNode
{
public:
    FBoundingBox Bounds;
    UStaticMeshComponent* Component = nullptr;
    EKDAxis SplitAxis = EKDAxis::X;
    float SplitValue = 0.0f;

    FKDTreeNode* Left = nullptr;
    FKDTreeNode* Right = nullptr;

    bool bIsLeaf = false;

    FKDTreeNode() = default;
    ~FKDTreeNode();

    void Build(TArray<UStaticMeshComponent*>& InComponents, int Depth = 0);
    UStaticMeshComponent* Raycast(const FRay& Ray, float& OutDistance) const;
    
};

class FKDTree
{
public:
    FKDTree();
    ~FKDTree();
    TArray<UStaticMeshComponent*> PendingComponents; // 각 SubTree에 의해 분배된 컴포넌트

    void Build();
    UStaticMeshComponent* Raycast(const FRay& Ray, float& OutDistance) const;

private:
    FKDTreeNode* Root = nullptr;
};
