#include "Graphics/DescriptorHeap.h"
#include "Graphics/Device.h"
#include "Core/Log.h"

namespace Sea
{
    DescriptorHeap::DescriptorHeap(Device& device, const DescriptorHeapDesc& desc)
        : m_Device(device), m_Desc(desc) {}

    DescriptorHeap::DescriptorHeap(Device& device, const D3D12_DESCRIPTOR_HEAP_DESC& d3dDesc)
        : m_Device(device)
    {
        m_Desc.type = static_cast<DescriptorHeapType>(d3dDesc.Type);
        m_Desc.numDescriptors = d3dDesc.NumDescriptors;
        m_Desc.shaderVisible = (d3dDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) != 0;
    }

    DescriptorHeap::~DescriptorHeap() { m_Heap.Reset(); }

    bool DescriptorHeap::Initialize()
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(m_Desc.type);
        heapDesc.NumDescriptors = m_Desc.numDescriptors;
        heapDesc.Flags = m_Desc.shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        if (FAILED(m_Device.GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_Heap))))
        {
            SEA_CORE_ERROR("Failed to create descriptor heap");
            return false;
        }

        m_DescriptorSize = m_Device.GetDevice()->GetDescriptorHandleIncrementSize(heapDesc.Type);
        m_NumFreeDescriptors = m_Desc.numDescriptors;
        return true;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCPUHandle(u32 index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = m_Heap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += index * m_DescriptorSize;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGPUHandle(u32 index) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = m_Heap->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += index * m_DescriptorSize;
        return handle;
    }

    DescriptorHandle DescriptorHeap::Allocate()
    {
        DescriptorHandle handle;
        u32 index;

        if (!m_FreeList.empty())
        {
            index = m_FreeList.back();
            m_FreeList.pop_back();
        }
        else if (m_NextFreeIndex < m_Desc.numDescriptors)
        {
            index = m_NextFreeIndex++;
        }
        else
        {
            SEA_CORE_ERROR("Descriptor heap is full");
            return handle;
        }

        handle.heapIndex = index;
        handle.cpu.ptr = m_Heap->GetCPUDescriptorHandleForHeapStart().ptr + index * m_DescriptorSize;
        if (m_Desc.shaderVisible)
            handle.gpu.ptr = m_Heap->GetGPUDescriptorHandleForHeapStart().ptr + index * m_DescriptorSize;

        m_NumFreeDescriptors--;
        return handle;
    }

    void DescriptorHeap::Free(const DescriptorHandle& handle)
    {
        if (handle.IsValid())
        {
            m_FreeList.push_back(handle.heapIndex);
            m_NumFreeDescriptors++;
        }
    }
}
