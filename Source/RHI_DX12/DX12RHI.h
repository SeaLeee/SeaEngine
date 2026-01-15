#pragma once

// DX12 RHI Implementation - Internal header
// This file should only be included by DX12 RHI implementation files

#include "RHI/RHI.h"

// Windows and DirectX 12 headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "Graphics/d3dx12.h"  // D3D12 helper structures

namespace Sea
{
    using Microsoft::WRL::ComPtr;

    //=============================================================================
    // DX12 Format Conversion
    //=============================================================================
    DXGI_FORMAT ConvertToDXGIFormat(RHIFormat format);
    RHIFormat ConvertFromDXGIFormat(DXGI_FORMAT format);
    
    D3D12_RESOURCE_STATES ConvertToD3D12ResourceState(RHIResourceState state);
    D3D12_PRIMITIVE_TOPOLOGY ConvertToD3D12PrimitiveTopology(RHIPrimitiveTopology topology);
    D3D12_COMMAND_LIST_TYPE ConvertToD3D12CommandListType(RHICommandQueueType type);
    D3D12_DESCRIPTOR_HEAP_TYPE ConvertToD3D12DescriptorHeapType(RHIDescriptorHeapType type);

    //=============================================================================
    // DX12Buffer
    //=============================================================================
    class DX12Buffer : public RHIBuffer
    {
    public:
        DX12Buffer(ID3D12Device* device, const RHIBufferDesc& desc);
        ~DX12Buffer() override;
        
        bool IsValid() const override { return m_Resource != nullptr; }
        u64 GetGPUVirtualAddress() const override;
        void* Map() override;
        void Unmap() override;
        void Update(const void* data, u64 size, u64 offset = 0) override;
        
        ID3D12Resource* GetResource() const { return m_Resource.Get(); }
        
    protected:
        void OnNameChanged() override;
        
    private:
        ComPtr<ID3D12Resource> m_Resource;
        void* m_MappedData = nullptr;
    };

    //=============================================================================
    // DX12Texture
    //=============================================================================
    class DX12Texture : public RHITexture
    {
    public:
        DX12Texture(ID3D12Device* device, const RHITextureDesc& desc);
        ~DX12Texture() override;
        
        bool IsValid() const override { return m_Resource != nullptr; }
        
        ID3D12Resource* GetResource() const { return m_Resource.Get(); }
        
    protected:
        void OnNameChanged() override;
        
    private:
        ComPtr<ID3D12Resource> m_Resource;
    };

    //=============================================================================
    // DX12RenderTarget
    //=============================================================================
    class DX12RenderTarget : public RHIRenderTarget
    {
    public:
        DX12RenderTarget(ID3D12Device* device, const RHITextureDesc& desc,
                        ID3D12DescriptorHeap* rtvHeap, u32 rtvIndex,
                        ID3D12DescriptorHeap* dsvHeap = nullptr, u32 dsvIndex = 0,
                        ID3D12DescriptorHeap* srvHeap = nullptr, u32 srvIndex = 0);
        DX12RenderTarget(ID3D12Resource* existingResource, const RHITextureDesc& desc,
                        ID3D12Device* device,
                        ID3D12DescriptorHeap* rtvHeap, u32 rtvIndex);
        ~DX12RenderTarget() override;
        
        bool IsValid() const override { return m_Resource != nullptr; }
        
        RHIDescriptorHandle GetRTV() const override { return m_RTV; }
        RHIDescriptorHandle GetDSV() const override { return m_DSV; }
        RHIDescriptorHandle GetSRV() const override { return m_SRV; }
        RHIDescriptorHandle GetUAV() const override { return m_UAV; }
        
        void Resize(u32 width, u32 height) override;
        
        ID3D12Resource* GetResource() const { return m_Resource.Get(); }
        
    protected:
        void OnNameChanged() override;
        
    private:
        void CreateViews(ID3D12Device* device);
        
        ComPtr<ID3D12Resource> m_Resource;
        bool m_OwnsResource = true;
        
