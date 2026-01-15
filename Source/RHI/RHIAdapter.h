#pragma once

// RHI Adapter - Bridge between legacy Graphics code and new RHI layer
// This provides compatibility during the migration period

#include "RHI/RHI.h"
#include "Graphics/GraphicsTypes.h"

namespace Sea
{
    //=============================================================================
    // Legacy Type Converters
    //=============================================================================

    // Convert legacy ResourceState to RHIResourceState
    inline RHIResourceState ConvertToRHIState(ResourceState state)
    {
        switch (state)
        {
            case ResourceState::Common:          return RHIResourceState::Common;
            case ResourceState::VertexBuffer:    return RHIResourceState::VertexBuffer;
            case ResourceState::IndexBuffer:     return RHIResourceState::IndexBuffer;
            case ResourceState::ConstantBuffer:  return RHIResourceState::ConstantBuffer;
            case ResourceState::RenderTarget:    return RHIResourceState::RenderTarget;
            case ResourceState::UnorderedAccess: return RHIResourceState::UnorderedAccess;
            case ResourceState::DepthWrite:      return RHIResourceState::DepthWrite;
            case ResourceState::DepthRead:       return RHIResourceState::DepthRead;
            case ResourceState::ShaderResource:  return RHIResourceState::ShaderResource;
            case ResourceState::CopyDest:        return RHIResourceState::CopyDest;
            case ResourceState::CopySource:      return RHIResourceState::CopySource;
            case ResourceState::Present:         return RHIResourceState::Present;
            default:                             return RHIResourceState::Common;
        }
    }

    // Convert RHIResourceState to legacy ResourceState
    inline ResourceState ConvertFromRHIState(RHIResourceState state)
    {
        switch (state)
        {
            case RHIResourceState::Common:          return ResourceState::Common;
            case RHIResourceState::VertexBuffer:    return ResourceState::VertexBuffer;
            case RHIResourceState::IndexBuffer:     return ResourceState::IndexBuffer;
            case RHIResourceState::ConstantBuffer:  return ResourceState::ConstantBuffer;
            case RHIResourceState::RenderTarget:    return ResourceState::RenderTarget;
            case RHIResourceState::UnorderedAccess: return ResourceState::UnorderedAccess;
            case RHIResourceState::DepthWrite:      return ResourceState::DepthWrite;
            case RHIResourceState::DepthRead:       return ResourceState::DepthRead;
            case RHIResourceState::ShaderResource:  return ResourceState::ShaderResource;
            case RHIResourceState::CopyDest:        return ResourceState::CopyDest;
            case RHIResourceState::CopySource:      return ResourceState::CopySource;
            case RHIResourceState::Present:         return ResourceState::Present;
            default:                                return ResourceState::Common;
        }
    }

    // Convert legacy Viewport to RHIViewport
    inline RHIViewport ConvertToRHIViewport(const Viewport& vp)
    {
        RHIViewport rhiVp;
        rhiVp.x = vp.x;
        rhiVp.y = vp.y;
        rhiVp.width = vp.width;
        rhiVp.height = vp.height;
        rhiVp.minDepth = vp.minDepth;
        rhiVp.maxDepth = vp.maxDepth;
        return rhiVp;
    }

    // Convert legacy ScissorRect to RHIScissorRect
    inline RHIScissorRect ConvertToRHIScissorRect(const ScissorRect& rect)
    {
        RHIScissorRect rhiRect;
        rhiRect.left = rect.left;
        rhiRect.top = rect.top;
        rhiRect.right = rect.right;
        rhiRect.bottom = rect.bottom;
        return rhiRect;
    }

