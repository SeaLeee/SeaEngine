#pragma once

// RHI Resource Wrappers
// These classes wrap existing Graphics resources to provide RHI interface compatibility
// This enables gradual migration without breaking existing code

#include "RHI/RHI.h"
#include "Graphics/Graphics.h"

namespace Sea
{
    //=============================================================================
    // RHITextureWrapper - Wraps a Graphics::Texture as RHITexture
    //=============================================================================
    class RHITextureWrapper : public RHITexture
    {
    public:
        RHITextureWrapper(Texture* texture)
            : m_Texture(texture)
        {
            if (texture)
            {
                // Copy descriptor from wrapped texture
                RHITextureDesc desc;
                // TODO: Convert from legacy TextureDesc
                m_Desc = desc;
            }
        }

        bool IsValid() const override { return m_Texture != nullptr; }

        Texture* GetWrappedTexture() const { return m_Texture; }
        ID3D12Resource* GetNativeResource() const { return m_Texture ? m_Texture->GetResource() : nullptr; }

    private:
        Texture* m_Texture = nullptr;
    };

    //=============================================================================
    // RHIBufferWrapper - Wraps a Graphics::Buffer as RHIBuffer
    //=============================================================================
    class RHIBufferWrapper : public RHIBuffer
    {
    public:
        RHIBufferWrapper(Buffer* buffer)
            : m_Buffer(buffer)
        {
            if (buffer)
            {
                RHIBufferDesc desc;
                desc.size = buffer->GetSize();
                // TODO: Copy other properties
                m_Desc = desc;
            }
        }

        bool IsValid() const override { return m_Buffer != nullptr; }
        u64 GetGPUVirtualAddress() const override
        {
            return m_Buffer ? m_Buffer->GetGPUVirtualAddress() : 0;
        }
        void* Map() override { return m_Buffer ? m_Buffer->Map() : nullptr; }
        void Unmap() override { if (m_Buffer) m_Buffer->Unmap(); }
        void Update(const void* data, u64 size, u64 offset = 0) override
        {
            if (m_Buffer) m_Buffer->Update(data, size, offset);
        }

        Buffer* GetWrappedBuffer() const { return m_Buffer; }

    private:
        Buffer* m_Buffer = nullptr;
    };

    //=============================================================================
    // RHIRenderTargetWrapper - Wraps Graphics::RenderTarget as RHIRenderTarget
    //=============================================================================
    class RHIRenderTargetWrapper : public RHIRenderTarget
    {
    public:
        RHIRenderTargetWrapper(Texture* colorTexture, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                               Texture* depthTexture = nullptr, D3D12_CPU_DESCRIPTOR_HANDLE dsv = {})
            : m_ColorTexture(colorTexture)
            , m_DepthTexture(depthTexture)
        {
            m_RTV.cpuHandle = rtv.ptr;
            if (depthTexture)
            {
                m_DSV.cpuHandle = dsv.ptr;
            }
        }

        bool IsValid() const override { return m_ColorTexture != nullptr; }

        RHIDescriptorHandle GetRTV() const override { return m_RTV; }
        RHIDescriptorHandle GetDSV() const override { return m_DSV; }
        RHIDescriptorHandle GetSRV() const override { return m_SRV; }
        RHIDescriptorHandle GetUAV() const override { return m_UAV; }

        void Resize(u32 width, u32 height) override
        {
            // Wrapper doesn't own the resource - do nothing
        }

        Texture* GetColorTexture() const { return m_ColorTexture; }
        Texture* GetDepthTexture() const { return m_DepthTexture; }

    private:
        Texture* m_ColorTexture = nullptr;
        Texture* m_DepthTexture = nullptr;
        RHIDescriptorHandle m_RTV;
        RHIDescriptorHandle m_DSV;
        RHIDescriptorHandle m_SRV;
        RHIDescriptorHandle m_UAV;
    };

