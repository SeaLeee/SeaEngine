#pragma once
#include "Graphics/GraphicsTypes.h"
#include "Core/Types.h"

namespace Sea
{
    class Device;

    struct TextureDesc
    {
        u32 width = 1, height = 1, depth = 1;
        u32 mipLevels = 1, arraySize = 1;
        Format format = Format::R8G8B8A8_UNORM;
        TextureType type = TextureType::Texture2D;
        TextureUsage usage = TextureUsage::ShaderResource;
        std::string name;
    };

    class Texture : public NonCopyable
    {
    public:
        Texture(Device& device, const TextureDesc& desc);
        ~Texture();

        bool Initialize(const void* data = nullptr);
        ID3D12Resource* GetResource() const { return m_Resource.Get(); }
        const TextureDesc& GetDesc() const { return m_Desc; }

    private:
        Device& m_Device;
        TextureDesc m_Desc;
        ComPtr<ID3D12Resource> m_Resource;
    };
}
