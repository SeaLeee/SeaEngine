#pragma once
#include "Core/Application.h"
#include "Graphics/Graphics.h"
#include "RenderGraph/RenderGraph.h"
#include "Shader/Shader.h"
#include "Editor/EditorModule.h"

namespace Sea
{
    class SampleApp : public Application
    {
    public:
        SampleApp();

    protected:
        bool OnInitialize() override;
        void OnShutdown() override;
        void OnUpdate(f32 deltaTime) override;
        void OnRender() override;
        void OnResize(u32 width, u32 height) override;

    private:
        void SetupRenderGraph();
        void CreateResources();

    private:
        Scope<Device> m_Device;
        Scope<SwapChain> m_SwapChain;
        Scope<CommandQueue> m_GraphicsQueue;
        Scope<CommandList> m_CommandList;
        Scope<ImGuiRenderer> m_ImGuiRenderer;
        Scope<RenderGraph> m_RenderGraph;
        Scope<NodeEditor> m_NodeEditor;
        Scope<PropertyPanel> m_PropertyPanel;
        Scope<ShaderEditor> m_ShaderEditor;
        Scope<ShaderLibrary> m_ShaderLibrary;

        // 帧同步
        std::vector<u64> m_FrameFenceValues;
        u32 m_FrameIndex = 0;
    };
}
