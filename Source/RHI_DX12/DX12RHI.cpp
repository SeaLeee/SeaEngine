#include "DX12RHI.h"
#include <stdexcept>
#include <algorithm>

namespace Sea
{
    //=============================================================================
    // Format Conversion Functions
    //=============================================================================
    DXGI_FORMAT ConvertToDXGIFormat(RHIFormat format)
    {
        switch (format)
        {
            case RHIFormat::Unknown:            return DXGI_FORMAT_UNKNOWN;
            case RHIFormat::R8_UNORM:           return DXGI_FORMAT_R8_UNORM;
            case RHIFormat::R8_SNORM:           return DXGI_FORMAT_R8_SNORM;
            case RHIFormat::R8_UINT:            return DXGI_FORMAT_R8_UINT;
            case RHIFormat::R8_SINT:            return DXGI_FORMAT_R8_SINT;
            case RHIFormat::R8G8_UNORM:         return DXGI_FORMAT_R8G8_UNORM;
            case RHIFormat::R8G8_SNORM:         return DXGI_FORMAT_R8G8_SNORM;
            case RHIFormat::R8G8_UINT:          return DXGI_FORMAT_R8G8_UINT;
            case RHIFormat::R8G8_SINT:          return DXGI_FORMAT_R8G8_SINT;
            case RHIFormat::R8G8B8A8_UNORM:     return DXGI_FORMAT_R8G8B8A8_UNORM;
            case RHIFormat::R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            case RHIFormat::R8G8B8A8_SNORM:     return DXGI_FORMAT_R8G8B8A8_SNORM;
            case RHIFormat::R8G8B8A8_UINT:      return DXGI_FORMAT_R8G8B8A8_UINT;
            case RHIFormat::R8G8B8A8_SINT:      return DXGI_FORMAT_R8G8B8A8_SINT;
            case RHIFormat::B8G8R8A8_UNORM:     return DXGI_FORMAT_B8G8R8A8_UNORM;
            case RHIFormat::B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
            case RHIFormat::R16_FLOAT:          return DXGI_FORMAT_R16_FLOAT;
            case RHIFormat::R16_UNORM:          return DXGI_FORMAT_R16_UNORM;
            case RHIFormat::R16_SNORM:          return DXGI_FORMAT_R16_SNORM;
            case RHIFormat::R16_UINT:           return DXGI_FORMAT_R16_UINT;
            case RHIFormat::R16_SINT:           return DXGI_FORMAT_R16_SINT;
            case RHIFormat::R16G16_FLOAT:       return DXGI_FORMAT_R16G16_FLOAT;
            case RHIFormat::R16G16_UNORM:       return DXGI_FORMAT_R16G16_UNORM;
            case RHIFormat::R16G16_SNORM:       return DXGI_FORMAT_R16G16_SNORM;
            case RHIFormat::R16G16_UINT:        return DXGI_FORMAT_R16G16_UINT;
            case RHIFormat::R16G16_SINT:        return DXGI_FORMAT_R16G16_SINT;
            case RHIFormat::R16G16B16A16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;
            case RHIFormat::R16G16B16A16_UNORM: return DXGI_FORMAT_R16G16B16A16_UNORM;
            case RHIFormat::R16G16B16A16_SNORM: return DXGI_FORMAT_R16G16B16A16_SNORM;
            case RHIFormat::R16G16B16A16_UINT:  return DXGI_FORMAT_R16G16B16A16_UINT;
            case RHIFormat::R16G16B16A16_SINT:  return DXGI_FORMAT_R16G16B16A16_SINT;
            case RHIFormat::R32_FLOAT:          return DXGI_FORMAT_R32_FLOAT;
            case RHIFormat::R32_UINT:           return DXGI_FORMAT_R32_UINT;
            case RHIFormat::R32_SINT:           return DXGI_FORMAT_R32_SINT;
            case RHIFormat::R32G32_FLOAT:       return DXGI_FORMAT_R32G32_FLOAT;
            case RHIFormat::R32G32_UINT:        return DXGI_FORMAT_R32G32_UINT;
            case RHIFormat::R32G32_SINT:        return DXGI_FORMAT_R32G32_SINT;
            case RHIFormat::R32G32B32_FLOAT:    return DXGI_FORMAT_R32G32B32_FLOAT;
            case RHIFormat::R32G32B32_UINT:     return DXGI_FORMAT_R32G32B32_UINT;
            case RHIFormat::R32G32B32_SINT:     return DXGI_FORMAT_R32G32B32_SINT;
            case RHIFormat::R32G32B32A32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
            case RHIFormat::R32G32B32A32_UINT:  return DXGI_FORMAT_R32G32B32A32_UINT;
            case RHIFormat::R32G32B32A32_SINT:  return DXGI_FORMAT_R32G32B32A32_SINT;
            case RHIFormat::R10G10B10A2_UNORM:  return DXGI_FORMAT_R10G10B10A2_UNORM;
            case RHIFormat::R10G10B10A2_UINT:   return DXGI_FORMAT_R10G10B10A2_UINT;
            case RHIFormat::R11G11B10_FLOAT:    return DXGI_FORMAT_R11G11B10_FLOAT;
            case RHIFormat::D16_UNORM:          return DXGI_FORMAT_D16_UNORM;
            case RHIFormat::D24_UNORM_S8_UINT:  return DXGI_FORMAT_D24_UNORM_S8_UINT;
            case RHIFormat::D32_FLOAT:          return DXGI_FORMAT_D32_FLOAT;
            case RHIFormat::D32_FLOAT_S8X24_UINT: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
            case RHIFormat::BC1_UNORM:          return DXGI_FORMAT_BC1_UNORM;
            case RHIFormat::BC1_UNORM_SRGB:     return DXGI_FORMAT_BC1_UNORM_SRGB;
            case RHIFormat::BC2_UNORM:          return DXGI_FORMAT_BC2_UNORM;
            case RHIFormat::BC2_UNORM_SRGB:     return DXGI_FORMAT_BC2_UNORM_SRGB;
            case RHIFormat::BC3_UNORM:          return DXGI_FORMAT_BC3_UNORM;
            case RHIFormat::BC3_UNORM_SRGB:     return DXGI_FORMAT_BC3_UNORM_SRGB;
            case RHIFormat::BC4_UNORM:          return DXGI_FORMAT_BC4_UNORM;
            case RHIFormat::BC4_SNORM:          return DXGI_FORMAT_BC4_SNORM;
            case RHIFormat::BC5_UNORM:          return DXGI_FORMAT_BC5_UNORM;
            case RHIFormat::BC5_SNORM:          return DXGI_FORMAT_BC5_SNORM;
            case RHIFormat::BC6H_UF16:          return DXGI_FORMAT_BC6H_UF16;
            case RHIFormat::BC6H_SF16:          return DXGI_FORMAT_BC6H_SF16;
            case RHIFormat::BC7_UNORM:          return DXGI_FORMAT_BC7_UNORM;
            case RHIFormat::BC7_UNORM_SRGB:     return DXGI_FORMAT_BC7_UNORM_SRGB;
            default:                            return DXGI_FORMAT_UNKNOWN;
        }
    }
    
