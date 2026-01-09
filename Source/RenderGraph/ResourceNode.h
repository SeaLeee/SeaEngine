#pragma once
#include "Core/Types.h"
#include "Graphics/GraphicsTypes.h"
#include <string>

namespace Sea
{
    // 资源节点类型
    enum class ResourceNodeType
    {
        Texture2D,
        Texture3D,
        TextureCube,
        Buffer,
        DepthStencil
    };

    // 资源节点 - 表示RenderGraph中的一个资源
    class ResourceNode
    {
    public:
        ResourceNode() = default;
        ResourceNode(u32 id, const std::string& name, ResourceNodeType type = ResourceNodeType::Texture2D);
        ~ResourceNode() = default;

        // 基本属性
        u32 GetId() const { return m_Id; }
        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& name) { m_Name = name; }
        
        ResourceNodeType GetType() const { return m_Type; }
        void SetType(ResourceNodeType type) { m_Type = type; }

        // Texture属性
        void SetDimensions(u32 width, u32 height, u32 depth = 1);
        u32 GetWidth() const { return m_Width; }
        u32 GetHeight() const { return m_Height; }
        u32 GetDepth() const { return m_Depth; }

        void SetFormat(Format format) { m_Format = format; }
        Format GetFormat() const { return m_Format; }

        void SetMipLevels(u32 mips) { m_MipLevels = mips; }
        u32 GetMipLevels() const { return m_MipLevels; }

        // Buffer属性
        void SetBufferSize(u64 size) { m_BufferSize = size; }
        u64 GetBufferSize() const { return m_BufferSize; }

        void SetBufferStride(u32 stride) { m_BufferStride = stride; }
        u32 GetBufferStride() const { return m_BufferStride; }

        // 使用标志
        void SetUsage(TextureUsage usage) { m_Usage = usage; }
        TextureUsage GetUsage() const { return m_Usage; }

        // 是否为外部资源
        void SetExternal(bool external) { m_IsExternal = external; }
        bool IsExternal() const { return m_IsExternal; }

        // 节点编辑器位置
        void SetPosition(float x, float y) { m_PosX = x; m_PosY = y; }
        float GetPosX() const { return m_PosX; }
        float GetPosY() const { return m_PosY; }

        // 生命周期
        void SetLifetime(u32 firstUse, u32 lastUse) { m_FirstUsePass = firstUse; m_LastUsePass = lastUse; }
        u32 GetFirstUsePass() const { return m_FirstUsePass; }
        u32 GetLastUsePass() const { return m_LastUsePass; }

        static const char* GetTypeString(ResourceNodeType type);

    private:
        u32 m_Id = UINT32_MAX;
        std::string m_Name;
        ResourceNodeType m_Type = ResourceNodeType::Texture2D;

        u32 m_Width = 0;
        u32 m_Height = 0;
        u32 m_Depth = 1;
        Format m_Format = Format::R8G8B8A8_UNORM;
        u32 m_MipLevels = 1;
        TextureUsage m_Usage = TextureUsage::ShaderResource;

        u64 m_BufferSize = 0;
        u32 m_BufferStride = 0;

        bool m_IsExternal = false;

        float m_PosX = 0.0f;
        float m_PosY = 0.0f;

        u32 m_FirstUsePass = UINT32_MAX;
        u32 m_LastUsePass = 0;
    };
}
