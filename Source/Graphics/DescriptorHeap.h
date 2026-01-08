#pragma once

#include "Graphics/GraphicsTypes.h"
#include "Core/Types.h"

namespace Sea
{
    class Device;

    struct DescriptorHeapDesc
    {
        DescriptorHeapType type = DescriptorHeapType::CBV_SRV_UAV;
        u32 numDescriptors = 1024;
        bool shaderVisible = true;
    };

    class DescriptorHeap : public NonCopyable
    {
    public:
        DescriptorHeap(Device& device, const DescriptorHeapDesc& desc);
        ~DescriptorHeap();

        bool Initialize();
        DescriptorHandle Allocate();
        void Free(const DescriptorHandle& handle);

        ID3D12DescriptorHeap* GetHeap() const { return m_Heap.Get(); }
        u32 GetDescriptorSize() const { return m_DescriptorSize; }

    private:
        Device& m_Device;
        DescriptorHeapDesc m_Desc;
        ComPtr<ID3D12DescriptorHeap> m_Heap;
        u32 m_DescriptorSize = 0;
        u32 m_NumFreeDescriptors = 0;
        u32 m_NextFreeIndex = 0;
        std::vector<u32> m_FreeList;
    };
}
