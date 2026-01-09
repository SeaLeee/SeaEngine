#include "Graphics/SwapChain.h"
#include "Graphics/Device.h"
#include "Graphics/CommandQueue.h"
#include "Graphics/d3dx12.h"
#include "Core/Log.h"

namespace Sea
{
    SwapChain::SwapChain(Device& device, CommandQueue& queue, const SwapChainDesc& desc)
        : m_Device(device)
        , m_Queue(queue)
        , m_Hwnd(desc.hwnd)
        , m_Width(desc.width)
        , m_Height(desc.height)
        , m_BufferCount(desc.bufferCount)
        , m_Format(desc.format)
        , m_VSync(desc.vsync)
    {
        // 检查tearing支持
        if (desc.allowTearing)
        {
            ComPtr<IDXGIFactory5> factory5;
            if (SUCCEEDED(device.GetFactory()->QueryInterface(IID_PPV_ARGS(&factory5))))
            {
                BOOL allowTearing = FALSE;
                if (SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, 
                                                            &allowTearing, sizeof(allowTearing))))
                {
                    m_TearingSupported = (allowTearing == TRUE);
                }
            }
        }
    }

    SwapChain::~SwapChain()
    {
        Shutdown();
    }

    bool SwapChain::Initialize()
    {
        if (!CreateSwapChain())
            return false;

        if (!CreateRTVHeap())
            return false;

        CreateRenderTargetViews();

        SEA_CORE_INFO("SwapChain created: {}x{} ({} buffers)", m_Width, m_Height, m_BufferCount);
        return true;
    }

    void SwapChain::Shutdown()
    {
        ReleaseBackBuffers();
        m_RTVHeap.Reset();
        m_SwapChain.Reset();
    }

    bool SwapChain::CreateSwapChain()
    {
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = m_Width;
        swapChainDesc.Height = m_Height;
        swapChainDesc.Format = static_cast<DXGI_FORMAT>(m_Format);
        swapChainDesc.Stereo = FALSE;
        swapChainDesc.SampleDesc = { 1, 0 };
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = m_BufferCount;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapChainDesc.Flags = m_TearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

        ComPtr<IDXGISwapChain1> swapChain1;
        HRESULT hr = m_Device.GetFactory()->CreateSwapChainForHwnd(
            m_Queue.GetQueue(),
            m_Hwnd,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1
        );

        if (FAILED(hr))
        {
            SEA_CORE_ERROR("Failed to create swap chain");
            return false;
        }

        // 禁用Alt+Enter全屏切换
        m_Device.GetFactory()->MakeWindowAssociation(m_Hwnd, DXGI_MWA_NO_ALT_ENTER);

        swapChain1.As(&m_SwapChain);

        return true;
    }

    bool SwapChain::CreateRTVHeap()
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heapDesc.NumDescriptors = m_BufferCount;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        HRESULT hr = m_Device.GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_RTVHeap));
        if (FAILED(hr))
        {
            SEA_CORE_ERROR("Failed to create RTV descriptor heap");
            return false;
        }

        m_RTVDescriptorSize = m_Device.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        return true;
    }

    void SwapChain::CreateRenderTargetViews()
    {
        m_BackBuffers.resize(m_BufferCount);
        
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RTVHeap->GetCPUDescriptorHandleForHeapStart());

        for (u32 i = 0; i < m_BufferCount; ++i)
        {
            HRESULT hr = m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_BackBuffers[i]));
            if (FAILED(hr))
            {
                SEA_CORE_ERROR("Failed to get swap chain buffer {}", i);
                continue;
            }

            m_Device.GetDevice()->CreateRenderTargetView(m_BackBuffers[i].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_RTVDescriptorSize);
        }
    }

    void SwapChain::ReleaseBackBuffers()
    {
        for (auto& buffer : m_BackBuffers)
        {
            buffer.Reset();
        }
        m_BackBuffers.clear();
    }

    void SwapChain::Present()
    {
        UINT syncInterval = m_VSync ? 1 : 0;
        UINT presentFlags = (!m_VSync && m_TearingSupported) ? DXGI_PRESENT_ALLOW_TEARING : 0;
        
        m_SwapChain->Present(syncInterval, presentFlags);
    }

    void SwapChain::Resize(u32 width, u32 height)
    {
        if (width == 0 || height == 0)
            return;

        if (width == m_Width && height == m_Height)
            return;

        m_Width = width;
        m_Height = height;

        ReleaseBackBuffers();

        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        m_SwapChain->GetDesc(&swapChainDesc);

        HRESULT hr = m_SwapChain->ResizeBuffers(
            m_BufferCount,
            m_Width,
            m_Height,
            swapChainDesc.BufferDesc.Format,
            swapChainDesc.Flags
        );

        if (FAILED(hr))
        {
            SEA_CORE_ERROR("Failed to resize swap chain");
            return;
        }

        CreateRenderTargetViews();
        SEA_CORE_INFO("SwapChain resized: {}x{}", m_Width, m_Height);
    }

    u32 SwapChain::GetCurrentBackBufferIndex() const
    {
        return m_SwapChain->GetCurrentBackBufferIndex();
    }

    ID3D12Resource* SwapChain::GetCurrentBackBuffer() const
    {
        return m_BackBuffers[GetCurrentBackBufferIndex()].Get();
    }

    ID3D12Resource* SwapChain::GetBackBuffer(u32 index) const
    {
        if (index >= m_BackBuffers.size())
            return nullptr;
        return m_BackBuffers[index].Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::GetCurrentRTV() const
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RTVHeap->GetCPUDescriptorHandleForHeapStart());
        rtvHandle.Offset(GetCurrentBackBufferIndex(), m_RTVDescriptorSize);
        return rtvHandle;
    }
}
