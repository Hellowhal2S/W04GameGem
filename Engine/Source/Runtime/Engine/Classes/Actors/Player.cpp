﻿#include "Player.h"

#include "UnrealClient.h"
#include "World.h"
#include "BaseGizmos/GizmoArrowComponent.h"
#include "BaseGizmos/GizmoCircleComponent.h"
#include "BaseGizmos/GizmoRectangleComponent.h"
#include "BaseGizmos/TransformGizmo.h"
#include "Camera/CameraComponent.h"
#include "Components/LightComponent.h"
#include "KDTree/KDTree.h"
#include "KDTree/KDTreeSystem.h"
#include "LevelEditor/SLevelEditor.h"
#include "Math/JungleMath.h"
#include "Math/MathUtility.h"
#include "Math/Ray.h"
#include "Octree/Octree.h"
#include "Profiling/PlatformTime.h"
#include "Profiling/StatRegistry.h"
#include "PropertyEditor/ShowFlags.h"
#include "UnrealEd/EditorViewportClient.h"
#include "UObject/UObjectIterator.h"


using namespace DirectX;

AEditorPlayer::AEditorPlayer()
{
}

void AEditorPlayer::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    Input();
}

void AEditorPlayer::Input()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
    {
        if (!bLeftMouseDown)
        {
            bLeftMouseDown = true;
            HandleLeftClick();
            /*
            POINT mousePos;
            GetCursorPos(&mousePos);
            GetCursorPos(&m_LastMousePos);

            uint32 UUID = GetEngine().graphicDevice.GetPixelUUID(mousePos);
            // TArray<UObject*> objectArr = GetWorld()->GetObjectArr();
            for ( const auto obj : TObjectRange<USceneComponent>())
            {
                if (obj->GetUUID() != UUID) continue;

                UE_LOG(LogLevel::Display, *obj->GetName());
            }
            ScreenToClient(GetEngine().hWnd, &mousePos);

            FVector pickPosition;

            const auto& ActiveViewport = GetEngine().GetLevelEditor()->GetActiveViewportClient();
            ScreenToViewSpace(mousePos.x, mousePos.y, ActiveViewport->GetViewMatrix(), ActiveViewport->GetProjectionMatrix(), pickPosition);
            // bool res = PickGizmo(pickPosition);
            bool res =true;
            if (!res) PickActor(pickPosition);
            */
        }
        else
        {
            PickedObjControl();
        }
    }
    else
    {
        if (bLeftMouseDown)
        {
            bLeftMouseDown = false; // ���콺 ������ ��ư�� ���� ���� �ʱ�ȭ
            GetWorld()->SetPickingGizmo(nullptr);
        }
    }
    if (GetAsyncKeyState(VK_SPACE) & 0x8000)
    {
        if (!bSpaceDown)
        {
            AddControlMode();
            bSpaceDown = true;
        }
    }
    else
    {
        if (bSpaceDown)
        {
            bSpaceDown = false;
        }
    }
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000)
    {
        if (!bRightMouseDown)
        {
            bRightMouseDown = true;
        }
    }
    else
    {
        bRightMouseDown = false;

        if (GetAsyncKeyState('Q') & 0x8000)
        {
            //GetWorld()->SetPickingObj(nullptr);
        }
        if (GetAsyncKeyState('W') & 0x8000)
        {
            cMode = CM_TRANSLATION;
        }
        if (GetAsyncKeyState('E') & 0x8000)
        {
            cMode = CM_ROTATION;
        }
        if (GetAsyncKeyState('R') & 0x8000)
        {
            cMode = CM_SCALE;
        }
    }

    if (GetAsyncKeyState(VK_DELETE) & 0x8000)
    {
        UWorld* World = GetWorld();
        if (AActor* PickedActor = World->GetSelectedActor())
        {
            World->DestroyActor(PickedActor);
            World->SetPickedActor(nullptr);
        }
    }
}

void AEditorPlayer::HandleLeftClick()
{
    POINT mousePos;
    GetCursorPos(&mousePos);
    GetCursorPos(&m_LastMousePos);
    /*
        // Pick UUID (if any)
        uint32 UUID = GetEngine().graphicDevice.GetPixelUUID(mousePos);
    
        // 현재 클릭한 객체 찾기
        UObject* PickedObj = nullptr;
        for (const auto obj : TObjectRange<USceneComponent>())
        {
            if (obj->GetUUID() == UUID)
            {
                PickedObj = obj;
                UE_LOG(LogLevel::Display, *obj->GetName());
                break;
            }
        }
    */
    // 월드 픽킹 위치 계산
    //ScreenToClient(GetEngine().hWnd, &mousePos);

    //const auto& ViewportClient = GetEngine().GetLevelEditor()->GetActiveViewportClient();
    //ScreenToViewSpace(mousePos.x, mousePos.y, ViewportClient->GetViewMatrix(), ViewportClient->GetProjectionMatrix(), pickPosition);

    // Picking 처리 (RayCast)
    TryPickActor();
}

