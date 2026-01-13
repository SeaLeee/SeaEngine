#pragma once
#include "Core/Types.h"
#include "Graphics/Graphics.h"
#include "Scene/Scene.h"
#include "Scene/Camera.h"
#include <DirectXMath.h>
#include <array>

namespace Sea
{
    using namespace DirectX;

    // G-Buffer 布局
    struct GBufferLayout
    {
        static constexpr u32 ALBEDO_INDEX = 0;      // RGB: Albedo, A: Metallic
        static constexpr u32 NORMAL_INDEX = 1;      // RGB: World Normal (encoded), A: Roughness
        static constexpr u32 POSITION_INDEX = 2;    // RGB: World Position, A: AO
        static constexpr u32 EMISSIVE_INDEX = 3;    // RGB: Emissive, A: unused
        static constexpr u32 COUNT = 4;
    };

    // Deferred 渲染器设置
    struct DeferredSettings
    {
        bool DebugGBuffer = false;
        u32 DebugGBufferIndex = 0;  // 0=Albedo, 1=Normal, 2=Position, 3=Emissive
        bool UseSSAO = false;
        float AmbientIntensity = 0.3f;
    };

    // G-Buffer Pass 常量
    struct GBufferConstants
    {
        XMFLOAT4X4 ViewProjection;
        XMFLOAT4X4 View;
        XMFLOAT4X4 Projection;
        XMFLOAT3 CameraPosition;
        float Time;
    };

    // G-Buffer 物体常量
    struct GBufferObjectConstants
    {
        XMFLOAT4X4 World;
        XMFLOAT4X4 WorldInvTranspose;
        XMFLOAT4 BaseColor;
        float Metallic;
        float Roughness;
        float AO;
        float EmissiveIntensity;
        XMFLOAT3 EmissiveColor;
        float _Padding;
    };

    // Lighting Pass 常量
    struct LightingConstants
    {
        XMFLOAT4X4 InvViewProjection;
        XMFLOAT3 CameraPosition;
        float Time;
        XMFLOAT3 LightDirection;
        float LightIntensity;
        XMFLOAT3 LightColor;
        float AmbientIntensity;
        XMFLOAT3 AmbientColor;
        float _Padding;
    };

    class DeferredRenderer : public NonCopyable
    {
    public:
        DeferredRenderer(Device& device);
        ~DeferredRenderer();

        bool Initialize(u32 width, u32 height);
        void Shutdown();
        void Resize(u32 width, u32 height);

        // 渲染流程
        void BeginGBufferPass(CommandList& cmdList, Camera& camera, float time);
        void RenderObjectToGBuffer(CommandList& cmdList, const struct SceneObject& obj);
        void EndGBufferPass(CommandList& cmdList);

        void LightingPass(CommandList& cmdList, 
                          D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
                          ID3D12Resource* outputResource,
                          u32 outputWidth, u32 outputHeight);

        // 设置
        DeferredSettings& GetSettings() { return m_Settings; }
        const DeferredSettings& GetSettings() const { return m_Settings; }
        
        void SetLightDirection(const XMFLOAT3& dir) { m_LightDirection = dir; }
        void SetLightColor(const XMFLOAT3& color) { m_LightColor = color; }
        void SetLightIntensity(float intensity) { m_LightIntensity = intensity; }
        void SetAmbientColor(const XMFLOAT3& color) { m_AmbientColor = color; }
        
        // 视图模式 (0=Lit, 1=Wireframe, 2=Normals)
        void SetViewMode(int mode) { m_ViewMode = mode; }
        int GetViewMode() const { return m_ViewMode; }
        
        // 重新编译着色器
        bool RecompileShaders();

        // 获取 G-Buffer 用于调试
        ID3D12Resource* GetGBufferResource(u32 index) const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetGBufferSRV(u32 index) const;

    private:
        bool CreateGBufferResources(u32 width, u32 height);
        bool CreatePipelines();
        bool CreateConstantBuffers();
        void ReleaseGBufferResources();

        Device& m_Device;
        DeferredSettings m_Settings;

        u32 m_Width = 0;
        u32 m_Height = 0;

        // G-Buffer 资源
        struct GBufferRT
        {
            ComPtr<ID3D12Resource> Resource;
            D3D12_CPU_DESCRIPTOR_HANDLE RTV = {};
            D3D12_GPU_DESCRIPTOR_HANDLE SRV = {};
        };
        std::array<GBufferRT, GBufferLayout::COUNT> m_GBuffer;
        
        // 深度缓冲
        ComPtr<ID3D12Resource> m_DepthBuffer;
        D3D12_CPU_DESCRIPTOR_HANDLE m_DSV = {};
        D3D12_GPU_DESCRIPTOR_HANDLE m_DepthSRV = {};

        // 描述符堆
        Scope<DescriptorHeap> m_RTVHeap;
        Scope<DescriptorHeap> m_DSVHeap;
        Scope<DescriptorHeap> m_SRVHeap;

        // PSO
        Scope<RootSignature> m_GBufferRootSignature;
        Scope<RootSignature> m_LightingRootSignature;
        Ref<PipelineState> m_GBufferPSO;
        Ref<PipelineState> m_GBufferWireframePSO;  // Wireframe 模式
        Ref<PipelineState> m_LightingPSO;

        // 常量缓冲区
        Scope<Buffer> m_GBufferConstantBuffer;
        Scope<Buffer> m_GBufferObjectConstantBuffer;
        Scope<Buffer> m_LightingConstantBuffer;
        
        // 物体常量缓冲区动态分配
        static constexpr u32 MAX_OBJECTS_PER_FRAME = 256;
        static constexpr u32 OBJECT_CB_ALIGNMENT = 256;
        u32 m_CurrentObjectIndex = 0;

        // 光照参数
        XMFLOAT3 m_LightDirection = { -0.5f, -1.0f, 0.5f };
        XMFLOAT3 m_LightColor = { 1.0f, 0.98f, 0.95f };
        float m_LightIntensity = 2.0f;
        XMFLOAT3 m_AmbientColor = { 0.15f, 0.18f, 0.22f };
        
        // 视图模式
        int m_ViewMode = 0;  // 0=Lit, 1=Wireframe, 2=Normals

        // 当前帧数据
        GBufferConstants m_FrameConstants;
    };
}
