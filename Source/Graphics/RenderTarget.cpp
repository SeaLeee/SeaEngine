#include "Graphics/RenderTarget.h"
#include "Graphics/Device.h"

namespace Sea
{
    RenderTarget::RenderTarget(Device& device, u32 width, u32 height, Format format, bool hasDepth)
        : m_Device(device), m_Width(width), m_Height(height), m_ColorFormat(format), m_HasDepth(hasDepth) {}
    
    RenderTarget::~RenderTarget() {}

    bool RenderTarget::Initialize()
    {
        // Create RTV heap
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE };
        m_Device.GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_RTVHeap));

        // Create color resource
        D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_DEFAULT };
        D3D12_RESOURCE_DESC colorDesc = {};
        colorDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        colorDesc.Width = m_Width; colorDesc.Height = m_Height;
        colorDesc.DepthOrArraySize = 1; colorDesc.MipLevels = 1;
        colorDesc.Format = static_cast<DXGI_FORMAT>(m_ColorFormat);
        colorDesc.SampleDesc = { 1, 0 };
        colorDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clearValue = { colorDesc.Format, { 0, 0, 0, 1 } };
        m_ColorResource = m_Device.CreateCommittedResource(heapProps, D3D12_HEAP_FLAG_NONE, colorDesc, 
            D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue);

        m_RTV = m_RTVHeap->GetCPUDescriptorHandleForHeapStart();
        m_Device.GetDevice()->CreateRenderTargetView(m_ColorResource.Get(), nullptr, m_RTV);

        if (m_HasDepth)
        {
            D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = { D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE };
            m_Device.GetDevice()->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_DSVHeap));

            D3D12_RESOURCE_DESC depthDesc = colorDesc;
            depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
            depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            D3D12_CLEAR_VALUE depthClear = {}; depthClear.Format = DXGI_FORMAT_D32_FLOAT;
            depthClear.DepthStencil = { 1.0f, 0 };
            m_DepthResource = m_Device.CreateCommittedResource(heapProps, D3D12_HEAP_FLAG_NONE, depthDesc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClear);

            m_DSV = m_DSVHeap->GetCPUDescriptorHandleForHeapStart();
            m_Device.GetDevice()->CreateDepthStencilView(m_DepthResource.Get(), nullptr, m_DSV);
        }
        return true;
    }

    void RenderTarget::Resize(u32 width, u32 height)
    {
        m_Width = width; m_Height = height;
        m_ColorResource.Reset(); m_DepthResource.Reset();
        Initialize();
    }
}