void AEditorPlayer::TryPickActor()
{
    std::shared_ptr<FEditorViewportClient> ViewportClient = GetEngine().GetLevelEditor()->GetActiveViewportClient();
    auto viewport = ViewportClient->GetViewport()->GetViewport();
    const FRect ViewRect(viewport.TopLeftX, viewport.TopLeftY, viewport.Width, viewport.Height); // 뷰포트의 위치와 크기

    FRay Ray;
    Ray = FRay::GetRayFromViewport(ViewportClient, ViewRect); // ← 새로운 방식의 레이 생성

    FScopeCycleCounter pickCounter("Picking");
    ++FStatRegistry::TotalPickCount;

    float ClosestDistance = FLT_MAX;
    UPrimitiveComponent* Closest = GetEngine().GetWorld()->SceneOctree->Raycast(Ray, ClosestDistance);
    //UPrimitiveComponent* Closest = GetEngine().GetWorld()->SceneKDTreeSystem->Raycast(Ray, ClosestDistance);

    if (Closest)
    {
        //UE_LOG(LogLevel::Display, TEXT("[Pick] Closest: %s"), *Closest->GetName());
        GetWorld()->SetPickedActor(Closest->GetOwner());
    }
    FStatRegistry::TotalPickTime += FStatRegistry::RegisterResult(pickCounter);
    GetWorld()->SetHighlightedComponent(dynamic_cast<UStaticMeshComponent*>(Closest));
}


bool AEditorPlayer::PickGizmo(FVector& pickPosition)
{
    bool isPickedGizmo = false;
    if (GetWorld()->GetSelectedActor())
    {
        if (cMode == CM_TRANSLATION)
        {
            for (auto iter : GetWorld()->LocalGizmo->GetArrowArr())
            {
                int maxIntersect = 0;
                float minDistance = FLT_MAX;
                float Distance = 0.0f;
                int currentIntersectCount = 0;
                if (!iter) continue;
                if (RayIntersectsObject(pickPosition, iter, Distance, currentIntersectCount))
                {
                    if (Distance < minDistance)
                    {
                        minDistance = Distance;
                        maxIntersect = currentIntersectCount;
                        GetWorld()->SetPickingGizmo(iter);
                        isPickedGizmo = true;
                    }
                    else if (abs(Distance - minDistance) < FLT_EPSILON && currentIntersectCount > maxIntersect)
                    {
                        maxIntersect = currentIntersectCount;
                        GetWorld()->SetPickingGizmo(iter);
                        isPickedGizmo = true;
                    }
                }
            }
        }
        else if (cMode == CM_ROTATION)
        {
            for (auto iter : GetWorld()->LocalGizmo->GetDiscArr())
            {
                int maxIntersect = 0;
                float minDistance = FLT_MAX;
                float Distance = 0.0f;
                int currentIntersectCount = 0;
                //UPrimitiveComponent* localGizmo = dynamic_cast<UPrimitiveComponent*>(GetWorld()->LocalGizmo[i]);
                if (!iter) continue;
                if (RayIntersectsObject(pickPosition, iter, Distance, currentIntersectCount))
                {
                    if (Distance < minDistance)
                    {
                        minDistance = Distance;
                        maxIntersect = currentIntersectCount;
                        GetWorld()->SetPickingGizmo(iter);
                        isPickedGizmo = true;
                    }
                    else if (abs(Distance - minDistance) < FLT_EPSILON && currentIntersectCount > maxIntersect)
                    {
                        maxIntersect = currentIntersectCount;
                        GetWorld()->SetPickingGizmo(iter);
                        isPickedGizmo = true;
                    }
                }
            }
        }
        else if (cMode == CM_SCALE)
        {
            for (auto iter : GetWorld()->LocalGizmo->GetScaleArr())
            {
                int maxIntersect = 0;
                float minDistance = FLT_MAX;
                float Distance = 0.0f;
                int currentIntersectCount = 0;
                if (!iter) continue;
                if (RayIntersectsObject(pickPosition, iter, Distance, currentIntersectCount))
                {
                    if (Distance < minDistance)
                    {
                        minDistance = Distance;
                        maxIntersect = currentIntersectCount;
                        GetWorld()->SetPickingGizmo(iter);
                        isPickedGizmo = true;
                    }
                    else if (abs(Distance - minDistance) < FLT_EPSILON && currentIntersectCount > maxIntersect)
                    {
                        maxIntersect = currentIntersectCount;
                        GetWorld()->SetPickingGizmo(iter);
                        isPickedGizmo = true;
                    }
                }
            }
        }
    }
    return isPickedGizmo;
}

