#include "Editor/Editor.h"
#include "Editor/NodeEditor.h"
#include "Core/Log.h"
#include <imgui.h>

namespace Sea
{
    Editor::Editor(Device& device, Window& window, RenderGraph& graph)
        : m_Device(device), m_Window(window), m_Graph(graph) {}
    
    Editor::~Editor() { Shutdown(); }

    bool Editor::Initialize()
    {
        SEA_CORE_INFO("Editor initialized");
        return true;
    }

    void Editor::Shutdown() {}

    void Editor::Update(f32 deltaTime)
    {
        // 主菜单栏
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New Graph")) {}
                if (ImGui::MenuItem("Open...")) {}
                if (ImGui::MenuItem("Save")) {}
                if (ImGui::MenuItem("Save As...")) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) {}
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "Ctrl+Z")) {}
                if (ImGui::MenuItem("Redo", "Ctrl+Y")) {}
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Node Editor", nullptr, &m_ShowDemo);
                ImGui::MenuItem("Properties", nullptr);
                ImGui::MenuItem("Shader Editor", nullptr);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help"))
            {
                ImGui::MenuItem("About SeaEngine");
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }

    void Editor::Render()
    {
        if (m_ShowDemo) ImGui::ShowDemoWindow(&m_ShowDemo);
    }

    void Editor::ShowNodeEditor(bool* open) {}
    void Editor::ShowPropertyPanel(bool* open) {}
    void Editor::ShowShaderEditor(bool* open) {}
}
