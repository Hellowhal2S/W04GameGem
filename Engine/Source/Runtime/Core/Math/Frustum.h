// Frustum.h
#pragma once

#include "Math.h"
#include "Vector.h"

struct FMatrix;
struct FBoundingBox;
enum class EFrustumContainment
{
    Outside,    // 완전히 프러스텀 밖
    Intersects, // 일부만 겹침
    Contains    // 프러스텀이 완전히 포함함
};
enum class EFrustumPlane
{
    Near,
    Far,
    Left,
    Right,
    Top,
    Bottom,
    Count
};

struct FFrustumPlane
{
    FVector Normal;
    float Distance;

    bool IntersectAABB(const FBoundingBox& AABB) const;
};

class FFrustum
{
public:
    FFrustum() = default;

    /** ViewProjection 행렬로부터 6개의 평면을 생성합니다 */
    void ConstructFrustum(const FMatrix& ViewProjectionMatrix);

    /** AABB가 프러스텀에 속해있는지 확인합니다 */
    bool Intersect(const FBoundingBox& AABB) const;

    EFrustumContainment CheckContainment(const FBoundingBox& AABB) const;

private:
    FFrustumPlane Planes[static_cast<int>(EFrustumPlane::Count)];
};