void AEditorPlayer::PickActor(const FVector& pickPosition)
{
    //불용 처리
    if (!(ShowFlags::GetInstance().currentFlags & EEngineShowFlags::SF_Primitives)) return;

    const UActorComponent* Possible = nullptr;
    int maxIntersect = 0;
    float minDistance = FLT_MAX;
    for (const auto iter : TObjectRange<UPrimitiveComponent>())
    {
        UPrimitiveComponent* pObj;
        if (iter->IsA<UPrimitiveComponent>() || iter->IsA<ULightComponentBase>())
        {
            pObj = static_cast<UPrimitiveComponent*>(iter);
        }
        else
        {
            continue;
        }

        if (pObj && !pObj->IsA<UGizmoBaseComponent>())
        {
            float Distance = 0.0f;
            int currentIntersectCount = 0;
            if (RayIntersectsObject(pickPosition, pObj, Distance, currentIntersectCount))
            {
                if (Distance < minDistance)
                {
                    minDistance = Distance;
                    maxIntersect = currentIntersectCount;
                    Possible = pObj;
                }
                else if (abs(Distance - minDistance) < FLT_EPSILON && currentIntersectCount > maxIntersect)
                {
                    maxIntersect = currentIntersectCount;
                    Possible = pObj;
                }
            }
        }
    }
    if (Possible)
    {
        GetWorld()->SetPickedActor(Possible->GetOwner());
    }
}

void AEditorPlayer::AddControlMode()
{
    cMode = static_cast<ControlMode>((cMode + 1) % CM_END);
}

void AEditorPlayer::AddCoordiMode()
{
    cdMode = static_cast<CoordiMode>((cdMode + 1) % CDM_END);
}

void AEditorPlayer::ScreenToViewSpace(int screenX, int screenY, const FMatrix& viewMatrix, const FMatrix& projectionMatrix, FVector& pickPosition)
{
    D3D11_VIEWPORT viewport = GetEngine().GetLevelEditor()->GetActiveViewportClient()->GetD3DViewport();

    float viewportX = screenX - viewport.TopLeftX;
    float viewportY = screenY - viewport.TopLeftY;

    pickPosition.x = ((2.0f * viewportX / viewport.Width) - 1) / projectionMatrix[0][0];
    pickPosition.y = -((2.0f * viewportY / viewport.Height) - 1) / projectionMatrix[1][1];
    if (GetEngine().GetLevelEditor()->GetActiveViewportClient()->IsOrtho())
    {
        pickPosition.z = 0.0f; // 오쏘 모드에서는 unproject 시 near plane 위치를 기준
    }
    else
    {
        pickPosition.z = 1.0f; // 퍼스펙티브 모드: near plane
    }
}

void AEditorPlayer::GetMousePositionClient(int& OutX, int& OutY)
{
    POINT mousePos;

    if (GetCursorPos(&mousePos))
    {
        ScreenToClient(GetEngine().hWnd, &mousePos);

        OutX = mousePos.x;
        OutY = mousePos.y;
    }
    else
    {
        OutX = OutY = -1; // 실패 시 기본값
    }
}

