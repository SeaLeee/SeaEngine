#pragma once

#include "RHI/RHITypes.h"
#include "RHI/RHIResource.h"
#include "Core/Types.h"
#include <span>
#include <vector>

namespace Sea
{
    //=============================================================================
    // RHICommandList - Abstract command list for recording GPU commands
    //=============================================================================
    class RHICommandList
    {
    public:
        virtual ~RHICommandList() = default;
        
        //! Reset command list for new recording
        virtual void Reset() = 0;
        
        //! Close command list
        virtual void Close() = 0;
        
        //=========================================================================
        // Resource Barriers
        //=========================================================================
        
        //! Transition resource state
        virtual void TransitionBarrier(RHITexture* resource, RHIResourceState before, RHIResourceState after) = 0;
        virtual void TransitionBarrier(RHIBuffer* resource, RHIResourceState before, RHIResourceState after) = 0;
        
        //! UAV barrier
        virtual void UAVBarrier(RHIResource* resource) = 0;
        
        //! Flush pending barriers
        virtual void FlushBarriers() = 0;
        
        //=========================================================================
        // Clear Operations
        //=========================================================================
        
        //! Clear render target view
        virtual void ClearRenderTarget(RHIDescriptorHandle rtv, const f32 color[4]) = 0;
        
        //! Clear depth stencil view
        virtual void ClearDepthStencil(RHIDescriptorHandle dsv, f32 depth, u8 stencil = 0) = 0;
        
        //=========================================================================
        // Render State
        //=========================================================================
        
        //! Set render targets
        virtual void SetRenderTargets(std::span<RHIDescriptorHandle> rtvs, 
                                      const RHIDescriptorHandle* dsv = nullptr) = 0;
        
        //! Set viewport
        virtual void SetViewport(const RHIViewport& viewport) = 0;
        
        //! Set scissor rect
        virtual void SetScissorRect(const RHIScissorRect& rect) = 0;
        
        //! Set pipeline state
        virtual void SetPipelineState(RHIPipelineState* pso) = 0;
        
        //! Set graphics root signature
        virtual void SetGraphicsRootSignature(RHIRootSignature* rootSig) = 0;
        
        //! Set compute root signature
        virtual void SetComputeRootSignature(RHIRootSignature* rootSig) = 0;
        
        //! Set descriptor heaps
        virtual void SetDescriptorHeaps(std::span<RHIDescriptorHeap*> heaps) = 0;
        
        //=========================================================================
        // Root Parameters
        //=========================================================================
        
        //! Set graphics root constant
        virtual void SetGraphicsRootConstant(u32 rootIndex, u32 value, u32 offset) = 0;
        
        //! Set graphics root constants (multiple)
        virtual void SetGraphicsRootConstants(u32 rootIndex, const void* data, u32 count) = 0;
        
        //! Set graphics root CBV
        virtual void SetGraphicsRootCBV(u32 rootIndex, u64 gpuAddress) = 0;
        
        //! Set graphics root SRV
        virtual void SetGraphicsRootSRV(u32 rootIndex, u64 gpuAddress) = 0;
        
        //! Set graphics root UAV
        virtual void SetGraphicsRootUAV(u32 rootIndex, u64 gpuAddress) = 0;
        
        //! Set graphics root descriptor table
        virtual void SetGraphicsRootDescriptorTable(u32 rootIndex, RHIDescriptorHandle baseHandle) = 0;
        
        //! Set compute root constant
        virtual void SetComputeRootConstant(u32 rootIndex, u32 value, u32 offset) = 0;
        
        //! Set compute root constants (multiple)
        virtual void SetComputeRootConstants(u32 rootIndex, const void* data, u32 count) = 0;
        
        //! Set compute root CBV
        virtual void SetComputeRootCBV(u32 rootIndex, u64 gpuAddress) = 0;
        
        //! Set compute root SRV
        virtual void SetComputeRootSRV(u32 rootIndex, u64 gpuAddress) = 0;
        
        //! Set compute root UAV
        virtual void SetComputeRootUAV(u32 rootIndex, u64 gpuAddress) = 0;
        
        //! Set compute root descriptor table
        virtual void SetComputeRootDescriptorTable(u32 rootIndex, RHIDescriptorHandle baseHandle) = 0;
        
        //=========================================================================
        // Input Assembly
        //=========================================================================
        
        //! Set vertex buffer
        virtual void SetVertexBuffer(u32 slot, const RHIVertexBufferView& view) = 0;
        
        //! Set index buffer
        virtual void SetIndexBuffer(const RHIIndexBufferView& view) = 0;
        
        //! Set primitive topology
        virtual void SetPrimitiveTopology(RHIPrimitiveTopology topology) = 0;
        
        //=========================================================================
        // Draw Commands
        //=========================================================================
        
        //! Draw non-indexed
        virtual void Draw(u32 vertexCount, u32 instanceCount = 1, 
                         u32 startVertex = 0, u32 startInstance = 0) = 0;
        
        //! Draw indexed
        virtual void DrawIndexed(u32 indexCount, u32 instanceCount = 1, 
                                u32 startIndex = 0, i32 baseVertex = 0, 
                                u32 startInstance = 0) = 0;
        
        //=========================================================================
        // Compute Commands
        //=========================================================================
        
        //! Dispatch compute shader
        virtual void Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) = 0;
        
        //=========================================================================
        // Copy Commands
        //=========================================================================
        
        //! Copy entire buffer
        virtual void CopyBuffer(RHIBuffer* dest, RHIBuffer* src) = 0;
        
        //! Copy buffer region
        virtual void CopyBufferRegion(RHIBuffer* dest, u64 destOffset, 
                                      RHIBuffer* src, u64 srcOffset, u64 size) = 0;
        
        //! Copy texture
        virtual void CopyTexture(RHITexture* dest, RHITexture* src) = 0;
        
        //! Copy texture region
        virtual void CopyTextureRegion(RHITexture* dest, u32 destX, u32 destY, u32 destZ,
                                       RHITexture* src, const RHISubResource* srcSubresource = nullptr) = 0;
        
        //=========================================================================
        // Debug Markers
        //=========================================================================
        
        //! Begin debug event
        virtual void BeginEvent(const char* name) = 0;
        
        //! End debug event
        virtual void EndEvent() = 0;
        
        //! Set debug marker
        virtual void SetMarker(const char* name) = 0;
    };

    //=============================================================================
    // RHICommandQueue - Abstract command queue for submitting commands
    //=============================================================================
    class RHICommandQueue
    {
    public:
        virtual ~RHICommandQueue() = default;
        
        //! Get queue type
        virtual RHICommandQueueType GetType() const = 0;
        
        //! Execute command lists
        virtual void ExecuteCommandLists(std::span<RHICommandList*> cmdLists) = 0;
        
        //! Signal fence
        virtual void Signal(RHIFence* fence, u64 value) = 0;
        
        //! Wait for fence (GPU wait)
        virtual void Wait(RHIFence* fence, u64 value) = 0;
        
        //! Wait for queue to be idle
        virtual void WaitForIdle() = 0;
    };

} // namespace Sea
