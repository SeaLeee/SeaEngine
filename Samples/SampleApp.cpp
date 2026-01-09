#include "SampleApp.h"
#include "Core/Input.h"
#include "Core/Log.h"
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
        SEA_CORE_INFO("SampleApp::OnInitialize starting...");
        
        // 初始化RenderDoc
        RenderDocCapture::Initialize();

        // 初始化图形设备
        SEA_CORE_INFO("Creating Device...");
        m_Device = MakeScope<Device>();
        if (!m_Device->Initialize())
            return false;

        // 创建命令队列
        SEA_CORE_INFO("Creating CommandQueue...");
        m_GraphicsQueue = MakeScope<CommandQueue>(*m_Device, CommandQueueType::Graphics);
        if (!m_GraphicsQueue->Initialize())
            return false;

        // 创建交换链
        SEA_CORE_INFO("Creating SwapChain...");
        SwapChainDesc swapDesc;
        swapDesc.hwnd = m_Window->GetHandle();
        swapDesc.width = m_Window->GetWidth();
        swapDesc.height = m_Window->GetHeight();
        m_SwapChain = MakeScope<SwapChain>(*m_Device, *m_GraphicsQueue, swapDesc);
        if (!m_SwapChain->Initialize())
            return false;

        // 创建命令列表（每帧缓冲一个）
        SEA_CORE_INFO("Creating CommandLists...");
        u32 bufferCount = m_SwapChain->GetBufferCount();
        m_CommandLists.resize(bufferCount);
        for (u32 i = 0; i < bufferCount; ++i)
        {
            m_CommandLists[i] = MakeScope<CommandList>(*m_Device, CommandQueueType::Graphics);
            if (!m_CommandLists[i]->Initialize())
                return false;
        }

        // 初始化ImGui
        SEA_CORE_INFO("Initializing ImGui...");
        m_ImGuiRenderer = MakeScope<ImGuiRenderer>(*m_Device, *m_Window);
        if (!m_ImGuiRenderer->Initialize(m_SwapChain->GetBufferCount(), m_SwapChain->GetFormat()))
            return false;
        
        // 通知窗口 ImGui 已就绪
        m_Window->SetImGuiReady(true);

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
        m_CommandLists.clear();
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

        // 获取当前帧的命令列表
        auto& cmdList = m_CommandLists[m_FrameIndex];

        // 重置命令列表
        cmdList->Reset();

        // 转换后缓冲到渲染目标状态
        cmdList->TransitionBarrier(
            m_SwapChain->GetCurrentBackBuffer(),
            ResourceState::Present,
            ResourceState::RenderTarget
        );
        cmdList->FlushBarriers();

        // 清除后缓冲
        f32 clearColor[] = { 0.1f, 0.1f, 0.15f, 1.0f };
        cmdList->ClearRenderTarget(m_SwapChain->GetCurrentRTV(), clearColor);

        // 设置渲染目标
        auto rtv = m_SwapChain->GetCurrentRTV();
        cmdList->SetRenderTargets({ &rtv, 1 });

        // 设置视口
        Viewport viewport = { 0, 0, static_cast<f32>(m_Window->GetWidth()), 
                              static_cast<f32>(m_Window->GetHeight()), 0, 1 };
        ScissorRect scissor = { 0, 0, static_cast<i32>(m_Window->GetWidth()), 
                                static_cast<i32>(m_Window->GetHeight()) };
        cmdList->SetViewport(viewport);
        cmdList->SetScissorRect(scissor);

        // 执行RenderGraph (如果已编译)
        // m_RenderGraph->Execute(*cmdList);

        // 渲染ImGui
        m_ImGuiRenderer->Render(cmdList->GetCommandList());

        // 转换后缓冲到呈现状态
        cmdList->TransitionBarrier(
            m_SwapChain->GetCurrentBackBuffer(),
            ResourceState::RenderTarget,
            ResourceState::Present
        );

        // 关闭并执行命令列表
        cmdList->Close();
        m_GraphicsQueue->ExecuteCommandList(cmdList.get());

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
        // 初始化RenderGraph
        m_RenderGraph->Initialize(m_Device.get());

        // 创建GBuffer资源
        u32 albedoId = m_RenderGraph->CreateResource("Albedo", ResourceNodeType::Texture2D);
        if (auto* albedo = m_RenderGraph->GetResource(albedoId))
        {
            albedo->SetDimensions(1920, 1080);
            albedo->SetFormat(Format::R8G8B8A8_UNORM);
            albedo->SetPosition(50, 50);
        }

        u32 normalId = m_RenderGraph->CreateResource("Normal", ResourceNodeType::Texture2D);
        if (auto* normal = m_RenderGraph->GetResource(normalId))
        {
            normal->SetDimensions(1920, 1080);
            normal->SetFormat(Format::R16G16B16A16_FLOAT);
            normal->SetPosition(50, 150);
        }

        u32 hdrId = m_RenderGraph->CreateResource("HDR Color", ResourceNodeType::Texture2D);
        if (auto* hdr = m_RenderGraph->GetResource(hdrId))
        {
            hdr->SetDimensions(1920, 1080);
            hdr->SetFormat(Format::R16G16B16A16_FLOAT);
            hdr->SetPosition(350, 100);
        }

        u32 ldrId = m_RenderGraph->CreateResource("LDR Output", ResourceNodeType::Texture2D);
        if (auto* ldr = m_RenderGraph->GetResource(ldrId))
        {
            ldr->SetDimensions(1920, 1080);
            ldr->SetFormat(Format::R8G8B8A8_UNORM);
            ldr->SetPosition(650, 100);
        }

        // GBuffer Pass
        u32 gbufferId = m_RenderGraph->AddPass("GBuffer Pass", PassType::Graphics);
        if (auto* gbuffer = m_RenderGraph->GetPass(gbufferId))
        {
            gbuffer->AddOutput("Albedo");
            gbuffer->AddOutput("Normal");
            gbuffer->SetOutput(0, albedoId);
            gbuffer->SetOutput(1, normalId);
            gbuffer->SetPosition(200, 100);
        }

        // Lighting Pass
        u32 lightingId = m_RenderGraph->AddPass("Lighting Pass", PassType::Graphics);
        if (auto* lighting = m_RenderGraph->GetPass(lightingId))
        {
            lighting->AddInput("Albedo");
            lighting->AddInput("Normal");
            lighting->AddOutput("HDR");
            lighting->SetInput(0, albedoId);
            lighting->SetInput(1, normalId);
            lighting->SetOutput(0, hdrId);
            lighting->SetPosition(450, 100);
        }

        // Tonemap Pass
        u32 tonemapId = m_RenderGraph->AddPass("Tonemap", PassType::Graphics);
        if (auto* tonemap = m_RenderGraph->GetPass(tonemapId))
        {
            tonemap->AddInput("HDR");
            tonemap->AddOutput("LDR");
            tonemap->SetInput(0, hdrId);
            tonemap->SetOutput(0, ldrId);
            tonemap->SetPosition(700, 100);
        }

        m_RenderGraph->Compile();
        SEA_CORE_INFO("Default render graph created with {} passes", m_RenderGraph->GetPasses().size());
    }

    void SampleApp::CreateResources()
    {
        // 创建其他GPU资源...
    }
}
