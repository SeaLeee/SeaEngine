#include "Scene/SceneRenderer.h"
#include "Graphics/Device.h"
#include "Graphics/Texture.h"
#include "Graphics/DescriptorHeap.h"
#include "Graphics/CommandList.h"
#include "Core/Log.h"

namespace Sea
{
    SceneRenderer::SceneRenderer(Device& device)
        : m_Device(device)
    {
    }

    SceneRenderer::~SceneRenderer()
    {
        Shutdown();
    }

    bool SceneRenderer::Initialize(const SceneRendererConfig& config)
    {
        m_Config = config;

        // 创建底层渲染器
        m_Renderer = MakeScope<SimpleRenderer>(m_Device);
        if (!m_Renderer->Initialize())
        {
            SEA_CORE_ERROR("SceneRenderer: Failed to initialize SimpleRenderer");
            return false;
        }

        // 创建渲染目标
        if (!CreateRenderTargets())
        {
            SEA_CORE_ERROR("SceneRenderer: Failed to create render targets");
            return false;
        }

        SEA_CORE_INFO("SceneRenderer initialized: {}x{}", m_Config.width, m_Config.height);
        return true;
    }

    void SceneRenderer::Shutdown()
    {
        m_ColorTarget.reset();
        m_DepthTarget.reset();
        m_RTVHeap.reset();
        m_DSVHeap.reset();
        m_SRVHeap.reset();
        m_Renderer.reset();
    }

    bool SceneRenderer::CreateRenderTargets()
    {
        // RTV Heap
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.NumDescriptors = 1;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        m_RTVHeap = MakeScope<DescriptorHeap>(m_Device, rtvHeapDesc);
        if (!m_RTVHeap->Initialize())
            return false;

        // DSV Heap
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        m_DSVHeap = MakeScope<DescriptorHeap>(m_Device, dsvHeapDesc);
        if (!m_DSVHeap->Initialize())
            return false;

        // SRV Heap (shader visible)
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.NumDescriptors = 1;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_SRVHeap = MakeScope<DescriptorHeap>(m_Device, srvHeapDesc);
        if (!m_SRVHeap->Initialize())
            return false;

        // Color target
        TextureDesc colorDesc{};
        colorDesc.width = m_Config.width;
        colorDesc.height = m_Config.height;
        colorDesc.format = m_Config.colorFormat;
        colorDesc.usage = TextureUsage::RenderTarget | TextureUsage::ShaderResource;

        m_ColorTarget = MakeScope<Texture>(m_Device, colorDesc);
        if (!m_ColorTarget->Initialize())
            return false;

        // Create RTV
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = static_cast<DXGI_FORMAT>(m_Config.colorFormat);
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        m_Device.GetDevice()->CreateRenderTargetView(
            m_ColorTarget->GetResource(),
            &rtvDesc,
            m_RTVHeap->GetCPUHandle(0));

        // Create SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = static_cast<DXGI_FORMAT>(m_Config.colorFormat);
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        m_Device.GetDevice()->CreateShaderResourceView(
            m_ColorTarget->GetResource(),
            &srvDesc,
            m_SRVHeap->GetCPUHandle(0));
        m_ColorSRV = m_SRVHeap->GetGPUHandle(0);

        // Depth target
        if (m_Config.enableDepth)
        {
            TextureDesc depthDesc{};
            depthDesc.width = m_Config.width;
            depthDesc.height = m_Config.height;
            depthDesc.format = m_Config.depthFormat;
            depthDesc.usage = TextureUsage::DepthStencil;

            m_DepthTarget = MakeScope<Texture>(m_Device, depthDesc);
            if (!m_DepthTarget->Initialize())
                return false;

            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = static_cast<DXGI_FORMAT>(m_Config.depthFormat);
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            m_Device.GetDevice()->CreateDepthStencilView(
                m_DepthTarget->GetResource(),
                &dsvDesc,
                m_DSVHeap->GetCPUHandle(0));
        }

        return true;
    }

