#pragma once
#include "Graphics/GraphicsTypes.h"
#include "Core/Types.h"

namespace Sea
{
    class Device;

    class RenderTarget : public NonCopyable
    {
    public:
        RenderTarget(Device& device, u32 width, u32 height, Format format, bool hasDepth = true);
        ~RenderTarget();

        bool Initialize();
        void Resize(u32 width, u32 height);

        ID3D12Resource* GetColorResource() const { return m_ColorResource.Get(); }
        ID3D12Resource* GetDepthResource() const { return m_DepthResource.Get(); }
        D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const { return m_RTV; }
        D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const { return m_DSV; }

    private:
        Device& m_Device;
        u32 m_Width, m_Height;
        Format m_ColorFormat;
        bool m_HasDepth;
        ComPtr<ID3D12Resource> m_ColorResource, m_DepthResource;
        ComPtr<ID3D12DescriptorHeap> m_RTVHeap, m_DSVHeap;
        D3D12_CPU_DESCRIPTOR_HANDLE m_RTV = {}, m_DSV = {};
    };
}
