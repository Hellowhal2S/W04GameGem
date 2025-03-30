#include "EngineLoop.h"
#include "ImGuiManager.h"
#include "World.h"
#include "Camera/CameraComponent.h"
#include "PropertyEditor/ViewportTypePanel.h"
#include "UnrealEd/EditorViewportClient.h"
#include "UnrealEd/UnrealEd.h"
#include "UnrealClient.h"
#include "Engine/Octree/Octree.h"
#include "slate/Widgets/Layout/SSplitter.h"
#include "LevelEditor/SLevelEditor.h"
#include "Profiling/PlatformTime.h"
#include "Profiling/StatRegistry.h"
#include "Core/Math/JungleMath.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
    {
        return true;
    }
    int zDelta = 0;
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            //UGraphicsDevice 객체의 OnResize 함수 호출
            if (FEngineLoop::graphicDevice.SwapChain)
            {
                FEngineLoop::graphicDevice.OnResize(hWnd);
            }
            for (int i = 0; i < 4; i++)
            {
                if (GEngineLoop.GetLevelEditor())
                {
                    if (GEngineLoop.GetLevelEditor()->GetViewports()[i])
                    {
                        GEngineLoop.GetLevelEditor()->GetViewports()[i]->ResizeViewport(FEngineLoop::graphicDevice.SwapchainDesc);
                    }
                }
            }
        }
     Console::GetInstance().OnResize(hWnd);
    // ControlPanel::GetInstance().OnResize(hWnd);
    // PropertyPanel::GetInstance().OnResize(hWnd);
    // Outliner::GetInstance().OnResize(hWnd);
    // ViewModeDropdown::GetInstance().OnResize(hWnd);
    // ShowFlags::GetInstance().OnResize(hWnd);
        if (GEngineLoop.GetUnrealEditor())
        {
            GEngineLoop.GetUnrealEditor()->OnResize(hWnd);
        }
        ViewportTypePanel::GetInstance().OnResize(hWnd);
        break;
    case WM_MOUSEWHEEL:
        if (ImGui::GetIO().WantCaptureMouse)
            return 0;
        zDelta = GET_WHEEL_DELTA_WPARAM(wParam); // 휠 회전 값 (+120 / -120)
        if (GEngineLoop.GetLevelEditor())
        {
            if (GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->IsPerspective())
            {
                if (GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->GetIsOnRBMouseClick())
                {
                    GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->SetCameraSpeedScalar(
                        static_cast<float>(GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->GetCameraSpeedScalar() + zDelta * 0.01)
                    );
                }
                else
                {
                    GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->CameraMoveForward(zDelta * 0.1f);
                }
            }
            else
            {
                FEditorViewportClient::SetOthoSize(-zDelta * 0.01f);
            }
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

FGraphicsDevice FEngineLoop::graphicDevice;
FRenderer FEngineLoop::renderer;
FResourceMgr FEngineLoop::resourceMgr;
uint32 FEngineLoop::TotalAllocationBytes = 0;
uint32 FEngineLoop::TotalAllocationCount = 0;

FEngineLoop::FEngineLoop()
    : hWnd(nullptr)
    , UIMgr(nullptr)
    , GWorld(nullptr)
    , LevelEditor(nullptr)
    , UnrealEditor(nullptr)
{
}

int32 FEngineLoop::PreInit()
{
    return 0;
}

int32 FEngineLoop::Init(HINSTANCE hInstance)
{
    /* must be initialized before window. */
    UnrealEditor = new UnrealEd();
    UnrealEditor->Initialize();

    WindowInit(hInstance);
    graphicDevice.Initialize(hWnd);
    renderer.Initialize(&graphicDevice);

    UIMgr = new UImGuiManager;
    UIMgr->Initialize(hWnd, graphicDevice.Device, graphicDevice.DeviceContext);

    resourceMgr.Initialize(&renderer, &graphicDevice);
    LevelEditor = new SLevelEditor();
    LevelEditor->Initialize();

    GWorld = new UWorld;
    GWorld->Initialize();

    return 0;
}


void FEngineLoop::Render()
{
    graphicDevice.Prepare();
    renderer.PrepareRender();
    renderer.Render(GetWorld(),LevelEditor->GetActiveViewportClient());
    // if (LevelEditor->IsMultiViewport())
    // {
    //     std::shared_ptr<FEditorViewportClient> viewportClient = GetLevelEditor()->GetActiveViewportClient();
    //     for (int i = 0; i < 4; ++i)
    //     {
    //         LevelEditor->SetViewportClient(i);
    //         // graphicDevice.DeviceContext->RSSetViewports(1, &LevelEditor->GetViewports()[i]->GetD3DViewport());
    //         // graphicDevice.ChangeRasterizer(LevelEditor->GetActiveViewportClient()->GetViewMode());
    //         // renderer.ChangeViewMode(LevelEditor->GetActiveViewportClient()->GetViewMode());
    //         // renderer.PrepareShader();
    //         // renderer.UpdateLightBuffer();
    //         // RenderWorld();
    //         renderer.PrepareRender();
    //         renderer.Render(GetWorld(),LevelEditor->GetActiveViewportClient());
    //     }
    //     GetLevelEditor()->SetViewportClient(viewportClient);
    // }
    // else
    // {
        // graphicDevice.DeviceContext->RSSetViewports(1, &LevelEditor->GetActiveViewportClient()->GetD3DViewport());
        // graphicDevice.ChangeRasterizer(LevelEditor->GetActiveViewportClient()->GetViewMode());
        // renderer.ChangeViewMode(LevelEditor->GetActiveViewportClient()->GetViewMode());
        // renderer.PrepareShader();
        // renderer.UpdateLightBuffer();
        // // RenderWorld();
        // renderer.PrepareRender();
        // renderer.Render(GetWorld(),LevelEditor->GetActiveViewportClient());
    // }
}

void FEngineLoop::Tick()
{
    LARGE_INTEGER frequency;
    const double targetFrameTime = 1000.0 / targetFPS; // 한 프레임의 목표 시간 (밀리초 단위)

    QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER startTime, endTime;
    double elapsedTime = 1.0;

    FStatRegistry::SetMainFrameStat("MainFrame"); // 앱 시작 시 1회만 호출
    LevelEditor->OffMultiViewport();
    while (bIsExit == false)
    {
		FScopeCycleCounter Timer("MainFrame");
        QueryPerformanceCounter(&startTime);
        ADVANCE_FRAME();
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg); // 키보드 입력 메시지를 문자메시지로 변경
            DispatchMessage(&msg);  // 메시지를 WndProc에 전달

            if (msg.message == WM_QUIT)
            {
                bIsExit = true;
                break;
            }
        }

        // Input();
        FScopeCycleCounter Timer1("Tick");
        GWorld->Tick(elapsedTime);
        LevelEditor->Tick(elapsedTime);
        FStatRegistry::RegisterResult(Timer1);
        Render();
        
        FScopeCycleCounter Timer2("UI,Editor");
        UIMgr->BeginFrame();
        UnrealEditor->Render();

        //Console::GetInstance().Draw();

        UIMgr->EndFrame();
        FStatRegistry::RegisterResult(Timer2);
        FScopeCycleCounter Timer3("DestroyObjects");
        // Pending 처리된 오브젝트 제거
        GUObjectArray.ProcessPendingDestroyObjects();
        FStatRegistry::RegisterResult(Timer3);

        FScopeCycleCounter Timer4("SwapBuffer");
        graphicDevice.SwapBuffer();
        FStatRegistry::RegisterResult(Timer4);
        do
        {
            Sleep(0);
            QueryPerformanceCounter(&endTime);
            elapsedTime = (endTime.QuadPart - startTime.QuadPart) * 1000.0 / frequency.QuadPart;
        }
        while (elapsedTime < targetFrameTime);

		FStatRegistry::RegisterResult(Timer);

        //FScopeCycleCounter Timer5("SIMD");
        //FVector vec3 = FVector(2, 3, 4);
        //FVector4 vec4 = FVector4(3, 4, 5, 1);
        //FMatrix A = JungleMath::CreateModelMatrix(
        //    FVector(1.0f, 2.0f, 3.0f),
        //    FVector(45.0f, 30.0f, 90.0f),
        //    FVector(1.0f, 1.0f, 1.0f)
        //);
        //FMatrix B = JungleMath::CreateModelMatrix(
        //    FVector(0.0f, 5.0f, -2.0f),
        //    FVector(60.0f, 0.0f, 45.0f),
        //    FVector(2.0f, 2.0f, 2.0f)
        //);
        //FMatrix resultm = FMatrix::Identity;
        //FVector result3 = vec3;
        //FVector4 result4 = vec4;
        //
        //for (size_t i = 0; i < 1'000'000; i++)
        //{
        //    A = A + FMatrix::Identity * i;
        //    //result = result * (A * B);
        //    //result = B * 4.f;
        //    //B = FMatrix::Transpose(B);
        //    //result3 = FMatrix::TransformVector(vec3, A);
        //    //result4 = FMatrix::TransformVector(vec4, A);
        //    A.TransformPosition(vec3);
        //}
        //FStatRegistry::RegisterResult(Timer5);

        //UE_LOG(LogLevel::Error, "Dummy: %f", A);
        //UE_LOG(LogLevel::Error, "Dummy: %f", resultm);
        //UE_LOG(LogLevel::Error, "Dummy: %f", result3);
        //UE_LOG(LogLevel::Error, "Dummy: %f", result4);
    }
}

