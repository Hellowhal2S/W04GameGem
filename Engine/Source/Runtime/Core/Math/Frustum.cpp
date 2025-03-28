// Frustum.cpp
#include "Frustum.h"

#include "Define.h"
#include "MathUtility.h"

void FFrustum::ConstructFrustum(const FMatrix& VP)
{
    // VP = View * Projection 행렬
    // 각 프러스텀 평면은 VP의 열(row)을 조합하여 구함
    /*
    //row-major
    // Left
    Planes[(int)EFrustumPlane::Left].Normal.x = VP[3][0] + VP[0][0];
    Planes[(int)EFrustumPlane::Left].Normal.y = VP[3][1] + VP[0][1];
    Planes[(int)EFrustumPlane::Left].Normal.z = VP[3][2] + VP[0][2];
    Planes[(int)EFrustumPlane::Left].Distance  = VP[3][3] + VP[0][3];

    // Right
    Planes[(int)EFrustumPlane::Right].Normal.x = VP[3][0] - VP[0][0];
    Planes[(int)EFrustumPlane::Right].Normal.y = VP[3][1] - VP[0][1];
    Planes[(int)EFrustumPlane::Right].Normal.z = VP[3][2] - VP[0][2];
    Planes[(int)EFrustumPlane::Right].Distance  = VP[3][3] - VP[0][3];

    // Bottom
    Planes[(int)EFrustumPlane::Bottom].Normal.x = VP[3][0] + VP[1][0];
    Planes[(int)EFrustumPlane::Bottom].Normal.y = VP[3][1] + VP[1][1];
    Planes[(int)EFrustumPlane::Bottom].Normal.z = VP[3][2] + VP[1][2];
    Planes[(int)EFrustumPlane::Bottom].Distance  = VP[3][3] + VP[1][3];

    // Top
    Planes[(int)EFrustumPlane::Top].Normal.x = VP[3][0] - VP[1][0];
    Planes[(int)EFrustumPlane::Top].Normal.y = VP[3][1] - VP[1][1];
    Planes[(int)EFrustumPlane::Top].Normal.z = VP[3][2] - VP[1][2];
    Planes[(int)EFrustumPlane::Top].Distance  = VP[3][3] - VP[1][3];

    // Near
    Planes[(int)EFrustumPlane::Near].Normal.x = VP[3][0] + VP[2][0];
    Planes[(int)EFrustumPlane::Near].Normal.y = VP[3][1] + VP[2][1];
    Planes[(int)EFrustumPlane::Near].Normal.z = VP[3][2] + VP[2][2];
    Planes[(int)EFrustumPlane::Near].Distance  = VP[3][3] + VP[2][3];

    // Far
    Planes[(int)EFrustumPlane::Far].Normal.x = VP[3][0] - VP[2][0];
    Planes[(int)EFrustumPlane::Far].Normal.y = VP[3][1] - VP[2][1];
    Planes[(int)EFrustumPlane::Far].Normal.z = VP[3][2] - VP[2][2];
    Planes[(int)EFrustumPlane::Far].Distance  = VP[3][3] - VP[2][3];

    */
    //column major
    // Left
    Planes[(int)EFrustumPlane::Left].Normal.x = VP[0][3] + VP[0][0];
    Planes[(int)EFrustumPlane::Left].Normal.y = VP[1][3] + VP[1][0];
    Planes[(int)EFrustumPlane::Left].Normal.z = VP[2][3] + VP[2][0];
    Planes[(int)EFrustumPlane::Left].Distance = VP[3][3] + VP[3][0];

    // Right
    Planes[(int)EFrustumPlane::Right].Normal.x = VP[0][3] - VP[0][0];
    Planes[(int)EFrustumPlane::Right].Normal.y = VP[1][3] - VP[1][0];
    Planes[(int)EFrustumPlane::Right].Normal.z = VP[2][3] - VP[2][0];
    Planes[(int)EFrustumPlane::Right].Distance = VP[3][3] - VP[3][0];

    // Bottom
    Planes[(int)EFrustumPlane::Bottom].Normal.x = VP[0][3] + VP[0][1];
    Planes[(int)EFrustumPlane::Bottom].Normal.y = VP[1][3] + VP[1][1];
    Planes[(int)EFrustumPlane::Bottom].Normal.z = VP[2][3] + VP[2][1];
    Planes[(int)EFrustumPlane::Bottom].Distance = VP[3][3] + VP[3][1];

    // Top
    Planes[(int)EFrustumPlane::Top].Normal.x = VP[0][3] - VP[0][1];
    Planes[(int)EFrustumPlane::Top].Normal.y = VP[1][3] - VP[1][1];
    Planes[(int)EFrustumPlane::Top].Normal.z = VP[2][3] - VP[2][1];
    Planes[(int)EFrustumPlane::Top].Distance = VP[3][3] - VP[3][1];

    // Near
    Planes[(int)EFrustumPlane::Near].Normal.x = VP[0][3] + VP[0][2];
    Planes[(int)EFrustumPlane::Near].Normal.y = VP[1][3] + VP[1][2];
    Planes[(int)EFrustumPlane::Near].Normal.z = VP[2][3] + VP[2][2];
    Planes[(int)EFrustumPlane::Near].Distance = VP[3][3] + VP[3][2];

    // Far
    Planes[(int)EFrustumPlane::Far].Normal.x = VP[0][3] - VP[0][2];
    Planes[(int)EFrustumPlane::Far].Normal.y = VP[1][3] - VP[1][2];
    Planes[(int)EFrustumPlane::Far].Normal.z = VP[2][3] - VP[2][2];
    Planes[(int)EFrustumPlane::Far].Distance = VP[3][3] - VP[3][2];
    
    // 정규화
    for (int i = 0; i < (int)EFrustumPlane::Count; ++i)
    {
        float Length = Planes[i].Normal.Magnitude();
        if (Length > SMALL_NUMBER)
        {
            Planes[i].Normal = Planes[i].Normal / Length;
            Planes[i].Distance /= Length;
        }
    }
}

