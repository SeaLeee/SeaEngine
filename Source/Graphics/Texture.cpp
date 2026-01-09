#include "Graphics/Texture.h"
#include "Graphics/Device.h"
#include "Core/Log.h"

namespace Sea
{
    Texture::Texture(Device& device, const TextureDesc& desc) : m_Device(device), m_Desc(desc) {}
    Texture::~Texture() { m_Resource.Reset(); }

    bool Texture::Initialize(const void* data)
    {
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        // TextureType枚举: Texture1D=0, Texture2D=1, Texture3D=2, TextureCube=3
        // D3D12_RESOURCE_DIMENSION: TEXTURE1D=2, TEXTURE2D=3, TEXTURE3D=4
        // 所以需要 +2 来转换（TextureCube使用TEXTURE2D）
        D3D12_RESOURCE_DIMENSION dimension;
        switch (m_Desc.type)
        {
        case TextureType::Texture1D:
            dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
            break;
        case TextureType::Texture2D:
        case TextureType::TextureCube:
            dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            break;
        case TextureType::Texture3D:
            dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
            break;
        default:
            dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            break;
        }

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = dimension;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = m_Desc.width;
        resourceDesc.Height = m_Desc.height;
        resourceDesc.DepthOrArraySize = m_Desc.type == TextureType::Texture3D ? static_cast<u16>(m_Desc.depth) : static_cast<u16>(m_Desc.arraySize);
        resourceDesc.MipLevels = static_cast<u16>(m_Desc.mipLevels);
        resourceDesc.Format = static_cast<DXGI_FORMAT>(m_Desc.format);
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        if (HasFlag(m_Desc.usage, TextureUsage::RenderTarget))
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (HasFlag(m_Desc.usage, TextureUsage::DepthStencil))
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        if (HasFlag(m_Desc.usage, TextureUsage::UnorderedAccess))
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        // 对于 DEFAULT 堆上的资源，初始状态必须是 COMMON
        // 后续需要通过命令列表进行状态转换
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_CLEAR_VALUE* pClearValue = nullptr;
        D3D12_CLEAR_VALUE clearValue = {};

        if (HasFlag(m_Desc.usage, TextureUsage::DepthStencil))
        {
            clearValue.Format = resourceDesc.Format;
            clearValue.DepthStencil.Depth = 1.0f;
            clearValue.DepthStencil.Stencil = 0;
            pClearValue = &clearValue;
        }
        else if (HasFlag(m_Desc.usage, TextureUsage::RenderTarget))
        {
            clearValue.Format = resourceDesc.Format;
            clearValue.Color[0] = 0.0f;
            clearValue.Color[1] = 0.0f;
            clearValue.Color[2] = 0.0f;
            clearValue.Color[3] = 1.0f;
            pClearValue = &clearValue;
        }

        m_Resource = m_Device.CreateCommittedResource(heapProps, D3D12_HEAP_FLAG_NONE, resourceDesc, initialState, pClearValue);
        return m_Resource != nullptr;
    }
}
