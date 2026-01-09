#include "SampleApp.h"
#include "Core/Input.h"
#include "Core/Log.h"
#include "Graphics/RenderDocCapture.h"
#include <imgui_internal.h>

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

    bool SampleApp::CreateDepthBuffer()
    {
        SEA_CORE_INFO("CreateDepthBuffer: Creating DSV heap...");
        // 创建DSV描述符堆
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        m_DSVHeap = MakeScope<DescriptorHeap>(*m_Device, dsvHeapDesc);
        if (!m_DSVHeap->Initialize())
        {
            SEA_CORE_ERROR("CreateDepthBuffer: DSV heap init failed");
            return false;
        }

        SEA_CORE_INFO("CreateDepthBuffer: Creating depth texture {}x{}", m_Window->GetWidth(), m_Window->GetHeight());
        // 创建深度纹理
        TextureDesc depthDesc;
        depthDesc.width = m_Window->GetWidth();
        depthDesc.height = m_Window->GetHeight();
        depthDesc.format = Format::D32_FLOAT;
        depthDesc.usage = TextureUsage::DepthStencil;
        depthDesc.name = "DepthBuffer";

        m_DepthBuffer = MakeScope<Texture>(*m_Device, depthDesc);
        if (!m_DepthBuffer->Initialize())
        {
            SEA_CORE_ERROR("CreateDepthBuffer: Texture init failed");
            return false;
        }

        SEA_CORE_INFO("CreateDepthBuffer: Creating DSV...");
        // 创建DSV
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

        m_Device->GetDevice()->CreateDepthStencilView(
            m_DepthBuffer->GetResource(),
            &dsvDesc,
            m_DSVHeap->GetCPUHandle(0)
        );

        SEA_CORE_INFO("CreateDepthBuffer: Done");
        return true;
    }

    bool SampleApp::CreateSceneRenderTarget(u32 width, u32 height)
    {
        if (width == 0 || height == 0)
            return false;
            
        m_ViewportWidth = width;
        m_ViewportHeight = height;
        
        // 等待 GPU 完成
        if (m_GraphicsQueue)
            m_GraphicsQueue->WaitForIdle();
        
        // 释放旧资源
        m_SceneRenderTarget.reset();
        m_SceneRTVHeap.reset();
        m_DepthBuffer.reset();
        m_DSVHeap.reset();
        
        // 创建 RTV 堆
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.NumDescriptors = 1;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        
        m_SceneRTVHeap = MakeScope<DescriptorHeap>(*m_Device, rtvHeapDesc);
        if (!m_SceneRTVHeap->Initialize())
            return false;
        
        // 创建场景渲染目标纹理
        TextureDesc rtDesc;
        rtDesc.width = width;
        rtDesc.height = height;
        rtDesc.format = Format::R8G8B8A8_UNORM;
        rtDesc.usage = TextureUsage::RenderTarget | TextureUsage::ShaderResource;
        rtDesc.name = "SceneRenderTarget";
        
        m_SceneRenderTarget = MakeScope<Texture>(*m_Device, rtDesc);
        if (!m_SceneRenderTarget->Initialize())
            return false;
        
        // 创建 RTV
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        
        m_Device->GetDevice()->CreateRenderTargetView(
            m_SceneRenderTarget->GetResource(),
            &rtvDesc,
            m_SceneRTVHeap->GetCPUHandle(0)
        );
        
        // 使用 ImGuiRenderer 注册纹理（在 ImGui 的 SRV 堆中创建 SRV）
        if (m_ImGuiRenderer)
        {
            m_SceneTextureHandle = m_ImGuiRenderer->RegisterTexture(
                m_SceneRenderTarget->GetResource(),
                DXGI_FORMAT_R8G8B8A8_UNORM
            );
        }
        
        // 重建深度缓冲（匹配新尺寸）
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        
        m_DSVHeap = MakeScope<DescriptorHeap>(*m_Device, dsvHeapDesc);
        if (!m_DSVHeap->Initialize())
            return false;
        
        TextureDesc depthDesc;
        depthDesc.width = width;
        depthDesc.height = height;
        depthDesc.format = Format::D32_FLOAT;
        depthDesc.usage = TextureUsage::DepthStencil;
        depthDesc.name = "ViewportDepthBuffer";
        
        m_DepthBuffer = MakeScope<Texture>(*m_Device, depthDesc);
        if (!m_DepthBuffer->Initialize())
            return false;
        
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        
        m_Device->GetDevice()->CreateDepthStencilView(
            m_DepthBuffer->GetResource(),
            &dsvDesc,
            m_DSVHeap->GetCPUHandle(0)
        );
        
        // 更新相机宽高比
        if (m_Camera)
        {
            m_Camera->SetPerspective(m_Camera->GetFOV(),
                static_cast<f32>(width) / static_cast<f32>(height),
                m_Camera->GetNearZ(), m_Camera->GetFarZ());
        }
        
        SEA_CORE_INFO("Scene render target created: {}x{}", width, height);
        return true;
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

        // 创建命令列表（每帧都有一个）
        SEA_CORE_INFO("Creating CommandLists...");
        u32 bufferCount = m_SwapChain->GetBufferCount();
        m_CommandLists.resize(bufferCount);
        for (u32 i = 0; i < bufferCount; ++i)
        {
            m_CommandLists[i] = MakeScope<CommandList>(*m_Device, CommandQueueType::Graphics);
            if (!m_CommandLists[i]->Initialize())
                return false;
        }

        // 初始化ImGui（需要先初始化，以便注册场景纹理）
        SEA_CORE_INFO("Initializing ImGui...");
        m_ImGuiRenderer = MakeScope<ImGuiRenderer>(*m_Device, *m_Window);
        if (!m_ImGuiRenderer->Initialize(m_SwapChain->GetBufferCount(), m_SwapChain->GetFormat()))
            return false;
        
        // 通知窗口 ImGui 已就绪
        m_Window->SetImGuiReady(true);

        // 创建场景渲染目标（需要在 ImGui 初始化之后，以便注册纹理）
        SEA_CORE_INFO("Creating Scene RenderTarget...");
        if (!CreateSceneRenderTarget(1280, 720))
            return false;

        // 初始化Shader系统
        ShaderCompiler::Initialize();
        m_ShaderLibrary = MakeScope<ShaderLibrary>();

        // 创建3D渲染器
        SEA_CORE_INFO("Creating SimpleRenderer...");
        m_Renderer = MakeScope<SimpleRenderer>(*m_Device);
        if (!m_Renderer->Initialize())
            return false;

        // 创建相机
        m_Camera = MakeScope<Camera>();
        m_Camera->SetPosition({ 0, 3, -8 });
        m_Camera->SetPerspective(45.0f, m_Window->GetAspectRatio(), 0.1f, 1000.0f);
        m_Camera->LookAt({ 0, 0, 0 });

        // 创建场景
        CreateScene();

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

    void SampleApp::CreateScene()
    {
        SEA_CORE_INFO("Creating scene meshes...");

        // 创建地面网格
        m_GridMesh = Mesh::CreatePlane(*m_Device, 100.0f, 100.0f);

        // 创建几何体
        auto cube = Mesh::CreateCube(*m_Device, 1.0f);
        auto sphere = Mesh::CreateSphere(*m_Device, 0.5f, 32, 16);
        auto torus = Mesh::CreateTorus(*m_Device, 0.6f, 0.2f, 32, 24);

        if (cube) m_Meshes.push_back(std::move(cube));
        if (sphere) m_Meshes.push_back(std::move(sphere));
        if (torus) m_Meshes.push_back(std::move(torus));

        // 创建场景对象
        // 多个立方体
        for (int i = -2; i <= 2; ++i)
        {
            SceneObject obj;
            obj.mesh = m_Meshes[0].get();
            XMStoreFloat4x4(&obj.transform, XMMatrixTranslation(static_cast<f32>(i) * 2.5f, 0.5f, 0));
            
            // 不同颜色
            float hue = (i + 2) / 5.0f;
            obj.color = { 
                0.5f + 0.5f * std::sin(hue * 6.28f),
                0.5f + 0.5f * std::sin((hue + 0.33f) * 6.28f),
                0.5f + 0.5f * std::sin((hue + 0.66f) * 6.28f),
                1.0f
            };
            obj.metallic = 0.1f + i * 0.2f;
            obj.roughness = 0.3f;
            m_SceneObjects.push_back(obj);
        }

        // 球体
        for (int i = -2; i <= 2; ++i)
        {
            SceneObject obj;
            obj.mesh = m_Meshes[1].get();
            XMStoreFloat4x4(&obj.transform, XMMatrixTranslation(static_cast<f32>(i) * 2.5f, 0.5f, 3.0f));
            obj.color = { 1.0f, 0.8f, 0.6f, 1.0f };
            obj.metallic = 0.9f;
            obj.roughness = (i + 2) * 0.2f;
            m_SceneObjects.push_back(obj);
        }

        // 甜甜圈
        for (int i = -1; i <= 1; ++i)
        {
            SceneObject obj;
            obj.mesh = m_Meshes[2].get();
            XMStoreFloat4x4(&obj.transform, 
                XMMatrixRotationX(XM_PIDIV2) * XMMatrixTranslation(static_cast<f32>(i) * 3.0f, 1.0f, -3.0f));
            obj.color = { 0.8f, 0.2f, 0.9f, 1.0f };
            obj.metallic = 0.5f;
            obj.roughness = 0.2f;
            m_SceneObjects.push_back(obj);
        }

        SEA_CORE_INFO("Scene created with {} objects", m_SceneObjects.size());
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
        
        m_SceneObjects.clear();
        m_Meshes.clear();
        m_GridMesh.reset();
        m_Renderer.reset();
        m_Camera.reset();
        
        m_DepthBuffer.reset();
        m_DSVHeap.reset();
        
        m_ImGuiRenderer.reset();
        m_CommandLists.clear();
        m_SwapChain.reset();
        m_GraphicsQueue.reset();
        m_Device.reset();

        RenderDocCapture::Shutdown();
    }

    void SampleApp::UpdateCamera(f32 deltaTime)
    {
        // 右键控制相机
        if (Input::IsMouseButtonDown(KeyCode::MouseRight))
        {
            if (!m_CameraControl)
            {
                m_CameraControl = true;
                m_LastMousePos = Input::GetMousePosition();
            }
            else
            {
                auto mousePos = Input::GetMousePosition();
                f32 dx = static_cast<f32>(mousePos.first - m_LastMousePos.first);
                f32 dy = static_cast<f32>(mousePos.second - m_LastMousePos.second);
                m_LastMousePos = mousePos;

                m_Camera->ProcessMouseMovement(dx, -dy);
            }

            // WASD 移动
            f32 forward = 0, right = 0, up = 0;
            if (Input::IsKeyDown(KeyCode::W)) forward = 1;
            if (Input::IsKeyDown(KeyCode::S)) forward = -1;
            if (Input::IsKeyDown(KeyCode::D)) right = 1;
            if (Input::IsKeyDown(KeyCode::A)) right = -1;
            if (Input::IsKeyDown(KeyCode::E)) up = 1;
            if (Input::IsKeyDown(KeyCode::Q)) up = -1;

            // Shift 加速
            f32 speed = Input::IsKeyDown(KeyCode::Shift) ? 3.0f : 1.0f;
            m_Camera->SetMoveSpeed(5.0f * speed);
            m_Camera->ProcessKeyboard(forward, right, up, deltaTime);
        }
        else
        {
            m_CameraControl = false;
        }
    }

    void SampleApp::SetupEditorLayout()
    {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

        ImGuiID dock_main_id = dockspace_id;
        
        // 左侧面板 (20%)
        ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.2f, nullptr, &dock_main_id);
        
        // 右侧面板 (25%)
        ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
        
        // 底部面板 (25%)
        ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);
        
        // 左侧分割为上下两部分
        ImGuiID dock_left_bottom = ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.4f, nullptr, &dock_left);
        
        // 右侧分割为上下两部分
        ImGuiID dock_right_bottom = ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Down, 0.5f, nullptr, &dock_right);

        // 分配窗口到 dock 位置
        ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
        ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
        ImGui::DockBuilderDockWindow("Scene", dock_left_bottom);
        ImGui::DockBuilderDockWindow("Inspector", dock_right);
        ImGui::DockBuilderDockWindow("Statistics", dock_right_bottom);
        ImGui::DockBuilderDockWindow("Node Editor", dock_bottom);
        ImGui::DockBuilderDockWindow("Shader Editor", dock_bottom);
        ImGui::DockBuilderDockWindow("Console", dock_bottom);
        ImGui::DockBuilderDockWindow("Asset Browser", dock_bottom);
        ImGui::DockBuilderDockWindow("Properties", dock_right);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    void SampleApp::RenderMainMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New Project", "Ctrl+N")) {}
                if (ImGui::MenuItem("Open Project", "Ctrl+O")) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Save", "Ctrl+S")) {}
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) { m_Running = false; }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "Ctrl+Z")) {}
                if (ImGui::MenuItem("Redo", "Ctrl+Y")) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Cut", "Ctrl+X")) {}
                if (ImGui::MenuItem("Copy", "Ctrl+C")) {}
                if (ImGui::MenuItem("Paste", "Ctrl+V")) {}
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Viewport", nullptr, &m_ShowViewport);
                ImGui::MenuItem("Hierarchy", nullptr, &m_ShowHierarchy);
                ImGui::MenuItem("Inspector", nullptr, &m_ShowInspector);
                ImGui::MenuItem("Console", nullptr, &m_ShowConsole);
                ImGui::MenuItem("Asset Browser", nullptr, &m_ShowAssetBrowser);
                ImGui::Separator();
                ImGui::MenuItem("ImGui Demo", nullptr, &m_ShowDemoWindow);
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Window"))
            {
                if (ImGui::MenuItem("Reset Layout"))
                {
                    m_FirstFrame = true;
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Help"))
            {
                if (ImGui::MenuItem("About SeaEngine")) {}
                if (ImGui::MenuItem("Documentation")) {}
                ImGui::EndMenu();
            }
            
            ImGui::EndMainMenuBar();
        }
    }

    void SampleApp::RenderToolbar()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        
        ImGuiWindowFlags toolbarFlags = ImGuiWindowFlags_NoScrollbar | 
                                         ImGuiWindowFlags_NoSavedSettings |
                                         ImGuiWindowFlags_NoNav |
                                         ImGuiWindowFlags_NoTitleBar |
                                         ImGuiWindowFlags_NoResize |
                                         ImGuiWindowFlags_NoMove |
                                         ImGuiWindowFlags_NoDocking;
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        
        float toolbarHeight = 40.0f;
        float menuBarHeight = ImGui::GetFrameHeight();
        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + menuBarHeight));
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, toolbarHeight));
        ImGui::SetNextWindowViewport(viewport->ID);
        
        if (ImGui::Begin("##Toolbar", nullptr, toolbarFlags))
        {
            // 文件操作按钮
            if (ImGui::Button("New", ImVec2(50, 30))) {}
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("New Project (Ctrl+N)");
            ImGui::SameLine();
            
            if (ImGui::Button("Open", ImVec2(50, 30))) {}
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open Project (Ctrl+O)");
            ImGui::SameLine();
            
            if (ImGui::Button("Save", ImVec2(50, 30))) {}
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save (Ctrl+S)");
            ImGui::SameLine();
            
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            
            // 编辑操作
            if (ImGui::Button("<-", ImVec2(30, 30))) {}
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Undo (Ctrl+Z)");
            ImGui::SameLine();
            
            if (ImGui::Button("->", ImVec2(30, 30))) {}
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Redo (Ctrl+Y)");
            ImGui::SameLine();
            
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            
            // 播放控制
            static bool isPlaying = false;
            ImGui::PushStyleColor(ImGuiCol_Button, isPlaying ? ImVec4(0.2f, 0.6f, 0.2f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Button));
            if (ImGui::Button(isPlaying ? "[O]" : "|>", ImVec2(30, 30)))
            {
                isPlaying = !isPlaying;
            }
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(isPlaying ? "Stop" : "Play");
            ImGui::SameLine();
            
            if (ImGui::Button("||", ImVec2(30, 30))) {}
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pause");
            ImGui::SameLine();
            
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            
            // 变换工具
            static int transformTool = 0;
            if (ImGui::RadioButton("Move", transformTool == 0)) transformTool = 0;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move Tool (W)");
            ImGui::SameLine();
            
            if (ImGui::RadioButton("Rotate", transformTool == 1)) transformTool = 1;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rotate Tool (E)");
            ImGui::SameLine();
            
            if (ImGui::RadioButton("Scale", transformTool == 2)) transformTool = 2;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scale Tool (R)");
            ImGui::SameLine();
            
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            
            // RenderDoc 按钮
            if (ImGui::Button("F12: Capture", ImVec2(100, 30)))
            {
                RenderDocCapture::TriggerCapture();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Capture frame with RenderDoc");
        }
        ImGui::End();
        
        ImGui::PopStyleVar(3);
    }

    void SampleApp::RenderStatusBar()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        
        ImGuiWindowFlags statusFlags = ImGuiWindowFlags_NoScrollbar | 
                                        ImGuiWindowFlags_NoSavedSettings |
                                        ImGuiWindowFlags_NoNav |
                                        ImGuiWindowFlags_NoTitleBar |
                                        ImGuiWindowFlags_NoResize |
                                        ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoDocking;
        
        float statusHeight = 25.0f;
        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - statusHeight));
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, statusHeight));
        ImGui::SetNextWindowViewport(viewport->ID);
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
        
        if (ImGui::Begin("##StatusBar", nullptr, statusFlags))
        {
            ImGui::Text("Ready");
            ImGui::SameLine(ImGui::GetWindowWidth() - 350);
            ImGui::Text("Objects: %zu | Passes: %zu | FPS: %.0f", 
                m_SceneObjects.size(), 
                m_RenderGraph->GetPasses().size(),
                ImGui::GetIO().Framerate);
        }
        ImGui::End();
        
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    void SampleApp::RenderViewport()
    {
        if (!m_ShowViewport) return;
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        
        if (ImGui::Begin("Viewport", &m_ShowViewport))
        {
            ImVec2 viewportSize = ImGui::GetContentRegionAvail();
            
            // 视口工具栏
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
            ImGui::BeginChild("ViewportToolbar", ImVec2(0, 30), true);
            {
                // 视图模式
                static int viewMode = 0;
                ImGui::Text("View:");
                ImGui::SameLine();
                ImGui::RadioButton("Lit", &viewMode, 0);
                ImGui::SameLine();
                ImGui::RadioButton("Wireframe", &viewMode, 1);
                ImGui::SameLine();
                ImGui::RadioButton("Normals", &viewMode, 2);
                
                ImGui::SameLine(ImGui::GetWindowWidth() - 250);
                ImGui::Text("%ux%u | FPS: %.0f", m_ViewportWidth, m_ViewportHeight, ImGui::GetIO().Framerate);
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            
            // 计算渲染区域尺寸
            ImVec2 renderSize = ImVec2(viewportSize.x, viewportSize.y - 30);
            u32 newWidth = static_cast<u32>(renderSize.x);
            u32 newHeight = static_cast<u32>(renderSize.y);
            
            // 如果尺寸变化，重新创建渲染目标
            if (newWidth > 0 && newHeight > 0 && 
                (newWidth != m_ViewportWidth || newHeight != m_ViewportHeight))
            {
                CreateSceneRenderTarget(newWidth, newHeight);
            }
            
            // 显示场景纹理
            if (m_SceneTextureHandle.ptr != 0 && m_SceneRenderTarget)
            {
                ImGui::Image(
                    (ImTextureID)m_SceneTextureHandle.ptr,
                    renderSize
                );
                
                // 检查视口是否被鼠标悬停（用于相机控制）
                if (ImGui::IsItemHovered())
                {
                    // 在视口内显示控制提示
                    ImVec2 p = ImGui::GetItemRectMin();
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    
                    // 相机信息
                    char camInfo[128];
                    snprintf(camInfo, sizeof(camInfo), "Camera: (%.1f, %.1f, %.1f) | Right-click + WASD to navigate",
                        m_Camera->GetPosition().x, m_Camera->GetPosition().y, m_Camera->GetPosition().z);
                    drawList->AddText(ImVec2(p.x + 10, p.y + 10), IM_COL32(255, 255, 255, 200), camInfo);
                }
            }
        }
        ImGui::End();
        
        ImGui::PopStyleVar();
    }

    void SampleApp::RenderHierarchy()
    {
        if (!m_ShowHierarchy) return;
        
        if (ImGui::Begin("Hierarchy", &m_ShowHierarchy))
        {
            // 搜索框
            static char searchBuf[128] = "";
            ImGui::InputTextWithHint("##Search", "Search...", searchBuf, sizeof(searchBuf));
            ImGui::Separator();
            
            // 场景树
            if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow))
            {
                // 相机
                if (ImGui::TreeNodeEx("[C] Camera", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen))
                {
                }
                
                // 灯光
                if (ImGui::TreeNodeEx("[L] Directional Light", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen))
                {
                }
                
                // 网格物体
                if (ImGui::TreeNodeEx("Geometry", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    for (size_t i = 0; i < m_SceneObjects.size(); ++i)
                    {
                        char name[64];
                        if (i < 5) snprintf(name, sizeof(name), "[M] Cube_%zu", i);
                        else if (i < 10) snprintf(name, sizeof(name), "[M] Sphere_%zu", i - 5);
                        else snprintf(name, sizeof(name), "[M] Torus_%zu", i - 10);
                        
                        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                        ImGui::TreeNodeEx(name, flags);
                    }
                    ImGui::TreePop();
                }
                
                // Grid
                if (ImGui::TreeNodeEx("[G] Ground Grid", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen))
                {
                }
                
                ImGui::TreePop();
            }
        }
        ImGui::End();
    }

    void SampleApp::RenderInspector()
    {
        if (!m_ShowInspector) return;
        
        if (ImGui::Begin("Inspector", &m_ShowInspector))
        {
            ImGui::Text("Select an object to inspect");
            ImGui::Separator();
            
            // 光照设置 - 始终显示
            if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
            {
                static XMFLOAT3 lightDir = { -0.5f, -1.0f, 0.5f };
                static XMFLOAT3 lightColor = { 1.0f, 0.98f, 0.95f };
                static f32 lightIntensity = 1.5f;
                
                if (ImGui::DragFloat3("Direction", &lightDir.x, 0.01f))
                    m_Renderer->SetLightDirection(lightDir);
                if (ImGui::ColorEdit3("Color", &lightColor.x))
                    m_Renderer->SetLightColor(lightColor);
                if (ImGui::DragFloat("Intensity", &lightIntensity, 0.1f, 0.0f, 10.0f))
                    m_Renderer->SetLightIntensity(lightIntensity);
            }
            
            // 相机设置
            if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("Position: %.2f, %.2f, %.2f", 
                    m_Camera->GetPosition().x, m_Camera->GetPosition().y, m_Camera->GetPosition().z);
                ImGui::Text("FOV: %.1f", m_Camera->GetFOV());
                ImGui::Text("Near/Far: %.2f / %.2f", m_Camera->GetNearZ(), m_Camera->GetFarZ());
            }
        }
        ImGui::End();
    }

    void SampleApp::RenderConsole()
    {
        if (!m_ShowConsole) return;
        
        if (ImGui::Begin("Console", &m_ShowConsole))
        {
            // 控制台工具栏
            if (ImGui::Button("Clear")) {}
            ImGui::SameLine();
            
            static bool showInfo = true, showWarning = true, showError = true;
            ImGui::Checkbox("Info", &showInfo);
            ImGui::SameLine();
            ImGui::Checkbox("Warning", &showWarning);
            ImGui::SameLine();
            ImGui::Checkbox("Error", &showError);
            
            ImGui::Separator();
            
            // 日志区域
            ImGui::BeginChild("LogRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            {
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "[INFO] SeaEngine initialized successfully");
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "[INFO] Scene loaded with %zu objects", m_SceneObjects.size());
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "[INFO] RenderGraph compiled: %zu passes", m_RenderGraph->GetPasses().size());
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    void SampleApp::RenderAssetBrowser()
    {
        if (!m_ShowAssetBrowser) return;
        
        if (ImGui::Begin("Asset Browser", &m_ShowAssetBrowser))
        {
            // 路径导航
            ImGui::Text("> Assets > Shaders");
            ImGui::Separator();
            
            // 资源网格
            float iconSize = 64.0f;
            float padding = 16.0f;
            int columns = static_cast<int>((ImGui::GetContentRegionAvail().x) / (iconSize + padding));
            if (columns < 1) columns = 1;
            
            if (ImGui::BeginTable("AssetGrid", columns))
            {
                const char* assets[] = { 
                    "[S] Basic.hlsl", 
                    "[S] Grid.hlsl", 
                    "[S] GBuffer.hlsl",
                    "[S] Tonemap.hlsl",
                    "[S] Common.hlsli",
                    "[M] Cube",
                    "[M] Sphere",
                    "[M] Torus"
                };
                
                for (int i = 0; i < 8; ++i)
                {
                    ImGui::TableNextColumn();
                    
                    ImGui::PushID(i);
                    if (ImGui::Button(assets[i], ImVec2(iconSize + 20, iconSize)))
                    {
                        // 选择资源
                    }
                    ImGui::PopID();
                }
                
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    void SampleApp::OnUpdate(f32 deltaTime)
    {
        Input::Update();
        m_TotalTime += deltaTime;

        // F12触发RenderDoc截帧
        if (Input::IsKeyPressed(KeyCode::F12))
        {
            RenderDocCapture::TriggerCapture();
            SEA_CORE_INFO("RenderDoc capture triggered");
        }

        // 更新相机
        UpdateCamera(deltaTime);

        // 旋转场景中的物体
        for (size_t i = 0; i < m_SceneObjects.size(); ++i)
        {
            auto& obj = m_SceneObjects[i];
            XMMATRIX current = XMLoadFloat4x4(&obj.transform);
            XMVECTOR scale, rot, trans;
            XMMatrixDecompose(&scale, &rot, &trans, current);

            // 缓慢旋转
            f32 rotSpeed = 0.5f + (i % 3) * 0.2f;
            XMMATRIX newRot = XMMatrixRotationY(m_TotalTime * rotSpeed);
            XMMATRIX newTransform = XMMatrixScalingFromVector(scale) * newRot * XMMatrixTranslationFromVector(trans);
            XMStoreFloat4x4(&obj.transform, newTransform);
        }

        m_ImGuiRenderer->BeginFrame();

        // 主菜单栏
        RenderMainMenuBar();
        
        // 工具栏
        RenderToolbar();
        
        // 设置 Dock 空间（在菜单和工具栏下方）
        float toolbarHeight = 40.0f + ImGui::GetFrameHeight();
        float statusHeight = 25.0f;
        
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + toolbarHeight));
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y - toolbarHeight - statusHeight));
        ImGui::SetNextWindowViewport(viewport->ID);
        
        ImGuiWindowFlags dockspaceFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                          ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                          ImGuiWindowFlags_NoBackground;
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        
        ImGui::Begin("DockSpaceWindow", nullptr, dockspaceFlags);
        ImGui::PopStyleVar(3);
        
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        
        // 首次启动时设置默认布局
        if (m_FirstFrame)
        {
            m_FirstFrame = false;
            SetupEditorLayout();
        }
        
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::End();
        
        // 渲染各个面板
        RenderViewport();
        RenderHierarchy();
        RenderInspector();
        RenderConsole();
        RenderAssetBrowser();
        
        // 原有的编辑器面板
        m_NodeEditor->Render();
        m_PropertyPanel->Render();
        m_ShaderEditor->Render();
        
        // Statistics 窗口
        ImGui::Begin("Statistics");
        ImGui::Text("Frame Time: %.3f ms", deltaTime * 1000.0f);
        ImGui::Text("FPS: %.1f", 1.0f / deltaTime);
        ImGui::Separator();
        ImGui::Text("Passes: %zu", m_RenderGraph->GetPasses().size());
        ImGui::Text("Resources: %zu", m_RenderGraph->GetResources().size());
        ImGui::Separator();
        ImGui::Text("Scene Objects: %zu", m_SceneObjects.size());
        ImGui::Text("Meshes: %zu", m_Meshes.size());
        if (ImGui::Button("Compile Graph"))
            m_RenderGraph->Compile();
        ImGui::End();
        
        // ImGui Demo 窗口
        if (m_ShowDemoWindow)
            ImGui::ShowDemoWindow(&m_ShowDemoWindow);
        
        // 状态栏
        RenderStatusBar();

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

        // ========== 1. 渲染 3D 场景到离屏纹理 ==========
        if (m_SceneRenderTarget && m_ViewportWidth > 0 && m_ViewportHeight > 0)
        {
            // 转换场景渲染目标到 RenderTarget 状态
            cmdList->TransitionBarrier(
                m_SceneRenderTarget->GetResource(),
                ResourceState::Common,
                ResourceState::RenderTarget
            );
            cmdList->TransitionBarrier(
                m_DepthBuffer->GetResource(),
                ResourceState::Common,
                ResourceState::DepthWrite
            );
            cmdList->FlushBarriers();

            // 清除场景渲染目标
            f32 sceneClearColor[] = { 0.1f, 0.1f, 0.15f, 1.0f };
            D3D12_CPU_DESCRIPTOR_HANDLE sceneRtv = m_SceneRTVHeap->GetCPUHandle(0);
            cmdList->GetCommandList()->ClearRenderTargetView(sceneRtv, sceneClearColor, 0, nullptr);
            
            D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_DSVHeap->GetCPUHandle(0);
            cmdList->GetCommandList()->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

            // 设置场景渲染目标
            cmdList->GetCommandList()->OMSetRenderTargets(1, &sceneRtv, FALSE, &dsv);

            // 设置场景视口
            Viewport sceneViewport = { 0, 0, static_cast<f32>(m_ViewportWidth), 
                                       static_cast<f32>(m_ViewportHeight), 0, 1 };
            ScissorRect sceneScissor = { 0, 0, static_cast<i32>(m_ViewportWidth), 
                                         static_cast<i32>(m_ViewportHeight) };
            cmdList->SetViewport(sceneViewport);
            cmdList->SetScissorRect(sceneScissor);

            // 开始3D渲染
            m_Renderer->BeginFrame(*m_Camera, m_TotalTime);

            // 渲染网格
            if (m_GridMesh)
            {
                m_Renderer->RenderGrid(*cmdList, *m_GridMesh);
            }

            // 渲染场景对象
            for (const auto& obj : m_SceneObjects)
            {
                m_Renderer->RenderObject(*cmdList, obj);
            }

            // 转换场景渲染目标到 ShaderResource 状态（供 ImGui 使用）
            cmdList->TransitionBarrier(
                m_SceneRenderTarget->GetResource(),
                ResourceState::RenderTarget,
                ResourceState::Common  // Common 可以作为 SRV 读取
            );
            cmdList->TransitionBarrier(
                m_DepthBuffer->GetResource(),
                ResourceState::DepthWrite,
                ResourceState::Common
            );
            cmdList->FlushBarriers();
        }

        // ========== 2. 渲染 ImGui 到 SwapChain ==========
        cmdList->TransitionBarrier(
            m_SwapChain->GetCurrentBackBuffer(),
            ResourceState::Present,
            ResourceState::RenderTarget
        );
        cmdList->FlushBarriers();

        // 清除后缓冲
        f32 clearColor[] = { 0.06f, 0.06f, 0.08f, 1.0f };
        cmdList->ClearRenderTarget(m_SwapChain->GetCurrentRTV(), clearColor);

        // 设置 SwapChain 渲染目标
        auto rtv = m_SwapChain->GetCurrentRTV();
        cmdList->GetCommandList()->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

        // 设置窗口视口
        Viewport viewport = { 0, 0, static_cast<f32>(m_Window->GetWidth()), 
                              static_cast<f32>(m_Window->GetHeight()), 0, 1 };
        ScissorRect scissor = { 0, 0, static_cast<i32>(m_Window->GetWidth()), 
                                static_cast<i32>(m_Window->GetHeight()) };
        cmdList->SetViewport(viewport);
        cmdList->SetScissorRect(scissor);

        // 渲染 ImGui (ImGui会自动设置自己的描述符堆)
        m_ImGuiRenderer->Render(cmdList->GetCommandList());

        // 转换后缓冲到呈现状态
        cmdList->TransitionBarrier(
            m_SwapChain->GetCurrentBackBuffer(),
            ResourceState::RenderTarget,
            ResourceState::Present
        );
        cmdList->FlushBarriers();

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
        // 注意：场景渲染目标的尺寸由 Viewport 面板控制，窗口 resize 不影响它
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
        // 预留用于创建GPU资源...
    }
}
