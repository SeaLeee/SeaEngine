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
        : m_Device(device), m_Window(window) {
    }

    ImGuiRenderer::~ImGuiRenderer() { Shutdown(); }

    bool ImGuiRenderer::Initialize(u32 numFrames, Format rtvFormat)
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = MAX_DESCRIPTORS;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        if (FAILED(m_Device.GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_SrvHeap))))
        {
            SEA_CORE_ERROR("Failed to create ImGui SRV heap");
            return false;
        }

        m_SrvDescriptorSize = m_Device.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 5.0f;
        style.FrameRounding = 3.0f;
        style.Colors[ImGuiCol_WindowBg].w = 0.95f;

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

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault(nullptr, cmdList);
        }
    }

    D3D12_GPU_DESCRIPTOR_HANDLE ImGuiRenderer::RegisterTexture(ID3D12Resource* texture, DXGI_FORMAT format)
    {
        if (m_NextDescriptorIndex >= MAX_DESCRIPTORS)
        {
            SEA_CORE_ERROR("ImGui SRV heap is full!");
            return { 0 };
        }

        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_SrvHeap->GetCPUDescriptorHandleForHeapStart();
        cpuHandle.ptr += m_NextDescriptorIndex * m_SrvDescriptorSize;

        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_SrvHeap->GetGPUDescriptorHandleForHeapStart();
        gpuHandle.ptr += m_NextDescriptorIndex * m_SrvDescriptorSize;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;

        m_Device.GetDevice()->CreateShaderResourceView(texture, &srvDesc, cpuHandle);
        m_NextDescriptorIndex++;

        return gpuHandle;
    }
}