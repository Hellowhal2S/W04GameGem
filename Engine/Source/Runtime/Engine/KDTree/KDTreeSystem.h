#pragma once

#include "KDTree.h"

class FKDTreeSystem
{
public:
    FKDTreeSystem();
    ~FKDTreeSystem();

    void Build(const FBoundingBox& RootBounds);
    UStaticMeshComponent* Raycast(const FRay& Ray, float& OutDistance) const;

private:
    static constexpr int TreeCount = 8;
    FKDTree* Trees[TreeCount];
    FBoundingBox SubBounds[TreeCount];

    int GetRayTargetTreeIndex(const FRay& Ray) const;
};
