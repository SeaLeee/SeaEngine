#pragma once

#include "Graphics/GraphicsTypes.h"
#include "Core/Types.h"

namespace Sea
{
    class Device;
    class CommandQueue;

    struct SwapChainDesc
    {
        HWND hwnd = nullptr;
        u32 width = 1920;
        u32 height = 1080;
        u32 bufferCount = 3;
        Format format = Format::R8G8B8A8_UNORM;
        bool vsync = true;
        bool allowTearing = true;
    };

    class SwapChain : public NonCopyable
    {
    public:
        SwapChain(Device& device, CommandQueue& queue, const SwapChainDesc& desc);
        ~SwapChain();

        bool Initialize();
        void Shutdown();

        void Present();
        void Resize(u32 width, u32 height);

        u32 GetCurrentBackBufferIndex() const;
        ID3D12Resource* GetCurrentBackBuffer() const;
        ID3D12Resource* GetBackBuffer(u32 index) const;
        D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV() const;

        u32 GetWidth() const { return m_Width; }
        u32 GetHeight() const { return m_Height; }
        Format GetFormat() const { return m_Format; }
        u32 GetBufferCount() const { return m_BufferCount; }

    private:
        bool CreateSwapChain();
        bool CreateRTVHeap();
        void CreateRenderTargetViews();
        void ReleaseBackBuffers();

    private:
        Device& m_Device;
        CommandQueue& m_Queue;
        
        ComPtr<IDXGISwapChain4> m_SwapChain;
        ComPtr<ID3D12DescriptorHeap> m_RTVHeap;
        std::vector<ComPtr<ID3D12Resource>> m_BackBuffers;

        HWND m_Hwnd = nullptr;
        u32 m_Width = 0;
        u32 m_Height = 0;
        u32 m_BufferCount = 3;
        Format m_Format = Format::R8G8B8A8_UNORM;
        bool m_VSync = true;
        bool m_TearingSupported = false;
        u32 m_RTVDescriptorSize = 0;
    };
}
