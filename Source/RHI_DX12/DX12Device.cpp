#include "DX12RHI.h"
#include <stdexcept>

namespace Sea
{
    //=============================================================================
    // RHIDevice Factory
    //=============================================================================
    std::unique_ptr<RHIDevice> RHIDevice::Create()
    {
        return std::make_unique<DX12Device>();
    }

    //=============================================================================
    // DX12Device Implementation
    //=============================================================================
    DX12Device::DX12Device() = default;
    
    DX12Device::~DX12Device()
    {
        Shutdown();
    }
    
    bool DX12Device::Initialize(const RHIDeviceDesc& desc)
    {
        m_DeviceDesc = desc;
        
        if (desc.enableDebugLayer)
        {
            EnableDebugLayer();
        }
        
        if (!CreateFactory()) return false;
        if (!SelectAdapter()) return false;
        if (!CreateDevice()) return false;
        
        // Create internal descriptor heaps
        m_RTVHeap = std::make_unique<DX12DescriptorHeap>(
            m_Device.Get(), RHIDescriptorHeapType::RTV, 256, false);
        m_DSVHeap = std::make_unique<DX12DescriptorHeap>(
            m_Device.Get(), RHIDescriptorHeapType::DSV, 64, false);
        m_SRVHeap = std::make_unique<DX12DescriptorHeap>(
            m_Device.Get(), RHIDescriptorHeapType::CBV_SRV_UAV, 4096, true);
        
        return true;
    }
    
    void DX12Device::Shutdown()
    {
        WaitForIdle();
        
        m_SRVHeap.reset();
        m_DSVHeap.reset();
        m_RTVHeap.reset();
        m_InfoQueue.Reset();
        m_Device.Reset();
        m_Adapter.Reset();
        m_Factory.Reset();
        m_DebugController.Reset();
    }
    
    void DX12Device::EnableDebugLayer()
    {
#if defined(_DEBUG) || defined(DEBUG)
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_DebugController))))
        {
            m_DebugController->EnableDebugLayer();
            
            // Enable GPU-based validation (slower but more thorough)
            ComPtr<ID3D12Debug1> debugController1;
            if (SUCCEEDED(m_DebugController.As(&debugController1)))
            {
                // debugController1->SetEnableGPUBasedValidation(TRUE);
            }
        }