float FEngineLoop::GetAspectRatio(IDXGISwapChain* swapChain) const
{
    DXGI_SWAP_CHAIN_DESC desc;
    swapChain->GetDesc(&desc);
    return static_cast<float>(desc.BufferDesc.Width) / static_cast<float>(desc.BufferDesc.Height);
}

void FEngineLoop::Input()
{
    if (GetAsyncKeyState('M') & 0x8000)
    {
        if (!bTestInput)
        {
            bTestInput = true;
            if (LevelEditor->IsMultiViewport())
            {
                LevelEditor->OffMultiViewport();
            }
            else
                LevelEditor->OnMultiViewport();
        }
    }
    else
    {
        bTestInput = false;
    }
}

void FEngineLoop::Exit()
{
    LevelEditor->Release();
    GWorld->Release();
    delete GWorld;
    UIMgr->Shutdown();
    delete UIMgr;
    resourceMgr.Release(&renderer);
    renderer.Release();
    graphicDevice.Release();
}


void FEngineLoop::WindowInit(HINSTANCE hInstance)
{
    WCHAR WindowClass[] = L"JungleWindowClass";

    WCHAR Title[] = L"Game Tech Lab";

    WNDCLASSW wndclass = {0};
    wndclass.lpfnWndProc = WndProc;
    wndclass.hInstance = hInstance;
    wndclass.lpszClassName = WindowClass;

    RegisterClassW(&wndclass);

    hWnd = CreateWindowExW(
        0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 1000,
        nullptr, nullptr, hInstance, nullptr
    );
}
