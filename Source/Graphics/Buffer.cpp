#include "Graphics/Buffer.h"
#include "Graphics/Device.h"
#include "Core/Log.h"

namespace Sea
{
    Buffer::Buffer(Device& device, const BufferDesc& desc) : m_Device(device), m_Desc(desc) {}
    Buffer::~Buffer() { Unmap(); m_Resource.Reset(); m_UploadBuffer.Reset(); }

    bool Buffer::Initialize(const void* data)
    {
        bool useUploadHeap = (m_Desc.type == BufferType::Upload || m_Desc.type == BufferType::Constant);
        
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = useUploadHeap ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_UPLOAD;  // 暂时都用 Upload 堆，简化实现

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = m_Desc.size;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_GENERIC_READ;

        m_Resource = m_Device.CreateCommittedResource(heapProps, D3D12_HEAP_FLAG_NONE, resourceDesc, initialState);
        if (!m_Resource) return false;

        // 对于 Upload 堆，直接写入数据
        if (data)
        {
            void* mapped = Map();
            if (mapped)
            {
                memcpy(mapped, data, m_Desc.size);
                Unmap();
            }
        }
        return true;
    }

    void* Buffer::Map()
    {
        if (!m_MappedData)
        {
            D3D12_RANGE readRange = { 0, 0 };
            m_Resource->Map(0, &readRange, &m_MappedData);
        }
        return m_MappedData;
    }

    void Buffer::Unmap()
    {
        if (m_MappedData)
        {
            m_Resource->Unmap(0, nullptr);
            m_MappedData = nullptr;
        }
    }

    void Buffer::Update(const void* data, u64 size, u64 offset)
    {
        void* mapped = Map();
        memcpy(static_cast<u8*>(mapped) + offset, data, size);
        Unmap();
    }

    D3D12_VERTEX_BUFFER_VIEW Buffer::GetVertexBufferView() const
    {
        return { m_Resource->GetGPUVirtualAddress(), static_cast<UINT>(m_Desc.size), m_Desc.stride };
    }

    D3D12_INDEX_BUFFER_VIEW Buffer::GetIndexBufferView() const
    {
        DXGI_FORMAT format = m_Desc.stride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
        return { m_Resource->GetGPUVirtualAddress(), static_cast<UINT>(m_Desc.size), format };
    }
}
