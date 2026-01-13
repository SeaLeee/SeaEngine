#include "SampleApp.h"
#include "Core/Input.h"
#include "Core/Log.h"
#include "Graphics/RenderDocCapture.h"
#include "Scene/SceneManager.h"
#include "Scene/TonemapRenderer.h"
#include "Shader/ShaderCompiler.h"
#include <imgui_internal.h>
#include <filesystem>

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
        m_HDRRenderTarget.reset();
        m_SceneRTVHeap.reset();
        m_HDRRTVHeap.reset();
        m_PostProcessSRVHeap.reset();
        m_DepthBuffer.reset();
        m_DSVHeap.reset();
        
        // ========== 1. 创建 LDR 场景渲染目标（用于 ImGui 显示） ==========
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.NumDescriptors = 1;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        
        m_SceneRTVHeap = MakeScope<DescriptorHeap>(*m_Device, rtvHeapDesc);
        if (!m_SceneRTVHeap->Initialize())
            return false;
        
        TextureDesc ldrRtDesc;
        ldrRtDesc.width = width;
        ldrRtDesc.height = height;
        ldrRtDesc.format = Format::R8G8B8A8_UNORM;
        ldrRtDesc.usage = TextureUsage::RenderTarget | TextureUsage::ShaderResource;
        ldrRtDesc.name = "SceneRenderTarget_LDR";
        
        m_SceneRenderTarget = MakeScope<Texture>(*m_Device, ldrRtDesc);
        if (!m_SceneRenderTarget->Initialize())
            return false;
        
        D3D12_RENDER_TARGET_VIEW_DESC ldrRtvDesc = {};
        ldrRtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        ldrRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        
        m_Device->GetDevice()->CreateRenderTargetView(
            m_SceneRenderTarget->GetResource(),
            &ldrRtvDesc,
            m_SceneRTVHeap->GetCPUHandle(0)
        );
        
        // 注册 LDR 纹理到 ImGui
        if (m_ImGuiRenderer)
        {
            m_SceneTextureHandle = m_ImGuiRenderer->RegisterTexture(
                m_SceneRenderTarget->GetResource(),
                DXGI_FORMAT_R8G8B8A8_UNORM
            );
        }
        
        // ========== 2. 创建 HDR 场景渲染目标（用于后处理） ==========
        if (m_UseHDRPipeline)
        {
            D3D12_DESCRIPTOR_HEAP_DESC hdrRtvHeapDesc = {};
            hdrRtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            hdrRtvHeapDesc.NumDescriptors = 1;
            hdrRtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            
            m_HDRRTVHeap = MakeScope<DescriptorHeap>(*m_Device, hdrRtvHeapDesc);
            if (!m_HDRRTVHeap->Initialize())
                return false;
            
            TextureDesc hdrRtDesc;
            hdrRtDesc.width = width;
            hdrRtDesc.height = height;
            hdrRtDesc.format = Format::R16G16B16A16_FLOAT;  // HDR 格式
            hdrRtDesc.usage = TextureUsage::RenderTarget | TextureUsage::ShaderResource;
            hdrRtDesc.name = "SceneRenderTarget_HDR";
            
            m_HDRRenderTarget = MakeScope<Texture>(*m_Device, hdrRtDesc);
            if (!m_HDRRenderTarget->Initialize())
                return false;
            
            D3D12_RENDER_TARGET_VIEW_DESC hdrRtvDesc = {};
            hdrRtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            hdrRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            
            m_Device->GetDevice()->CreateRenderTargetView(
                m_HDRRenderTarget->GetResource(),
                &hdrRtvDesc,
                m_HDRRTVHeap->GetCPUHandle(0)
            );
            
            // ========== 3. 创建后处理 SRV 描述符堆 ==========
            DescriptorHeapDesc ppSrvHeapDesc;
            ppSrvHeapDesc.type = DescriptorHeapType::CBV_SRV_UAV;
            ppSrvHeapDesc.numDescriptors = 4;  // HDR Scene + Bloom + 预留
            ppSrvHeapDesc.shaderVisible = true;
            
            m_PostProcessSRVHeap = MakeScope<DescriptorHeap>(*m_Device, ppSrvHeapDesc);
            if (!m_PostProcessSRVHeap->Initialize())
                return false;
            
            // 创建 HDR 场景 SRV (slot 0)
            D3D12_SHADER_RESOURCE_VIEW_DESC hdrSrvDesc = {};
            hdrSrvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            hdrSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            hdrSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            hdrSrvDesc.Texture2D.MipLevels = 1;
            
            m_Device->GetDevice()->CreateShaderResourceView(
                m_HDRRenderTarget->GetResource(),
                &hdrSrvDesc,
                m_PostProcessSRVHeap->GetCPUHandle(0)
            );
            m_HDRSceneSRV = m_PostProcessSRVHeap->GetGPUHandle(0);
            
            // Bloom SRV 将由 BloomRenderer 填充 (slot 1)
            // 暂时创建一个占位 SRV
            m_Device->GetDevice()->CreateShaderResourceView(
                m_HDRRenderTarget->GetResource(),  // 临时使用 HDR RT
                &hdrSrvDesc,
                m_PostProcessSRVHeap->GetCPUHandle(1)
            );
            m_BloomResultSRV = m_PostProcessSRVHeap->GetGPUHandle(1);
        }
        
        // ========== 4. 重建深度缓冲 ==========
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
        
        // 更新 Deferred 渲染器资源尺寸
        if (m_DeferredRenderer)
        {
            m_DeferredRenderer->Resize(width, height);
        }
        
        // 更新 Bloom 渲染器资源尺寸
        if (m_BloomRenderer)
        {
            m_BloomRenderer->Resize(width, height);
        }
        
        SEA_CORE_INFO("Scene render target created: {}x{} (HDR: {})", width, height, m_UseHDRPipeline);
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

        // 创建天空渲染器
        SEA_CORE_INFO("Creating SkyRenderer...");
        m_SkyRenderer = MakeScope<SkyRenderer>(*m_Device);
        if (!m_SkyRenderer->Initialize())
        {
            SEA_CORE_WARN("Failed to initialize SkyRenderer - Sky will not be rendered");
            m_SkyRenderer.reset();
        }

        // 创建相机 - 调整位置以观察 PBR 材质球阵列
        m_Camera = MakeScope<Camera>();
        m_Camera->SetPosition({ 0, 8, -12 });
        m_Camera->SetPerspective(45.0f, m_Window->GetAspectRatio(), 0.1f, 1000.0f);
        m_Camera->LookAt({ 0, 0, 5 });

        // 创建场景
        CreateScene();

        // 创建RenderGraph
        m_RenderGraph = MakeScope<RenderGraph>();
        SetupRenderGraph();

        // 初始化 Pass 模板库
        PassTemplateLibrary::Initialize();

        // 创建编辑器组件
        m_NodeEditor = MakeScope<NodeEditor>(m_RenderGraph.get(), m_Device.get());
        m_NodeEditor->Initialize();

        m_PropertyPanel = MakeScope<PropertyPanel>(m_RenderGraph.get());
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
        SEA_CORE_INFO("Initializing SceneManager...");

        // 创建场景管理器
        m_SceneManager = MakeScope<SceneManager>(*m_Device);
        
        // 创建天空渲染器
        m_SkyRenderer = MakeScope<SkyRenderer>(*m_Device);
        if (!m_SkyRenderer->Initialize())
        {
            SEA_CORE_WARN("Failed to initialize SkyRenderer - Sky rendering disabled");
            m_SkyRenderer.reset();
        }
        
        // 创建海洋模拟
        m_Ocean = MakeScope<Ocean>(*m_Device);
        OceanParams oceanParams;
        oceanParams.patchSize = 500.0f;
        oceanParams.gridSize = 200.0f;
        oceanParams.windSpeed = 20.0f;
        oceanParams.amplitude = 1.0f;
        if (!m_Ocean->Initialize(oceanParams))
        {
            SEA_CORE_WARN("Failed to initialize Ocean simulation - Ocean scenes will not be available");
            m_Ocean.reset();
        }
        
        // 创建 Bloom 渲染器
        m_BloomRenderer = MakeScope<BloomRenderer>(*m_Device);
        if (!m_BloomRenderer->Initialize(m_ViewportWidth, m_ViewportHeight))
        {
            SEA_CORE_WARN("Failed to initialize BloomRenderer - Bloom will not be available");
            m_BloomRenderer.reset();
        }
        
        // 创建 Tonemap 渲染器
        m_TonemapRenderer = MakeScope<TonemapRenderer>(*m_Device);
        if (!m_TonemapRenderer->Initialize())
        {
            SEA_CORE_WARN("Failed to initialize TonemapRenderer - Tonemapping will not be available");
            m_TonemapRenderer.reset();
        }
        
        // 创建 Deferred 渲染器
        m_DeferredRenderer = MakeScope<DeferredRenderer>(*m_Device);
        if (!m_DeferredRenderer->Initialize(m_ViewportWidth, m_ViewportHeight))
        {
            SEA_CORE_WARN("Failed to initialize DeferredRenderer - Deferred pipeline will not be available");
            m_DeferredRenderer.reset();
        }
        
        // 设置场景切换回调
        m_SceneManager->SetOnSceneChanged([this](const std::string& sceneName) {
            SEA_CORE_INFO("Scene changed to: {}", sceneName);
            
            // 检测是否为海洋场景
            m_OceanSceneActive = (sceneName.find("Ocean") != std::string::npos);
            
            if (m_OceanSceneActive)
            {
                // 为海洋场景设置相机
                m_Camera->SetPosition({ 0, 15, -40 });
                m_Camera->LookAt({ 0, 0, 50 });
            }
        });
        
        // 扫描场景目录
        m_SceneManager->ScanScenes("Scenes");
        
        // 如果有可用场景，加载第一个；否则创建默认 PBR Demo 场景
        if (!m_SceneManager->GetSceneFiles().empty())
        {
            m_SceneManager->LoadScene(0);
        }
        else
        {
            m_SceneManager->CreatePBRDemoScene();
        }
        
        // 获取网格和场景对象的引用
        m_GridMesh = Mesh::CreatePlane(*m_Device, 100.0f, 100.0f);
        
        // 从 SceneManager 复制场景对象
        m_SceneObjects = m_SceneManager->GetSceneObjects();
        
        // 应用场景设置
        m_SceneManager->ApplyToRenderer(*m_Renderer);
        m_SceneManager->ApplyToCamera(*m_Camera);
        
        SEA_CORE_INFO("Scene initialized with {} objects", m_SceneObjects.size());
        
        // 扫描可用的外部模型
        ScanAvailableModels();
    }

    void SampleApp::ScanAvailableModels()
    {
        m_AvailableModels.clear();
        
        std::filesystem::path modelsPath = "Assets/Models";
        if (!std::filesystem::exists(modelsPath))
        {
            SEA_CORE_WARN("Models directory not found: {}", modelsPath.string());
            return;
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(modelsPath))
        {
            if (entry.is_regular_file())
            {
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".obj")
                {
                    m_AvailableModels.push_back(entry.path().string());
                    SEA_CORE_INFO("Found model: {}", entry.path().filename().string());
                }
            }
        }
        
        SEA_CORE_INFO("Scanned {} OBJ models", m_AvailableModels.size());
    }

    bool SampleApp::LoadExternalModel(const std::string& filepath)
    {
        SEA_CORE_INFO("Loading external model: {}", filepath);
        
        auto mesh = MakeScope<Mesh>();
        if (!mesh->LoadFromOBJ(*m_Device, filepath))
        {
            SEA_CORE_ERROR("Failed to load model: {}", filepath);
            return false;
        }
        
        // 添加到场景
        SceneObject obj;
        obj.mesh = mesh.get();
        XMStoreFloat4x4(&obj.transform, XMMatrixTranslation(0.0f, 0.5f, 0.0f));
        obj.color = { 0.9f, 0.85f, 0.7f, 1.0f };
        obj.metallic = 0.3f;
        obj.roughness = 0.4f;
        
        m_Meshes.push_back(std::move(mesh));
        m_SceneObjects.push_back(obj);
        
        SEA_CORE_INFO("Model loaded and added to scene");
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
        PassTemplateLibrary::Shutdown();
        
        m_SceneObjects.clear();
        m_Meshes.clear();
        m_GridMesh.reset();
        m_SkyRenderer.reset();
        m_Ocean.reset();
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

            // Shift 加速 (使用乘法因子，不覆盖用户设置的基础速度)
            f32 speedMultiplier = Input::IsKeyDown(KeyCode::Shift) ? 3.0f : 1.0f;
            m_Camera->ProcessKeyboard(forward * speedMultiplier, right * speedMultiplier, up * speedMultiplier, deltaTime);
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
        ImGui::DockBuilderDockWindow("Node Editor", dock_main_id);  // 和 Viewport 同一层级
        ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
        ImGui::DockBuilderDockWindow("Scene", dock_left_bottom);
        ImGui::DockBuilderDockWindow("Inspector", dock_right);
        ImGui::DockBuilderDockWindow("Render Settings", dock_right);
        ImGui::DockBuilderDockWindow("Statistics", dock_right_bottom);
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
                ImGui::MenuItem("Render Settings", nullptr, &m_ShowRenderSettings);
                ImGui::Separator();
                
                // UI 缩放选项 - 适用于远程桌面
                if (ImGui::BeginMenu("UI Scale"))
                {
                    ImGuiIO& io = ImGui::GetIO();
                    static float uiScale = 1.0f;
                    if (ImGui::MenuItem("100%", nullptr, uiScale == 1.0f)) { uiScale = 1.0f; io.FontGlobalScale = uiScale; }
                    if (ImGui::MenuItem("125%", nullptr, uiScale == 1.25f)) { uiScale = 1.25f; io.FontGlobalScale = uiScale; }
                    if (ImGui::MenuItem("150%", nullptr, uiScale == 1.5f)) { uiScale = 1.5f; io.FontGlobalScale = uiScale; }
                    if (ImGui::MenuItem("175%", nullptr, uiScale == 1.75f)) { uiScale = 1.75f; io.FontGlobalScale = uiScale; }
                    if (ImGui::MenuItem("200%", nullptr, uiScale == 2.0f)) { uiScale = 2.0f; io.FontGlobalScale = uiScale; }
                    ImGui::Separator();
                    if (ImGui::MenuItem("75%", nullptr, uiScale == 0.75f)) { uiScale = 0.75f; io.FontGlobalScale = uiScale; }
                    if (ImGui::MenuItem("50%", nullptr, uiScale == 0.5f)) { uiScale = 0.5f; io.FontGlobalScale = uiScale; }
                    ImGui::EndMenu();
                }
                
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
                if (!m_PendingCapture)
                {
                    m_PendingCapture = true;
                    RenderDocCapture::TriggerCapture(); // 开始截帧
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Capture frame with RenderDoc");
            
            ImGui::SameLine();
            
            // 显示截帧数量并提供打开RenderDoc UI的按钮
            uint32_t numCaptures = RenderDocCapture::GetNumCaptures();
            char captureLabel[64];
            snprintf(captureLabel, sizeof(captureLabel), "View (%u)", numCaptures);
            if (ImGui::Button(captureLabel, ImVec2(80, 30)))
            {
                RenderDocCapture::LaunchReplayUI();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open RenderDoc to view captures");
            
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            
            // ========== 场景选择器 ==========
            if (m_SceneManager)
            {
                ImGui::Text("Scene:");
                ImGui::SameLine();
                
                // 上一个场景按钮
                if (ImGui::Button("<##PrevScene", ImVec2(25, 30)))
                {
                    m_SceneManager->PreviousScene();
                    m_SceneObjects = m_SceneManager->GetSceneObjects();
                    m_SceneManager->ApplyToRenderer(*m_Renderer);
                    m_SceneManager->ApplyToCamera(*m_Camera);
                    // 检测是否为海洋场景
                    auto sceneName = m_SceneManager->GetCurrentSceneName();
                    m_OceanSceneActive = (sceneName.find("Ocean") != std::string::npos);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Previous Scene (Page Up)");
                ImGui::SameLine();
                
                // 场景下拉框
                const auto& sceneNames = m_SceneManager->GetSceneNames();
                int currentScene = m_SceneManager->GetCurrentSceneIndex();
                ImGui::SetNextItemWidth(150);
                if (!sceneNames.empty())
                {
                    if (ImGui::BeginCombo("##SceneCombo", currentScene >= 0 ? sceneNames[currentScene].c_str() : "No Scene"))
                    {
                        for (int i = 0; i < static_cast<int>(sceneNames.size()); ++i)
                        {
                            bool isSelected = (currentScene == i);
                            if (ImGui::Selectable(sceneNames[i].c_str(), isSelected))
                            {
                                m_SceneManager->LoadScene(i);
                                m_SceneObjects = m_SceneManager->GetSceneObjects();
                                m_SceneManager->ApplyToRenderer(*m_Renderer);
                                m_SceneManager->ApplyToCamera(*m_Camera);
                                // 检测是否为海洋场景
                                m_OceanSceneActive = (sceneNames[i].find("Ocean") != std::string::npos);
                            }
                            if (isSelected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
                else
                {
                    ImGui::Text("PBR Demo");
                }
                ImGui::SameLine();
                
                // 下一个场景按钮
                if (ImGui::Button(">##NextScene", ImVec2(25, 30)))
                {
                    m_SceneManager->NextScene();
                    m_SceneObjects = m_SceneManager->GetSceneObjects();
                    m_SceneManager->ApplyToRenderer(*m_Renderer);
                    m_SceneManager->ApplyToCamera(*m_Camera);
                    // 检测是否为海洋场景
                    auto sceneName = m_SceneManager->GetCurrentSceneName();
                    m_OceanSceneActive = (sceneName.find("Ocean") != std::string::npos);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Next Scene (Page Down)");
            }
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
                ImGui::Text("View:");
                ImGui::SameLine();
                ImGui::RadioButton("Lit", &m_ViewMode, 0);
                ImGui::SameLine();
                ImGui::RadioButton("Wireframe", &m_ViewMode, 1);
                ImGui::SameLine();
                ImGui::RadioButton("Normals", &m_ViewMode, 2);
                
                // 更新渲染器的视图模式
                if (m_Renderer)
                {
                    m_Renderer->SetViewMode(m_ViewMode);
                }
                if (m_DeferredRenderer)
                {
                    m_DeferredRenderer->SetViewMode(m_ViewMode);
                }
                if (m_Ocean)
                {
                    m_Ocean->SetViewMode(m_ViewMode);
                }
                
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
                
                // 拖放目标 - 从资源浏览器拖入模型
                if (ImGui::BeginDragDropTarget())
                {
                    // 处理内置几何体拖放
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_ITEM"))
                    {
                        const char* assetType = static_cast<const char*>(payload->Data);
                        
                        if (strcmp(assetType, "BUILTIN_CUBE") == 0)
                        {
                            auto mesh = Mesh::CreateCube(*m_Device, 1.0f);
                            if (mesh)
                            {
                                SceneObject obj;
                                obj.mesh = mesh.get();
                                XMStoreFloat4x4(&obj.transform, XMMatrixTranslation(0, 0.5f, 0));
                                obj.color = { 0.8f, 0.4f, 0.2f, 1.0f };
                                m_Meshes.push_back(std::move(mesh));
                                m_SceneObjects.push_back(obj);
                            }
                        }
                        else if (strcmp(assetType, "BUILTIN_SPHERE") == 0)
                        {
                            auto mesh = Mesh::CreateSphere(*m_Device, 0.5f, 32, 16);
                            if (mesh)
                            {
                                SceneObject obj;
                                obj.mesh = mesh.get();
                                XMStoreFloat4x4(&obj.transform, XMMatrixTranslation(0, 0.5f, 0));
                                obj.color = { 0.2f, 0.6f, 0.9f, 1.0f };
                                m_Meshes.push_back(std::move(mesh));
                                m_SceneObjects.push_back(obj);
                            }
                        }
                        else if (strcmp(assetType, "BUILTIN_TORUS") == 0)
                        {
                            auto mesh = Mesh::CreateTorus(*m_Device, 0.6f, 0.2f, 32, 24);
                            if (mesh)
                            {
                                SceneObject obj;
                                obj.mesh = mesh.get();
                                XMStoreFloat4x4(&obj.transform, XMMatrixTranslation(0, 0.5f, 0));
                                obj.color = { 0.9f, 0.3f, 0.8f, 1.0f };
                                m_Meshes.push_back(std::move(mesh));
                                m_SceneObjects.push_back(obj);
                            }
                        }
                        else if (strcmp(assetType, "BUILTIN_PLANE") == 0)
                        {
                            auto mesh = Mesh::CreatePlane(*m_Device, 5.0f, 5.0f);
                            if (mesh)
                            {
                                SceneObject obj;
                                obj.mesh = mesh.get();
                                XMStoreFloat4x4(&obj.transform, XMMatrixIdentity());
                                obj.color = { 0.5f, 0.5f, 0.5f, 1.0f };
                                m_Meshes.push_back(std::move(mesh));
                                m_SceneObjects.push_back(obj);
                            }
                        }
                    }
                    
                    // 处理外部模型拖放
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MODEL_PATH"))
                    {
                        size_t modelIndex = *static_cast<const size_t*>(payload->Data);
                        if (modelIndex < m_AvailableModels.size())
                        {
                            LoadExternalModel(m_AvailableModels[modelIndex]);
                        }
                    }
                    
                    ImGui::EndDragDropTarget();
                }
                
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
                        snprintf(name, sizeof(name), "[M] Object_%zu", i);
                        
                        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                        if (m_SelectedObjectIndex == static_cast<int>(i))
                        {
                            flags |= ImGuiTreeNodeFlags_Selected;
                        }
                        
                        ImGui::TreeNodeEx(name, flags);
                        
                        // 点击选择对象
                        if (ImGui::IsItemClicked())
                        {
                            m_SelectedObjectIndex = static_cast<int>(i);
                        }
                        
                        // 右键菜单
                        if (ImGui::BeginPopupContextItem())
                        {
                            if (ImGui::MenuItem("Delete"))
                            {
                                m_SceneObjects.erase(m_SceneObjects.begin() + i);
                                if (m_SelectedObjectIndex >= static_cast<int>(m_SceneObjects.size()))
                                {
                                    m_SelectedObjectIndex = -1;
                                }
                                ImGui::EndPopup();
                                break;  // 退出循环因为容器已更改
                            }
                            if (ImGui::MenuItem("Duplicate"))
                            {
                                SceneObject copy = m_SceneObjects[i];
                                // 稍微偏移位置
                                XMMATRIX transform = XMLoadFloat4x4(&copy.transform);
                                transform = transform * XMMatrixTranslation(1.0f, 0.0f, 0.0f);
                                XMStoreFloat4x4(&copy.transform, transform);
                                m_SceneObjects.push_back(copy);
                            }
                            ImGui::EndPopup();
                        }
                    }
                    ImGui::TreePop();
                }
                
                // Grid
                if (ImGui::TreeNodeEx("[G] Ground Grid", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen))
                {
                }
                
                ImGui::TreePop();
            }
            
            // 场景信息
            ImGui::Separator();
            ImGui::Text("Objects: %zu", m_SceneObjects.size());
            if (m_SelectedObjectIndex >= 0)
            {
                ImGui::Text("Selected: Object_%d", m_SelectedObjectIndex);
            }
        }
        ImGui::End();
    }

    void SampleApp::RenderInspector()
    {
        if (!m_ShowInspector) return;
        
        if (ImGui::Begin("Inspector", &m_ShowInspector))
        {
            // 选中对象的详细信息
            if (m_SelectedObjectIndex >= 0 && m_SelectedObjectIndex < static_cast<int>(m_SceneObjects.size()))
            {
                SceneObject& obj = m_SceneObjects[m_SelectedObjectIndex];
                
                ImGui::Text("Object_%d", m_SelectedObjectIndex);
                ImGui::Separator();
                
                // Transform 编辑
                if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    XMMATRIX transform = XMLoadFloat4x4(&obj.transform);
                    
                    // 提取位置
                    XMFLOAT3 position;
                    position.x = obj.transform._41;
                    position.y = obj.transform._42;
                    position.z = obj.transform._43;
                    
                    if (ImGui::DragFloat3("Position", &position.x, 0.1f))
                    {
                        obj.transform._41 = position.x;
                        obj.transform._42 = position.y;
                        obj.transform._43 = position.z;
                    }
                    
                    // 简单缩放 (假设统一缩放)
                    static float scale = 1.0f;
                    if (ImGui::DragFloat("Scale", &scale, 0.01f, 0.01f, 10.0f))
                    {
                        // 重建变换矩阵
                        XMMATRIX scaleMatrix = XMMatrixScaling(scale, scale, scale);
                        XMMATRIX translationMatrix = XMMatrixTranslation(position.x, position.y, position.z);
                        XMStoreFloat4x4(&obj.transform, scaleMatrix * translationMatrix);
                    }
                }
                
                // Material 编辑
                if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::ColorEdit4("Base Color", &obj.color.x);
                    ImGui::SliderFloat("Metallic", &obj.metallic, 0.0f, 1.0f);
                    ImGui::SliderFloat("Roughness", &obj.roughness, 0.0f, 1.0f);
                    ImGui::SliderFloat("AO", &obj.ao, 0.0f, 1.0f);
                    
                    ImGui::Separator();
                    ImGui::Text("Emissive");
                    ImGui::ColorEdit3("Emissive Color", &obj.emissiveColor.x);
                    ImGui::SliderFloat("Emissive Intensity", &obj.emissiveIntensity, 0.0f, 10.0f);
                    
                    ImGui::Separator();
                    
                    // 材质预设
                    ImGui::Text("Presets:");
                    if (ImGui::Button("Metal"))
                    {
                        obj.metallic = 1.0f;
                        obj.roughness = 0.3f;
                        obj.color = { 0.9f, 0.9f, 0.9f, 1.0f };
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Plastic"))
                    {
                        obj.metallic = 0.0f;
                        obj.roughness = 0.4f;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Gold"))
                    {
                        obj.metallic = 1.0f;
                        obj.roughness = 0.2f;
                        obj.color = { 1.0f, 0.766f, 0.336f, 1.0f };
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Rubber"))
                    {
                        obj.metallic = 0.0f;
                        obj.roughness = 0.9f;
                        obj.color = { 0.1f, 0.1f, 0.1f, 1.0f };
                    }
                    
                    if (ImGui::Button("Copper"))
                    {
                        obj.metallic = 1.0f;
                        obj.roughness = 0.25f;
                        obj.color = { 0.955f, 0.637f, 0.538f, 1.0f };
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Chrome"))
                    {
                        obj.metallic = 1.0f;
                        obj.roughness = 0.1f;
                        obj.color = { 0.55f, 0.55f, 0.55f, 1.0f };
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Wood"))
                    {
                        obj.metallic = 0.0f;
                        obj.roughness = 0.6f;
                        obj.color = { 0.6f, 0.4f, 0.2f, 1.0f };
                    }
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Select an object in Hierarchy to edit");
            }
            
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
            // 路径导航栏
            ImGui::Text("Path:");
            ImGui::SameLine();
            if (ImGui::Button("Assets"))
            {
                m_CurrentAssetPath = "Assets";
            }
            
            // 显示当前路径下的目录
            std::filesystem::path currentPath(m_CurrentAssetPath);
            if (std::filesystem::exists(currentPath))
            {
                // 显示子目录按钮
                auto relativePath = std::filesystem::relative(currentPath, "Assets");
                if (!relativePath.empty() && relativePath.string() != ".")
                {
                    ImGui::SameLine();
                    ImGui::Text(">");
                    for (auto& part : relativePath)
                    {
                        ImGui::SameLine();
                        if (ImGui::Button(part.string().c_str()))
                        {
                            // 导航到该路径
                        }
                    }
                }
            }
            
            ImGui::Separator();
            
            // 返回上级目录按钮
            if (m_CurrentAssetPath != "Assets")
            {
                if (ImGui::Button(".. [Up]"))
                {
                    std::filesystem::path p(m_CurrentAssetPath);
                    m_CurrentAssetPath = p.parent_path().string();
                }
                ImGui::Separator();
            }
            
            // 刷新按钮
            if (ImGui::Button("Refresh"))
            {
                ScanAvailableModels();
            }
            ImGui::SameLine();
            ImGui::Text("Drag items to Viewport to add to scene");
            
            ImGui::Separator();
            
            // 内置几何体区域
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Built-in Geometry (Drag to Viewport):");
            float iconSize = 80.0f;
            
            // Cube - 可拖拽
            ImGui::Button("[Cube]", ImVec2(iconSize, iconSize / 2));
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                const char* type = "BUILTIN_CUBE";
                ImGui::SetDragDropPayload("ASSET_ITEM", type, strlen(type) + 1);
                ImGui::Text("Drag to add Cube");
                ImGui::EndDragDropSource();
            }
            if (ImGui::IsItemClicked())
            {
                auto cube = Mesh::CreateCube(*m_Device, 1.0f);
                if (cube)
                {
                    SceneObject obj;
                    obj.mesh = cube.get();
                    XMStoreFloat4x4(&obj.transform, XMMatrixTranslation(0, 0.5f, 0));
                    obj.color = { 0.8f, 0.4f, 0.2f, 1.0f };
                    m_Meshes.push_back(std::move(cube));
                    m_SceneObjects.push_back(obj);
                }
            }
            
            ImGui::SameLine();
            
            // Sphere - 可拖拽
            ImGui::Button("[Sphere]", ImVec2(iconSize, iconSize / 2));
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                const char* type = "BUILTIN_SPHERE";
                ImGui::SetDragDropPayload("ASSET_ITEM", type, strlen(type) + 1);
                ImGui::Text("Drag to add Sphere");
                ImGui::EndDragDropSource();
            }
            if (ImGui::IsItemClicked())
            {
                auto sphere = Mesh::CreateSphere(*m_Device, 0.5f, 32, 16);
                if (sphere)
                {
                    SceneObject obj;
                    obj.mesh = sphere.get();
                    XMStoreFloat4x4(&obj.transform, XMMatrixTranslation(0, 0.5f, 0));
                    obj.color = { 0.2f, 0.6f, 0.9f, 1.0f };
                    m_Meshes.push_back(std::move(sphere));
                    m_SceneObjects.push_back(obj);
                }
            }
            
            ImGui::SameLine();
            
            // Torus - 可拖拽
            ImGui::Button("[Torus]", ImVec2(iconSize, iconSize / 2));
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                const char* type = "BUILTIN_TORUS";
                ImGui::SetDragDropPayload("ASSET_ITEM", type, strlen(type) + 1);
                ImGui::Text("Drag to add Torus");
                ImGui::EndDragDropSource();
            }
            if (ImGui::IsItemClicked())
            {
                auto torus = Mesh::CreateTorus(*m_Device, 0.6f, 0.2f, 32, 24);
                if (torus)
                {
                    SceneObject obj;
                    obj.mesh = torus.get();
                    XMStoreFloat4x4(&obj.transform, XMMatrixTranslation(0, 0.5f, 0));
                    obj.color = { 0.9f, 0.3f, 0.8f, 1.0f };
                    m_Meshes.push_back(std::move(torus));
                    m_SceneObjects.push_back(obj);
                }
            }
            
            ImGui::SameLine();
            
            // Plane - 可拖拽
            ImGui::Button("[Plane]", ImVec2(iconSize, iconSize / 2));
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                const char* type = "BUILTIN_PLANE";
                ImGui::SetDragDropPayload("ASSET_ITEM", type, strlen(type) + 1);
                ImGui::Text("Drag to add Plane");
                ImGui::EndDragDropSource();
            }
            if (ImGui::IsItemClicked())
            {
                auto plane = Mesh::CreatePlane(*m_Device, 5.0f, 5.0f);
                if (plane)
                {
                    SceneObject obj;
                    obj.mesh = plane.get();
                    XMStoreFloat4x4(&obj.transform, XMMatrixIdentity());
                    obj.color = { 0.5f, 0.5f, 0.5f, 1.0f };
                    m_Meshes.push_back(std::move(plane));
                    m_SceneObjects.push_back(obj);
                }
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            
            // 外部模型列表
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "OBJ Models (Drag to Viewport):");
            
            if (m_AvailableModels.empty())
            {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No OBJ models found in Assets/Models/");
            }
            else
            {
                for (size_t i = 0; i < m_AvailableModels.size(); ++i)
                {
                    std::filesystem::path p(m_AvailableModels[i]);
                    std::string filename = p.filename().string();
                    
                    ImGui::PushID(static_cast<int>(i));
                    
                    // 模型可选择和拖拽
                    bool isSelected = (m_SelectedModelIndex == static_cast<int>(i));
                    ImGui::Selectable(filename.c_str(), isSelected, 0, ImVec2(0, 24));
                    
                    // 拖拽源
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                    {
                        // 存储模型路径索引
                        ImGui::SetDragDropPayload("MODEL_PATH", &i, sizeof(size_t));
                        ImGui::Text("Drag: %s", filename.c_str());
                        ImGui::EndDragDropSource();
                    }
                    
                    if (ImGui::IsItemClicked())
                    {
                        m_SelectedModelIndex = static_cast<int>(i);
                    }
                    
                    // 双击加载
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                    {
                        LoadExternalModel(m_AvailableModels[i]);
                    }
                    
                    ImGui::SameLine(ImGui::GetWindowWidth() - 60);
                    if (ImGui::SmallButton("Load"))
                    {
                        LoadExternalModel(m_AvailableModels[i]);
                    }
                    
                    ImGui::PopID();
                }
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Scene Objects: %zu", m_SceneObjects.size());
            
            if (ImGui::Button("Clear All Objects"))
            {
                m_SceneObjects.clear();
                m_SelectedObjectIndex = -1;
            }
        }
        ImGui::End();
    }

    void SampleApp::RenderRenderSettings()
    {
        if (!m_ShowRenderSettings) return;
        
        if (ImGui::Begin("Render Settings", &m_ShowRenderSettings))
        {
            // 天空和大气设置
            if (ImGui::CollapsingHeader("Sky & Atmosphere", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (m_SkyRenderer)
                {
                    auto& settings = m_SkyRenderer->GetSettings();
                    
                    ImGui::Checkbox("Enable Sky", &settings.EnableSky);
                    ImGui::Checkbox("Enable Atmosphere", &settings.EnableAtmosphere);
                    ImGui::Checkbox("Enable Clouds", &settings.EnableClouds);
                    
                    ImGui::Separator();
                    ImGui::Text("Sun Settings");
                    
                    // 时间控制
                    float timeOfDay = m_SkyRenderer->GetTimeOfDay();
                    if (ImGui::SliderFloat("Time of Day", &timeOfDay, 0.0f, 24.0f, "%.1f h"))
                    {
                        m_SkyRenderer->SetTimeOfDay(timeOfDay);
                    }
                    
                    bool autoTime = m_SkyRenderer->GetAutoTimeProgress();
                    if (ImGui::Checkbox("Auto Time Progress", &autoTime))
                    {
                        m_SkyRenderer->SetAutoTimeProgress(autoTime);
                    }
                    
                    // 太阳方向控制
                    float azimuth = m_SkyRenderer->GetSunAzimuth();
                    float elevation = m_SkyRenderer->GetSunElevation();
                    if (ImGui::SliderFloat("Sun Azimuth", &azimuth, 0.0f, 360.0f, "%.1f deg"))
                    {
                        m_SkyRenderer->SetSunAzimuth(azimuth);
                    }
                    if (ImGui::SliderFloat("Sun Elevation", &elevation, -20.0f, 90.0f, "%.1f deg"))
                    {
                        m_SkyRenderer->SetSunElevation(elevation);
                    }
                    
                    ImGui::DragFloat("Sun Intensity", &settings.SunIntensity, 0.1f, 0.0f, 20.0f);
                    ImGui::ColorEdit3("Sun Color", &settings.SunColor.x);
                    
                    ImGui::Separator();
                    ImGui::Text("Atmosphere");
                    ImGui::DragFloat("Atmosphere Scale", &settings.AtmosphereScale, 0.1f, 0.1f, 5.0f);
                    ImGui::ColorEdit3("Ground Color", &settings.GroundColor.x);
                }
                else
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Sky Renderer not available");
                }
            }
            
            // 体积云设置
            if (ImGui::CollapsingHeader("Volumetric Clouds", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (m_SkyRenderer)
                {
                    auto& settings = m_SkyRenderer->GetSettings();
                    
                    if (!settings.EnableClouds)
                    {
                        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Clouds are disabled");
                    }
                    else
                    {
                        ImGui::SliderFloat("Cloud Coverage", &settings.CloudCoverage, 0.0f, 1.0f);
                        ImGui::SliderFloat("Cloud Density", &settings.CloudDensity, 0.1f, 3.0f);
                        ImGui::DragFloat("Cloud Height", &settings.CloudHeight, 100.0f, 500.0f, 10000.0f, "%.0f m");
                    }
                }
            }
            
            // 渲染器设置
            if (ImGui::CollapsingHeader("Renderer", ImGuiTreeNodeFlags_DefaultOpen))
            {
                // Shader 刷新按钮
                if (ImGui::Button("Refresh Shaders (F5)"))
                {
                    RecompileAllShaders();
                }
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Recompile all shaders");
                ImGui::Separator();
                
                // 渲染管线切换
                const char* pipelineNames[] = { "Forward", "Deferred" };
                int pipelineIndex = static_cast<int>(m_CurrentPipeline);
                
                bool deferredAvailable = m_DeferredRenderer != nullptr;
                
                if (!deferredAvailable)
                {
                    ImGui::BeginDisabled();
                }
                
                if (ImGui::Combo("Render Pipeline", &pipelineIndex, pipelineNames, IM_ARRAYSIZE(pipelineNames)))
                {
                    RenderPipeline newPipeline = static_cast<RenderPipeline>(pipelineIndex);
                    if (newPipeline != m_CurrentPipeline)
                    {
                        // 等待GPU完成所有工作以避免资源竞争
                        m_GraphicsQueue->WaitForIdle();
                        
                        m_CurrentPipeline = newPipeline;
                        // 重建RenderGraph以反映新的渲染管线
                        SetupRenderGraph();
                        // 更新编辑器组件的RenderGraph引用
                        m_NodeEditor->SetRenderGraph(m_RenderGraph.get());
                        m_PropertyPanel->SetRenderGraph(m_RenderGraph.get());
                        SEA_CORE_INFO("Switched to {} pipeline", pipelineNames[pipelineIndex]);
                    }
                }
                
                if (!deferredAvailable)
                {
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "(Deferred not available)");
                }
                
                // Deferred 渲染设置
                if (m_CurrentPipeline == RenderPipeline::Deferred && m_DeferredRenderer)
                {
                    ImGui::Separator();
                    ImGui::Text("Deferred Settings");
                    ImGui::Indent();
                    
                    auto& deferredSettings = m_DeferredRenderer->GetSettings();
                    ImGui::Checkbox("Debug G-Buffer", &deferredSettings.DebugGBuffer);
                    
                    if (deferredSettings.DebugGBuffer)
                    {
                        const char* gbufferNames[] = { "Albedo", "Normal", "Position", "Emissive" };
                        int debugIndex = static_cast<int>(deferredSettings.DebugGBufferIndex);
                        if (ImGui::Combo("G-Buffer View", &debugIndex, gbufferNames, IM_ARRAYSIZE(gbufferNames)))
                        {
                            deferredSettings.DebugGBufferIndex = static_cast<u32>(debugIndex);
                        }
                    }
                    
                    ImGui::SliderFloat("Ambient Intensity", &deferredSettings.AmbientIntensity, 0.0f, 1.0f);
                    ImGui::Checkbox("SSAO (未实现)", &deferredSettings.UseSSAO);
                    
                    ImGui::Unindent();
                }
                
                ImGui::Separator();
                
                if (m_Renderer)
                {
                    bool usePBR = m_Renderer->GetUsePBR();
                    if (ImGui::Checkbox("Use PBR Pipeline", &usePBR))
                    {
                        m_Renderer->SetUsePBR(usePBR);
                    }
                }
            }
            
            // 灯光设置
            if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (m_Renderer)
                {
                    static XMFLOAT3 lightDir = { -0.5f, -1.0f, 0.5f };
                    static XMFLOAT3 lightColor = { 1.0f, 0.98f, 0.95f };
                    static f32 lightIntensity = 2.0f;
                    static XMFLOAT3 ambientColor = { 0.15f, 0.18f, 0.22f };
                    
                    ImGui::Text("Directional Light");
                    
                    // 简化的方向控制 - 用角度
                    static float lightYaw = -30.0f;
                    static float lightPitch = -60.0f;
                    bool dirChanged = false;
                    dirChanged |= ImGui::SliderFloat("Light Yaw", &lightYaw, -180.0f, 180.0f, "%.1f deg");
                    dirChanged |= ImGui::SliderFloat("Light Pitch", &lightPitch, -90.0f, 0.0f, "%.1f deg");
                    
                    if (dirChanged)
                    {
                        float yawRad = lightYaw * 3.14159f / 180.0f;
                        float pitchRad = lightPitch * 3.14159f / 180.0f;
                        lightDir.x = cosf(pitchRad) * sinf(yawRad);
                        lightDir.y = sinf(pitchRad);
                        lightDir.z = cosf(pitchRad) * cosf(yawRad);
                        m_Renderer->SetLightDirection(lightDir);
                        // 同步到 Deferred 渲染器
                        if (m_DeferredRenderer)
                            m_DeferredRenderer->SetLightDirection(lightDir);
                    }
                    
                    if (ImGui::ColorEdit3("Light Color", &lightColor.x))
                    {
                        m_Renderer->SetLightColor(lightColor);
                        if (m_DeferredRenderer)
                            m_DeferredRenderer->SetLightColor(lightColor);
                    }
                    if (ImGui::DragFloat("Light Intensity", &lightIntensity, 0.1f, 0.0f, 20.0f))
                    {
                        m_Renderer->SetLightIntensity(lightIntensity);
                        if (m_DeferredRenderer)
                            m_DeferredRenderer->SetLightIntensity(lightIntensity);
                    }
                    
                    ImGui::Separator();
                    ImGui::Text("Ambient");
                    if (ImGui::ColorEdit3("Ambient Color", &ambientColor.x))
                    {
                        m_Renderer->SetAmbientColor(ambientColor);
                        if (m_DeferredRenderer)
                            m_DeferredRenderer->SetAmbientColor(ambientColor);
                    }
                }
            }
            
            // 相机设置
            if (ImGui::CollapsingHeader("Camera"))
            {
                if (m_Camera)
                {
                    ImGui::Text("Position: %.2f, %.2f, %.2f", 
                        m_Camera->GetPosition().x, m_Camera->GetPosition().y, m_Camera->GetPosition().z);
                    
                    float moveSpeed = m_Camera->GetMoveSpeed();
                    if (ImGui::SliderFloat("Move Speed", &moveSpeed, 1.0f, 100.0f, "%.1f"))
                    {
                        m_Camera->SetMoveSpeed(moveSpeed);
                    }
                    
                    ImGui::Text("FOV: %.1f", m_Camera->GetFOV());
                    ImGui::Text("Near/Far: %.2f / %.2f", m_Camera->GetNearZ(), m_Camera->GetFarZ());
                }
            }
            
            // 后处理设置 - Unreal风格Bloom
            if (ImGui::CollapsingHeader("Post Processing"))
            {
                // Bloom设置 (Unreal风格) - 现在已集成
                if (m_BloomRenderer)
                {
                    auto& bloomSettings = m_BloomRenderer->GetSettings();
                    
                    ImGui::Checkbox("Bloom (Unreal Style)", &bloomSettings.Enabled);
                    if (bloomSettings.Enabled)
                    {
                        ImGui::Indent();
                        ImGui::SliderFloat("Intensity", &bloomSettings.Intensity, 0.0f, 5.0f, "%.3f");
                        ImGui::SliderFloat("Threshold", &bloomSettings.Threshold, 0.0f, 5.0f, "%.2f");
                        ImGui::SliderFloat("Radius", &bloomSettings.Radius, 0.5f, 4.0f, "%.2f");
                        
                        float tint[3] = { bloomSettings.TintR, bloomSettings.TintG, bloomSettings.TintB };
                        if (ImGui::ColorEdit3("Tint", tint))
                        {
                            bloomSettings.TintR = tint[0];
                            bloomSettings.TintG = tint[1];
                            bloomSettings.TintB = tint[2];
                        }
                        
                        if (ImGui::TreeNode("Per-Mip Weights"))
                        {
                            ImGui::SliderFloat("1/2 Res", &bloomSettings.Mip1Weight, 0.0f, 1.0f);
                            ImGui::SliderFloat("1/4 Res", &bloomSettings.Mip2Weight, 0.0f, 1.0f);
                            ImGui::SliderFloat("1/8 Res", &bloomSettings.Mip3Weight, 0.0f, 1.0f);
                            ImGui::SliderFloat("1/16 Res", &bloomSettings.Mip4Weight, 0.0f, 1.0f);
                            ImGui::SliderFloat("1/32 Res", &bloomSettings.Mip5Weight, 0.0f, 1.0f);
                            ImGui::SliderFloat("1/64 Res", &bloomSettings.Mip6Weight, 0.0f, 1.0f);
                            
                            if (ImGui::Button("Reset to Default"))
                            {
                                bloomSettings.Mip1Weight = 0.266f;
                                bloomSettings.Mip2Weight = 0.232f;
                                bloomSettings.Mip3Weight = 0.246f;
                                bloomSettings.Mip4Weight = 0.384f;
                                bloomSettings.Mip5Weight = 0.426f;
                                bloomSettings.Mip6Weight = 0.060f;
                            }
                            ImGui::TreePop();
                        }
                        ImGui::Unindent();
                    }
                }
                else
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "BloomRenderer not available");
                }
                
                ImGui::Separator();
                
                // HDR 管线开关
                if (ImGui::Checkbox("HDR Pipeline", &m_UseHDRPipeline))
                {
                    // 重建渲染目标以启用/禁用 HDR
                    CreateSceneRenderTarget(m_ViewportWidth, m_ViewportHeight);
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("Enable HDR rendering for Bloom and Tonemapping effects");
                    ImGui::EndTooltip();
                }
                
                ImGui::Separator();
                
                // Tonemapping设置
                if (m_TonemapRenderer)
                {
                    auto& tonemapSettings = m_TonemapRenderer->GetSettings();
                    const char* tonemapOps[] = { "ACES (Unreal)", "Reinhard", "Uncharted 2", "GT (Gran Turismo)", "None" };
                    
                    ImGui::Checkbox("Tone Mapping", &tonemapSettings.Enabled);
                    if (tonemapSettings.Enabled)
                    {
                        ImGui::Indent();
                        ImGui::Combo("Operator", &tonemapSettings.Operator, tonemapOps, 5);
                        ImGui::SliderFloat("Exposure", &tonemapSettings.Exposure, 0.1f, 5.0f, "%.2f");
                        ImGui::SliderFloat("Gamma", &tonemapSettings.Gamma, 1.0f, 3.0f, "%.2f");
                        ImGui::Unindent();
                    }
                }
                else
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "TonemapRenderer not available");
                }
                
                ImGui::Separator();
                
                // Color Grading设置 (TODO: 未来实现)
                static bool colorGradingEnabled = false;
                static float contrast = 1.0f;
                static float saturation = 1.0f;
                
                // Color Grading UI
                ImGui::Checkbox("Color Grading", &colorGradingEnabled);
                if (colorGradingEnabled)
                {
                    ImGui::Indent();
                    ImGui::SliderFloat("Contrast", &contrast, 0.5f, 2.0f);
                    ImGui::SliderFloat("Saturation", &saturation, 0.0f, 2.0f);
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "(Not implemented yet)");
                    ImGui::Unindent();
                }
                
                ImGui::Separator();
                
                // Shader 调试设置 (用于 RenderDoc)
                bool shaderDebug = ShaderCompiler::IsGlobalDebugEnabled();
                if (ImGui::Checkbox("Shader Debug Mode", &shaderDebug))
                {
                    ShaderCompiler::SetGlobalDebugEnabled(shaderDebug);
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("Enable shader debugging for RenderDoc.");
                    ImGui::Text("Shaders will be compiled with debug info");
                    ImGui::Text("and without optimization.");
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), 
                        "Note: Requires shader recompilation to take effect.");
                    ImGui::EndTooltip();
                }
            }
            
            // Ocean 渲染设置 (AAA 级)
            if (m_Ocean && ImGui::CollapsingHeader("Ocean Rendering (AAA)"))
            {
                auto& params = m_Ocean->GetParams();
                
                // QuadTree LOD 设置
                bool useQuadTree = m_Ocean->GetUseQuadTree();
                if (ImGui::Checkbox("QuadTree LOD", &useQuadTree))
                {
                    m_Ocean->SetUseQuadTree(useQuadTree);
                }
                ImGui::SameLine();
                if (useQuadTree && m_Ocean->GetQuadTree())
                {
                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), 
                        "(%u nodes)", m_Ocean->GetQuadTree()->GetLeafCount());
                }
                else
                {
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(Simple mesh)");
                }
                
                ImGui::Separator();
                
                // Wave Parameters
                ImGui::Text("Wave Simulation");
                ImGui::SliderFloat("Choppiness", &params.choppiness, 0.5f, 3.0f, "%.2f");
                ImGui::SliderFloat("Wind Speed", &params.windSpeed, 5.0f, 50.0f, "%.1f m/s");
                
                float windDir[2] = { params.windDirection.x, params.windDirection.y };
                if (ImGui::SliderFloat2("Wind Direction", windDir, -1.0f, 1.0f))
                {
                    params.windDirection = { windDir[0], windDir[1] };
                }
                
                ImGui::Separator();
                
                // Foam / Whitecaps
                ImGui::Text("Foam & Whitecaps");
                ImGui::SliderFloat("Foam Intensity", &params.foamIntensity, 0.0f, 3.0f, "%.2f");
                ImGui::SliderFloat("Foam Scale", &params.foamScale, 0.1f, 2.0f, "%.2f");
                ImGui::SliderFloat("Whitecap Threshold", &params.whitecapThreshold, 0.1f, 0.8f, "%.2f");
                
                ImGui::Separator();
                
                // Lighting
                ImGui::Text("Lighting & Atmosphere");
                ImGui::SliderFloat("Sun Intensity", &params.sunIntensity, 0.5f, 5.0f, "%.2f");
                ImGui::SliderFloat("Sun Disk Size", &params.sunDiskSize, 0.001f, 0.05f, "%.3f");
                ImGui::SliderFloat("Fog Density", &params.fogDensity, 0.0001f, 0.01f, "%.4f");
                
                ImGui::Separator();
                
                // Colors (advanced)
                if (ImGui::TreeNode("Ocean Colors"))
                {
                    // Get current colors as arrays
                    float deepColor[4], skyColor[4], scatterColor[4];
                    
                    // Deep water color - currently accessed via Ocean internal state
                    // For now, use default values as placeholders
                    deepColor[0] = 0.0f; deepColor[1] = 0.03f; deepColor[2] = 0.08f; deepColor[3] = 1.0f;
                    skyColor[0] = 0.5f; skyColor[1] = 0.7f; skyColor[2] = 0.9f; skyColor[3] = 1.0f;
                    scatterColor[0] = 0.0f; scatterColor[1] = 0.15f; scatterColor[2] = 0.2f; scatterColor[3] = 1.0f;
                    
                    if (ImGui::ColorEdit3("Deep Water", deepColor))
                    {
                        m_Ocean->SetOceanColor({ deepColor[0], deepColor[1], deepColor[2], 1.0f });
                    }
                    if (ImGui::ColorEdit3("Sky/Horizon", skyColor))
                    {
                        m_Ocean->SetSkyColor({ skyColor[0], skyColor[1], skyColor[2], 1.0f });
                    }
                    if (ImGui::ColorEdit3("SSS Scatter", scatterColor))
                    {
                        m_Ocean->SetScatterColor({ scatterColor[0], scatterColor[1], scatterColor[2], 1.0f });
                    }
                    
                    ImGui::TreePop();
                }
                
                // Presets
                if (ImGui::TreeNode("Presets"))
                {
                    if (ImGui::Button("Calm Sea"))
                    {
                        params.choppiness = 0.8f;
                        params.windSpeed = 8.0f;
                        params.foamIntensity = 0.3f;
                        params.whitecapThreshold = 0.6f;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Moderate"))
                    {
                        params.choppiness = 1.5f;
                        params.windSpeed = 20.0f;
                        params.foamIntensity = 1.0f;
                        params.whitecapThreshold = 0.3f;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Stormy"))
                    {
                        params.choppiness = 2.5f;
                        params.windSpeed = 40.0f;
                        params.foamIntensity = 2.5f;
                        params.whitecapThreshold = 0.15f;
                    }
                    ImGui::TreePop();
                }
            }
        }
        ImGui::End();
    }

    void SampleApp::OnUpdate(f32 deltaTime)
    {
        Input::Update();
        m_TotalTime += deltaTime;

        // F12触发RenderDoc截帧
        if (Input::IsKeyPressed(KeyCode::F12) && !m_PendingCapture)
        {
            m_PendingCapture = true;
            RenderDocCapture::TriggerCapture();
            SEA_CORE_INFO("RenderDoc capture triggered");
        }
        
        // F5 刷新着色器
        static bool f5WasPressed = false;
        if (Input::IsKeyDown(KeyCode::F5))
        {
            if (!f5WasPressed)
            {
                f5WasPressed = true;
                RecompileAllShaders();
            }
        }
        else
        {
            f5WasPressed = false;
        }

        // 场景切换快捷键
        if (m_SceneManager)
        {
            // Page Down: 下一个场景
            if (Input::IsKeyPressed(KeyCode::PageDown))
            {
                m_SceneManager->NextScene();
                m_SceneObjects = m_SceneManager->GetSceneObjects();
                m_SceneManager->ApplyToRenderer(*m_Renderer);
                m_SceneManager->ApplyToCamera(*m_Camera);
                SEA_CORE_INFO("Switched to scene: {}", m_SceneManager->GetCurrentSceneName());
            }
            // Page Up: 上一个场景
            if (Input::IsKeyPressed(KeyCode::PageUp))
            {
                m_SceneManager->PreviousScene();
                m_SceneObjects = m_SceneManager->GetSceneObjects();
                m_SceneManager->ApplyToRenderer(*m_Renderer);
                m_SceneManager->ApplyToCamera(*m_Camera);
                SEA_CORE_INFO("Switched to scene: {}", m_SceneManager->GetCurrentSceneName());
            }
        }

        // 更新相机
        UpdateCamera(deltaTime);

        // 更新天空渲染器
        if (m_SkyRenderer)
        {
            m_SkyRenderer->Update(deltaTime);
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
        RenderRenderSettings();
        
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

        // ========== 1. 渲染 3D 场景到 HDR RT ==========
        // 注意：所有渲染器的 PSO 使用 R16G16B16A16_FLOAT 格式，所以必须渲染到 HDR RT
        if (m_SceneRenderTarget && m_ViewportWidth > 0 && m_ViewportHeight > 0)
        {
            // 始终使用 HDR RT 进行场景渲染（因为 PSO 格式要求）
            // 如果 HDR RT 不存在，降级到 LDR（可能会有格式不匹配问题）
            bool hasHDRTarget = m_HDRRenderTarget != nullptr;
            
            ID3D12Resource* sceneRenderTargetResource = hasHDRTarget 
                ? m_HDRRenderTarget->GetResource() 
                : m_SceneRenderTarget->GetResource();
            D3D12_CPU_DESCRIPTOR_HANDLE sceneRtv = hasHDRTarget 
                ? m_HDRRTVHeap->GetCPUHandle(0) 
                : m_SceneRTVHeap->GetCPUHandle(0);
            
            // 转换场景渲染目标到 RenderTarget 状态
            cmdList->TransitionBarrier(
                sceneRenderTargetResource,
                ResourceState::Common,
                ResourceState::RenderTarget
            );
            cmdList->TransitionBarrier(
                m_DepthBuffer->GetResource(),
                ResourceState::Common,
                ResourceState::DepthWrite
            );
            cmdList->FlushBarriers();

            // 清除场景渲染目标（海洋场景使用天空色）
            f32 sceneClearColor[4];
            if (m_OceanSceneActive)
            {
                sceneClearColor[0] = 0.4f;  // 天空色
                sceneClearColor[1] = 0.6f;
                sceneClearColor[2] = 0.9f;
                sceneClearColor[3] = 1.0f;
            }
            else
            {
                sceneClearColor[0] = 0.1f;
                sceneClearColor[1] = 0.1f;
                sceneClearColor[2] = 0.15f;
                sceneClearColor[3] = 1.0f;
            }
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

            // 根据渲染管线类型选择渲染路径
            if (m_CurrentPipeline == RenderPipeline::Deferred && m_DeferredRenderer && !m_OceanSceneActive)
            {
                // ========== Deferred 渲染路径 ==========
                // 1. G-Buffer Pass
                m_DeferredRenderer->BeginGBufferPass(*cmdList, *m_Camera, m_TotalTime);
                
                // 渲染所有场景对象到 G-Buffer
                for (const auto& obj : m_SceneObjects)
                {
                    m_DeferredRenderer->RenderObjectToGBuffer(*cmdList, obj);
                }
                
                m_DeferredRenderer->EndGBufferPass(*cmdList);
                
                // 2. Lighting Pass - 输出到场景渲染目标
                m_DeferredRenderer->LightingPass(*cmdList, sceneRtv, 
                    sceneRenderTargetResource, m_ViewportWidth, m_ViewportHeight);
                
                // 3. 渲染天空（在 deferred 之后叠加，使用深度测试）
                if (m_SkyRenderer && m_SkyRenderer->GetSettings().EnableSky)
                {
                    cmdList->GetCommandList()->OMSetRenderTargets(1, &sceneRtv, FALSE, &dsv);
                    m_SkyRenderer->Render(*cmdList, *m_Camera);
                }
                
                // 4. 渲染网格（前向渲染叠加）
                if (m_GridMesh)
                {
                    cmdList->GetCommandList()->OMSetRenderTargets(1, &sceneRtv, FALSE, &dsv);
                    m_Renderer->BeginFrame(*m_Camera, m_TotalTime);
                    m_Renderer->RenderGrid(*cmdList, *m_GridMesh);
                }
            }
            else
            {
                // ========== Forward 渲染路径 ==========
                // 开始3D渲染
                m_Renderer->BeginFrame(*m_Camera, m_TotalTime);

                // 首先渲染天空（如果启用）
                if (m_SkyRenderer && m_SkyRenderer->GetSettings().EnableSky)
                {
                    m_SkyRenderer->Render(*cmdList, *m_Camera);
                }

                // 根据场景类型渲染
                if (m_OceanSceneActive && m_Ocean)
                {
                    // 渲染海洋场景
                    m_Ocean->Update(0.016f, *cmdList);  // 假设60fps
                    m_Ocean->Render(*cmdList, *m_Camera);
                }
                else
                {
                    // 渲染普通场景
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
                }
            }

            // 转换场景渲染目标到 ShaderResource 状态
            cmdList->TransitionBarrier(
                sceneRenderTargetResource,
                ResourceState::RenderTarget,
                ResourceState::Common  // Common 可以作为 SRV 读取
            );
            cmdList->TransitionBarrier(
                m_DepthBuffer->GetResource(),
                ResourceState::DepthWrite,
                ResourceState::Common
            );
            cmdList->FlushBarriers();
            
            // ========== 2. 后处理流程（Bloom + Tonemapping） ==========
            // 启用后处理条件：有 HDR RT 且 Tonemapping 启用
            bool runPostProcess = hasHDRTarget && m_TonemapRenderer && m_TonemapRenderer->GetSettings().Enabled;
            
            if (runPostProcess)
            {
                // 2.1 Bloom Pass
                bool bloomEnabled = m_BloomRenderer && m_BloomRenderer->GetSettings().Enabled;
                if (bloomEnabled)
                {
                    // 执行 Bloom：从 HDR 场景提取高亮并生成模糊结果
                    // BloomRenderer 不再做合成，只生成 bloom 纹理
                    m_BloomRenderer->Render(
                        *cmdList,
                        m_HDRSceneSRV,                      // HDR 场景作为输入
                        {},                                 // 输出 RTV 不再使用
                        nullptr,                            // 输出资源不再使用
                        m_ViewportWidth, m_ViewportHeight
                    );
                    
                    // 更新 PostProcessSRVHeap 的 slot 1 指向 Bloom 结果
                    if (auto* bloomResource = m_BloomRenderer->GetBloomResultResource())
                    {
                        D3D12_SHADER_RESOURCE_VIEW_DESC bloomSrvDesc = {};
                        bloomSrvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
                        bloomSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                        bloomSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                        bloomSrvDesc.Texture2D.MipLevels = 1;
                        
                        m_Device->GetDevice()->CreateShaderResourceView(
                            bloomResource,
                            &bloomSrvDesc,
                            m_PostProcessSRVHeap->GetCPUHandle(1)
                        );
                    }
                    
                    // 更新 Tonemap 设置中的 Bloom 参数
                    auto& tonemapSettings = m_TonemapRenderer->GetSettings();
                    tonemapSettings.BloomEnabled = true;
                    tonemapSettings.BloomIntensity = m_BloomRenderer->GetSettings().Intensity;
                    tonemapSettings.BloomTintR = m_BloomRenderer->GetSettings().TintR;
                    tonemapSettings.BloomTintG = m_BloomRenderer->GetSettings().TintG;
                    tonemapSettings.BloomTintB = m_BloomRenderer->GetSettings().TintB;
                }
                else
                {
                    // Bloom 禁用时
                    m_TonemapRenderer->GetSettings().BloomEnabled = false;
                }
                
                // 2.2 Tonemapping Pass: HDR -> LDR
                ID3D12DescriptorHeap* heaps[] = { m_PostProcessSRVHeap->GetHeap() };
                cmdList->GetCommandList()->SetDescriptorHeaps(1, heaps);
                
                // 执行 Tonemapping
                m_TonemapRenderer->Render(
                    *cmdList,
                    m_HDRSceneSRV,                      // HDR 场景 SRV
                    m_BloomResultSRV,                   // Bloom 结果 SRV (如果有)
                    m_SceneRTVHeap->GetCPUHandle(0),   // 输出到 LDR RT
                    m_SceneRenderTarget->GetResource(),
                    m_ViewportWidth, m_ViewportHeight
                );
            }
            else if (hasHDRTarget)
            {
                // HDR 管线启用但后处理禁用 - 直接复制 HDR 到 LDR（会丢失 HDR 信息）
                // 这里简单处理：通过 copy resource 复制
                cmdList->TransitionBarrier(
                    m_SceneRenderTarget->GetResource(),
                    ResourceState::Common,
                    ResourceState::CopyDest
                );
                cmdList->TransitionBarrier(
                    m_HDRRenderTarget->GetResource(),
                    ResourceState::Common,
                    ResourceState::CopySource
                );
                cmdList->FlushBarriers();
                
                // 注意：由于格式不同，不能直接 copy。使用默认 Tonemap 处理
                // 这里降级到不进行后处理，显示 HDR 信息时可能会过曝
                // 实际应用中应该总是使用后处理
                
                cmdList->TransitionBarrier(
                    m_SceneRenderTarget->GetResource(),
                    ResourceState::CopyDest,
                    ResourceState::Common
                );
                cmdList->TransitionBarrier(
                    m_HDRRenderTarget->GetResource(),
                    ResourceState::CopySource,
                    ResourceState::Common
                );
                cmdList->FlushBarriers();
                
                // 强制启用 Tonemapping 来处理 HDR -> LDR 转换
                if (m_TonemapRenderer)
                {
                    ID3D12DescriptorHeap* heaps[] = { m_PostProcessSRVHeap->GetHeap() };
                    cmdList->GetCommandList()->SetDescriptorHeaps(1, heaps);
                    
                    auto& settings = m_TonemapRenderer->GetSettings();
                    bool originalEnabled = settings.Enabled;
                    settings.Enabled = true;
                    settings.BloomEnabled = false;
                    
                    m_TonemapRenderer->Render(
                        *cmdList,
                        m_HDRSceneSRV,
                        m_BloomResultSRV,
                        m_SceneRTVHeap->GetCPUHandle(0),
                        m_SceneRenderTarget->GetResource(),
                        m_ViewportWidth, m_ViewportHeight
                    );
                    
                    settings.Enabled = originalEnabled;
                }
            }
        }

        // ========== 3. 渲染 ImGui 到 SwapChain ==========
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
        
        // 如果正在截帧，结束截帧并打开 RenderDoc
        if (m_PendingCapture)
        {
            m_PendingCapture = false;
            RenderDocCapture::EndCaptureAndOpen();
        }
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
        // 清理现有RenderGraph
        m_RenderGraph = MakeScope<RenderGraph>();
        
        // 初始化RenderGraph
        m_RenderGraph->Initialize(m_Device.get());

        // 创建公共资源节点
        u32 depthId = m_RenderGraph->CreateResource("Depth Buffer", ResourceNodeType::Texture2D);
        if (auto* depth = m_RenderGraph->GetResource(depthId))
        {
            depth->SetDimensions(1920, 1080);
            depth->SetFormat(Format::D32_FLOAT);
            depth->SetPosition(50, 100);
        }

        u32 sceneColorId = m_RenderGraph->CreateResource("Scene Color", ResourceNodeType::Texture2D);
        if (auto* sceneColor = m_RenderGraph->GetResource(sceneColorId))
        {
            sceneColor->SetDimensions(1920, 1080);
            sceneColor->SetFormat(Format::R8G8B8A8_UNORM);
            sceneColor->SetPosition(350, 100);
        }

        u32 backBufferId = m_RenderGraph->CreateResource("Back Buffer", ResourceNodeType::Texture2D);
        if (auto* backBuffer = m_RenderGraph->GetResource(backBufferId))
        {
            backBuffer->SetDimensions(1920, 1080);
            backBuffer->SetFormat(Format::R8G8B8A8_UNORM);
            backBuffer->SetPosition(650, 100);
        }

        if (m_CurrentPipeline == RenderPipeline::Forward)
        {
            // ========== Forward 渲染管线 ==========
            // Forward PBR Pass - 渲染场景对象 (Grid + PBR Objects 或 Ocean)
            u32 forwardId = m_RenderGraph->AddPass("Forward PBR", PassType::Graphics);
            if (auto* forward = m_RenderGraph->GetPass(forwardId))
            {
                forward->AddOutput("Scene Color");
                forward->AddOutput("Depth");
                forward->SetOutput(0, sceneColorId);
                forward->SetOutput(1, depthId);
                forward->SetPosition(200, 100);
            }

            // ImGui Pass - 渲染 UI 到后缓冲
            u32 imguiId = m_RenderGraph->AddPass("ImGui", PassType::Graphics);
            if (auto* imgui = m_RenderGraph->GetPass(imguiId))
            {
                imgui->AddInput("Scene Color");
                imgui->AddOutput("Back Buffer");
                imgui->SetInput(0, sceneColorId);
                imgui->SetOutput(0, backBufferId);
                imgui->SetPosition(500, 100);
            }

            SEA_CORE_INFO("Render graph created with Forward pipeline");
        }
        else if (m_CurrentPipeline == RenderPipeline::Deferred)
        {
            // ========== Deferred 渲染管线 ==========
            // G-Buffer资源
            u32 albedoMetallicId = m_RenderGraph->CreateResource("GBuffer Albedo+Metallic", ResourceNodeType::Texture2D);
            if (auto* rt = m_RenderGraph->GetResource(albedoMetallicId))
            {
                rt->SetDimensions(1920, 1080);
                rt->SetFormat(Format::R8G8B8A8_UNORM);
                rt->SetPosition(50, 250);
            }

            u32 normalRoughnessId = m_RenderGraph->CreateResource("GBuffer Normal+Roughness", ResourceNodeType::Texture2D);
            if (auto* rt = m_RenderGraph->GetResource(normalRoughnessId))
            {
                rt->SetDimensions(1920, 1080);
                rt->SetFormat(Format::R16G16B16A16_FLOAT);
                rt->SetPosition(50, 350);
            }

            u32 positionAoId = m_RenderGraph->CreateResource("GBuffer Position+AO", ResourceNodeType::Texture2D);
            if (auto* rt = m_RenderGraph->GetResource(positionAoId))
            {
                rt->SetDimensions(1920, 1080);
                rt->SetFormat(Format::R32G32B32A32_FLOAT);
                rt->SetPosition(50, 450);
            }

            u32 emissiveId = m_RenderGraph->CreateResource("GBuffer Emissive", ResourceNodeType::Texture2D);
            if (auto* rt = m_RenderGraph->GetResource(emissiveId))
            {
                rt->SetDimensions(1920, 1080);
                rt->SetFormat(Format::R16G16B16A16_FLOAT);
                rt->SetPosition(50, 550);
            }

            // G-Buffer Pass
            u32 gbufferId = m_RenderGraph->AddPass("G-Buffer", PassType::Graphics);
            if (auto* gbuffer = m_RenderGraph->GetPass(gbufferId))
            {
                gbuffer->AddOutput("Albedo+Metallic");
                gbuffer->AddOutput("Normal+Roughness");
                gbuffer->AddOutput("Position+AO");
                gbuffer->AddOutput("Emissive");
                gbuffer->AddOutput("Depth");
                gbuffer->SetOutput(0, albedoMetallicId);
                gbuffer->SetOutput(1, normalRoughnessId);
                gbuffer->SetOutput(2, positionAoId);
                gbuffer->SetOutput(3, emissiveId);
                gbuffer->SetOutput(4, depthId);
                gbuffer->SetPosition(200, 100);
            }

            // Deferred Lighting Pass
            u32 lightingId = m_RenderGraph->AddPass("Deferred Lighting", PassType::Graphics);
            if (auto* lighting = m_RenderGraph->GetPass(lightingId))
            {
                lighting->AddInput("Albedo+Metallic");
                lighting->AddInput("Normal+Roughness");
                lighting->AddInput("Position+AO");
                lighting->AddInput("Emissive");
                lighting->AddOutput("Scene Color");
                lighting->SetInput(0, albedoMetallicId);
                lighting->SetInput(1, normalRoughnessId);
                lighting->SetInput(2, positionAoId);
                lighting->SetInput(3, emissiveId);
                lighting->SetOutput(0, sceneColorId);
                lighting->SetPosition(400, 100);
            }

            // ImGui Pass
            u32 imguiId = m_RenderGraph->AddPass("ImGui", PassType::Graphics);
            if (auto* imgui = m_RenderGraph->GetPass(imguiId))
            {
                imgui->AddInput("Scene Color");
                imgui->AddOutput("Back Buffer");
                imgui->SetInput(0, sceneColorId);
                imgui->SetOutput(0, backBufferId);
                imgui->SetPosition(600, 100);
            }

            SEA_CORE_INFO("Render graph created with Deferred pipeline");
        }

        m_RenderGraph->Compile();
    }

    void SampleApp::CreateResources()
    {
        // 预留用于创建GPU资源...
    }

    void SampleApp::RecompileAllShaders()
    {
        SEA_CORE_INFO("=== Recompiling all shaders ===");
        
        // 等待GPU完成所有工作以避免资源竞争
        if (m_GraphicsQueue)
        {
            m_GraphicsQueue->WaitForIdle();
        }
        
        bool success = true;
        
        // 重新编译 SimpleRenderer 着色器
        if (m_Renderer)
        {
            if (!m_Renderer->RecompileShaders())
            {
                SEA_CORE_ERROR("Failed to recompile SimpleRenderer shaders");
                success = false;
            }
        }
        
        // 重新编译 DeferredRenderer 着色器
        if (m_DeferredRenderer)
        {
            if (!m_DeferredRenderer->RecompileShaders())
            {
                SEA_CORE_ERROR("Failed to recompile DeferredRenderer shaders");
                success = false;
            }
        }
        
        // 重新编译 Ocean 着色器
        if (m_Ocean)
        {
            if (!m_Ocean->RecompileShaders())
            {
                SEA_CORE_ERROR("Failed to recompile Ocean shaders");
                success = false;
            }
        }
        
        // TODO: 添加其他渲染器的着色器重新编译
        // - SkyRenderer
        // - BloomRenderer
        // - TonemapRenderer
        
        if (success)
        {
            SEA_CORE_INFO("=== All shaders recompiled successfully ===");
        }
        else
        {
            SEA_CORE_WARN("=== Some shaders failed to recompile ===");
        }
    }
}
