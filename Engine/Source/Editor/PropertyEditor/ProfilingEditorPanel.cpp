#include "ProfilingEditorPanel.h"

#include "World.h"
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
        const auto& StatMap = FStatRegistry::GetStatMap();
        static TStatId Stat_Frame("MainFrame");
        auto Stats = FStatRegistry::GetFPSStats(Stat_Frame);
        float fps = static_cast<float>(FStatRegistry::GetFPS(Stat_Frame));
        float ms = static_cast<float>(FStatRegistry::GetLastMilliseconds(Stat_Frame));
        ImGui::Text("FPS: %.2f (%.3f)ms", fps,ms);
        ImGui::Text("Picking Time %.4fms\nNum Attempts: %d\nAccumulated Time %.2fms",FStatRegistry::GetLastMilliseconds("Picking"),FStatRegistry::TotalPickCount,FStatRegistry::TotalPickTime);
        ImGui::Text("FPS (1s): %.2f", Stats.FPS_1Sec);
        ImGui::Text("FPS (5s): %.2f", Stats.FPS_5Sec);

        float& firstLOD = GEngineLoop.firstLOD;
        float& secondLOD = GEngineLoop.SecondLOD;
        
        ImGui::SliderFloat("First LOD", &firstLOD, 1.0f, 100.0f, "%.1f");
        ImGui::SliderFloat("Second LOD", &secondLOD, 1.0f, 100.0f, "%.1f");
        //ImGui::SliderInt("VertexBuffer Depth Min", &GRenderDepthMin, 0, 5);
        //ImGui::SliderInt("VertexBuffer Depth Max", &GRenderDepthMax, 0, 5);
        ImGui::SliderInt("Depth Min", &GRenderDepthMin, 0, 5);
        ImGui::SliderInt("Depth Max", &GRenderDepthMax, 0, 5);
        
        //ImGui::SliderInt("KDTreeDepth", &GEngineLoop.GetWorld()->SceneOctree->MaxDepthKD, 0, 5);
        //if (ImGui::Checkbox("UseKD",&GEngineLoop.GetWorld()->SceneOctree->bUseKD));
        //if (ImGui::Checkbox("Material Sorting",&FEngineLoop::renderer.bMaterialSort));
        if (ImGui::Checkbox("Debug OctreeAABB",&FEngineLoop::renderer.bDebugOctreeAABB));
        if (ImGui::Checkbox("Occlusion Culling", &FEngineLoop::renderer.bOcclusionCulling));
        if (ImGui::Button("Clear Cache"))
        {
            GEngineLoop.GetWorld()->SceneOctree->GetRoot()->ClearBatchDatas();
        }
        if (ImGui::Button("Clear Buffer"))
        {
            GEngineLoop.GetWorld()->SceneOctree->GetRoot()->TickBuffers(GCurrentFrame, 0);
        }

        // 드롭다운으로 StatMap 표시
        if (ImGui::CollapsingHeader("Stat Timings (ms)", ImGuiTreeNodeFlags_DefaultOpen))
        {
             // 함수로 접근한다고 가정
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
