#include "ProfilingEditorPanel.h"

#include "Engine/Octree/Octree.h"
#include "Container/String.h"
#include "ImGUI/imgui.h"
#include "Profiling/PlatformTime.h"
#include "Profiling/StatRegistry.h"
#include "UObject/NameTypes.h"

void ProfilingEditorPanel::Render()
{
    ImGui::SetNextWindowPos(ImVec2(10, 50), ImGuiCond_Always);
    //ImGui::SetNextWindowBgAlpha(0.35f); // 반투명 배경
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1)); // 완전한 검정 배경
    if (ImGui::Begin("Performance", nullptr,
                     ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus
                     ))
    {   
        // FPS 표시
        static TStatId Stat_Frame("MainFrame");
        float fps = static_cast<float>(FStatRegistry::GetFPS(Stat_Frame));
        ImGui::Text("FPS: %.2f", fps);
        auto Stats = FStatRegistry::GetFPSStats(Stat_Frame);
        ImGui::Text("FPS (1s): %.2f", Stats.FPS_1Sec);
        ImGui::Text("FPS (5s): %.2f", Stats.FPS_5Sec);
        ImGui::SliderInt("VertexBuffer Depth", &GVertexBufferCutoffDepth, 0, 5);

        // 드롭다운으로 StatMap 표시
        if (ImGui::CollapsingHeader("Stat Timings (ms)", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto& StatMap = FStatRegistry::GetStatMap(); // 함수로 접근한다고 가정
            for (const auto& Pair : StatMap)
            {
                const uint32 StatKey = Pair.Key;
                double Ms = Pair.Value;

                // DisplayIndex로 FName을 재생성 → 이름 얻기
                FName StatName(StatKey); // FName(uint32 DisplayIndex) 생성자 있어야 함
                FString NameString = StatName.ToString();

                ImGui::Text("%s: %.3f ms", GetData(NameString), Ms);
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}
void ProfilingEditorPanel::OnResize(HWND hWnd)
{
    /*
    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    Width = clientRect.right - clientRect.left;
    Height = clientRect.bottom - clientRect.top;*/
}