#endif
    }
    
    bool DX12Device::CreateFactory()
    {
        UINT factoryFlags = 0;
#if defined(_DEBUG) || defined(DEBUG)
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
        
        HRESULT hr = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_Factory));
        return SUCCEEDED(hr);
    }
    
    bool DX12Device::SelectAdapter()
    {
        ComPtr<IDXGIAdapter1> adapter;
        
        // Try to find the best GPU
        for (UINT i = 0;
             m_Factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                   IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
             ++i)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);
            
            // Skip software adapter
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
            
            // Check if adapter supports D3D12
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                                           __uuidof(ID3D12Device), nullptr)))
            {
                adapter.As(&m_Adapter);
                
                // Convert adapter name
                char name[256];
                WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, 256, nullptr, nullptr);
                m_AdapterName = name;
                m_DedicatedVideoMemory = desc.DedicatedVideoMemory;
                
                return true;
            }
        }
        
        return false;
    }
    
    bool DX12Device::CreateDevice()
    {
        HRESULT hr = D3D12CreateDevice(
            m_Adapter.Get(),
            D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(&m_Device));
            
        if (FAILED(hr)) return false;
        
        m_Device->SetName(L"SeaEngine Device");
        
#if defined(_DEBUG) || defined(DEBUG)
        // Setup debug info queue
        if (SUCCEEDED(m_Device.As(&m_InfoQueue)))
        {
            m_InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            m_InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
            
            // Suppress specific messages
            D3D12_MESSAGE_ID suppressIds[] = {
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
            };
            
            D3D12_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = _countof(suppressIds);
            filter.DenyList.pIDList = suppressIds;
            m_InfoQueue->AddStorageFilterEntries(&filter);
        }
#endif
        
        return true;
    }
    
    std::unique_ptr<RHIBuffer> DX12Device::CreateBuffer(const RHIBufferDesc& desc)
    {
        auto buffer = std::make_unique<DX12Buffer>(m_Device.Get(), desc);
        return buffer->IsValid() ? std::move(buffer) : nullptr;
    }
    
    std::unique_ptr<RHITexture> DX12Device::CreateTexture(const RHITextureDesc& desc)
    {
        auto texture = std::make_unique<DX12Texture>(m_Device.Get(), desc);
        return texture->IsValid() ? std::move(texture) : nullptr;
    }
    
    std::unique_ptr<RHIRenderTarget> DX12Device::CreateRenderTarget(const RHITextureDesc& desc)
    {
        u32 rtvIndex = UINT32_MAX;
        u32 dsvIndex = UINT32_MAX;
        u32 srvIndex = UINT32_MAX;
        
        ID3D12DescriptorHeap* rtvHeap = nullptr;
        ID3D12DescriptorHeap* dsvHeap = nullptr;
        ID3D12DescriptorHeap* srvHeap = nullptr;
        
        if (desc.usage & RHITextureUsage::RenderTarget)
        {
            rtvIndex = m_RTVHeap->Allocate();
            rtvHeap = m_RTVHeap->GetHeap();
        }
        
        if (desc.usage & RHITextureUsage::DepthStencil)
        {
            dsvIndex = m_DSVHeap->Allocate();
            dsvHeap = m_DSVHeap->GetHeap();
        }
        
        if (desc.usage & RHITextureUsage::ShaderResource)
        {
            srvIndex = m_SRVHeap->Allocate();
            srvHeap = m_SRVHeap->GetHeap();
        }
        
        auto rt = std::make_unique<DX12RenderTarget>(
            m_Device.Get(), desc, rtvHeap, rtvIndex, dsvHeap, dsvIndex, srvHeap, srvIndex);
            
        return rt->IsValid() ? std::move(rt) : nullptr;
    }
    
    std::unique_ptr<RHIDescriptorHeap> DX12Device::CreateDescriptorHeap(
        RHIDescriptorHeapType type, u32 count, bool shaderVisible)
    {
        auto heap = std::make_unique<DX12DescriptorHeap>(m_Device.Get(), type, count, shaderVisible);
        return heap->IsValid() ? std::move(heap) : nullptr;
    }
    
    std::unique_ptr<RHIPipelineState> DX12Device::CreateGraphicsPipelineState(const void* desc)
    {
        // TODO: Implement PSO creation wrapper
        return nullptr;
    }
    
    std::unique_ptr<RHIPipelineState> DX12Device::CreateComputePipelineState(const void* desc)
    {
        // TODO: Implement PSO creation wrapper
        return nullptr;
    }
    
    std::unique_ptr<RHIRootSignature> DX12Device::CreateRootSignature(const void* desc)
    {
        // TODO: Implement root signature wrapper
        return nullptr;
    }
    
    std::unique_ptr<RHIFence> DX12Device::CreateFence(u64 initialValue)
    {
        auto fence = std::make_unique<DX12Fence>(m_Device.Get(), initialValue);
        return fence->IsValid() ? std::move(fence) : nullptr;
    }
    
    std::unique_ptr<RHICommandQueue> DX12Device::CreateCommandQueue(RHICommandQueueType type)
    {
        return std::make_unique<DX12CommandQueue>(m_Device.Get(), type);
    }
    
    std::unique_ptr<RHICommandList> DX12Device::CreateCommandList(RHICommandQueueType type)
    {
        return std::make_unique<DX12CommandList>(m_Device.Get(), type);
    }
    
    std::unique_ptr<RHISwapChain> DX12Device::CreateSwapChain(
        RHICommandQueue* presentQueue, const RHISwapChainDesc& desc)
    {
        auto* dx12Queue = static_cast<DX12CommandQueue*>(presentQueue);
        if (!dx12Queue) return nullptr;
        
        return std::make_unique<DX12SwapChain>(
            m_Device.Get(), m_Factory.Get(), dx12Queue->GetQueue(), desc);
    }
    
    void DX12Device::WaitForIdle()
    {
        // Create a temporary fence and wait
        ComPtr<ID3D12Fence> fence;
        if (FAILED(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
            return;
            
        HANDLE event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        if (!event) return;
        
        // We need a queue to signal - create a temporary direct queue
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        
        ComPtr<ID3D12CommandQueue> queue;
        if (SUCCEEDED(m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue))))
        {
            queue->Signal(fence.Get(), 1);
            fence->SetEventOnCompletion(1, event);
            WaitForSingleObject(event, INFINITE);
        }
        
        CloseHandle(event);
    }

} // namespace Sea