int AEditorPlayer::RayIntersectsObject(const FVector& pickPosition, USceneComponent* obj, float& hitDistance, int& intersectCount)
{
    FMatrix scaleMatrix = FMatrix::CreateScale(
        obj->GetWorldScale().x,
        obj->GetWorldScale().y,
        obj->GetWorldScale().z
    );
    FMatrix rotationMatrix = FMatrix::CreateRotation(
        obj->GetWorldRotation().x,
        obj->GetWorldRotation().y,
        obj->GetWorldRotation().z
    );

    FMatrix translationMatrix = FMatrix::CreateTranslationMatrix(obj->GetWorldLocation());

    // ���� ��ȯ ���
    FMatrix worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
    FMatrix viewMatrix = GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->GetViewMatrix();

    bool bIsOrtho = GetEngine().GetLevelEditor()->GetActiveViewportClient()->IsOrtho();


    if (bIsOrtho)
    {
        // 오쏘 모드: ScreenToViewSpace()에서 계산된 pickPosition이 클립/뷰 좌표라고 가정
        FMatrix inverseView = FMatrix::Inverse(viewMatrix);
        // pickPosition을 월드 좌표로 변환
        FVector worldPickPos = inverseView.TransformPosition(pickPosition);
        // 오쏘에서는 픽킹 원점은 unproject된 픽셀의 위치
        FVector rayOrigin = worldPickPos;
        // 레이 방향은 카메라의 정면 방향 (평행)
        FVector orthoRayDir = GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->ViewTransformOrthographic.GetForwardVector().Normalize();

        // 객체의 로컬 좌표계로 변환
        FMatrix localMatrix = FMatrix::Inverse(worldMatrix);
        FVector localRayOrigin = localMatrix.TransformPosition(rayOrigin);
        FVector localRayDir = (localMatrix.TransformPosition(rayOrigin + orthoRayDir) - localRayOrigin).Normalize();

        intersectCount = obj->CheckRayIntersection(localRayOrigin, localRayDir, hitDistance);
        return intersectCount;
    }
    else
    {
        FMatrix inverseMatrix = FMatrix::Inverse(worldMatrix * viewMatrix);
        FVector cameraOrigin = {0, 0, 0};
        FVector pickRayOrigin = inverseMatrix.TransformPosition(cameraOrigin);
        // 퍼스펙티브 모드의 기존 로직 사용
        FVector transformedPick = inverseMatrix.TransformPosition(pickPosition);
        FVector rayDirection = (transformedPick - pickRayOrigin).Normalize();

        intersectCount = obj->CheckRayIntersection(pickRayOrigin, rayDirection, hitDistance);
        return intersectCount;
    }
}

void AEditorPlayer::PickedObjControl()
{
    if (GetWorld()->GetSelectedActor() && GetWorld()->GetPickingGizmo())
    {
        POINT currentMousePos;
        GetCursorPos(&currentMousePos);
        int32 deltaX = currentMousePos.x - m_LastMousePos.x;
        int32 deltaY = currentMousePos.y - m_LastMousePos.y;

        // USceneComponent* pObj = GetWorld()->GetPickingObj();
        AActor* PickedActor = GetWorld()->GetSelectedActor();
        UGizmoBaseComponent* Gizmo = static_cast<UGizmoBaseComponent*>(GetWorld()->GetPickingGizmo());
        switch (cMode)
        {
        case CM_TRANSLATION:
            ControlTranslation(PickedActor->GetRootComponent(), Gizmo, deltaX, deltaY);
            break;
        case CM_SCALE:
            ControlScale(PickedActor->GetRootComponent(), Gizmo, deltaX, deltaY);

            break;
        case CM_ROTATION:
            ControlRotation(PickedActor->GetRootComponent(), Gizmo, deltaX, deltaY);
            break;
        default:
            break;
        }
        m_LastMousePos = currentMousePos;
    }
}

void AEditorPlayer::ControlRotation(USceneComponent* pObj, UGizmoBaseComponent* Gizmo, int32 deltaX, int32 deltaY)
{
    FVector cameraForward = GetWorld()->GetCamera()->GetForwardVector();
    FVector cameraRight = GetWorld()->GetCamera()->GetRightVector();
    FVector cameraUp = GetWorld()->GetCamera()->GetUpVector();

    FQuat currentRotation = pObj->GetQuat();

    FQuat rotationDelta;

    if (Gizmo->GetGizmoType() == UGizmoBaseComponent::CircleX)
    {
        float rotationAmount = (cameraUp.z >= 0 ? -1.0f : 1.0f) * deltaY * 0.01f;
        rotationAmount = rotationAmount + (cameraRight.x >= 0 ? 1.0f : -1.0f) * deltaX * 0.01f;

        rotationDelta = FQuat(FVector(1.0f, 0.0f, 0.0f), rotationAmount); // ���� X �� ���� ȸ��
    }
    else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::CircleY)
    {
        float rotationAmount = (cameraRight.x >= 0 ? 1.0f : -1.0f) * deltaX * 0.01f;
        rotationAmount = rotationAmount + (cameraUp.z >= 0 ? 1.0f : -1.0f) * deltaY * 0.01f;

        rotationDelta = FQuat(FVector(0.0f, 1.0f, 0.0f), rotationAmount); // ���� Y �� ���� ȸ��
    }
    else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::CircleZ)
    {
        float rotationAmount = (cameraForward.x <= 0 ? -1.0f : 1.0f) * deltaX * 0.01f;
        rotationDelta = FQuat(FVector(0.0f, 0.0f, 1.0f), rotationAmount); // ���� Z �� ���� ȸ��
    }
    if (cdMode == CDM_LOCAL)
    {
        pObj->SetRotation(currentRotation * rotationDelta);
    }
    else if (cdMode == CDM_WORLD)
    {
        pObj->SetRotation(rotationDelta * currentRotation);
    }
}