    RHIFormat ConvertFromDXGIFormat(DXGI_FORMAT format)
    {
        switch (format)
        {
            case DXGI_FORMAT_UNKNOWN:               return RHIFormat::Unknown;
            case DXGI_FORMAT_R8_UNORM:              return RHIFormat::R8_UNORM;
            case DXGI_FORMAT_R8G8B8A8_UNORM:        return RHIFormat::R8G8B8A8_UNORM;
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:   return RHIFormat::R8G8B8A8_UNORM_SRGB;
            case DXGI_FORMAT_B8G8R8A8_UNORM:        return RHIFormat::B8G8R8A8_UNORM;
            case DXGI_FORMAT_R16G16B16A16_FLOAT:    return RHIFormat::R16G16B16A16_FLOAT;
            case DXGI_FORMAT_R32G32B32A32_FLOAT:    return RHIFormat::R32G32B32A32_FLOAT;
            case DXGI_FORMAT_D24_UNORM_S8_UINT:     return RHIFormat::D24_UNORM_S8_UINT;
            case DXGI_FORMAT_D32_FLOAT:             return RHIFormat::D32_FLOAT;
            // Add more conversions as needed
            default:                                return RHIFormat::Unknown;
        }
    }
    
    D3D12_RESOURCE_STATES ConvertToD3D12ResourceState(RHIResourceState state)
    {
        switch (state)
        {
            case RHIResourceState::Common:          return D3D12_RESOURCE_STATE_COMMON;
            case RHIResourceState::VertexBuffer:    return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            case RHIResourceState::ConstantBuffer:  return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            case RHIResourceState::IndexBuffer:     return D3D12_RESOURCE_STATE_INDEX_BUFFER;
            case RHIResourceState::RenderTarget:    return D3D12_RESOURCE_STATE_RENDER_TARGET;
            case RHIResourceState::UnorderedAccess: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            case RHIResourceState::DepthWrite:      return D3D12_RESOURCE_STATE_DEPTH_WRITE;
            case RHIResourceState::DepthRead:       return D3D12_RESOURCE_STATE_DEPTH_READ;
            case RHIResourceState::ShaderResource:  return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            case RHIResourceState::StreamOut:       return D3D12_RESOURCE_STATE_STREAM_OUT;
            case RHIResourceState::IndirectArgument:return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
            case RHIResourceState::CopyDest:        return D3D12_RESOURCE_STATE_COPY_DEST;
            case RHIResourceState::CopySource:      return D3D12_RESOURCE_STATE_COPY_SOURCE;
            case RHIResourceState::Present:         return D3D12_RESOURCE_STATE_PRESENT;
            case RHIResourceState::GenericRead:     return D3D12_RESOURCE_STATE_GENERIC_READ;
            default:                                return D3D12_RESOURCE_STATE_COMMON;
        }
    }
    
