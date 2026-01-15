#pragma once

#include "RHI/RHITypes.h"
#include "RHI/RHIResource.h"
#include "RHI/RHICommandList.h"
#include "Core/Types.h"
#include <memory>
#include <string>

namespace Sea
{
    //=============================================================================
    // Device Descriptor
    //=============================================================================
    struct RHIDeviceDesc
    {
        bool enableDebugLayer = true;
        bool enableGPUValidation = false;
        bool preferHighPerformanceAdapter = true;
    };

    //=============================================================================
    // Swap Chain Descriptor
    //=============================================================================
    struct RHISwapChainDesc
    {
        void* windowHandle = nullptr;
        u32 width = 1280;
        u32 height = 720;
        u32 bufferCount = 2;
        RHIFormat format = RHIFormat::R8G8B8A8_UNORM;
        bool vsync = true;
    };

    //=============================================================================
    // RHISwapChain - Abstract swap chain
    //=============================================================================
    class RHISwapChain
    {
    public:
        virtual ~RHISwapChain() = default;
        
        //! Get back buffer count
        virtual u32 GetBufferCount() const = 0;
        
        //! Get current back buffer index
        virtual u32 GetCurrentBackBufferIndex() const = 0;
        
        //! Get back buffer
        virtual RHIRenderTarget* GetBackBuffer(u32 index) = 0;
        
        //! Present
        virtual void Present(bool vsync = true) = 0;
        
        //! Resize
        virtual void Resize(u32 width, u32 height) = 0;
        
        //! Get width
        virtual u32 GetWidth() const = 0;
        
        //! Get height
        virtual u32 GetHeight() const = 0;
    };

    //=============================================================================
    // RHIDevice - Abstract render device
    //=============================================================================
    class RHIDevice
    {
    public:
        virtual ~RHIDevice() = default;
        
        //! Initialize device
        virtual bool Initialize(const RHIDeviceDesc& desc) = 0;
        
        //! Shutdown device
        virtual void Shutdown() = 0;
        
        //! Get adapter name
        virtual std::string GetAdapterName() const = 0;
        
        //! Get dedicated video memory
        virtual u64 GetDedicatedVideoMemory() const = 0;
        
        //=========================================================================
        // Resource Creation
        //=========================================================================
        
        //! Create buffer
        virtual std::unique_ptr<RHIBuffer> CreateBuffer(const RHIBufferDesc& desc) = 0;
        
        //! Create texture
        virtual std::unique_ptr<RHITexture> CreateTexture(const RHITextureDesc& desc) = 0;
        
        //! Create render target
        virtual std::unique_ptr<RHIRenderTarget> CreateRenderTarget(const RHITextureDesc& desc) = 0;
        
        //! Create descriptor heap
        virtual std::unique_ptr<RHIDescriptorHeap> CreateDescriptorHeap(
            RHIDescriptorHeapType type, u32 count, bool shaderVisible = true) = 0;
        
        //! Create pipeline state
        virtual std::unique_ptr<RHIPipelineState> CreateGraphicsPipelineState(
            const void* desc) = 0;  // Platform-specific desc
        
        //! Create compute pipeline state
        virtual std::unique_ptr<RHIPipelineState> CreateComputePipelineState(
            const void* desc) = 0;  // Platform-specific desc
        
        //! Create root signature
        virtual std::unique_ptr<RHIRootSignature> CreateRootSignature(
            const void* desc) = 0;  // Platform-specific desc
        
        //! Create fence
        virtual std::unique_ptr<RHIFence> CreateFence(u64 initialValue = 0) = 0;
        
        //=========================================================================
        // Command List / Queue
        //=========================================================================
        
        //! Create command queue
        virtual std::unique_ptr<RHICommandQueue> CreateCommandQueue(RHICommandQueueType type) = 0;
        
        //! Create command list
        virtual std::unique_ptr<RHICommandList> CreateCommandList(RHICommandQueueType type) = 0;
        
        //=========================================================================
        // Swap Chain
        //=========================================================================
        
        //! Create swap chain
        virtual std::unique_ptr<RHISwapChain> CreateSwapChain(
            RHICommandQueue* presentQueue, const RHISwapChainDesc& desc) = 0;
        
        //=========================================================================
        // Synchronization
        //=========================================================================
        
        //! Wait for device to be idle
        virtual void WaitForIdle() = 0;
        
        //=========================================================================
        // Static Factory
        //=========================================================================
        
        //! Create platform-specific device
        static std::unique_ptr<RHIDevice> Create();
    };

} // namespace Sea