        ID3D12Device* m_Device = nullptr;
        ID3D12DescriptorHeap* m_RTVHeap = nullptr;
        ID3D12DescriptorHeap* m_DSVHeap = nullptr;
        ID3D12DescriptorHeap* m_SRVHeap = nullptr;
        u32 m_RTVIndex = 0;
        u32 m_DSVIndex = 0;
        u32 m_SRVIndex = 0;
        
        RHIDescriptorHandle m_RTV = {};
        RHIDescriptorHandle m_DSV = {};
        RHIDescriptorHandle m_SRV = {};
        RHIDescriptorHandle m_UAV = {};
    };

    //=============================================================================
    // DX12DescriptorHeap
    //=============================================================================
    class DX12DescriptorHeap : public RHIDescriptorHeap
    {
    public:
        DX12DescriptorHeap(ID3D12Device* device, RHIDescriptorHeapType type, 
                          u32 count, bool shaderVisible);
        ~DX12DescriptorHeap() override;
        
        bool IsValid() const override { return m_Heap != nullptr; }
        
        RHIDescriptorHeapType GetType() const override { return m_Type; }
        u32 GetDescriptorCount() const override { return m_Count; }
        
        RHIDescriptorHandle GetCPUHandle(u32 index) const override;
        RHIDescriptorHandle GetGPUHandle(u32 index) const override;
        
        u32 Allocate() override;
        void Free(u32 index) override;
        
        ID3D12DescriptorHeap* GetHeap() const { return m_Heap.Get(); }
        u32 GetIncrementSize() const { return m_IncrementSize; }
        
    private:
        ComPtr<ID3D12DescriptorHeap> m_Heap;
        RHIDescriptorHeapType m_Type;
        u32 m_Count = 0;
        u32 m_IncrementSize = 0;
        std::vector<bool> m_FreeList;
        u32 m_NextFreeIndex = 0;
    };

    //=============================================================================
    // DX12Fence
    //=============================================================================
    class DX12Fence : public RHIFence
    {
    public:
        DX12Fence(ID3D12Device* device, u64 initialValue);
        ~DX12Fence() override;
        
        bool IsValid() const override { return m_Fence != nullptr; }
        
        u64 GetCompletedValue() const override;
        void Signal(u64 value) override;
        void Wait(u64 value) override;
        
        ID3D12Fence* GetFence() const { return m_Fence.Get(); }
        
    private:
        ComPtr<ID3D12Fence> m_Fence;
        HANDLE m_Event = nullptr;
    };

    //=============================================================================
    // DX12CommandList
    //=============================================================================
    class DX12CommandList : public RHICommandList
    {
    public:
        DX12CommandList(ID3D12Device* device, RHICommandQueueType type);
        ~DX12CommandList() override;
        
        void Reset() override;
        void Close() override;
        
        // Resource Barriers
        void TransitionBarrier(RHITexture* resource, RHIResourceState before, RHIResourceState after) override;
        void TransitionBarrier(RHIBuffer* resource, RHIResourceState before, RHIResourceState after) override;
        void UAVBarrier(RHIResource* resource) override;
        void FlushBarriers() override;
        
        // Clear Operations
        void ClearRenderTarget(RHIDescriptorHandle rtv, const f32 color[4]) override;
        void ClearDepthStencil(RHIDescriptorHandle dsv, f32 depth, u8 stencil = 0) override;
        
        // Render State
        void SetRenderTargets(std::span<RHIDescriptorHandle> rtvs, 
                             const RHIDescriptorHandle* dsv = nullptr) override;
        void SetViewport(const RHIViewport& viewport) override;
        void SetScissorRect(const RHIScissorRect& rect) override;
        void SetPipelineState(RHIPipelineState* pso) override;
        void SetGraphicsRootSignature(RHIRootSignature* rootSig) override;
        void SetComputeRootSignature(RHIRootSignature* rootSig) override;
        void SetDescriptorHeaps(std::span<RHIDescriptorHeap*> heaps) override;
        