    D3D12_PRIMITIVE_TOPOLOGY ConvertToD3D12PrimitiveTopology(RHIPrimitiveTopology topology)
    {
        switch (topology)
        {
            case RHIPrimitiveTopology::PointList:       return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
            case RHIPrimitiveTopology::LineList:        return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
            case RHIPrimitiveTopology::LineStrip:       return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
            case RHIPrimitiveTopology::TriangleList:    return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            case RHIPrimitiveTopology::TriangleStrip:   return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            default:                                    return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        }
    }
    
    D3D12_COMMAND_LIST_TYPE ConvertToD3D12CommandListType(RHICommandQueueType type)
    {
        switch (type)
        {
            case RHICommandQueueType::Direct:   return D3D12_COMMAND_LIST_TYPE_DIRECT;
            case RHICommandQueueType::Compute:  return D3D12_COMMAND_LIST_TYPE_COMPUTE;
            case RHICommandQueueType::Copy:     return D3D12_COMMAND_LIST_TYPE_COPY;
            default:                            return D3D12_COMMAND_LIST_TYPE_DIRECT;
        }
    }
    
    D3D12_DESCRIPTOR_HEAP_TYPE ConvertToD3D12DescriptorHeapType(RHIDescriptorHeapType type)
    {
        switch (type)
        {
            case RHIDescriptorHeapType::CBV_SRV_UAV:    return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            case RHIDescriptorHeapType::Sampler:        return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            case RHIDescriptorHeapType::RTV:            return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            case RHIDescriptorHeapType::DSV:            return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            default:                                    return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        }
    }

    //=============================================================================
    // DX12Buffer Implementation
    //=============================================================================
    DX12Buffer::DX12Buffer(ID3D12Device* device, const RHIBufferDesc& desc)
    {
        m_Desc = desc;
        
        D3D12_HEAP_PROPERTIES heapProps = {};
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
        
        switch (desc.usage)
        {
            case RHIBufferUsage::Default:
                heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
                break;
            case RHIBufferUsage::Upload:
                heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
                initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
                break;
            case RHIBufferUsage::Readback:
                heapProps.Type = D3D12_HEAP_TYPE_READBACK;
                initialState = D3D12_RESOURCE_STATE_COPY_DEST;
                break;
        }
        
        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = desc.size;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        
        if (desc.allowUAV)
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        
        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            initialState,
            nullptr,
            IID_PPV_ARGS(&m_Resource));
            