    //=============================================================================
    // RHICommandListWrapper - Wraps Graphics::CommandList as RHICommandList
    //=============================================================================
    class RHICommandListWrapper : public RHICommandList
    {
    public:
        explicit RHICommandListWrapper(CommandList* cmdList)
            : m_CommandList(cmdList)
        {
        }

        void Reset() override 
        { 
            if (m_CommandList) m_CommandList->Reset(); 
        }
        
        void Close() override 
        { 
            if (m_CommandList) m_CommandList->Close(); 
        }

        // Resource Barriers
        void TransitionBarrier(RHITexture* resource, RHIResourceState before, RHIResourceState after) override
        {
            if (!m_CommandList) return;
            
            // Get native resource from wrapper
            if (auto* wrapper = dynamic_cast<RHITextureWrapper*>(resource))
            {
                m_CommandList->TransitionBarrier(
                    wrapper->GetNativeResource(),
                    ConvertFromRHIState(before),
                    ConvertFromRHIState(after)
                );
            }
        }

        void TransitionBarrier(RHIBuffer* resource, RHIResourceState before, RHIResourceState after) override
        {
            // Similar implementation for buffer
        }

        void UAVBarrier(RHIResource* resource) override
        {
            if (!m_CommandList) return;
            // TODO: Implement UAV barrier
        }

        void FlushBarriers() override
        {
            if (m_CommandList) m_CommandList->FlushBarriers();
        }

        // Render Targets
        void SetRenderTargets(std::span<RHIRenderTarget*> renderTargets, 
                             RHIRenderTarget* depthStencil) override
        {
            // Convert and call legacy API
            // Implementation depends on legacy API signature
        }

        void ClearRenderTarget(RHIDescriptorHandle rtv, const f32* clearColor) override
        {
            if (!m_CommandList) return;
            D3D12_CPU_DESCRIPTOR_HANDLE d3dRtv;
            d3dRtv.ptr = rtv.cpuHandle;
            m_CommandList->ClearRenderTarget(d3dRtv, clearColor);
        }

        void ClearDepthStencil(RHIDescriptorHandle dsv, f32 depth, u8 stencil) override
        {
            if (!m_CommandList) return;
            D3D12_CPU_DESCRIPTOR_HANDLE d3dDsv;
            d3dDsv.ptr = dsv.cpuHandle;
            m_CommandList->ClearDepthStencil(d3dDsv, depth, stencil);
        }

        // Viewport and Scissor
        void SetViewport(const RHIViewport& viewport) override
        {
            if (!m_CommandList) return;
            Viewport vp;
            vp.x = viewport.x;
            vp.y = viewport.y;
            vp.width = viewport.width;
            vp.height = viewport.height;
            vp.minDepth = viewport.minDepth;
            vp.maxDepth = viewport.maxDepth;
            m_CommandList->SetViewport(vp);
        }

        void SetScissorRect(const RHIScissorRect& rect) override
        {
            if (!m_CommandList) return;
            ScissorRect sr;
            sr.left = rect.left;
            sr.top = rect.top;
            sr.right = rect.right;
            sr.bottom = rect.bottom;
            m_CommandList->SetScissorRect(sr);
        }

        // Pipeline State
        void SetPipelineState(RHIPipelineState* pso) override
        {
            // TODO: Implement when RHIPipelineState wrapper exists
        }

        void SetRootSignature(RHIRootSignature* rootSig) override
        {
            // TODO: Implement when RHIRootSignature wrapper exists
        }

        // Primitive Topology
        void SetPrimitiveTopology(RHIPrimitiveTopology topology) override
        {
            if (!m_CommandList) return;
            PrimitiveTopology pt = ConvertFromRHITopology(topology);
            m_CommandList->SetPrimitiveTopology(pt);
        }