    bool SceneRenderer::Resize(u32 width, u32 height)
    {
        if (width == m_Config.width && height == m_Config.height)
            return true;

        m_Config.width = width;
        m_Config.height = height;

        m_ColorTarget.reset();
        m_DepthTarget.reset();

        if (!CreateRenderTargets())
        {
            SEA_CORE_ERROR("SceneRenderer: Failed to resize to {}x{}", width, height);
            return false;
        }

        SEA_CORE_INFO("SceneRenderer resized to {}x{}", width, height);
        return true;
    }

    void SceneRenderer::BeginFrame(Camera& camera, f32 time)
    {
        m_Renderer->BeginFrame(camera, time);
    }

    void SceneRenderer::EndFrame()
    {
        // 可以在这里做后处理准备
    }

    void SceneRenderer::RenderScene(CommandList& cmdList,
                                    const std::vector<SceneObject>& objects,
                                    Mesh* gridMesh)
    {
        auto* d3dCmdList = cmdList.GetCommandList();

        // 获取 RTV 和 DSV
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_RTVHeap->GetCPUHandle(0);
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_Config.enableDepth ? m_DSVHeap->GetCPUHandle(0) : D3D12_CPU_DESCRIPTOR_HANDLE{};

        // 设置渲染目标
        d3dCmdList->OMSetRenderTargets(1, &rtv, FALSE, m_Config.enableDepth ? &dsv : nullptr);

        // 设置视口和裁剪矩形
        D3D12_VIEWPORT viewport = { 0, 0, static_cast<float>(m_Config.width), static_cast<float>(m_Config.height), 0.0f, 1.0f };
        D3D12_RECT scissor = { 0, 0, static_cast<LONG>(m_Config.width), static_cast<LONG>(m_Config.height) };
        d3dCmdList->RSSetViewports(1, &viewport);
        d3dCmdList->RSSetScissorRects(1, &scissor);

        // 清除
        const float clearColor[] = { 0.1f, 0.1f, 0.15f, 1.0f };
        d3dCmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
        if (m_Config.enableDepth)
        {
            d3dCmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        }

        // 渲染网格
        if (gridMesh)
        {
            m_Renderer->RenderGrid(cmdList, *gridMesh);
        }

        // 渲染场景对象
        for (const auto& obj : objects)
        {
            m_Renderer->RenderObject(cmdList, obj);
        }
    }

    void SceneRenderer::RenderSceneTo(CommandList& cmdList,
                                      Camera& camera,
                                      f32 time,
                                      const std::vector<SceneObject>& objects,
                                      D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                                      D3D12_CPU_DESCRIPTOR_HANDLE dsv,
                                      u32 width, u32 height,
                                      Mesh* gridMesh)
    {
        m_Renderer->BeginFrame(camera, time);

        auto* d3dCmdList = cmdList.GetCommandList();

        // 设置渲染目标
        d3dCmdList->OMSetRenderTargets(1, &rtv, FALSE, dsv.ptr != 0 ? &dsv : nullptr);

        // 设置视口和裁剪矩形
        D3D12_VIEWPORT viewport = { 0, 0, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
        D3D12_RECT scissor = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
        d3dCmdList->RSSetViewports(1, &viewport);
        d3dCmdList->RSSetScissorRects(1, &scissor);

        // 清除
        const float clearColor[] = { 0.1f, 0.1f, 0.15f, 1.0f };
        d3dCmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
        if (dsv.ptr != 0)
        {
            d3dCmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        }

        // 渲染网格
        if (gridMesh)
        {
            m_Renderer->RenderGrid(cmdList, *gridMesh);
        }

        // 渲染场景对象
        for (const auto& obj : objects)
        {
            m_Renderer->RenderObject(cmdList, obj);
        }
    }

    void SceneRenderer::SetLightDirection(const XMFLOAT3& dir)
    {
        m_Renderer->SetLightDirection(dir);
    }

    void SceneRenderer::SetLightColor(const XMFLOAT3& color)
    {
        m_Renderer->SetLightColor(color);
    }

    void SceneRenderer::SetLightIntensity(f32 intensity)
    {
        m_Renderer->SetLightIntensity(intensity);
    }

    void SceneRenderer::SetAmbientColor(const XMFLOAT3& color)
    {
        m_Renderer->SetAmbientColor(color);
    }
}
