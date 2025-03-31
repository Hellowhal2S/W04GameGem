#include "PrimitiveComponent.h"

#include "GameFramework/Actor.h"
#include "Math/JungleMath.h"

UPrimitiveComponent::UPrimitiveComponent()
{
}

UPrimitiveComponent::~UPrimitiveComponent()
{
}

void UPrimitiveComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UPrimitiveComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}

int UPrimitiveComponent::CheckRayIntersection(const FVector& rayOrigin,const FVector& rayDirection, float& pfNearHitDistance)
{
    if (!WorldAABB.Intersect(rayOrigin, rayDirection, pfNearHitDistance))
        return 0;

    // 기본 프리미티브는 AABB만 처리. 메시가 없다면 삼각형 검사 안 함.
    return 1;
}



bool UPrimitiveComponent::IntersectRayTriangle(const FVector& rayOrigin, const FVector& rayDirection, const FVector& v0, const FVector& v1, const FVector& v2, float& hitDistance)
{
    const float epsilon = 1e-6f;
    FVector edge1 = v1 - v0;
    const FVector edge2 = v2 - v0;
    FVector FrayDirection = rayDirection;
    FVector h = FrayDirection.Cross(edge2);
    float a = edge1.Dot(h);

    if (fabs(a) < epsilon)
        return false; // Ray와 삼각형이 평행한 경우

    float f = 1.0f / a;
    FVector s = rayOrigin - v0;
    float u = f * s.Dot(h);
    if (u < 0.0f || u > 1.0f)
        return false;

    FVector q = s.Cross(edge1);
    float v = f * FrayDirection.Dot(q);
    if (v < 0.0f || (u + v) > 1.0f)
        return false;

    float t = f * edge2.Dot(q);
    if (t > epsilon) {

        hitDistance = t;
        return true;
    }

    return false;
}
/*
bool UPrimitiveComponent::IntersectRaySphere(
    const FVector& rayOrigin,
    const FVector& rayDirection,
    float& hitDistance)
{
    // AABB로부터 중심점과 반지름 계산
    FVector Center = WorldAABB.GetCenter();
    FVector Extents = WorldAABB.GetExtents(); // (max - min) * 0.5
    float Radius = Extents.Magnitude(); // AABB를 감싸는 구의 반지름

    FVector m = rayOrigin - Center;
    float b = m.Dot(rayDirection);
    float c = m.Dot(m) - Radius * Radius;

    // 판별식이 음수면 교차 없음
    float discr = b * b - c;
    if (discr < 0.0f)
        return false;

    float sqrtDiscr = sqrtf(discr);
    float t = -b - sqrtDiscr;

    // 교차점이 광선의 앞에 있는지 확인
    if (t < 0.0f)
        t = -b + sqrtDiscr;
    if (t < 0.0f)
        return false;

    hitDistance = t;
    return true;
}
*/
void UPrimitiveComponent::UpdateWorldAABB()
{
    FMatrix ModelMatrix = GetOwner()->GetModelMatrix();
    WorldAABB = JungleMath::TransformAABB(AABB, ModelMatrix);
    BoundingSphere = WorldAABB.GetBoundingSphere(false);
}
