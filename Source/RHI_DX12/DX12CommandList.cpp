#include "DX12RHI.h"
#include <stdexcept>

namespace Sea
{
    //=============================================================================
    // DX12CommandList Implementation
    //=============================================================================
    DX12CommandList::DX12CommandList(ID3D12Device* device, RHICommandQueueType type)
        : m_Type(type)
    {
        D3D12_COMMAND_LIST_TYPE cmdType = ConvertToD3D12CommandListType(type);
        
        HRESULT hr = device->CreateCommandAllocator(cmdType, IID_PPV_ARGS(&m_Allocator));
        if (FAILED(hr)) return;
        
        hr = device->CreateCommandList(0, cmdType, m_Allocator.Get(), nullptr, IID_PPV_ARGS(&m_CommandList));
        if (FAILED(hr))
        {
            m_Allocator.Reset();
            return;
        }
        
        // Command list starts in recording state, close it
        m_CommandList->Close();
    }
    
    DX12CommandList::~DX12CommandList() = default;
    
    void DX12CommandList::Reset()
    {
        m_PendingBarriers.clear();
        m_Allocator->Reset();
        m_CommandList->Reset(m_Allocator.Get(), nullptr);
    }
    
    void DX12CommandList::Close()
    {
        FlushBarriers();
        m_CommandList->Close();
    }
    
    void DX12CommandList::TransitionBarrier(RHITexture* resource, RHIResourceState before, RHIResourceState after)
    {
        if (!resource) return;
        
        DX12Texture* dx12Tex = static_cast<DX12Texture*>(resource);
        if (!dx12Tex->GetResource()) return;
        
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = dx12Tex->GetResource();
        barrier.Transition.StateBefore = ConvertToD3D12ResourceState(before);
        barrier.Transition.StateAfter = ConvertToD3D12ResourceState(after);
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        
        m_PendingBarriers.push_back(barrier);
    }
    
    void DX12CommandList::TransitionBarrier(RHIBuffer* resource, RHIResourceState before, RHIResourceState after)
    {
        if (!resource) return;
        
        DX12Buffer* dx12Buf = static_cast<DX12Buffer*>(resource);
        if (!dx12Buf->GetResource()) return;
        
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = dx12Buf->GetResource();
        barrier.Transition.StateBefore = ConvertToD3D12ResourceState(before);
        barrier.Transition.StateAfter = ConvertToD3D12ResourceState(after);
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        
        m_PendingBarriers.push_back(barrier);
    }
    
    void DX12CommandList::UAVBarrier(RHIResource* resource)
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = nullptr; // NULL means barrier on all UAV resources
        
