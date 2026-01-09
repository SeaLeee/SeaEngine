#pragma once
#include "Core/Types.h"
#include "Graphics/GraphicsTypes.h"

namespace Sea
{
    class Device;
    class CommandQueue;
    class Window;

    class ImGuiRenderer : public NonCopyable
    {
    public:
        ImGuiRenderer(Device& device, Window& window);
        ~ImGuiRenderer();

        bool Initialize(u32 numFrames, Format rtvFormat);
        void Shutdown();

        void BeginFrame();
        void EndFrame();
        void Render(ID3D12GraphicsCommandList* cmdList);

        // 注册一个纹理用于 ImGui::Image()，返回 GPU handle
        D3D12_GPU_DESCRIPTOR_HANDLE RegisterTexture(ID3D12Resource* texture, DXGI_FORMAT format);
        
        // 获取 SRV 堆
        ID3D12DescriptorHeap* GetSrvHeap() const { return m_SrvHeap.Get(); }

    private:
        Device& m_Device;
        Window& m_Window;
        ComPtr<ID3D12DescriptorHeap> m_SrvHeap;
        u32 m_SrvDescriptorSize = 0;
        u32 m_NextDescriptorIndex = 1; // 0 is reserved for font texture
        static constexpr u32 MAX_DESCRIPTORS = 100;
    };
}
