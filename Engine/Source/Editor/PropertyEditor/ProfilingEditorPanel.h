#pragma once
#include "GameFramework/Actor.h"
#include "UnrealEd/EditorPanel.h"

class ProfilingEditorPanel : public UEditorPanel
{
public:
    virtual void Render() override;
    virtual void OnResize(HWND hWnd) override;
};
