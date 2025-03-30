#pragma once
#include "Core/HAL/PlatformType.h"
#include "D3D11RHI/GraphicDevice.h"
#include "Renderer/Renderer.h"
#include "Engine/ResourceMgr.h"
#include "EngineBaseTypes.h"
class UnrealEd;
class UImGuiManager;
class UWorld;
class FEditorViewportClient;
class SSplitterV;
class SSplitterH;
class SLevelEditor;

class FEngineLoop
{
public:
    FEngineLoop();

    int32 PreInit();
    int32 Init(HINSTANCE hInstance);
    void Render();
    void Tick();
    void Exit();
    float GetAspectRatio(IDXGISwapChain* swapChain) const;
    void Input();

private:
    void WindowInit(HINSTANCE hInstance);

public:
    static FGraphicsDevice graphicDevice;
    static FRenderer renderer;
    static FResourceMgr resourceMgr;
    static uint32 TotalAllocationBytes;
    static uint32 TotalAllocationCount;


    HWND hWnd;

private:
    UImGuiManager* UIMgr;
    UWorld* GWorld;
    SLevelEditor* LevelEditor;
    UnrealEd* UnrealEditor;
    bool bIsExit = false;
    const int32 targetFPS = 10000;
    bool bTestInput = false;
    EObjQuality quality = EObjQuality::OQ_High;
    FString curMapName = "Default";
public:
    FString GetCurMapName(){return curMapName;}
    void SetCurMap(FString newMap) {curMapName = newMap;}
    EObjQuality GetQuality() {return quality;}
    void SetQuality(EObjQuality newQuality) { quality = newQuality;}
    UWorld* GetWorld() const { return GWorld; }
    SLevelEditor* GetLevelEditor() const { return LevelEditor; }
    UnrealEd* GetUnrealEditor() const { return UnrealEditor; }
};
