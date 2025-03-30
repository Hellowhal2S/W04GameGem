#pragma once

#include <future>

#include "Define.h"
#include "MathUtility.h"
#include "Vector.h"
#include "Matrix.h"

class FEditorViewportClient;

struct FRay
{
    FVector Origin;
    FVector Direction;

    FRay() = default;
    FRay(const FVector& InOrigin, const FVector& InDirection)
        : Origin(InOrigin)
        , Direction(InDirection.Normalize()) {}

    // 거리 t에서의 점
    FVector GetPointAt(float t) const
    {
        return Origin + Direction * t;
    }

    // 공간 변환 (주로 Local <-> World 변환)
    FRay Transform(const FMatrix& Matrix) const
    {
        FVector NewOrigin = Matrix.TransformPosition(Origin);
        FVector NewTarget = Matrix.TransformPosition(Origin + Direction);
        FVector NewDirection = (NewTarget - NewOrigin).Normalize();
        return FRay(NewOrigin, NewDirection);
    }
    void InitFromScreen(const FVector& ScreenSpace, std::shared_ptr<FEditorViewportClient> ActiveViewport, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix, bool bIsOrtho);
    static FRay GetRayFromViewport(std::shared_ptr<FEditorViewportClient> ViewportClient, const FRect& ViewRect);


};
// Ray가 AABB와 교차하는지 여부 반환 (Slab 방식)
inline bool RayIntersectsAABB(const FRay& Ray, const FBoundingBox& Box, float& OutHitDistance)
{
    float tMin = 0.0f;
    float tMax = FLT_MAX;

    for (int i = 0; i < 3; ++i)
    {
        float RayDir = Ray.Direction[i];
        float RayOrigin = Ray.Origin[i];
        float BoxMin = Box.min[i];
        float BoxMax = Box.max[i];

        if (FMath::Abs(RayDir) < 1e-8f)
        {
            // 평행한 경우, AABB 안에 있는지 검사
            if (RayOrigin < BoxMin || RayOrigin > BoxMax)
                return false;
        }
        else
        {
            float InvDir = 1.0f / RayDir;
            float t1 = (BoxMin - RayOrigin) * InvDir;
            float t2 = (BoxMax - RayOrigin) * InvDir;

            if (t1 > t2) std::swap(t1, t2);

            tMin = FMath::Max(tMin, t1);
            tMax = FMath::Min(tMax, t2);

            if (tMin > tMax)
                return false;
        }
    }

    OutHitDistance = tMin;
    return true;
}