        // Vertex/Index Buffers
        void SetVertexBuffer(u32 slot, const RHIVertexBufferView& view) override
        {
            if (!m_CommandList) return;
            D3D12_VERTEX_BUFFER_VIEW d3dView;
            d3dView.BufferLocation = view.gpuAddress;
            d3dView.SizeInBytes = view.sizeInBytes;
            d3dView.StrideInBytes = view.strideInBytes;
            m_CommandList->SetVertexBuffer(slot, d3dView);
        }

        void SetIndexBuffer(const RHIIndexBufferView& view) override
        {
            if (!m_CommandList) return;
            D3D12_INDEX_BUFFER_VIEW d3dView;
            d3dView.BufferLocation = view.gpuAddress;
            d3dView.SizeInBytes = view.sizeInBytes;
            d3dView.Format = view.is32Bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
            m_CommandList->SetIndexBuffer(d3dView);
        }

        // Draw Commands
        void Draw(u32 vertexCount, u32 instanceCount, u32 startVertex, u32 startInstance) override
        {
            if (m_CommandList) 
                m_CommandList->Draw(vertexCount, instanceCount, startVertex, startInstance);
        }

        void DrawIndexed(u32 indexCount, u32 instanceCount, u32 startIndex, 
                        i32 baseVertex, u32 startInstance) override
        {
            if (m_CommandList)
                m_CommandList->DrawIndexed(indexCount, instanceCount, startIndex, baseVertex, startInstance);
        }

        void DrawInstanced(u32 vertexCountPerInstance, u32 instanceCount,
                          u32 startVertexLocation, u32 startInstanceLocation) override
        {
            if (m_CommandList)
                m_CommandList->Draw(vertexCountPerInstance, instanceCount, 
                                   startVertexLocation, startInstanceLocation);
        }

        void DrawIndexedInstanced(u32 indexCountPerInstance, u32 instanceCount,
                                  u32 startIndexLocation, i32 baseVertexLocation,
                                  u32 startInstanceLocation) override
        {
            if (m_CommandList)
                m_CommandList->DrawIndexed(indexCountPerInstance, instanceCount,
                                          startIndexLocation, baseVertexLocation, startInstanceLocation);
        }

        // Dispatch Commands
        void Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) override
        {
            if (m_CommandList)
                m_CommandList->Dispatch(groupCountX, groupCountY, groupCountZ);
        }

        // Copy Commands
        void CopyBuffer(RHIBuffer* dst, RHIBuffer* src, u64 size) override
        {
            // TODO: Implement
        }

        void CopyTexture(RHITexture* dst, RHITexture* src) override
        {
            // TODO: Implement
        }

        void CopyBufferToTexture(RHITexture* dst, RHIBuffer* src, 
                                const RHISubResource& subResource) override
        {
            // TODO: Implement
        }

        // Debug Markers
        void BeginEvent(const char* name) override
        {
            // PIX integration would go here
        }

        void EndEvent() override
        {
            // PIX integration would go here
        }

        void SetMarker(const char* name) override
        {
            // PIX integration would go here
        }

        // Access to wrapped command list
        CommandList* GetWrappedCommandList() const { return m_CommandList; }

    private:
        // Helper conversion functions
        static ResourceState ConvertFromRHIState(RHIResourceState state)
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

        static PrimitiveTopology ConvertFromRHITopology(RHIPrimitiveTopology topology)
        {
            switch (topology)
            {
                case RHIPrimitiveTopology::PointList:     return PrimitiveTopology::PointList;
                case RHIPrimitiveTopology::LineList:      return PrimitiveTopology::LineList;
                case RHIPrimitiveTopology::LineStrip:     return PrimitiveTopology::LineStrip;
                case RHIPrimitiveTopology::TriangleList:  return PrimitiveTopology::TriangleList;
                case RHIPrimitiveTopology::TriangleStrip: return PrimitiveTopology::TriangleStrip;
                default:                                  return PrimitiveTopology::TriangleList;
            }
        }

    private:
        CommandList* m_CommandList = nullptr;
    };

} // namespace Sea
