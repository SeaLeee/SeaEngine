#pragma once

#include "Core/Types.h"
#include "Graphics/Graphics.h"
#include "Graphics/Material.h"
#include "Scene/Scene.h"
#include <DirectXMath.h>
#include <vector>

namespace Sea
{
    using namespace DirectX;

    struct SceneObject
    {
        Mesh* mesh = nullptr;
        XMFLOAT4X4 transform;
        XMFLOAT4 color = { 1, 1, 1, 1 };
        f32 metallic = 0.0f;
        f32 roughness = 0.5f;
        f32 ao = 1.0f;                              // 环境遮蔽
        XMFLOAT3 emissiveColor = { 0, 0, 0 };       // 自发光颜色
        f32 emissiveIntensity = 0.0f;               // 自发光强度
        Ref<PBRMaterial> material = nullptr;         // PBR 材质 (可选)
    };

    struct FrameConstants
    {
        XMFLOAT4X4 ViewProjection;
        XMFLOAT4X4 View;
        XMFLOAT4X4 Projection;
        XMFLOAT3 CameraPosition;
        f32 Time;
        XMFLOAT3 LightDirection;
        f32 _Padding1;
        XMFLOAT3 LightColor;
        f32 LightIntensity;
        XMFLOAT3 AmbientColor;
        f32 _Padding2;
    };

    // PBR Object Constants (匹配 PBR.hlsl)
    struct ObjectConstants
    {
        XMFLOAT4X4 World;
        XMFLOAT4X4 WorldInvTranspose;
        XMFLOAT4 BaseColor;
        f32 Metallic;
        f32 Roughness;
        f32 AO;
        f32 EmissiveIntensity;
        XMFLOAT3 EmissiveColor;
        f32 NormalScale;
        u32 TextureFlags;       // 贴图标记
        XMFLOAT3 _Padding;
    };

    class SimpleRenderer : public NonCopyable
    {
    public:
        SimpleRenderer(Device& device);
        ~SimpleRenderer();

        bool Initialize();
        void Shutdown();
        
        // 重新编译着色器
        bool RecompileShaders();

        void BeginFrame(Camera& camera, f32 time);
        void RenderObject(CommandList& cmdList, const SceneObject& obj);
        void RenderGrid(CommandList& cmdList, Mesh& gridMesh);

        // PBR 设置
        void SetUsePBR(bool usePBR) { m_UsePBR = usePBR; }
        bool GetUsePBR() const { return m_UsePBR; }

        // 视图模式 (0=Lit, 1=Wireframe, 2=Normals)
        void SetViewMode(int mode) { m_ViewMode = mode; }
        int GetViewMode() const { return m_ViewMode; }

        void SetLightDirection(const XMFLOAT3& dir) { m_LightDirection = dir; }
        void SetLightColor(const XMFLOAT3& color) { m_LightColor = color; }
        void SetLightIntensity(f32 intensity) { m_LightIntensity = intensity; }
        void SetAmbientColor(const XMFLOAT3& color) { m_AmbientColor = color; }

    private:
        bool CreateRootSignature();
        bool CreatePipelineStates();
        bool CreateConstantBuffers();

    private:
        Device& m_Device;

        Scope<RootSignature> m_RootSignature;
        Ref<PipelineState> m_BasicPSO;
        Ref<PipelineState> m_PBRPSO;           // PBR 管线
        Ref<PipelineState> m_WireframePSO;     // Wireframe 管线
        Ref<PipelineState> m_NormalsPSO;       // Normals 可视化管线
        Ref<PipelineState> m_GridPSO;

        Scope<Buffer> m_FrameConstantBuffer;
        Scope<Buffer> m_ObjectConstantBuffer;
        
        // 动态常量缓冲区分配
        static constexpr u32 MAX_OBJECTS_PER_FRAME = 256;
        static constexpr u32 OBJECT_CB_ALIGNMENT = 256;  // D3D12 常量缓冲区对齐要求
        u32 m_CurrentObjectIndex = 0;

        FrameConstants m_FrameConstants;
        
        XMFLOAT3 m_LightDirection = { -0.5f, -1.0f, 0.5f };
        XMFLOAT3 m_LightColor = { 1.0f, 0.98f, 0.95f };
        f32 m_LightIntensity = 2.0f;      // 增加光照强度
        XMFLOAT3 m_AmbientColor = { 0.15f, 0.18f, 0.22f };  // 稍微增加环境光
        
        bool m_UsePBR = true;             // 默认使用 PBR
        int m_ViewMode = 0;               // 0=Lit, 1=Wireframe, 2=Normals
    };
}
