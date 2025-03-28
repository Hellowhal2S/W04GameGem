// OctreeSystem.h
#pragma once

#include "Define.h"
#include "Math.h"

class UPrimitiveBatch;
class FFrustum;
class UPrimitiveComponent;

class FOctreeNode
{
public:
    FBoundingBox Bounds;
    TArray<UPrimitiveComponent*> Components;
    FOctreeNode* Children[8] = { nullptr };
    bool bIsLeaf = true;
    int Depth = 0;

    FOctreeNode(const FBoundingBox& InBounds, int InDepth);
    ~FOctreeNode();

    void Insert(UPrimitiveComponent* Component, int MaxDepth = 5);
    void Query(const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutResults) const;
};

class FOctree
{
public:
    FOctree(const FBoundingBox& InBounds);
    ~FOctree();

    void Build();
    void QueryVisible(const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutResults) const;
    void DebugRenderOctreeNode(UPrimitiveBatch* PrimitiveBatch, const FOctreeNode* Node);
    const FOctreeNode* GetRoot() const{return Root;};
private:
    FOctreeNode* Root;
};
void DebugRenderOctreeNode(UPrimitiveBatch* PrimitiveBatch, const FOctreeNode* Node,int MaxDepth);