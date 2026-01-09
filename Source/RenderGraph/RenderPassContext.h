#pragma once
#include "Core/Types.h"
#include "Graphics/GraphicsTypes.h"
#include <vector>

namespace Sea
{
    class CommandList;
    class Device;

    // RenderPass执行上下文 - 在Pass执行时提供资源访问
    class RenderPassContext
    {
    public:
        RenderPassContext() = default;
        ~RenderPassContext() = default;

        // 设置设备
        void SetDevice(Device* device) { m_Device = device; }
        Device* GetDevice() const { return m_Device; }

        // 设置输入资源
        void SetInputResources(const std::vector<ID3D12Resource*>& inputs) { m_Inputs = inputs; }
        ID3D12Resource* GetInputResource(u32 index) const;

        // 设置输出资源
        void SetOutputResources(const std::vector<ID3D12Resource*>& outputs) { m_Outputs = outputs; }
        ID3D12Resource* GetOutputResource(u32 index) const;

        // 设置RTV
        void SetOutputRTVs(const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& rtvs) { m_RTVs = rtvs; }
        D3D12_CPU_DESCRIPTOR_HANDLE GetOutputRTV(u32 index) const;

        // 设置DSV
        void SetDSV(D3D12_CPU_DESCRIPTOR_HANDLE dsv) { m_DSV = dsv; }
        D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const { return m_DSV; }

        // 设置SRV
        void SetInputSRVs(const std::vector<D3D12_GPU_DESCRIPTOR_HANDLE>& srvs) { m_SRVs = srvs; }
        D3D12_GPU_DESCRIPTOR_HANDLE GetInputSRV(u32 index) const;

        // 视口和裁剪矩形
        void SetViewport(const D3D12_VIEWPORT& viewport) { m_Viewport = viewport; }
        const D3D12_VIEWPORT& GetViewport() const { return m_Viewport; }

        void SetScissorRect(const D3D12_RECT& rect) { m_ScissorRect = rect; }
        const D3D12_RECT& GetScissorRect() const { return m_ScissorRect; }

        // 渲染目标尺寸
        void SetRenderTargetSize(u32 width, u32 height) { m_Width = width; m_Height = height; }
        u32 GetRenderTargetWidth() const { return m_Width; }
        u32 GetRenderTargetHeight() const { return m_Height; }

    private:
        Device* m_Device = nullptr;
        std::vector<ID3D12Resource*> m_Inputs;
        std::vector<ID3D12Resource*> m_Outputs;
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_RTVs;
        D3D12_CPU_DESCRIPTOR_HANDLE m_DSV = {};
        std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> m_SRVs;
        D3D12_VIEWPORT m_Viewport = {};
        D3D12_RECT m_ScissorRect = {};
        u32 m_Width = 0;
        u32 m_Height = 0;
    };
}
