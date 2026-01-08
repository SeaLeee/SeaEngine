#include "Editor/ImGuiRenderer.h"
#include "Graphics/Device.h"
#include "Core/Window.h"
#include "Core/Log.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

namespace Sea
{
    ImGuiRenderer::ImGuiRenderer(Device& device, Window& window)
        : m_Device(device), m_Window(window) {}

    ImGuiRenderer::~ImGuiRenderer() { Shutdown(); }

    bool ImGuiRenderer::Initialize(u32 numFrames, Format rtvFormat)
    {
        // 创建SRV描述符堆
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 1;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        if (FAILED(m_Device.GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_SrvHeap))))
        {
            SEA_CORE_ERROR("Failed to create ImGui SRV heap");
            return false;
        }

        // 初始化ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        // 设置样式
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 5.0f;
        style.FrameRounding = 3.0f;
        style.Colors[ImGuiCol_WindowBg].w = 0.95f;

        // 初始化平台后端
        ImGui_ImplWin32_Init(m_Window.GetHandle());
        ImGui_ImplDX12_Init(m_Device.GetDevice(), numFrames,
            static_cast<DXGI_FORMAT>(rtvFormat),
            m_SrvHeap.Get(),
            m_SrvHeap->GetCPUDescriptorHandleForHeapStart(),
            m_SrvHeap->GetGPUDescriptorHandleForHeapStart());

        SEA_CORE_INFO("ImGui initialized");
        return true;
    }

    void ImGuiRenderer::Shutdown()
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        m_SrvHeap.Reset();
    }

    void ImGuiRenderer::BeginFrame()
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiRenderer::EndFrame()
    {
        ImGui::Render();
    }

    void ImGuiRenderer::Render(ID3D12GraphicsCommandList* cmdList)
    {
        ID3D12DescriptorHeap* heaps[] = { m_SrvHeap.Get() };
        cmdList->SetDescriptorHeaps(1, heaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);

        // 更新额外平台窗口
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault(nullptr, cmdList);
        }
    }
}
