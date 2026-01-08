#include "SampleApp.h"
#include "Core/Input.h"
#include "Graphics/RenderDocCapture.h"

namespace Sea
{
    SampleApp::SampleApp()
        : Application({
            .window = { .title = "SeaEngine - Render Graph Editor", .width = 1920, .height = 1080 },
            .enableValidation = true,
            .enableRenderDoc = true
        })
    {
    }

    bool SampleApp::OnInitialize()
    {
        // 初始化RenderDoc
        RenderDocCapture::Initialize();

        // 初始化图形设备
        m_Device = MakeScope<Device>();
        if (!m_Device->Initialize())
            return false;

        // 创建命令队列
        m_GraphicsQueue = MakeScope<CommandQueue>(*m_Device, CommandQueueType::Graphics);
        if (!m_GraphicsQueue->Initialize())
            return false;

        // 创建交换链
        SwapChainDesc swapDesc;
        swapDesc.hwnd = m_Window->GetHandle();
        swapDesc.width = m_Window->GetWidth();
        swapDesc.height = m_Window->GetHeight();
        m_SwapChain = MakeScope<SwapChain>(*m_Device, swapDesc);
        if (!m_SwapChain->Initialize())
            return false;

        // 创建命令列表
        m_CommandList = MakeScope<CommandList>(*m_Device, CommandQueueType::Graphics);
        if (!m_CommandList->Initialize())
            return false;

        // 初始化ImGui
        m_ImGuiRenderer = MakeScope<ImGuiRenderer>(*m_Device, *m_Window);
        if (!m_ImGuiRenderer->Initialize(m_SwapChain->GetBufferCount(), m_SwapChain->GetFormat()))
            return false;

        // 初始化Shader系统
        ShaderCompiler::Initialize();
        m_ShaderLibrary = MakeScope<ShaderLibrary>();

        // 创建RenderGraph
        m_RenderGraph = MakeScope<RenderGraph>();
        SetupRenderGraph();

        // 创建编辑器组件
        m_NodeEditor = MakeScope<NodeEditor>(*m_RenderGraph);
        m_NodeEditor->Initialize();

        m_PropertyPanel = MakeScope<PropertyPanel>(*m_RenderGraph);
        m_ShaderEditor = MakeScope<ShaderEditor>();

        // 帧同步
        m_FrameFenceValues.resize(m_SwapChain->GetBufferCount(), 0);

        // 初始化输入
        Input::Initialize(m_Window->GetHandle());

        SEA_CORE_INFO("SampleApp initialized successfully");
        return true;
    }

    void SampleApp::OnShutdown()
    {
        m_GraphicsQueue->WaitForIdle();

        m_ShaderEditor.reset();
        m_PropertyPanel.reset();
        m_NodeEditor.reset();
        m_RenderGraph.reset();
        m_ShaderLibrary.reset();
        ShaderCompiler::Shutdown();
        m_ImGuiRenderer.reset();
        m_CommandList.reset();
        m_SwapChain.reset();
        m_GraphicsQueue.reset();
        m_Device.reset();

        RenderDocCapture::Shutdown();
    }

    void SampleApp::OnUpdate(f32 deltaTime)
    {
        Input::Update();

        // F12触发RenderDoc截帧
        if (Input::IsKeyPressed(KeyCode::F12))
        {
            RenderDocCapture::TriggerCapture();
            SEA_CORE_INFO("RenderDoc capture triggered");
        }

        m_ImGuiRenderer->BeginFrame();

        // 主Dock空间
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // 渲染编辑器UI
        m_NodeEditor->Render();
        m_PropertyPanel->Render();
        m_ShaderEditor->Render();

        // 渲染统计窗口
        ImGui::Begin("Statistics");
        ImGui::Text("Frame Time: %.3f ms", deltaTime * 1000.0f);
        ImGui::Text("FPS: %.1f", 1.0f / deltaTime);
        ImGui::Separator();
        ImGui::Text("Passes: %zu", m_RenderGraph->GetPasses().size());
        ImGui::Text("Resources: %zu", m_RenderGraph->GetResources().size());
        if (ImGui::Button("Compile Graph"))
            m_RenderGraph->Compile();
        ImGui::End();

        m_ImGuiRenderer->EndFrame();
    }

