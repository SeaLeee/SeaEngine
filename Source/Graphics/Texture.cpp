#include "Graphics/Texture.h"
#include "Graphics/Device.h"
#include "Core/Log.h"

namespace Sea
{
    Texture::Texture(Device& device, const TextureDesc& desc) : m_Device(device), m_Desc(desc) {}
    Texture::~Texture() { m_Resource.Reset(); }

    bool Texture::Initialize(const void* data)
    {
        D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_DEFAULT };
        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(static_cast<u32>(m_Desc.type) + 1);
        resourceDesc.Width = m_Desc.width;
        resourceDesc.Height = m_Desc.height;
        resourceDesc.DepthOrArraySize = m_Desc.type == TextureType::Texture3D ? m_Desc.depth : m_Desc.arraySize;
        resourceDesc.MipLevels = m_Desc.mipLevels;
        resourceDesc.Format = static_cast<DXGI_FORMAT>(m_Desc.format);
        resourceDesc.SampleDesc = { 1, 0 };
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        if (HasFlag(m_Desc.usage, TextureUsage::RenderTarget))
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (HasFlag(m_Desc.usage, TextureUsage::DepthStencil))
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        if (HasFlag(m_Desc.usage, TextureUsage::UnorderedAccess))
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
        if (HasFlag(m_Desc.usage, TextureUsage::DepthStencil))
            initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

        m_Resource = m_Device.CreateCommittedResource(heapProps, D3D12_HEAP_FLAG_NONE, resourceDesc, initialState);
        return m_Resource != nullptr;
    }
}