void AEditorPlayer::ControlTranslation(USceneComponent* pObj, UGizmoBaseComponent* Gizmo, int32 deltaX, int32 deltaY)
{
    float DeltaX = static_cast<float>(deltaX);
    float DeltaY = static_cast<float>(deltaY);
    auto ActiveViewport = GetEngine().GetLevelEditor()->GetActiveViewportClient();

    FVector CamearRight = ActiveViewport->GetViewportType() == LVT_Perspective
                              ? ActiveViewport->ViewTransformPerspective.GetRightVector()
                              : ActiveViewport->ViewTransformOrthographic.GetRightVector();
    FVector CameraUp = ActiveViewport->GetViewportType() == LVT_Perspective
                           ? ActiveViewport->ViewTransformPerspective.GetUpVector()
                           : ActiveViewport->ViewTransformOrthographic.GetUpVector();

    FVector WorldMoveDirection = (CamearRight * DeltaX + CameraUp * -DeltaY) * 0.1f;

    if (cdMode == CDM_LOCAL)
    {
        if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ArrowX)
        {
            float moveAmount = WorldMoveDirection.Dot(pObj->GetForwardVector());
            pObj->AddLocation(pObj->GetForwardVector() * moveAmount);
        }
        else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ArrowY)
        {
            float moveAmount = WorldMoveDirection.Dot(pObj->GetRightVector());
            pObj->AddLocation(pObj->GetRightVector() * moveAmount);
        }
        else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ArrowZ)
        {
            float moveAmount = WorldMoveDirection.Dot(pObj->GetUpVector());
            pObj->AddLocation(pObj->GetUpVector() * moveAmount);
        }
    }
    else if (cdMode == CDM_WORLD)
    {
        // 월드 좌표계에서 카메라 방향을 고려한 이동
        if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ArrowX)
        {
            // 카메라의 오른쪽 방향을 X축 이동에 사용
            FVector moveDir = CamearRight * DeltaX * 0.05f;
            pObj->AddLocation(FVector(moveDir.x, 0.0f, 0.0f));
        }
        else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ArrowY)
        {
            // 카메라의 오른쪽 방향을 Y축 이동에 사용
            FVector moveDir = CamearRight * DeltaX * 0.05f;
            pObj->AddLocation(FVector(0.0f, moveDir.y, 0.0f));
        }
        else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ArrowZ)
        {
            // 카메라의 위쪽 방향을 Z축 이동에 사용
            FVector moveDir = CameraUp * -DeltaY * 0.05f;
            pObj->AddLocation(FVector(0.0f, 0.0f, moveDir.z));
        }
    }
}

void AEditorPlayer::ControlScale(USceneComponent* pObj, UGizmoBaseComponent* Gizmo, int32 deltaX, int32 deltaY)
{
    float DeltaX = static_cast<float>(deltaX);
    float DeltaY = static_cast<float>(deltaY);
    auto ActiveViewport = GetEngine().GetLevelEditor()->GetActiveViewportClient();

    FVector CamearRight = ActiveViewport->GetViewportType() == LVT_Perspective
                              ? ActiveViewport->ViewTransformPerspective.GetRightVector()
                              : ActiveViewport->ViewTransformOrthographic.GetRightVector();
    FVector CameraUp = ActiveViewport->GetViewportType() == LVT_Perspective
                           ? ActiveViewport->ViewTransformPerspective.GetUpVector()
                           : ActiveViewport->ViewTransformOrthographic.GetUpVector();

    // 월드 좌표계에서 카메라 방향을 고려한 이동
    if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ScaleX)
    {
        // 카메라의 오른쪽 방향을 X축 이동에 사용
        FVector moveDir = CamearRight * DeltaX * 0.05f;
        pObj->AddScale(FVector(moveDir.x, 0.0f, 0.0f));
    }
    else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ScaleY)
    {
        // 카메라의 오른쪽 방향을 Y축 이동에 사용
        FVector moveDir = CamearRight * DeltaX * 0.05f;
        pObj->AddScale(FVector(0.0f, moveDir.y, 0.0f));
    }
    else if (Gizmo->GetGizmoType() == UGizmoBaseComponent::ScaleZ)
    {
        // 카메라의 위쪽 방향을 Z축 이동에 사용
        FVector moveDir = CameraUp * -DeltaY * 0.05f;
        pObj->AddScale(FVector(0.0f, 0.0f, moveDir.z));
    }
}
