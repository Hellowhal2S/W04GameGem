#include "Ray.h"
#include "Math/Ray.h"

#include "World.h"
#include "Actors/Player.h"
#include "Camera/CameraComponent.h"
#include "UnrealEd/EditorViewportClient.h"

void FRay::InitFromScreen(const FVector& ScreenSpace, std::shared_ptr<FEditorViewportClient> ActiveViewport, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix, bool bIsOrtho)
{
    if (bIsOrtho)
    {
        // 오쏘모드에서는 ScreenSpace가 이미 뷰 공간 좌표라고 가정
        FMatrix InverseView = FMatrix::Inverse(ViewMatrix);
        FVector WorldPos = InverseView.TransformPosition(ScreenSpace);

        Origin = WorldPos;
        Direction = ViewMatrix.GetColumn(2).Normalize(); // 카메라 앞 방향
    }
    else
    {
        FMatrix VP = ViewMatrix * ProjectionMatrix;
        FMatrix InverseVP = FMatrix::Inverse(VP);

        // 픽킹 위치 (NDC) 좌표로 해석
        FVector NDC = ScreenSpace;

        FVector NearPoint = FVector(NDC.x, NDC.y, 0.f);
        FVector FarPoint = FVector(NDC.x, NDC.y, 1.f);

        FVector WorldNear = InverseVP.TransformPosition(NearPoint);
        FVector WorldFar = InverseVP.TransformPosition(FarPoint);

        //Origin = WorldNear;
        Origin = ActiveViewport->ViewTransformPerspective.GetLocation();
        Direction = (WorldFar - WorldNear).Normalize();
    }
}
FRay FRay::GetRayFromViewport(std::shared_ptr<FEditorViewportClient> ViewportClient, const FRect& ViewRect)
{
    FRay OutRay;

    int mouseX, mouseY;
    GEngineLoop.GetWorld()->GetEditorPlayer()->GetMousePositionClient(mouseX, mouseY);
    float localX = mouseX - ViewRect.leftTopX;
    float localY = mouseY - ViewRect.leftTopY;

    if (localX < 0 || localY < 0 || localX > ViewRect.width || localY > ViewRect.height)
        return OutRay;

    float ndcX = 2.0f * localX / ViewRect.width - 1.0f;
    float ndcY = -2.0f * localY / ViewRect.height + 1.0f;

    FVector ndcNear(ndcX, ndcY, 0.0f);
    FVector ndcFar (ndcX, ndcY, 1.0f);

    FMatrix invView = FMatrix::Inverse(ViewportClient->GetViewMatrix());
    FMatrix invProj = FMatrix::Inverse(ViewportClient->GetProjectionMatrix());

    FVector worldNear = invView.TransformPosition(invProj.TransformPosition(ndcNear));
    FVector worldFar  = invView.TransformPosition(invProj.TransformPosition(ndcFar));

    if (ViewportClient->IsOrtho())
    {
        OutRay.Origin = worldNear;
        OutRay.Direction = ViewportClient->GetCameraForward();
        //OutRay.Length = 10000.f;
    }
    else
    {
        OutRay.Origin = ViewportClient->GetCameraWorldPosition();
        OutRay.Direction = (worldFar - OutRay.Origin).Normalize();
        //OutRay.Length = (worldFar - OutRay.Origin).Length();
    }

    return OutRay;
}
