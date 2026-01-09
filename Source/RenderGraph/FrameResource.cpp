#include "RenderGraph/FrameResource.h"
#include "Graphics/Device.h"
#include "Graphics/Buffer.h"
#include "Core/Log.h"

namespace Sea
{
    // FrameResource实现
    FrameResource::FrameResource() = default;
    FrameResource::~FrameResource() { Shutdown(); }

    void FrameResource::Initialize(Device* device, u32 frameIndex)
    {
        m_Device = device;
        m_FrameIndex = frameIndex;
        m_FenceValue = 0;
        SEA_CORE_TRACE("FrameResource {} initialized", frameIndex);
    }

    void FrameResource::Shutdown()
    {
        m_ConstantBuffers.clear();
        m_DynamicVBs.clear();
        m_DynamicIBs.clear();
    }

    Buffer* FrameResource::AllocateConstantBuffer(size_t size)
    {
        if (m_CurrentCBIndex < m_ConstantBuffers.size())
        {
            return m_ConstantBuffers[m_CurrentCBIndex++].get();
        }

        // 创建新的常量缓冲区
        BufferDesc desc;
        desc.size = size;
        desc.type = BufferType::Constant;
        desc.stride = 0;
        desc.name = "ConstantBuffer_" + std::to_string(m_FrameIndex) + "_" + std::to_string(m_CurrentCBIndex);

        auto buffer = std::make_unique<Buffer>(*m_Device, desc);
        if (buffer->Initialize(nullptr))
        {
            Buffer* ptr = buffer.get();
            m_ConstantBuffers.push_back(std::move(buffer));
            m_CurrentCBIndex++;
            return ptr;
        }
        return nullptr;
    }

    void FrameResource::ResetConstantBuffers()
    {
        m_CurrentCBIndex = 0;
    }

    Buffer* FrameResource::AllocateDynamicVertexBuffer(size_t size)
    {
        if (m_CurrentVBIndex < m_DynamicVBs.size())
        {
            return m_DynamicVBs[m_CurrentVBIndex++].get();
        }

        BufferDesc desc;
        desc.size = size;
        desc.type = BufferType::Vertex;
        desc.stride = 0;
        desc.name = "DynamicVB_" + std::to_string(m_FrameIndex) + "_" + std::to_string(m_CurrentVBIndex);

        auto buffer = std::make_unique<Buffer>(*m_Device, desc);
        if (buffer->Initialize(nullptr))
        {
            Buffer* ptr = buffer.get();
            m_DynamicVBs.push_back(std::move(buffer));
            m_CurrentVBIndex++;
            return ptr;
        }
        return nullptr;
    }

    Buffer* FrameResource::AllocateDynamicIndexBuffer(size_t size)
    {
        if (m_CurrentIBIndex < m_DynamicIBs.size())
        {
            return m_DynamicIBs[m_CurrentIBIndex++].get();
        }

        BufferDesc desc;
        desc.size = size;
        desc.type = BufferType::Index;
        desc.stride = sizeof(u32);
        desc.name = "DynamicIB_" + std::to_string(m_FrameIndex) + "_" + std::to_string(m_CurrentIBIndex);

        auto buffer = std::make_unique<Buffer>(*m_Device, desc);
        if (buffer->Initialize(nullptr))
        {
            Buffer* ptr = buffer.get();
            m_DynamicIBs.push_back(std::move(buffer));
            m_CurrentIBIndex++;
            return ptr;
        }
        return nullptr;
    }

    void FrameResource::ResetDynamicBuffers()
    {
        m_CurrentVBIndex = 0;
        m_CurrentIBIndex = 0;
    }

    u32 FrameResource::AllocateSRV()
    {
        return m_NextSRVIndex++;
    }

    u32 FrameResource::AllocateRTV()
    {
        return m_NextRTVIndex++;
    }

    void FrameResource::ResetDescriptors()
    {
        m_NextSRVIndex = 0;
        m_NextRTVIndex = 0;
    }

    void FrameResource::BeginFrame()
    {
        ResetConstantBuffers();
        ResetDynamicBuffers();
        ResetDescriptors();
    }

    void FrameResource::EndFrame()
    {
        // 当前帧结束，等待GPU完成再重用资源
    }

    // FrameResourceManager实现
    FrameResourceManager::FrameResourceManager() = default;
    FrameResourceManager::~FrameResourceManager() { Shutdown(); }

    void FrameResourceManager::Initialize(Device* device)
    {
        m_Device = device;
        for (u32 i = 0; i < kMaxFramesInFlight; ++i)
        {
            m_Frames[i] = std::make_unique<FrameResource>();
            m_Frames[i]->Initialize(device, i);
        }
        m_CurrentFrameIndex = 0;
        SEA_CORE_INFO("FrameResourceManager initialized with {} frames", kMaxFramesInFlight);
    }

    void FrameResourceManager::Shutdown()
    {
        for (auto& frame : m_Frames)
        {
            if (frame) frame->Shutdown();
        }
    }

    FrameResource& FrameResourceManager::GetCurrentFrame()
    {
        return *m_Frames[m_CurrentFrameIndex];
    }

    FrameResource& FrameResourceManager::GetFrame(u32 index)
    {
        return *m_Frames[index % kMaxFramesInFlight];
    }

    void FrameResourceManager::BeginFrame()
    {
        GetCurrentFrame().BeginFrame();
    }

    void FrameResourceManager::EndFrame()
    {
        GetCurrentFrame().EndFrame();
        m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % kMaxFramesInFlight;
    }

    void FrameResourceManager::WaitForFrame(u32 index)
    {
        // 由外部通过Fence实现等待
        (void)index;
    }
}
