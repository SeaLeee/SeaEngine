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

        // ע��һ���������� ImGui::Image()������ GPU handle
        D3D12_GPU_DESCRIPTOR_HANDLE RegisterTexture(ID3D12Resource* texture, DXGI_FORMAT format);
        
        // ��ȡ SRV ��
        ID3D12DescriptorHeap* GetSrvHeap() const { return m_SrvHeap.Get(); }

    private:
        Device& m_Device;
        Window& m_Window;
        ComPtr<ID3D12DescriptorHeap> m_SrvHeap;
        u32 m_SrvDescriptorSize = 0;
        u32 m_NextDescriptorIndex = 1; // 0 is reserved for font texture
        bool m_FrameActive = false;    // 追踪帧是否正在进行
        static constexpr u32 MAX_DESCRIPTORS = 100;
    };
}
