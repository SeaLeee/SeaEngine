#pragma once
#include "Core/Application.h"
#include "Graphics/Graphics.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/PassTemplate.h"
#include "Shader/Shader.h"
#include "Editor/EditorModule.h"
#include "Scene/Scene.h"
#include "Scene/SceneRenderer.h"
#include "Scene/SceneManager.h"
#include "Scene/Ocean.h"
#include "Scene/SkyRenderer.h"
#include "Scene/BloomRenderer.h"
#include "Scene/TonemapRenderer.h"
#include "Scene/DeferredRenderer.h"

namespace Sea
{
    // 渲染管线类型
    enum class RenderPipeline
    {
        Forward,
        Deferred
    };
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
        void CreateScene();
        void UpdateCamera(f32 deltaTime);
        bool CreateDepthBuffer();

    private:
        Scope<Device> m_Device;
        Scope<SwapChain> m_SwapChain;
        Scope<CommandQueue> m_GraphicsQueue;
        std::vector<Scope<CommandList>> m_CommandLists;
        Scope<ImGuiRenderer> m_ImGuiRenderer;
        Scope<RenderGraph> m_RenderGraph;
        Scope<NodeEditor> m_NodeEditor;
        Scope<PropertyPanel> m_PropertyPanel;
        Scope<ShaderEditor> m_ShaderEditor;
        Scope<ShaderLibrary> m_ShaderLibrary;

        // 3D 场景
        Scope<SimpleRenderer> m_Renderer;
        Scope<Camera> m_Camera;
        Scope<SceneManager> m_SceneManager;
        Scope<Ocean> m_Ocean;
        Scope<SkyRenderer> m_SkyRenderer;
        Scope<BloomRenderer> m_BloomRenderer;
        Scope<TonemapRenderer> m_TonemapRenderer;
        Scope<DeferredRenderer> m_DeferredRenderer;
        RenderPipeline m_CurrentPipeline = RenderPipeline::Forward;
        bool m_OceanSceneActive = false;
        Scope<Mesh> m_GridMesh;
        std::vector<Scope<Mesh>> m_Meshes;
        std::vector<SceneObject> m_SceneObjects;
        
        // 场景选择
        int m_SelectedSceneIndex = 0;
        bool m_ShowSceneSelector = false;
        
        // 外部模型
        std::vector<std::string> m_AvailableModels;
        int m_SelectedModelIndex = -1;
        bool LoadExternalModel(const std::string& filepath);
        void ScanAvailableModels();
        
        // 资源浏览器
        std::string m_CurrentAssetPath = "Assets";
        std::vector<std::string> m_AssetPathHistory;
        
        // 对象选择与材质编辑
        int m_SelectedObjectIndex = -1;
        bool m_ShowDetailPanel = true;
        void RenderDetailPanel();
        
        // 视图模式 (0=Lit, 1=Wireframe, 2=Normals)
        int m_ViewMode = 0;
        
        // 深度缓冲
        Scope<Texture> m_DepthBuffer;
        Scope<DescriptorHeap> m_DSVHeap;
        
        // 离屏渲染目标（用于Viewport显示）
        Scope<Texture> m_SceneRenderTarget;          // LDR 最终输出（用于 ImGui 显示）
        Scope<Texture> m_HDRRenderTarget;            // HDR 场景渲染目标
        Scope<DescriptorHeap> m_SceneRTVHeap;        // LDR RTV 堆
        Scope<DescriptorHeap> m_HDRRTVHeap;          // HDR RTV 堆
        Scope<DescriptorHeap> m_PostProcessSRVHeap; // 后处理 SRV 堆
        D3D12_GPU_DESCRIPTOR_HANDLE m_SceneTextureHandle = { 0 };  // ImGui 使用的纹理句柄
        D3D12_GPU_DESCRIPTOR_HANDLE m_HDRSceneSRV = { 0 };         // HDR 场景 SRV
        D3D12_GPU_DESCRIPTOR_HANDLE m_BloomResultSRV = { 0 };      // Bloom 结果 SRV
        u32 m_ViewportWidth = 1280;
        u32 m_ViewportHeight = 720;
        bool m_UseHDRPipeline = true;  // 是否使用 HDR 后处理管线

        // 相机控制
        bool m_CameraControl = false;
        std::pair<i32, i32> m_LastMousePos = { 0, 0 };
        f32 m_TotalTime = 0.0f;

        // 帧同步
        std::vector<u64> m_FrameFenceValues;
        u32 m_FrameIndex = 0;
        
        // RenderDoc 截帧状态
        bool m_PendingCapture = false;

        // 编辑器状态
        bool m_FirstFrame = true;
        bool m_ShowDemoWindow = false;
        bool m_ShowViewport = true;
        bool m_ShowHierarchy = true;
        bool m_ShowInspector = true;
        bool m_ShowConsole = true;
        bool m_ShowAssetBrowser = true;
        bool m_ShowRenderSettings = true;
        
        // 编辑器方法
        void SetupEditorLayout();
        void RenderMainMenuBar();
        void RenderToolbar();
        void RenderStatusBar();
        void RenderViewport();
        void RenderHierarchy();
        void RenderInspector();
        void RenderConsole();
        void RenderAssetBrowser();
        void RenderRenderSettings();
        bool CreateSceneRenderTarget(u32 width, u32 height);
        void RenderSceneToTexture();
        void RecompileAllShaders();  // 重新编译所有着色器
    };
}
