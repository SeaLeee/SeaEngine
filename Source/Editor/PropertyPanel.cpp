#include "Editor/PropertyPanel.h"
#include <imgui.h>

namespace Sea
{
    PropertyPanel::PropertyPanel(RenderGraph& graph) : m_Graph(graph) {}

    void PropertyPanel::Render()
    {
        ImGui::Begin("Properties");

        if (m_SelectedPass != UINT32_MAX)
        {
            PassNode* pass = m_Graph.GetPass(m_SelectedPass);
            if (pass)
            {
                RenderPassProperties(*pass);
            }
        }
        else if (m_SelectedResource != UINT32_MAX)
        {
            ResourceNode* res = m_Graph.GetResource(m_SelectedResource);
            if (res)
            {
                RenderResourceProperties(*res);
            }
        }
        else
        {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Select a node to view properties");
        }

        ImGui::End();
    }

    void PropertyPanel::RenderPassProperties(PassNode& pass)
    {
        ImGui::Text("Pass: %s", pass.GetName().c_str());
        ImGui::Separator();

        static char nameBuf[256];
        strncpy_s(nameBuf, pass.GetName().c_str(), sizeof(nameBuf) - 1);
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
            pass.SetName(nameBuf);

        const char* typeNames[] = { "Graphics", "Compute", "Copy", "AsyncCompute" };
        int typeIdx = static_cast<int>(pass.GetType());
        if (ImGui::Combo("Type", &typeIdx, typeNames, 4))
            pass.SetType(static_cast<PassType>(typeIdx));

        bool enabled = pass.IsEnabled();
        if (ImGui::Checkbox("Enabled", &enabled))
            pass.SetEnabled(enabled);

        ImGui::Separator();
        ImGui::Text("Inputs: %zu", pass.GetInputs().size());
        for (size_t i = 0; i < pass.GetInputs().size(); ++i)
        {
            const auto& input = pass.GetInputs()[i];
            ImGui::BulletText("[%zu] %s -> Resource %u", i, input.name.c_str(), input.resourceId);
        }

        ImGui::Text("Outputs: %zu", pass.GetOutputs().size());
        for (size_t i = 0; i < pass.GetOutputs().size(); ++i)
        {
            const auto& output = pass.GetOutputs()[i];
            ImGui::BulletText("[%zu] %s -> Resource %u", i, output.name.c_str(), output.resourceId);
        }
    }

    void PropertyPanel::RenderResourceProperties(ResourceNode& res)
    {
        ImGui::Text("Resource: %s", res.GetName().c_str());
        ImGui::Separator();

        static char nameBuf[256];
        strncpy_s(nameBuf, res.GetName().c_str(), sizeof(nameBuf) - 1);
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
            res.SetName(nameBuf);

        const char* typeNames[] = { "Texture2D", "Texture3D", "TextureCube", "Buffer", "DepthStencil" };
        int typeIdx = static_cast<int>(res.GetType());
        if (ImGui::Combo("Type", &typeIdx, typeNames, 5))
            res.SetType(static_cast<ResourceNodeType>(typeIdx));

        int dims[2] = { static_cast<int>(res.GetWidth()), static_cast<int>(res.GetHeight()) };
        if (ImGui::InputInt2("Size", dims))
        {
            res.SetDimensions(static_cast<u32>(dims[0]), static_cast<u32>(dims[1]));
        }

        const char* formatNames[] = { "R8G8B8A8_UNORM", "R16G16B16A16_FLOAT", "R32G32B32A32_FLOAT", "D32_FLOAT" };
        int formatIdx = 0;
        switch (res.GetFormat())
        {
            case Format::R8G8B8A8_UNORM: formatIdx = 0; break;
            case Format::R16G16B16A16_FLOAT: formatIdx = 1; break;
            case Format::R32G32B32A32_FLOAT: formatIdx = 2; break;
            case Format::D32_FLOAT: formatIdx = 3; break;
            default: break;
        }
        if (ImGui::Combo("Format", &formatIdx, formatNames, 4))
        {
            Format formats[] = { Format::R8G8B8A8_UNORM, Format::R16G16B16A16_FLOAT, 
                                Format::R32G32B32A32_FLOAT, Format::D32_FLOAT };
            res.SetFormat(formats[formatIdx]);
        }

        bool external = res.IsExternal();
        ImGui::Checkbox("External", &external);
        // External is typically read-only

        ImGui::Separator();
        ImGui::Text("Lifetime: Pass %u to %u", res.GetFirstUsePass(), res.GetLastUsePass());
    }
}
