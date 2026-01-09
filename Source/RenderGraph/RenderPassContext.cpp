#include "RenderGraph/RenderPassContext.h"

namespace Sea
{
    ID3D12Resource* RenderPassContext::GetInputResource(u32 index) const
    {
        return index < m_Inputs.size() ? m_Inputs[index] : nullptr;
    }

    ID3D12Resource* RenderPassContext::GetOutputResource(u32 index) const
    {
        return index < m_Outputs.size() ? m_Outputs[index] : nullptr;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE RenderPassContext::GetOutputRTV(u32 index) const
    {
        return index < m_RTVs.size() ? m_RTVs[index] : D3D12_CPU_DESCRIPTOR_HANDLE{};
    }

    D3D12_GPU_DESCRIPTOR_HANDLE RenderPassContext::GetInputSRV(u32 index) const
    {
        return index < m_SRVs.size() ? m_SRVs[index] : D3D12_GPU_DESCRIPTOR_HANDLE{};
    }
}
