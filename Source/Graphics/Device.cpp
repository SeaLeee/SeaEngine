#include "Graphics/Device.h"
#include "Core/Log.h"

namespace Sea
{
    Device* Device::s_Instance = nullptr;

    Device::Device(const DeviceDesc& desc)
        : m_Desc(desc)
    {
        SEA_ASSERT(!s_Instance, "Device already exists!");
        s_Instance = this;
    }

    Device::~Device()
    {
        Shutdown();
        s_Instance = nullptr;
    }

    bool Device::Initialize()
    {
        SEA_CORE_INFO("Initializing D3D12 Device...");

        if (m_Desc.enableDebugLayer)
        {
            EnableDebugLayer();
        }

        if (!CreateFactory())
            return false;

        if (!SelectAdapter())
            return false;

        if (!CreateDevice())
            return false;

        SEA_CORE_INFO("D3D12 Device initialized successfully");
        SEA_CORE_INFO("  Adapter: {}", m_AdapterName);
        SEA_CORE_INFO("  VRAM: {} MB", m_DedicatedVideoMemory / (1024 * 1024));

        return true;
    }

    void Device::Shutdown()
    {
        WaitForIdle();

        m_Device.Reset();
        m_Adapter.Reset();
        m_Factory.Reset();
        m_DebugController.Reset();
        m_InfoQueue.Reset();
    }

    void Device::EnableDebugLayer()
    {
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_DebugController))))
        {
            m_DebugController->EnableDebugLayer();
            SEA_CORE_INFO("D3D12 Debug Layer Enabled");

            if (m_Desc.enableGPUValidation)
            {
                ComPtr<ID3D12Debug1> debug1;
                if (SUCCEEDED(m_DebugController.As(&debug1)))
                {
                    debug1->SetEnableGPUBasedValidation(TRUE);
                    SEA_CORE_INFO("GPU-Based Validation Enabled");
                }
            }
        }
    }

    bool Device::CreateFactory()
    {
        UINT factoryFlags = 0;
        if (m_Desc.enableDebugLayer)
        {
            factoryFlags = DXGI_CREATE_FACTORY_DEBUG;
        }

        HRESULT hr = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_Factory));
        if (FAILED(hr))
        {
            SEA_CORE_ERROR("Failed to create DXGI factory");
            return false;
        }
        return true;
    }

    bool Device::SelectAdapter()
    {
        ComPtr<IDXGIAdapter1> adapter1;
        
        if (m_Desc.preferHighPerformanceAdapter)
        {
            for (UINT i = 0; 
                 m_Factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, 
                                                        IID_PPV_ARGS(&adapter1)) != DXGI_ERROR_NOT_FOUND; 
                 ++i)
            {
                DXGI_ADAPTER_DESC1 desc;
                adapter1->GetDesc1(&desc);

                // 跳过软件适配器
                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                    continue;

                // 检查是否支持D3D12
                if (SUCCEEDED(D3D12CreateDevice(adapter1.Get(), D3D_FEATURE_LEVEL_12_0,
                                                _uuidof(ID3D12Device), nullptr)))
                {
                    adapter1.As(&m_Adapter);
                    
                    char name[128];
                    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, 128, nullptr, nullptr);
                    m_AdapterName = name;
                    m_DedicatedVideoMemory = desc.DedicatedVideoMemory;
                    return true;
                }
            }
        }

        // 回退到枚举所有适配器
        for (UINT i = 0; m_Factory->EnumAdapters1(i, &adapter1) != DXGI_ERROR_NOT_FOUND; ++i)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter1->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;

            if (SUCCEEDED(D3D12CreateDevice(adapter1.Get(), D3D_FEATURE_LEVEL_12_0,
                                            _uuidof(ID3D12Device), nullptr)))
            {
                adapter1.As(&m_Adapter);
                
                char name[128];
                WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, 128, nullptr, nullptr);
                m_AdapterName = name;
                m_DedicatedVideoMemory = desc.DedicatedVideoMemory;
                return true;
            }
        }

        SEA_CORE_ERROR("No suitable D3D12 adapter found");
        return false;
    }

    bool Device::CreateDevice()
    {
        HRESULT hr = D3D12CreateDevice(
            m_Adapter.Get(),
            D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(&m_Device)
        );

        if (FAILED(hr))
        {
            SEA_CORE_ERROR("Failed to create D3D12 device");
            return false;
        }

        // 设置调试信息队列
        if (m_Desc.enableDebugLayer)
        {
            if (SUCCEEDED(m_Device.As(&m_InfoQueue)))
            {
                m_InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
                m_InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

                // 过滤一些不重要的警告
                D3D12_MESSAGE_ID hide[] = {
                    D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                    D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
                };

                D3D12_INFO_QUEUE_FILTER filter = {};
                filter.DenyList.NumIDs = _countof(hide);
                filter.DenyList.pIDList = hide;
                m_InfoQueue->AddStorageFilterEntries(&filter);
            }
        }

        return true;
    }

    ComPtr<ID3D12Resource> Device::CreateCommittedResource(
        const D3D12_HEAP_PROPERTIES& heapProps,
        D3D12_HEAP_FLAGS heapFlags,
        const D3D12_RESOURCE_DESC& resourceDesc,
        D3D12_RESOURCE_STATES initialState,
        const D3D12_CLEAR_VALUE* clearValue)
    {
        ComPtr<ID3D12Resource> resource;
        
        SEA_CORE_INFO("CreateCommittedResource: Dim={}, {}x{}x{}, Format={}, Flags={}, State={}",
            static_cast<int>(resourceDesc.Dimension),
            resourceDesc.Width, resourceDesc.Height, resourceDesc.DepthOrArraySize,
            static_cast<int>(resourceDesc.Format),
            static_cast<int>(resourceDesc.Flags),
            static_cast<int>(initialState));
        
        HRESULT hr = m_Device->CreateCommittedResource(
            &heapProps,
            heapFlags,
            &resourceDesc,
            initialState,
            clearValue,
            IID_PPV_ARGS(&resource)
        );

        if (FAILED(hr))
        {
            SEA_CORE_ERROR("Failed to create committed resource, HRESULT: 0x{:08X}", static_cast<unsigned int>(hr));
            return nullptr;
        }

        return resource;
    }

    void Device::WaitForIdle()
    {
        // 注意: 这个方法不能正确等待GPU空闲，因为没有命令队列来发送fence信号
        // 实际的GPU同步应该通过CommandQueue::WaitForIdle()来完成
        // 这里只是一个占位实现，避免崩溃
        if (!m_Device)
            return;
    }
}
