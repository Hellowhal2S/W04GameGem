#pragma once

#include "Define.h"

struct AABB
{
    FVector min; // 최소 좌표 (X, Y, Z)
    FVector max; // 최대 좌표 (X, Y, Z)

    static AABB FromMinMax(const FVector& minPoint, const FVector& maxPoint)
    {
        AABB aabb;
        aabb.min = minPoint;
        aabb.max = maxPoint;
        return aabb;
    }

    FVector Size() const
    {
        return max - min;
    }
};


struct Plane {
    FVector normal;  // 평면의 법선 벡터 (A, B, C)
    float d;         // 평면의 D값 (원점에서의 거리)

    Plane() : normal(FVector(0, 0, 0)), d(0) {}

    // 평면을 정의하는 방식으로, 법선 벡터와 한 점을 이용해 평면을 정의할 수 있다.
    Plane(const FVector& Normal, float D) : normal(Normal), d(D) {}

    // 점과 평면의 내적 계산
    float DotCoord(const FVector& point) const {
        return normal.x * point.x + normal.y * point.y + normal.z * point.z + d;
    }

    // 평면과 법선 벡터 계산
    void Normalize() {
        float length = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        normal.x /= length;
        normal.y /= length;
        normal.z /= length;
        d /= length;
    }
};

struct Frustum {
    FVector ndcCorners[8] = {
        {-1, -1,  0}, // Near bottom-left
        { 1, -1,  0}, // Near bottom-right
        { 1,  1,  0}, // Near top-right
        {-1,  1,  0}, // Near top-left
        {-1, -1,  1}, // Far bottom-left
        { 1, -1,  1}, // Far bottom-right
        { 1,  1,  1}, // Far top-right
        {-1,  1,  1}, // Far top-left
     };

    AABB GetBoundingBox(const FMatrix& viewProjMatrix)
    {
        FMatrix inv = FMatrix::Inverse(viewProjMatrix);

        AABB bounds;
        bounds.min = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
        bounds.max = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (int i = 0; i < 8; ++i)
        {
            const FVector& ndc = ndcCorners[i];
            FVector4 ndc4(ndc.x, ndc.y, ndc.z, 1.0f);

            // NDC -> World space 변환
            FVector4 world = inv.TransformFVector4(ndc4);
            FVector pos = FVector(world.x, world.y, world.z) / world.a;

            bounds.min = FVector::Min(bounds.min, pos);
            bounds.max = FVector::Max(bounds.max, pos);
        }

        return bounds;
    }
};

struct Apple {
    FVector position; // 사과의 위치 (x, y, z)
    // 기타 사과의 속성들 (색상, 모델 등)
};

struct GridCell {
    AABB bounds;   // 셀의 AABB
    std::vector<Apple*> apples;  // 셀에 포함된 사과들
};

struct Group
{
    int id;
    AABB bounds;
    bool visible = true;
    int lastTestFrame = 0; // 마지막으로 쿼리한 프레임
    ID3D11Query* query = nullptr;
};