        // Root Parameters
        void SetGraphicsRootConstant(u32 rootIndex, u32 value, u32 offset) override;
        void SetGraphicsRootConstants(u32 rootIndex, const void* data, u32 count) override;
        void SetGraphicsRootCBV(u32 rootIndex, u64 gpuAddress) override;
        void SetGraphicsRootSRV(u32 rootIndex, u64 gpuAddress) override;
        void SetGraphicsRootUAV(u32 rootIndex, u64 gpuAddress) override;
        void SetGraphicsRootDescriptorTable(u32 rootIndex, RHIDescriptorHandle baseHandle) override;
        void SetComputeRootConstant(u32 rootIndex, u32 value, u32 offset) override;
        void SetComputeRootConstants(u32 rootIndex, const void* data, u32 count) override;
        void SetComputeRootCBV(u32 rootIndex, u64 gpuAddress) override;
        void SetComputeRootSRV(u32 rootIndex, u64 gpuAddress) override;
        void SetComputeRootUAV(u32 rootIndex, u64 gpuAddress) override;
        void SetComputeRootDescriptorTable(u32 rootIndex, RHIDescriptorHandle baseHandle) override;
        
        // Input Assembly
        void SetVertexBuffer(u32 slot, const RHIVertexBufferView& view) override;
        void SetIndexBuffer(const RHIIndexBufferView& view) override;
        void SetPrimitiveTopology(RHIPrimitiveTopology topology) override;
        
        // Draw Commands
        void Draw(u32 vertexCount, u32 instanceCount = 1, 
                 u32 startVertex = 0, u32 startInstance = 0) override;
        void DrawIndexed(u32 indexCount, u32 instanceCount = 1, 
                        u32 startIndex = 0, i32 baseVertex = 0, 
                        u32 startInstance = 0) override;
        
        // Compute Commands
        void Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) override;
        
        // Copy Commands
        void CopyBuffer(RHIBuffer* dest, RHIBuffer* src) override;
        void CopyBufferRegion(RHIBuffer* dest, u64 destOffset, 
                             RHIBuffer* src, u64 srcOffset, u64 size) override;
        void CopyTexture(RHITexture* dest, RHITexture* src) override;
        void CopyTextureRegion(RHITexture* dest, u32 destX, u32 destY, u32 destZ,
                              RHITexture* src, const RHISubResource* srcSubresource = nullptr) override;
        
        // Debug Markers
        void BeginEvent(const char* name) override;
        void EndEvent() override;
        void SetMarker(const char* name) override;
        
        ID3D12GraphicsCommandList* GetCommandList() const { return m_CommandList.Get(); }
        