    void SampleApp::OnRender()
    {
        // 等待当前帧资源可用
        m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
        m_GraphicsQueue->WaitForFence(m_FrameFenceValues[m_FrameIndex]);

        // 重置命令列表
        m_CommandList->Reset();

        // 转换后缓冲到渲染目标状态
        m_CommandList->TransitionBarrier(
            m_SwapChain->GetCurrentBackBuffer(),
            ResourceState::Present,
            ResourceState::RenderTarget
        );
        m_CommandList->FlushBarriers();

        // 清除后缓冲
        f32 clearColor[] = { 0.1f, 0.1f, 0.15f, 1.0f };
        m_CommandList->ClearRenderTarget(m_SwapChain->GetCurrentRTV(), clearColor);

        // 设置渲染目标
        auto rtv = m_SwapChain->GetCurrentRTV();
        m_CommandList->SetRenderTargets({ &rtv, 1 });

        // 设置视口
        Viewport viewport = { 0, 0, static_cast<f32>(m_Window->GetWidth()), 
                              static_cast<f32>(m_Window->GetHeight()), 0, 1 };
        ScissorRect scissor = { 0, 0, static_cast<i32>(m_Window->GetWidth()), 
                                static_cast<i32>(m_Window->GetHeight()) };
        m_CommandList->SetViewport(viewport);
        m_CommandList->SetScissorRect(scissor);

        // 执行RenderGraph (如果已编译)
        // m_RenderGraph->Execute(*m_CommandList);

        // 渲染ImGui
        m_ImGuiRenderer->Render(m_CommandList->GetCommandList());

        // 转换后缓冲到呈现状态
        m_CommandList->TransitionBarrier(
            m_SwapChain->GetCurrentBackBuffer(),
            ResourceState::RenderTarget,
            ResourceState::Present
        );

        // 关闭并执行命令列表
        m_CommandList->Close();
        m_GraphicsQueue->ExecuteCommandList(m_CommandList.get());

        // 呈现
        m_SwapChain->Present();

        // 发送fence信号
        m_FrameFenceValues[m_FrameIndex] = m_GraphicsQueue->Signal();
    }

    void SampleApp::OnResize(u32 width, u32 height)
    {
        if (width == 0 || height == 0)
            return;

        m_GraphicsQueue->WaitForIdle();
        m_SwapChain->Resize(width, height);
    }

    void SampleApp::SetupRenderGraph()
    {
        // 创建一些默认的Pass作为示例
        
        // GBuffer Pass
        RenderPassDesc gbufferPass;
        gbufferPass.name = "GBuffer Pass";
        gbufferPass.type = PassType::Graphics;
        gbufferPass.nodeX = 100; gbufferPass.nodeY = 100;
        
        // 创建GBuffer资源
        RGResourceDesc albedoDesc;
        albedoDesc.name = "Albedo";
        albedoDesc.width = 1920; albedoDesc.height = 1080;
        albedoDesc.format = Format::R8G8B8A8_UNORM;
        auto albedo = m_RenderGraph->CreateResource(albedoDesc);
        gbufferPass.outputs.push_back(albedo);

        RGResourceDesc normalDesc;
        normalDesc.name = "Normal";
        normalDesc.width = 1920; normalDesc.height = 1080;
        normalDesc.format = Format::R16G16B16A16_FLOAT;
        auto normal = m_RenderGraph->CreateResource(normalDesc);
        gbufferPass.outputs.push_back(normal);

        m_RenderGraph->AddPass(gbufferPass);

        // Lighting Pass
        RenderPassDesc lightingPass;
        lightingPass.name = "Lighting Pass";
        lightingPass.type = PassType::Graphics;
        lightingPass.nodeX = 400; lightingPass.nodeY = 100;
        lightingPass.inputs.push_back(albedo);
        lightingPass.inputs.push_back(normal);

        RGResourceDesc hdrDesc;
        hdrDesc.name = "HDR Color";
        hdrDesc.width = 1920; hdrDesc.height = 1080;
        hdrDesc.format = Format::R16G16B16A16_FLOAT;
        auto hdr = m_RenderGraph->CreateResource(hdrDesc);
        lightingPass.outputs.push_back(hdr);

        m_RenderGraph->AddPass(lightingPass);

        // Tonemap Pass
        RenderPassDesc tonemapPass;
        tonemapPass.name = "Tonemap";
        tonemapPass.type = PassType::Graphics;
        tonemapPass.nodeX = 700; tonemapPass.nodeY = 100;
        tonemapPass.inputs.push_back(hdr);

        RGResourceDesc ldrDesc;
        ldrDesc.name = "LDR Output";
        ldrDesc.width = 1920; ldrDesc.height = 1080;
        ldrDesc.format = Format::R8G8B8A8_UNORM;
        auto ldr = m_RenderGraph->CreateResource(ldrDesc);
        tonemapPass.outputs.push_back(ldr);

        m_RenderGraph->AddPass(tonemapPass);

        m_RenderGraph->Compile();
        SEA_CORE_INFO("Default render graph created with {} passes", m_RenderGraph->GetPasses().size());
    }

    void SampleApp::CreateResources()
    {
        // 创建其他GPU资源...
    }
}
