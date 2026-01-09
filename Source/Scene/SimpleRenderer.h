#pragma once

#include "Core/Types.h"
#include "Graphics/Graphics.h"
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

    struct ObjectConstants
    {
        XMFLOAT4X4 World;
        XMFLOAT4X4 WorldInvTranspose;
        XMFLOAT4 BaseColor;
        f32 Metallic;
        f32 Roughness;
        XMFLOAT2 _Padding;
    };

    class SimpleRenderer : public NonCopyable
    {
    public:
        SimpleRenderer(Device& device);
        ~SimpleRenderer();

        bool Initialize();
        void Shutdown();

        void BeginFrame(Camera& camera, f32 time);
        void RenderObject(CommandList& cmdList, const SceneObject& obj);
        void RenderGrid(CommandList& cmdList, Mesh& gridMesh);

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
        Ref<PipelineState> m_GridPSO;

        Scope<Buffer> m_FrameConstantBuffer;
        Scope<Buffer> m_ObjectConstantBuffer;

        FrameConstants m_FrameConstants;
        
        XMFLOAT3 m_LightDirection = { -0.5f, -1.0f, 0.5f };
        XMFLOAT3 m_LightColor = { 1.0f, 0.98f, 0.95f };
        f32 m_LightIntensity = 1.5f;
        XMFLOAT3 m_AmbientColor = { 0.1f, 0.12f, 0.15f };
    };
}
