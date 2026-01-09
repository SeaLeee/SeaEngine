#pragma once
#include "Core/Types.h"
#include "Graphics/GraphicsTypes.h"
#include "RenderGraph/ResourceNode.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <queue>

namespace Sea
{
    class Device;
    class Texture;

    // 池化资源描述符（用于匹配）
    struct PooledResourceDesc
    {
        ResourceNodeType type = ResourceNodeType::Texture2D;
        u32 width = 0;
        u32 height = 0;
        u32 depth = 1;
        Format format = Format::R8G8B8A8_UNORM;
        TextureUsage usage = TextureUsage::ShaderResource;

        bool Matches(const ResourceNode& node) const;
        size_t GetHash() const;
    };

    // 池化资源
    struct PooledResource
    {
        std::shared_ptr<Texture> texture;
        PooledResourceDesc desc;
        u32 lastUsedFrame = 0;
        bool inUse = false;
    };

    // 资源池 - 管理transient资源的分配和重用
    class ResourcePool
    {
    public:
        ResourcePool();
        ~ResourcePool();

        void Initialize(Device* device);
        void Shutdown();

        // 获取或创建资源
        std::shared_ptr<Texture> AcquireTexture(const ResourceNode& node);
        
        // 释放资源（返回池中）
        void ReleaseTexture(std::shared_ptr<Texture> texture);

        // 帧结束时更新
        void BeginFrame(u32 frameIndex);
        void EndFrame();

        // 清理未使用的资源
        void GarbageCollect(u32 maxUnusedFrames = 3);

        // 统计
        u32 GetPooledResourceCount() const { return static_cast<u32>(m_Pool.size()); }
        u32 GetActiveResourceCount() const;
        size_t GetTotalMemoryUsage() const { return m_TotalMemory; }

    private:
        std::shared_ptr<Texture> CreateTexture(const ResourceNode& node);
        PooledResource* FindAvailableResource(const PooledResourceDesc& desc);

    private:
        Device* m_Device = nullptr;
        std::vector<PooledResource> m_Pool;
        u32 m_CurrentFrame = 0;
        size_t m_TotalMemory = 0;
    };
}
