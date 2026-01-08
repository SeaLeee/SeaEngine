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

    private:
        Device& m_Device;
        Window& m_Window;
        ComPtr<ID3D12DescriptorHeap> m_SrvHeap;
    };
}