        // TODO: Get actual resource if needed
        m_PendingBarriers.push_back(barrier);
    }
    
    void DX12CommandList::FlushBarriers()
    {
        if (!m_PendingBarriers.empty())
        {
            m_CommandList->ResourceBarrier(
                static_cast<UINT>(m_PendingBarriers.size()),
                m_PendingBarriers.data());
            m_PendingBarriers.clear();
        }
    }
    
    void DX12CommandList::ClearRenderTarget(RHIDescriptorHandle rtv, const f32 color[4])
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle;
        handle.ptr = rtv.cpuHandle;
        m_CommandList->ClearRenderTargetView(handle, color, 0, nullptr);
    }
    
    void DX12CommandList::ClearDepthStencil(RHIDescriptorHandle dsv, f32 depth, u8 stencil)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle;
        handle.ptr = dsv.cpuHandle;
        m_CommandList->ClearDepthStencilView(
            handle,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            depth, stencil, 0, nullptr);
    }
    
    void DX12CommandList::SetRenderTargets(std::span<RHIDescriptorHandle> rtvs, 
                                           const RHIDescriptorHandle* dsv)
    {
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles;
        rtvHandles.reserve(rtvs.size());
        
        for (const auto& rtv : rtvs)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle;
            handle.ptr = rtv.cpuHandle;
            rtvHandles.push_back(handle);
        }
        
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
        D3D12_CPU_DESCRIPTOR_HANDLE* pDsv = nullptr;
        if (dsv && dsv->cpuHandle != 0)
        {
            dsvHandle.ptr = dsv->cpuHandle;
            pDsv = &dsvHandle;
        }
        
        m_CommandList->OMSetRenderTargets(
            static_cast<UINT>(rtvHandles.size()),
            rtvHandles.empty() ? nullptr : rtvHandles.data(),
            FALSE,
            pDsv);
    }
    
    void DX12CommandList::SetViewport(const RHIViewport& viewport)
    {
        D3D12_VIEWPORT vp;
        vp.TopLeftX = viewport.x;
        vp.TopLeftY = viewport.y;
        vp.Width = viewport.width;
        vp.Height = viewport.height;
        vp.MinDepth = viewport.minDepth;
        vp.MaxDepth = viewport.maxDepth;
        m_CommandList->RSSetViewports(1, &vp);
    }
    
    void DX12CommandList::SetScissorRect(const RHIScissorRect& rect)
    {
        D3D12_RECT r;
        r.left = rect.left;
        r.top = rect.top;
        r.right = rect.right;
        r.bottom = rect.bottom;
        m_CommandList->RSSetScissorRects(1, &r);
    }
    
    void DX12CommandList::SetPipelineState(RHIPipelineState* pso)
    {
        // TODO: Implement when PSO wrapper is ready
        // For now, direct cast if needed
    }
    
    void DX12CommandList::SetGraphicsRootSignature(RHIRootSignature* rootSig)
    {
        // TODO: Implement when RootSignature wrapper is ready
    }
    
    void DX12CommandList::SetComputeRootSignature(RHIRootSignature* rootSig)
    {
        // TODO: Implement when RootSignature wrapper is ready
    }
    
    void DX12CommandList::SetDescriptorHeaps(std::span<RHIDescriptorHeap*> heaps)
    {
        std::vector<ID3D12DescriptorHeap*> dx12Heaps;
        dx12Heaps.reserve(heaps.size());
        
        for (auto* heap : heaps)
        {
            if (auto* dx12Heap = static_cast<DX12DescriptorHeap*>(heap))
            {
                dx12Heaps.push_back(dx12Heap->GetHeap());
            }
        }
        
        if (!dx12Heaps.empty())
        {
            m_CommandList->SetDescriptorHeaps(static_cast<UINT>(dx12Heaps.size()), dx12Heaps.data());
        }
    }
    
    // Root Parameters
    void DX12CommandList::SetGraphicsRootConstant(u32 rootIndex, u32 value, u32 offset)
    {
        m_CommandList->SetGraphicsRoot32BitConstant(rootIndex, value, offset);
    }
    
    void DX12CommandList::SetGraphicsRootConstants(u32 rootIndex, const void* data, u32 count)
    {
        m_CommandList->SetGraphicsRoot32BitConstants(rootIndex, count, data, 0);
    }
    
    void DX12CommandList::SetGraphicsRootCBV(u32 rootIndex, u64 gpuAddress)
    {
        m_CommandList->SetGraphicsRootConstantBufferView(rootIndex, gpuAddress);
    }
    
    void DX12CommandList::SetGraphicsRootSRV(u32 rootIndex, u64 gpuAddress)
    {
        m_CommandList->SetGraphicsRootShaderResourceView(rootIndex, gpuAddress);
    }
    
    void DX12CommandList::SetGraphicsRootUAV(u32 rootIndex, u64 gpuAddress)
    {
        m_CommandList->SetGraphicsRootUnorderedAccessView(rootIndex, gpuAddress);
    }
    
    void DX12CommandList::SetGraphicsRootDescriptorTable(u32 rootIndex, RHIDescriptorHandle baseHandle)
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle;
        handle.ptr = baseHandle.gpuHandle;
        m_CommandList->SetGraphicsRootDescriptorTable(rootIndex, handle);
    }
    
    void DX12CommandList::SetComputeRootConstant(u32 rootIndex, u32 value, u32 offset)
    {
        m_CommandList->SetComputeRoot32BitConstant(rootIndex, value, offset);
    }
    
    void DX12CommandList::SetComputeRootConstants(u32 rootIndex, const void* data, u32 count)
    {
        m_CommandList->SetComputeRoot32BitConstants(rootIndex, count, data, 0);
    }
    
    void DX12CommandList::SetComputeRootCBV(u32 rootIndex, u64 gpuAddress)
    {
        m_CommandList->SetComputeRootConstantBufferView(rootIndex, gpuAddress);
    }
    
    void DX12CommandList::SetComputeRootSRV(u32 rootIndex, u64 gpuAddress)
    {
        m_CommandList->SetComputeRootShaderResourceView(rootIndex, gpuAddress);
    }
    
    void DX12CommandList::SetComputeRootUAV(u32 rootIndex, u64 gpuAddress)
    {
        m_CommandList->SetComputeRootUnorderedAccessView(rootIndex, gpuAddress);
    }
    
    void DX12CommandList::SetComputeRootDescriptorTable(u32 rootIndex, RHIDescriptorHandle baseHandle)
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle;
        handle.ptr = baseHandle.gpuHandle;
        m_CommandList->SetComputeRootDescriptorTable(rootIndex, handle);
    }
    
    // Input Assembly
    void DX12CommandList::SetVertexBuffer(u32 slot, const RHIVertexBufferView& view)
    {
        D3D12_VERTEX_BUFFER_VIEW vbv;
        vbv.BufferLocation = view.gpuAddress;
        vbv.SizeInBytes = view.sizeInBytes;
        vbv.StrideInBytes = view.strideInBytes;
        m_CommandList->IASetVertexBuffers(slot, 1, &vbv);
    }
    
    void DX12CommandList::SetIndexBuffer(const RHIIndexBufferView& view)
    {
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = view.gpuAddress;
        ibv.SizeInBytes = view.sizeInBytes;
        ibv.Format = view.is32Bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
        m_CommandList->IASetIndexBuffer(&ibv);
    }
    
    void DX12CommandList::SetPrimitiveTopology(RHIPrimitiveTopology topology)
    {
        m_CommandList->IASetPrimitiveTopology(ConvertToD3D12PrimitiveTopology(topology));
    }
    
    // Draw Commands
    void DX12CommandList::Draw(u32 vertexCount, u32 instanceCount, u32 startVertex, u32 startInstance)
    {
        m_CommandList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
    }
    
    void DX12CommandList::DrawIndexed(u32 indexCount, u32 instanceCount, u32 startIndex, 
                                      i32 baseVertex, u32 startInstance)
    {
        m_CommandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
    }
    
    // Compute Commands
    void DX12CommandList::Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ)
    {
        m_CommandList->Dispatch(groupCountX, groupCountY, groupCountZ);
    }
    
    // Copy Commands
    void DX12CommandList::CopyBuffer(RHIBuffer* dest, RHIBuffer* src)
    {
        auto* dx12Dest = static_cast<DX12Buffer*>(dest);
        auto* dx12Src = static_cast<DX12Buffer*>(src);
        
        if (dx12Dest && dx12Src)
        {
            m_CommandList->CopyResource(dx12Dest->GetResource(), dx12Src->GetResource());
        }
    }
    
    void DX12CommandList::CopyBufferRegion(RHIBuffer* dest, u64 destOffset, 
                                           RHIBuffer* src, u64 srcOffset, u64 size)
    {
        auto* dx12Dest = static_cast<DX12Buffer*>(dest);
        auto* dx12Src = static_cast<DX12Buffer*>(src);
        
        if (dx12Dest && dx12Src)
        {
            m_CommandList->CopyBufferRegion(
                dx12Dest->GetResource(), destOffset,
                dx12Src->GetResource(), srcOffset,
                size);
        }
    }
    
    void DX12CommandList::CopyTexture(RHITexture* dest, RHITexture* src)
    {
        auto* dx12Dest = static_cast<DX12Texture*>(dest);
        auto* dx12Src = static_cast<DX12Texture*>(src);
        
        if (dx12Dest && dx12Src)
        {
            m_CommandList->CopyResource(dx12Dest->GetResource(), dx12Src->GetResource());
        }
    }
    
    void DX12CommandList::CopyTextureRegion(RHITexture* dest, u32 destX, u32 destY, u32 destZ,
                                            RHITexture* src, const RHISubResource* srcSubresource)
    {
        auto* dx12Dest = static_cast<DX12Texture*>(dest);
        auto* dx12Src = static_cast<DX12Texture*>(src);
        
        if (!dx12Dest || !dx12Src) return;
        
        D3D12_TEXTURE_COPY_LOCATION destLoc = {};
        destLoc.pResource = dx12Dest->GetResource();
        destLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destLoc.SubresourceIndex = 0;
        
        D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
        srcLoc.pResource = dx12Src->GetResource();
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLoc.SubresourceIndex = 0;
        
        if (srcSubresource)
        {
            srcLoc.SubresourceIndex = srcSubresource->mipLevel + 
                                      srcSubresource->arraySlice * src->GetDesc().mipLevels;
        }
        
        m_CommandList->CopyTextureRegion(&destLoc, destX, destY, destZ, &srcLoc, nullptr);
    }
    
    // Debug Markers
    void DX12CommandList::BeginEvent(const char* name)
    {
        std::wstring wname(name, name + strlen(name));
        m_CommandList->BeginEvent(0, wname.c_str(), static_cast<UINT>((wname.size() + 1) * sizeof(wchar_t)));
    }
    
    void DX12CommandList::EndEvent()
    {
        m_CommandList->EndEvent();
    }
    
    void DX12CommandList::SetMarker(const char* name)
    {
        std::wstring wname(name, name + strlen(name));
        m_CommandList->SetMarker(0, wname.c_str(), static_cast<UINT>((wname.size() + 1) * sizeof(wchar_t)));
    }

    //=============================================================================
    // DX12CommandQueue Implementation
    //=============================================================================
    DX12CommandQueue::DX12CommandQueue(ID3D12Device* device, RHICommandQueueType type)
        : m_Type(type)
    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = ConvertToD3D12CommandListType(type);
        desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        
        HRESULT hr = device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_Queue));
        if (FAILED(hr)) return;
        
        hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_IdleFence));
        if (SUCCEEDED(hr))
        {
            m_IdleEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        }
    }
    
    DX12CommandQueue::~DX12CommandQueue()
    {
        WaitForIdle();
        if (m_IdleEvent)
        {
            CloseHandle(m_IdleEvent);
        }
    }
    
    void DX12CommandQueue::ExecuteCommandLists(std::span<RHICommandList*> cmdLists)
    {
        std::vector<ID3D12CommandList*> dx12Lists;
        dx12Lists.reserve(cmdLists.size());
        
        for (auto* cmdList : cmdLists)
        {
            if (auto* dx12CmdList = static_cast<DX12CommandList*>(cmdList))
            {
                dx12Lists.push_back(dx12CmdList->GetCommandList());
            }
        }
        
        if (!dx12Lists.empty())
        {
            m_Queue->ExecuteCommandLists(static_cast<UINT>(dx12Lists.size()), dx12Lists.data());
        }
    }
    
    void DX12CommandQueue::Signal(RHIFence* fence, u64 value)
    {
        if (auto* dx12Fence = static_cast<DX12Fence*>(fence))
        {
            m_Queue->Signal(dx12Fence->GetFence(), value);
        }
    }
    
    void DX12CommandQueue::Wait(RHIFence* fence, u64 value)
    {
        if (auto* dx12Fence = static_cast<DX12Fence*>(fence))
        {
            m_Queue->Wait(dx12Fence->GetFence(), value);
        }
    }
    
    void DX12CommandQueue::WaitForIdle()
    {
        if (!m_Queue || !m_IdleFence || !m_IdleEvent) return;
        
        m_IdleFenceValue++;
        m_Queue->Signal(m_IdleFence.Get(), m_IdleFenceValue);
        
        if (m_IdleFence->GetCompletedValue() < m_IdleFenceValue)
        {
            m_IdleFence->SetEventOnCompletion(m_IdleFenceValue, m_IdleEvent);
            WaitForSingleObject(m_IdleEvent, INFINITE);
        }
    }

    //=============================================================================
    // DX12SwapChain Implementation
    //=============================================================================
    DX12SwapChain::DX12SwapChain(ID3D12Device* device, IDXGIFactory4* factory,
                                 ID3D12CommandQueue* presentQueue, const RHISwapChainDesc& desc)
        : m_Device(device), m_BufferCount(desc.bufferCount), 
          m_Width(desc.width), m_Height(desc.height), m_Format(desc.format)
    {
        // Create RTV heap for swap chain back buffers
        m_RTVHeap = std::make_unique<DX12DescriptorHeap>(
            device, RHIDescriptorHeapType::RTV, desc.bufferCount, false);
        
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = desc.width;
        swapChainDesc.Height = desc.height;
        swapChainDesc.Format = ConvertToDXGIFormat(desc.format);
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = desc.bufferCount;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        
        ComPtr<IDXGISwapChain1> swapChain1;
        HRESULT hr = factory->CreateSwapChainForHwnd(
            presentQueue,
            static_cast<HWND>(desc.windowHandle),
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1);
            
        if (FAILED(hr)) return;
        
        // Disable Alt+Enter fullscreen toggle
        factory->MakeWindowAssociation(static_cast<HWND>(desc.windowHandle), DXGI_MWA_NO_ALT_ENTER);
        
        swapChain1.As(&m_SwapChain);
        
        CreateBackBuffers();
    }
    
    DX12SwapChain::~DX12SwapChain()
    {
        m_BackBuffers.clear();
    }
    
    void DX12SwapChain::CreateBackBuffers()
    {
        m_BackBuffers.clear();
        m_BackBuffers.reserve(m_BufferCount);
        
        for (u32 i = 0; i < m_BufferCount; ++i)
        {
            ComPtr<ID3D12Resource> backBuffer;
            m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer));
            
            RHITextureDesc desc;
            desc.width = m_Width;
            desc.height = m_Height;
            desc.format = m_Format;
            desc.usage = RHITextureUsage::RenderTarget;
            desc.name = "SwapChainBackBuffer" + std::to_string(i);
            
            auto rt = std::make_unique<DX12RenderTarget>(
                backBuffer.Get(), desc, m_Device,
                m_RTVHeap->GetHeap(), i);
                
            m_BackBuffers.push_back(std::move(rt));
        }
    }
    
    u32 DX12SwapChain::GetCurrentBackBufferIndex() const
    {
        return m_SwapChain ? m_SwapChain->GetCurrentBackBufferIndex() : 0;
    }
    
    RHIRenderTarget* DX12SwapChain::GetBackBuffer(u32 index)
    {
        return (index < m_BackBuffers.size()) ? m_BackBuffers[index].get() : nullptr;
    }
    
    void DX12SwapChain::Present(bool vsync)
    {
        if (m_SwapChain)
        {
            UINT syncInterval = vsync ? 1 : 0;
            UINT flags = vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING;
            m_SwapChain->Present(syncInterval, flags);
        }
    }
    
    void DX12SwapChain::Resize(u32 width, u32 height)
    {
        if (width == m_Width && height == m_Height) return;
        
        m_Width = width;
        m_Height = height;
        
        // Release back buffer references
        m_BackBuffers.clear();
        
        // Resize swap chain
        HRESULT hr = m_SwapChain->ResizeBuffers(
            m_BufferCount,
            width, height,
            ConvertToDXGIFormat(m_Format),
            DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
            
        if (SUCCEEDED(hr))
        {
            CreateBackBuffers();
        }
    }

} // namespace Sea
