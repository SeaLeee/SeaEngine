#pragma once
#include "Core/Types.h"
#include "Scene/SimpleRenderer.h"
#include "Scene/Camera.h"
#include "Scene/Mesh.h"
#include "Graphics/Graphics.h"
#include <vector>
#include <memory>

namespace Sea
{
    class Device;
    class CommandList;
    class Texture;
    class DescriptorHeap;

    // 场景渲染器配置
    struct SceneRendererConfig
    {
        u32 width = 1920;
        u32 height = 1080;
        bool enableDepth = true;
        Format colorFormat = Format::R8G8B8A8_UNORM;
        Format depthFormat = Format::D32_FLOAT;
    };

    // 场景渲染器 - 独立的场景渲染模块
    // 可以被 RenderGraph 的 Pass 调用，也可以独立使用
    class SceneRenderer : public NonCopyable
    {
    public:
        SceneRenderer(Device& device);
        ~SceneRenderer();

        bool Initialize(const SceneRendererConfig& config);
        void Shutdown();

        // 调整渲染目标尺寸
        bool Resize(u32 width, u32 height);

        // 渲染场景到内部渲染目标
        void BeginFrame(Camera& camera, f32 time);
        void RenderScene(CommandList& cmdList, 
                        const std::vector<SceneObject>& objects,
                        Mesh* gridMesh = nullptr);
        void EndFrame();

        // 渲染场景到指定的渲染目标
        void RenderSceneTo(CommandList& cmdList,
                          Camera& camera,
                          f32 time,
                          const std::vector<SceneObject>& objects,
                          D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                          D3D12_CPU_DESCRIPTOR_HANDLE dsv,
                          u32 width, u32 height,
                          Mesh* gridMesh = nullptr);

        // 获取渲染结果
        Texture* GetColorTarget() const { return m_ColorTarget.get(); }
        Texture* GetDepthTarget() const { return m_DepthTarget.get(); }
        D3D12_GPU_DESCRIPTOR_HANDLE GetColorSRV() const { return m_ColorSRV; }

        // 光照设置
        void SetLightDirection(const XMFLOAT3& dir);
        void SetLightColor(const XMFLOAT3& color);
        void SetLightIntensity(f32 intensity);
        void SetAmbientColor(const XMFLOAT3& color);

        // 获取尺寸
        u32 GetWidth() const { return m_Config.width; }
        u32 GetHeight() const { return m_Config.height; }

    private:
        bool CreateRenderTargets();

    private:
        Device& m_Device;
        SceneRendererConfig m_Config;

        Scope<SimpleRenderer> m_Renderer;

        // 渲染目标
        Scope<Texture> m_ColorTarget;
        Scope<Texture> m_DepthTarget;
        Scope<DescriptorHeap> m_RTVHeap;
        Scope<DescriptorHeap> m_DSVHeap;
        Scope<DescriptorHeap> m_SRVHeap;
        D3D12_GPU_DESCRIPTOR_HANDLE m_ColorSRV = {};
    };
}
