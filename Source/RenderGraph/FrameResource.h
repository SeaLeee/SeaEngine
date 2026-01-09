#pragma once
#include "Core/Types.h"
#include <memory>
#include <vector>
#include <array>
#include <unordered_map>

namespace Sea
{
    class Device;
    class CommandList;
    class CommandQueue;
    class DescriptorHeap;
    class Buffer;

    // 每帧资源 - 管理需要多帧缓冲的资源
    class FrameResource
    {
    public:
        FrameResource();
        ~FrameResource();

        void Initialize(Device* device, u32 frameIndex);
        void Shutdown();

        // 获取帧索引
        u32 GetFrameIndex() const { return m_FrameIndex; }

        // Fence值管理
        void SetFenceValue(u64 value) { m_FenceValue = value; }
        u64 GetFenceValue() const { return m_FenceValue; }

        // 常量缓冲区管理
        Buffer* AllocateConstantBuffer(size_t size);
        void ResetConstantBuffers();

        // 动态顶点/索引缓冲区
        Buffer* AllocateDynamicVertexBuffer(size_t size);
        Buffer* AllocateDynamicIndexBuffer(size_t size);
        void ResetDynamicBuffers();

        // 描述符分配
        u32 AllocateSRV();
        u32 AllocateRTV();
        void ResetDescriptors();

        // 帧开始/结束
        void BeginFrame();
        void EndFrame();

    private:
        Device* m_Device = nullptr;
        u32 m_FrameIndex = 0;
        u64 m_FenceValue = 0;

        // 常量缓冲区池
        std::vector<std::unique_ptr<Buffer>> m_ConstantBuffers;
        u32 m_CurrentCBIndex = 0;

        // 动态缓冲区池
        std::vector<std::unique_ptr<Buffer>> m_DynamicVBs;
        std::vector<std::unique_ptr<Buffer>> m_DynamicIBs;
        u32 m_CurrentVBIndex = 0;
        u32 m_CurrentIBIndex = 0;

        // 描述符索引
        u32 m_NextSRVIndex = 0;
        u32 m_NextRTVIndex = 0;
    };

    // 帧资源管理器
    class FrameResourceManager
    {
    public:
        static constexpr u32 kMaxFramesInFlight = 3;

        FrameResourceManager();
        ~FrameResourceManager();

        void Initialize(Device* device);
        void Shutdown();

        // 获取当前帧资源
        FrameResource& GetCurrentFrame();
        FrameResource& GetFrame(u32 index);

        // 帧管理
        void BeginFrame();
        void EndFrame();
        void WaitForFrame(u32 index);

        u32 GetCurrentFrameIndex() const { return m_CurrentFrameIndex; }
        u32 GetFrameCount() const { return kMaxFramesInFlight; }

    private:
        Device* m_Device = nullptr;
        std::array<std::unique_ptr<FrameResource>, kMaxFramesInFlight> m_Frames;
        u32 m_CurrentFrameIndex = 0;
    };
}