bool FFrustumPlane::IntersectAABB(const FBoundingBox& AABB) const
{
    // AABB의 중심과 반경
    FVector Center = (AABB.min + AABB.max) * 0.5f;
    FVector Extents = (AABB.max - AABB.min) * 0.5f;

    float Radius =
        Extents.x * FMath::Abs(Normal.x) +
        Extents.y * FMath::Abs(Normal.y) +
        Extents.z * FMath::Abs(Normal.z);

    float DistanceToCenter = Normal.Dot(Center) + Distance;
    //return DistanceToCenter - Radius <= 0;
    return DistanceToCenter + Radius >= 0;
}

bool FFrustum::Intersect(const FBoundingBox& AABB) const
{
    for (int i = 0; i < (int)EFrustumPlane::Count; ++i)
    {
        if (!Planes[i].IntersectAABB(AABB))
            return false;
    }
    return true;
}
EFrustumContainment FFrustum::CheckContainment(const FBoundingBox& AABB) const
{
    bool bAllInside = true;

    for (int i = 0; i < (int)EFrustumPlane::Count; ++i)
    {
        const FFrustumPlane& Plane = Planes[i];

        // AABB의 중심과 반경
        FVector Center = (AABB.min + AABB.max) * 0.5f;
        FVector Extents = (AABB.max - AABB.min) * 0.5f;

        float Radius =
            Extents.x * FMath::Abs(Plane.Normal.x) +
            Extents.y * FMath::Abs(Plane.Normal.y) +
            Extents.z * FMath::Abs(Plane.Normal.z);

        float DistanceToCenter = Plane.Normal.Dot(Center) + Plane.Distance;

        if (DistanceToCenter + Radius < 0)
        {
            return EFrustumContainment::Outside; // 완전히 밖
        }
        else if (DistanceToCenter - Radius < 0)
        {
            bAllInside = false; // 완전히 포함된 건 아님
        }
    }

    return bAllInside ? EFrustumContainment::Contains : EFrustumContainment::Intersects;
}
