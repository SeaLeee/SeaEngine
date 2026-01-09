#include "RenderGraph/ResourcePool.h"
#include "Graphics/Device.h"
#include "Graphics/Texture.h"
#include "Core/Log.h"

namespace Sea
{
    bool PooledResourceDesc::Matches(const ResourceNode& node) const
    {
        return type == node.GetType() &&
               width == node.GetWidth() &&
               height == node.GetHeight() &&
               depth == node.GetDepth() &&
               format == node.GetFormat() &&
               usage == node.GetUsage();
    }

    size_t PooledResourceDesc::GetHash() const
    {
        size_t hash = 0;
        hash ^= std::hash<int>()(static_cast<int>(type)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<u32>()(width) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<u32>()(height) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<u32>()(depth) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<int>()(static_cast<int>(format)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        return hash;
    }

    ResourcePool::ResourcePool() = default;
    ResourcePool::~ResourcePool() { Shutdown(); }

    void ResourcePool::Initialize(Device* device)
    {
        m_Device = device;
        m_Pool.clear();
        m_CurrentFrame = 0;
        m_TotalMemory = 0;
        SEA_CORE_INFO("ResourcePool initialized");
    }

    void ResourcePool::Shutdown()
    {
        m_Pool.clear();
        m_TotalMemory = 0;
    }

    std::shared_ptr<Texture> ResourcePool::AcquireTexture(const ResourceNode& node)
    {
        PooledResourceDesc desc;
        desc.type = node.GetType();
        desc.width = node.GetWidth();
        desc.height = node.GetHeight();
        desc.depth = node.GetDepth();
        desc.format = node.GetFormat();
        desc.usage = node.GetUsage();

        // 尝试找到可用的匹配资源
        PooledResource* available = FindAvailableResource(desc);
        if (available)
        {
            available->inUse = true;
            available->lastUsedFrame = m_CurrentFrame;
            return available->texture;
        }

        // 创建新资源
        auto texture = CreateTexture(node);
        if (texture)
        {
            PooledResource pooled;
            pooled.texture = texture;
            pooled.desc = desc;
            pooled.lastUsedFrame = m_CurrentFrame;
            pooled.inUse = true;
            m_Pool.push_back(pooled);

            // 估算内存使用
            size_t pixelSize = 4; // 简化估算
            m_TotalMemory += desc.width * desc.height * desc.depth * pixelSize;
        }

        return texture;
    }

    void ResourcePool::ReleaseTexture(std::shared_ptr<Texture> texture)
    {
        for (auto& pooled : m_Pool)
        {
            if (pooled.texture == texture)
            {
                pooled.inUse = false;
                return;
            }
        }
    }

    void ResourcePool::BeginFrame(u32 frameIndex)
    {
        m_CurrentFrame = frameIndex;
    }

    void ResourcePool::EndFrame()
    {
        // 释放所有标记为使用中的资源
        for (auto& pooled : m_Pool)
        {
            pooled.inUse = false;
        }
    }

    void ResourcePool::GarbageCollect(u32 maxUnusedFrames)
    {
        auto it = m_Pool.begin();
        while (it != m_Pool.end())
        {
            if (!it->inUse && (m_CurrentFrame - it->lastUsedFrame) > maxUnusedFrames)
            {
                size_t pixelSize = 4;
                m_TotalMemory -= it->desc.width * it->desc.height * it->desc.depth * pixelSize;
                it = m_Pool.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    u32 ResourcePool::GetActiveResourceCount() const
    {
        u32 count = 0;
        for (const auto& pooled : m_Pool)
        {
            if (pooled.inUse) count++;
        }
        return count;
    }

    std::shared_ptr<Texture> ResourcePool::CreateTexture(const ResourceNode& node)
    {
        if (!m_Device) return nullptr;

        TextureDesc desc;
        desc.width = node.GetWidth();
        desc.height = node.GetHeight();
        desc.format = node.GetFormat();
        desc.usage = node.GetUsage();
        desc.mipLevels = node.GetMipLevels();
        desc.name = node.GetName();

        auto texture = std::make_shared<Texture>(*m_Device, desc);
        if (texture->Initialize(nullptr))
        {
            return texture;
        }
        return nullptr;
    }

    PooledResource* ResourcePool::FindAvailableResource(const PooledResourceDesc& desc)
    {
        for (auto& pooled : m_Pool)
        {
            if (!pooled.inUse && pooled.desc.Matches(ResourceNode(0, "", desc.type)))
            {
                // 简单匹配检查
                if (pooled.desc.width == desc.width &&
                    pooled.desc.height == desc.height &&
                    pooled.desc.format == desc.format)
                {
                    return &pooled;
                }
            }
        }
        return nullptr;
    }
}