        if (FAILED(hr))
        {
            m_Resource = nullptr;
        }
        
        if (m_Resource && !desc.name.empty())
        {
            std::wstring wname(desc.name.begin(), desc.name.end());
            m_Resource->SetName(wname.c_str());
        }
    }
    
    DX12Buffer::~DX12Buffer()
    {
        if (m_MappedData)
        {
            Unmap();
        }
    }
    
    u64 DX12Buffer::GetGPUVirtualAddress() const
    {
        return m_Resource ? m_Resource->GetGPUVirtualAddress() : 0;
    }
    
    void* DX12Buffer::Map()
    {
        if (m_MappedData) return m_MappedData;
        
        D3D12_RANGE readRange = { 0, 0 };
        if (m_Desc.usage == RHIBufferUsage::Readback)
        {
            readRange.End = static_cast<SIZE_T>(m_Desc.size);
        }
        
        HRESULT hr = m_Resource->Map(0, &readRange, &m_MappedData);
        if (FAILED(hr))
        {
            m_MappedData = nullptr;
        }
        
        return m_MappedData;
    }
    
    void DX12Buffer::Unmap()
    {
        if (!m_MappedData) return;
        
        D3D12_RANGE writeRange = { 0, 0 };
        if (m_Desc.usage == RHIBufferUsage::Upload)
        {
            writeRange.End = static_cast<SIZE_T>(m_Desc.size);
        }
        
        m_Resource->Unmap(0, &writeRange);
        m_MappedData = nullptr;
    }
    
    void DX12Buffer::Update(const void* data, u64 size, u64 offset)
    {
        void* mapped = Map();
        if (mapped)
        {
            memcpy(static_cast<u8*>(mapped) + offset, data, size);
            Unmap();
        }
    }
    
    void DX12Buffer::OnNameChanged()
    {
        if (m_Resource && !GetName().empty())
        {
            std::wstring wname(GetName().begin(), GetName().end());
            m_Resource->SetName(wname.c_str());
        }
    }

    //=============================================================================
    // DX12Texture Implementation
    //=============================================================================
    DX12Texture::DX12Texture(ID3D12Device* device, const RHITextureDesc& desc)
    {
        m_Desc = desc;
        
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        
        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Format = ConvertToDXGIFormat(desc.format);
        resourceDesc.Width = desc.width;
        resourceDesc.Height = desc.height;
        resourceDesc.DepthOrArraySize = desc.depth;
        resourceDesc.MipLevels = static_cast<UINT16>(desc.mipLevels);
        resourceDesc.SampleDesc.Count = desc.sampleCount;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        
        switch (desc.dimension)
        {
            case RHITextureDimension::Texture1D:
                resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
                break;
            case RHITextureDimension::Texture2D:
                resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                break;
            case RHITextureDimension::Texture3D:
                resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
                break;
            case RHITextureDimension::TextureCube:
                resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                resourceDesc.DepthOrArraySize = 6;
                break;
        }
        
        if (desc.usage & RHITextureUsage::RenderTarget)
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (desc.usage & RHITextureUsage::DepthStencil)
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        if (desc.usage & RHITextureUsage::UnorderedAccess)
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        
        D3D12_CLEAR_VALUE clearValue = {};
        D3D12_CLEAR_VALUE* pClearValue = nullptr;
        
        if (desc.usage & RHITextureUsage::RenderTarget)
        {
            clearValue.Format = resourceDesc.Format;
            memcpy(clearValue.Color, desc.clearValue.color, sizeof(f32) * 4);
            pClearValue = &clearValue;
        }
        else if (desc.usage & RHITextureUsage::DepthStencil)
        {
            clearValue.Format = resourceDesc.Format;
            clearValue.DepthStencil.Depth = desc.clearValue.depthStencil.depth;
            clearValue.DepthStencil.Stencil = desc.clearValue.depthStencil.stencil;
            pClearValue = &clearValue;
        }
        
        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_COMMON,
            pClearValue,
            IID_PPV_ARGS(&m_Resource));
            
        if (FAILED(hr))
        {
            m_Resource = nullptr;
        }
        
        if (m_Resource && !desc.name.empty())
        {
            std::wstring wname(desc.name.begin(), desc.name.end());
            m_Resource->SetName(wname.c_str());
        }
    }
    
    DX12Texture::~DX12Texture() = default;
    
    void DX12Texture::OnNameChanged()
    {
        if (m_Resource && !GetName().empty())
        {
            std::wstring wname(GetName().begin(), GetName().end());
            m_Resource->SetName(wname.c_str());
        }
    }

    //=============================================================================
    // DX12RenderTarget Implementation
    //=============================================================================
    DX12RenderTarget::DX12RenderTarget(ID3D12Device* device, const RHITextureDesc& desc,
                                       ID3D12DescriptorHeap* rtvHeap, u32 rtvIndex,
                                       ID3D12DescriptorHeap* dsvHeap, u32 dsvIndex,
                                       ID3D12DescriptorHeap* srvHeap, u32 srvIndex)
        : m_Device(device), m_RTVHeap(rtvHeap), m_RTVIndex(rtvIndex),
          m_DSVHeap(dsvHeap), m_DSVIndex(dsvIndex), m_SRVHeap(srvHeap), m_SRVIndex(srvIndex)
    {
        m_Desc = desc;
        m_OwnsResource = true;
        
        // Create the texture resource
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        
        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resourceDesc.Format = ConvertToDXGIFormat(desc.format);
        resourceDesc.Width = desc.width;
        resourceDesc.Height = desc.height;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.SampleDesc.Count = desc.sampleCount;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        
        if (desc.usage & RHITextureUsage::RenderTarget)
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (desc.usage & RHITextureUsage::DepthStencil)
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        if (desc.usage & RHITextureUsage::UnorderedAccess)
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        
        D3D12_CLEAR_VALUE clearValue = {};
        D3D12_CLEAR_VALUE* pClearValue = nullptr;
        
        if (desc.usage & RHITextureUsage::RenderTarget)
        {
            clearValue.Format = resourceDesc.Format;
            memcpy(clearValue.Color, desc.clearValue.color, sizeof(f32) * 4);
            pClearValue = &clearValue;
        }
        else if (desc.usage & RHITextureUsage::DepthStencil)
        {
            // For depth-stencil, use a compatible format for the resource
            clearValue.Format = resourceDesc.Format;
            clearValue.DepthStencil.Depth = desc.clearValue.depthStencil.depth;
            clearValue.DepthStencil.Stencil = desc.clearValue.depthStencil.stencil;
            pClearValue = &clearValue;
        }
        
        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_COMMON,
            pClearValue,
            IID_PPV_ARGS(&m_Resource));
            
        if (FAILED(hr))
        {
            m_Resource = nullptr;
            return;
        }
        
        if (!desc.name.empty())
        {
            std::wstring wname(desc.name.begin(), desc.name.end());
            m_Resource->SetName(wname.c_str());
        }
        
        CreateViews(device);
    }
    
    DX12RenderTarget::DX12RenderTarget(ID3D12Resource* existingResource, const RHITextureDesc& desc,
                                       ID3D12Device* device,
                                       ID3D12DescriptorHeap* rtvHeap, u32 rtvIndex)
        : m_Device(device), m_RTVHeap(rtvHeap), m_RTVIndex(rtvIndex)
    {
        m_Desc = desc;
        m_OwnsResource = false;
        m_Resource = existingResource;
        
        CreateViews(device);
    }
    
    DX12RenderTarget::~DX12RenderTarget()
    {
        if (!m_OwnsResource)
        {
            m_Resource.Detach(); // Don't release the resource
        }
    }
    
    void DX12RenderTarget::CreateViews(ID3D12Device* device)
    {
        if (!m_Resource) return;
        
        DXGI_FORMAT format = ConvertToDXGIFormat(m_Desc.format);
        
        // RTV
        if (m_RTVHeap && (m_Desc.usage & RHITextureUsage::RenderTarget))
        {
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RTVHeap->GetCPUDescriptorHandleForHeapStart();
            UINT rtvIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            rtvHandle.ptr += static_cast<SIZE_T>(m_RTVIndex) * rtvIncrement;
            
            device->CreateRenderTargetView(m_Resource.Get(), nullptr, rtvHandle);
            m_RTV.cpuHandle = rtvHandle.ptr;
        }
        
        // DSV
        if (m_DSVHeap && (m_Desc.usage & RHITextureUsage::DepthStencil))
        {
            D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_DSVHeap->GetCPUDescriptorHandleForHeapStart();
            UINT dsvIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            dsvHandle.ptr += static_cast<SIZE_T>(m_DSVIndex) * dsvIncrement;
            
            device->CreateDepthStencilView(m_Resource.Get(), nullptr, dsvHandle);
            m_DSV.cpuHandle = dsvHandle.ptr;
        }
        
        // SRV
        if (m_SRVHeap && (m_Desc.usage & RHITextureUsage::ShaderResource))
        {
            D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_SRVHeap->GetCPUDescriptorHandleForHeapStart();
            UINT srvIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            srvHandle.ptr += static_cast<SIZE_T>(m_SRVIndex) * srvIncrement;
            
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2D.MipLevels = m_Desc.mipLevels;
            
            // Handle depth format for SRV
            if (m_Desc.usage & RHITextureUsage::DepthStencil)
            {
                if (format == DXGI_FORMAT_D24_UNORM_S8_UINT)
                    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
                else if (format == DXGI_FORMAT_D32_FLOAT)
                    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
            }
            
            device->CreateShaderResourceView(m_Resource.Get(), &srvDesc, srvHandle);
            m_SRV.cpuHandle = srvHandle.ptr;
        }
    }
    
    void DX12RenderTarget::Resize(u32 width, u32 height)
    {
        if (m_Desc.width == width && m_Desc.height == height)
            return;
            
        m_Desc.width = width;
        m_Desc.height = height;
        
        if (m_OwnsResource)
        {
            m_Resource.Reset();
            
            // Recreate the resource
            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
            
            D3D12_RESOURCE_DESC resourceDesc = {};
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            resourceDesc.Format = ConvertToDXGIFormat(m_Desc.format);
            resourceDesc.Width = width;
            resourceDesc.Height = height;
            resourceDesc.DepthOrArraySize = 1;
            resourceDesc.MipLevels = 1;
            resourceDesc.SampleDesc.Count = m_Desc.sampleCount;
            resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            
            if (m_Desc.usage & RHITextureUsage::RenderTarget)
                resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            if (m_Desc.usage & RHITextureUsage::DepthStencil)
                resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            if (m_Desc.usage & RHITextureUsage::UnorderedAccess)
                resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            
            D3D12_CLEAR_VALUE clearValue = {};
            D3D12_CLEAR_VALUE* pClearValue = nullptr;
            
            if (m_Desc.usage & RHITextureUsage::RenderTarget)
            {
                clearValue.Format = resourceDesc.Format;
                memcpy(clearValue.Color, m_Desc.clearValue.color, sizeof(f32) * 4);
                pClearValue = &clearValue;
            }
            else if (m_Desc.usage & RHITextureUsage::DepthStencil)
            {
                clearValue.Format = resourceDesc.Format;
                clearValue.DepthStencil.Depth = m_Desc.clearValue.depthStencil.depth;
                clearValue.DepthStencil.Stencil = m_Desc.clearValue.depthStencil.stencil;
                pClearValue = &clearValue;
            }
            
            m_Device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_COMMON,
                pClearValue,
                IID_PPV_ARGS(&m_Resource));
                
            CreateViews(m_Device);
        }
    }
    
    void DX12RenderTarget::OnNameChanged()
    {
        if (m_Resource && !GetName().empty())
        {
            std::wstring wname(GetName().begin(), GetName().end());
            m_Resource->SetName(wname.c_str());
        }
    }

    //=============================================================================
    // DX12DescriptorHeap Implementation
    //=============================================================================
    DX12DescriptorHeap::DX12DescriptorHeap(ID3D12Device* device, RHIDescriptorHeapType type,
                                           u32 count, bool shaderVisible)
        : m_Type(type), m_Count(count)
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = ConvertToD3D12DescriptorHeapType(type);
        heapDesc.NumDescriptors = count;
        heapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE 
                                       : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        
        // RTV and DSV heaps cannot be shader visible
        if (type == RHIDescriptorHeapType::RTV || type == RHIDescriptorHeapType::DSV)
        {
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        }
        
        HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_Heap));
        if (SUCCEEDED(hr))
        {
            m_IncrementSize = device->GetDescriptorHandleIncrementSize(heapDesc.Type);
            m_FreeList.resize(count, true);
        }
    }
    
    DX12DescriptorHeap::~DX12DescriptorHeap() = default;
    
    RHIDescriptorHandle DX12DescriptorHeap::GetCPUHandle(u32 index) const
    {
        RHIDescriptorHandle handle = {};
        if (m_Heap && index < m_Count)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_Heap->GetCPUDescriptorHandleForHeapStart();
            cpuHandle.ptr += static_cast<SIZE_T>(index) * m_IncrementSize;
            handle.cpuHandle = cpuHandle.ptr;
        }
        return handle;
    }
    
    RHIDescriptorHandle DX12DescriptorHeap::GetGPUHandle(u32 index) const
    {
        RHIDescriptorHandle handle = {};
        if (m_Heap && index < m_Count)
        {
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_Heap->GetGPUDescriptorHandleForHeapStart();
            gpuHandle.ptr += static_cast<SIZE_T>(index) * m_IncrementSize;
            handle.gpuHandle = gpuHandle.ptr;
        }
        return handle;
    }
    
    u32 DX12DescriptorHeap::Allocate()
    {
        for (u32 i = m_NextFreeIndex; i < m_Count; ++i)
        {
            if (m_FreeList[i])
            {
                m_FreeList[i] = false;
                m_NextFreeIndex = i + 1;
                return i;
            }
        }
        
        // Wrap around
        for (u32 i = 0; i < m_NextFreeIndex; ++i)
        {
            if (m_FreeList[i])
            {
                m_FreeList[i] = false;
                m_NextFreeIndex = i + 1;
                return i;
            }
        }
        
        return UINT32_MAX; // No free descriptor
    }
    
    void DX12DescriptorHeap::Free(u32 index)
    {
        if (index < m_Count)
        {
            m_FreeList[index] = true;
            if (index < m_NextFreeIndex)
            {
                m_NextFreeIndex = index;
            }
        }
    }

    //=============================================================================
    // DX12Fence Implementation
    //=============================================================================
    DX12Fence::DX12Fence(ID3D12Device* device, u64 initialValue)
    {
        HRESULT hr = device->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));
        if (SUCCEEDED(hr))
        {
            m_Event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        }
    }
    
    DX12Fence::~DX12Fence()
    {
        if (m_Event)
        {
            CloseHandle(m_Event);
        }
    }
    
    u64 DX12Fence::GetCompletedValue() const
    {
        return m_Fence ? m_Fence->GetCompletedValue() : 0;
    }
    
    void DX12Fence::Signal(u64 value)
    {
        // CPU signal - for GPU signal, use CommandQueue::Signal
        if (m_Fence)
        {
            m_Fence->Signal(value);
        }
    }
    
    void DX12Fence::Wait(u64 value)
    {
        if (!m_Fence || !m_Event) return;
        
        if (m_Fence->GetCompletedValue() < value)
        {
            m_Fence->SetEventOnCompletion(value, m_Event);
            WaitForSingleObject(m_Event, INFINITE);
        }
    }

} // namespace Sea