    private:
        ComPtr<ID3D12CommandAllocator> m_Allocator;
        ComPtr<ID3D12GraphicsCommandList> m_CommandList;
        std::vector<D3D12_RESOURCE_BARRIER> m_PendingBarriers;
        RHICommandQueueType m_Type;
    };

    //=============================================================================
    // DX12CommandQueue
    //=============================================================================
    class DX12CommandQueue : public RHICommandQueue
    {
    public:
        DX12CommandQueue(ID3D12Device* device, RHICommandQueueType type);
        ~DX12CommandQueue() override;
        
        RHICommandQueueType GetType() const override { return m_Type; }
        
        void ExecuteCommandLists(std::span<RHICommandList*> cmdLists) override;
        void Signal(RHIFence* fence, u64 value) override;
        void Wait(RHIFence* fence, u64 value) override;
        void WaitForIdle() override;
        
        ID3D12CommandQueue* GetQueue() const { return m_Queue.Get(); }
        
    private:
        ComPtr<ID3D12CommandQueue> m_Queue;
        ComPtr<ID3D12Fence> m_IdleFence;
        HANDLE m_IdleEvent = nullptr;
        u64 m_IdleFenceValue = 0;
        RHICommandQueueType m_Type;
    };

    //=============================================================================
    // DX12SwapChain
    //=============================================================================
    class DX12SwapChain : public RHISwapChain
    {
    public:
        DX12SwapChain(ID3D12Device* device, IDXGIFactory4* factory,
                     ID3D12CommandQueue* presentQueue, const RHISwapChainDesc& desc);
        ~DX12SwapChain() override;
        
        u32 GetBufferCount() const override { return m_BufferCount; }
        u32 GetCurrentBackBufferIndex() const override;
        RHIRenderTarget* GetBackBuffer(u32 index) override;
        void Present(bool vsync = true) override;
        void Resize(u32 width, u32 height) override;
        u32 GetWidth() const override { return m_Width; }
        u32 GetHeight() const override { return m_Height; }
        
    private:
        void CreateBackBuffers();
        
        ComPtr<IDXGISwapChain4> m_SwapChain;
        ID3D12Device* m_Device = nullptr;
        std::unique_ptr<DX12DescriptorHeap> m_RTVHeap;
        std::vector<std::unique_ptr<DX12RenderTarget>> m_BackBuffers;
        
        u32 m_BufferCount = 2;
        u32 m_Width = 0;
        u32 m_Height = 0;
        RHIFormat m_Format = RHIFormat::R8G8B8A8_UNORM;
    };

    //=============================================================================
    // DX12Device
    //=============================================================================
    class DX12Device : public RHIDevice
    {
    public:
        DX12Device();
        ~DX12Device() override;
        
        bool Initialize(const RHIDeviceDesc& desc) override;
        void Shutdown() override;
        
        std::string GetAdapterName() const override { return m_AdapterName; }
        u64 GetDedicatedVideoMemory() const override { return m_DedicatedVideoMemory; }
        
        // Resource Creation
        std::unique_ptr<RHIBuffer> CreateBuffer(const RHIBufferDesc& desc) override;
        std::unique_ptr<RHITexture> CreateTexture(const RHITextureDesc& desc) override;
        std::unique_ptr<RHIRenderTarget> CreateRenderTarget(const RHITextureDesc& desc) override;
        std::unique_ptr<RHIDescriptorHeap> CreateDescriptorHeap(
            RHIDescriptorHeapType type, u32 count, bool shaderVisible = true) override;
        std::unique_ptr<RHIPipelineState> CreateGraphicsPipelineState(const void* desc) override;
        std::unique_ptr<RHIPipelineState> CreateComputePipelineState(const void* desc) override;
        std::unique_ptr<RHIRootSignature> CreateRootSignature(const void* desc) override;
        std::unique_ptr<RHIFence> CreateFence(u64 initialValue = 0) override;
        
        // Command List / Queue
        std::unique_ptr<RHICommandQueue> CreateCommandQueue(RHICommandQueueType type) override;
        std::unique_ptr<RHICommandList> CreateCommandList(RHICommandQueueType type) override;
        
        // Swap Chain
        std::unique_ptr<RHISwapChain> CreateSwapChain(
            RHICommandQueue* presentQueue, const RHISwapChainDesc& desc) override;
        
        // Synchronization
        void WaitForIdle() override;
        
        // DX12 specific getters
        ID3D12Device* GetDevice() const { return m_Device.Get(); }
        IDXGIFactory6* GetFactory() const { return m_Factory.Get(); }
        
    private:
        bool CreateFactory();
        bool SelectAdapter();
        bool CreateDevice();
        void EnableDebugLayer();
        
        RHIDeviceDesc m_DeviceDesc;
        
        ComPtr<IDXGIFactory6> m_Factory;
        ComPtr<IDXGIAdapter4> m_Adapter;
        ComPtr<ID3D12Device> m_Device;
        ComPtr<ID3D12Debug> m_DebugController;
        ComPtr<ID3D12InfoQueue> m_InfoQueue;
        
        std::string m_AdapterName;
        u64 m_DedicatedVideoMemory = 0;
        
        // Internal descriptor heaps for render targets
        std::unique_ptr<DX12DescriptorHeap> m_RTVHeap;
        std::unique_ptr<DX12DescriptorHeap> m_DSVHeap;
        std::unique_ptr<DX12DescriptorHeap> m_SRVHeap;
    };

} // namespace Sea