    // Convert D3D12 descriptor handle to RHI handle
    inline RHIDescriptorHandle ConvertToRHIHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle)
    {
        RHIDescriptorHandle handle;
        handle.cpuHandle = cpuHandle.ptr;
        handle.gpuHandle = 0;
        return handle;
    }

    inline RHIDescriptorHandle ConvertToRHIHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, 
                                                   D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
    {
        RHIDescriptorHandle handle;
        handle.cpuHandle = cpuHandle.ptr;
        handle.gpuHandle = gpuHandle.ptr;
        return handle;
    }

    // Convert RHI handle to D3D12 descriptor handle
    inline D3D12_CPU_DESCRIPTOR_HANDLE ConvertToCPUHandle(RHIDescriptorHandle handle)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
        cpuHandle.ptr = handle.cpuHandle;
        return cpuHandle;
    }

    inline D3D12_GPU_DESCRIPTOR_HANDLE ConvertToGPUHandle(RHIDescriptorHandle handle)
    {
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
        gpuHandle.ptr = handle.gpuHandle;
        return gpuHandle;
    }

    // Convert legacy PrimitiveTopology to RHIPrimitiveTopology
    inline RHIPrimitiveTopology ConvertToRHITopology(PrimitiveTopology topology)
    {
        switch (topology)
        {
            case PrimitiveTopology::PointList:      return RHIPrimitiveTopology::PointList;
            case PrimitiveTopology::LineList:       return RHIPrimitiveTopology::LineList;
            case PrimitiveTopology::LineStrip:      return RHIPrimitiveTopology::LineStrip;
            case PrimitiveTopology::TriangleList:   return RHIPrimitiveTopology::TriangleList;
            case PrimitiveTopology::TriangleStrip:  return RHIPrimitiveTopology::TriangleStrip;
            default:                                return RHIPrimitiveTopology::TriangleList;
        }
    }

    // Convert D3D12 format to RHI format
    inline RHIFormat ConvertToRHIFormat(DXGI_FORMAT format)
    {
        switch (format)
        {
            case DXGI_FORMAT_R8G8B8A8_UNORM:        return RHIFormat::R8G8B8A8_UNORM;
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:   return RHIFormat::R8G8B8A8_UNORM_SRGB;
            case DXGI_FORMAT_B8G8R8A8_UNORM:        return RHIFormat::B8G8R8A8_UNORM;
            case DXGI_FORMAT_R16G16B16A16_FLOAT:    return RHIFormat::R16G16B16A16_FLOAT;
            case DXGI_FORMAT_R32G32B32A32_FLOAT:    return RHIFormat::R32G32B32A32_FLOAT;
            case DXGI_FORMAT_R32_FLOAT:             return RHIFormat::R32_FLOAT;
            case DXGI_FORMAT_R32G32_FLOAT:          return RHIFormat::R32G32_FLOAT;
            case DXGI_FORMAT_R11G11B10_FLOAT:       return RHIFormat::R11G11B10_FLOAT;
            case DXGI_FORMAT_D24_UNORM_S8_UINT:     return RHIFormat::D24_UNORM_S8_UINT;
            case DXGI_FORMAT_D32_FLOAT:             return RHIFormat::D32_FLOAT;
            default:                                return RHIFormat::Unknown;
        }
    }

    //=============================================================================
    // RHICommandListAdapter - Wraps RHICommandList for legacy CommandList interface
    //=============================================================================
    class RHICommandListAdapter
    {
    public:
        explicit RHICommandListAdapter(RHICommandList* cmdList) : m_CmdList(cmdList) {}

        // Barrier operations
        void TransitionBarrier(ID3D12Resource* resource, ResourceState before, ResourceState after)
        {
            // Note: For full migration, we'd need to wrap D3D12 resources
            // For now, this is a compatibility layer
        }

        void ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv, const f32* color)
        {
            if (m_CmdList)
            {
                m_CmdList->ClearRenderTarget(ConvertToRHIHandle(rtv), color);
            }
        }

        void ClearDepthStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv, f32 depth, u8 stencil = 0)
        {
            if (m_CmdList)
            {
                m_CmdList->ClearDepthStencil(ConvertToRHIHandle(dsv), depth, stencil);
            }
        }

        void SetViewport(const Viewport& viewport)
        {
            if (m_CmdList)
            {
                m_CmdList->SetViewport(ConvertToRHIViewport(viewport));
            }
        }

        void SetScissorRect(const ScissorRect& rect)
        {
            if (m_CmdList)
            {
                m_CmdList->SetScissorRect(ConvertToRHIScissorRect(rect));
            }
        }

        void SetPrimitiveTopology(PrimitiveTopology topology)
        {
            if (m_CmdList)
            {
                m_CmdList->SetPrimitiveTopology(ConvertToRHITopology(topology));
            }
        }

        void Draw(u32 vertexCount, u32 instanceCount = 1, u32 startVertex = 0, u32 startInstance = 0)
        {
            if (m_CmdList)
            {
                m_CmdList->Draw(vertexCount, instanceCount, startVertex, startInstance);
            }
        }

        void DrawIndexed(u32 indexCount, u32 instanceCount = 1, u32 startIndex = 0, 
                        i32 baseVertex = 0, u32 startInstance = 0)
        {
            if (m_CmdList)
            {
                m_CmdList->DrawIndexed(indexCount, instanceCount, startIndex, baseVertex, startInstance);
            }
        }

        void Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ)
        {
            if (m_CmdList)
            {
                m_CmdList->Dispatch(groupCountX, groupCountY, groupCountZ);
            }
        }

        void BeginEvent(const char* name)
        {
            if (m_CmdList)
            {
                m_CmdList->BeginEvent(name);
            }
        }

        void EndEvent()
        {
            if (m_CmdList)
            {
                m_CmdList->EndEvent();
            }
        }

        RHICommandList* GetRHICommandList() const { return m_CmdList; }

    private:
        RHICommandList* m_CmdList;
    };

} // namespace Sea
