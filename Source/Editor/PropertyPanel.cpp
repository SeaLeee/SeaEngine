#include "Editor/PropertyPanel.h"
#include "RenderGraph/RenderPass.h"
#include <imgui.h>

namespace Sea
{
    PropertyPanel::PropertyPanel(RenderGraph& graph) : m_Graph(graph) {}

    void PropertyPanel::Render()
    {
        ImGui::Begin("Properties");

        if (m_SelectedPass != UINT32_MAX && m_SelectedPass < m_Graph.GetPasses().size())
        {
            RenderPassProperties(m_Graph.GetPasses()[m_SelectedPass]);
        }
        else if (m_SelectedResource != UINT32_MAX && m_SelectedResource < m_Graph.GetResources().size())
        {
            RenderResourceProperties(m_Graph.GetResources()[m_SelectedResource]);
        }
        else
        {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Select a node to view properties");
        }

        ImGui::End();
    }

    void PropertyPanel::RenderPassProperties(RenderPassDesc& pass)
    {
        ImGui::Text("Pass: %s", pass.name.c_str());
        ImGui::Separator();

        char nameBuf[256];
        strcpy_s(nameBuf, pass.name.c_str());
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
            pass.name = nameBuf;

        const char* typeNames[] = { "Graphics", "Compute", "Copy" };
        int typeIdx = static_cast<int>(pass.type);
        if (ImGui::Combo("Type", &typeIdx, typeNames, 3))
            pass.type = static_cast<PassType>(typeIdx);

        ImGui::Separator();
        ImGui::Text("Inputs: %zu", pass.inputs.size());
        ImGui::Text("Outputs: %zu", pass.outputs.size());
    }

    void PropertyPanel::RenderResourceProperties(RGResourceDesc& res)
    {
        ImGui::Text("Resource: %s", res.name.c_str());
        ImGui::Separator();

        char nameBuf[256];
        strcpy_s(nameBuf, res.name.c_str());
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
            res.name = nameBuf;

        int dims[2] = { static_cast<int>(res.width), static_cast<int>(res.height) };
        if (ImGui::InputInt2("Size", dims))
        {
            res.width = static_cast<u32>(dims[0]);
            res.height = static_cast<u32>(dims[1]);
        }

        const char* formatNames[] = { "R8G8B8A8_UNORM", "R16G16B16A16_FLOAT", "R32G32B32A32_FLOAT", "D32_FLOAT" };
        int formatIdx = 0;
        ImGui::Combo("Format", &formatIdx, formatNames, 4);
    }
}
