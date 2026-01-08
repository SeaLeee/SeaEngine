#pragma once
#include "Graphics/GraphicsTypes.h"
#include "Core/Types.h"

namespace Sea
{
    class Device;

    struct BufferDesc
    {
        u64 size = 0;
        BufferType type = BufferType::Vertex;
        u32 stride = 0;
        std::string name;
    };

    class Buffer : public NonCopyable
    {
    public:
        Buffer(Device& device, const BufferDesc& desc);
        ~Buffer();

        bool Initialize(const void* data = nullptr);
        void* Map();
        void Unmap();
        void Update(const void* data, u64 size, u64 offset = 0);

        ID3D12Resource* GetResource() const { return m_Resource.Get(); }
        D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const { return m_Resource->GetGPUVirtualAddress(); }
        D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const;
        D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const;
        u64 GetSize() const { return m_Desc.size; }

    private:
        Device& m_Device;
        BufferDesc m_Desc;
        ComPtr<ID3D12Resource> m_Resource;
        ComPtr<ID3D12Resource> m_UploadBuffer;
        void* m_MappedData = nullptr;
    };
}
