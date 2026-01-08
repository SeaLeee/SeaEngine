#include "Graphics/CommandList.h"
#include "Graphics/Device.h"
#include "Graphics/PipelineState.h"
#include "Graphics/RootSignature.h"
#include "Core/Log.h"

namespace Sea
{
    CommandList::CommandList(Device& device, CommandQueueType type)
        : m_Device(device)
        , m_Type(type)
    {
    }

    CommandList::~CommandList()
    {
        Shutdown();
    }

    bool CommandList::Initialize()
    {
        D3D12_COMMAND_LIST_TYPE type = static_cast<D3D12_COMMAND_LIST_TYPE>(m_Type);

        HRESULT hr = m_Device.GetDevice()->CreateCommandAllocator(type, IID_PPV_ARGS(&m_Allocator));
        if (FAILED(hr))
        {
            SEA_CORE_ERROR("Failed to create command allocator");
            return false;
        }

        hr = m_Device.GetDevice()->CreateCommandList(0, type, m_Allocator.Get(), nullptr, 
                                                      IID_PPV_ARGS(&m_CommandList));
        if (FAILED(hr))
        {
            SEA_CORE_ERROR("Failed to create command list");
            return false;
        }

        // 命令列表创建后默认是开启状态,关闭它
        m_CommandList->Close();

        return true;
    }

    void CommandList::Shutdown()
    {
        m_CommandList.Reset();
        m_Allocator.Reset();
    }

    void CommandList::Reset()
    {
        m_Allocator->Reset();
        m_CommandList->Reset(m_Allocator.Get(), nullptr);
        m_PendingBarriers.clear();
    }

    void CommandList::Close()
    {
        FlushBarriers();
        m_CommandList->Close();
    }

    void CommandList::TransitionBarrier(ID3D12Resource* resource, ResourceState before, ResourceState after)
    {
        if (before == after)
            return;

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = static_cast<D3D12_RESOURCE_STATES>(before);
        barrier.Transition.StateAfter = static_cast<D3D12_RESOURCE_STATES>(after);
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        m_PendingBarriers.push_back(barrier);

        if (m_PendingBarriers.size() >= MaxPendingBarriers)
        {
            FlushBarriers();
        }
    }

    void CommandList::UAVBarrier(ID3D12Resource* resource)
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.UAV.pResource = resource;

        m_PendingBarriers.push_back(barrier);

        if (m_PendingBarriers.size() >= MaxPendingBarriers)
        {
            FlushBarriers();
        }
    }

    void CommandList::FlushBarriers()
    {
        if (m_PendingBarriers.empty())
            return;

        m_CommandList->ResourceBarrier(
            static_cast<UINT>(m_PendingBarriers.size()),
            m_PendingBarriers.data()
        );
        m_PendingBarriers.clear();
    }

    void CommandList::ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv, const f32* color)
    {
        m_CommandList->ClearRenderTargetView(rtv, color, 0, nullptr);
    }

    void CommandList::ClearDepthStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv, f32 depth, u8 stencil)
    {
        m_CommandList->ClearDepthStencilView(dsv, 
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 
            depth, stencil, 0, nullptr);
    }

    void CommandList::SetRenderTargets(std::span<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs, 
                                       const D3D12_CPU_DESCRIPTOR_HANDLE* dsv)
    {
        m_CommandList->OMSetRenderTargets(
            static_cast<UINT>(rtvs.size()),
            rtvs.data(),
            FALSE,
            dsv
        );
    }

    void CommandList::SetViewport(const Viewport& viewport)
    {
        D3D12_VIEWPORT vp = viewport.ToD3D12();
        m_CommandList->RSSetViewports(1, &vp);
    }

    void CommandList::SetScissorRect(const ScissorRect& rect)
    {
        D3D12_RECT r = rect.ToD3D12();
        m_CommandList->RSSetScissorRects(1, &r);
    }

    void CommandList::SetPipelineState(PipelineState* pso)
    {
        if (pso)
        {
            m_CommandList->SetPipelineState(pso->GetPipelineState());
        }
    }

    void CommandList::SetGraphicsRootSignature(RootSignature* rootSig)
    {
        if (rootSig)
        {
            m_CommandList->SetGraphicsRootSignature(rootSig->GetRootSignature());
        }
    }

    void CommandList::SetComputeRootSignature(RootSignature* rootSig)
    {
        if (rootSig)
        {
            m_CommandList->SetComputeRootSignature(rootSig->GetRootSignature());
        }
    }

    void CommandList::SetDescriptorHeaps(std::span<ID3D12DescriptorHeap*> heaps)
    {
        m_CommandList->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());
    }

    void CommandList::SetGraphicsRootConstant(u32 rootIndex, u32 value, u32 offset)
    {
        m_CommandList->SetGraphicsRoot32BitConstant(rootIndex, value, offset);
    }

    void CommandList::SetGraphicsRootConstants(u32 rootIndex, const void* data, u32 count)
    {
        m_CommandList->SetGraphicsRoot32BitConstants(rootIndex, count, data, 0);
    }

    void CommandList::SetGraphicsRootCBV(u32 rootIndex, D3D12_GPU_VIRTUAL_ADDRESS address)
    {
        m_CommandList->SetGraphicsRootConstantBufferView(rootIndex, address);
    }

    void CommandList::SetGraphicsRootSRV(u32 rootIndex, D3D12_GPU_VIRTUAL_ADDRESS address)
    {
        m_CommandList->SetGraphicsRootShaderResourceView(rootIndex, address);
    }

    void CommandList::SetGraphicsRootUAV(u32 rootIndex, D3D12_GPU_VIRTUAL_ADDRESS address)
    {
        m_CommandList->SetGraphicsRootUnorderedAccessView(rootIndex, address);
    }

    void CommandList::SetGraphicsRootDescriptorTable(u32 rootIndex, D3D12_GPU_DESCRIPTOR_HANDLE baseHandle)
    {
        m_CommandList->SetGraphicsRootDescriptorTable(rootIndex, baseHandle);
    }

    void CommandList::SetComputeRootConstant(u32 rootIndex, u32 value, u32 offset)
    {
        m_CommandList->SetComputeRoot32BitConstant(rootIndex, value, offset);
    }

    void CommandList::SetComputeRootConstants(u32 rootIndex, const void* data, u32 count)
    {
        m_CommandList->SetComputeRoot32BitConstants(rootIndex, count, data, 0);
    }

    void CommandList::SetComputeRootCBV(u32 rootIndex, D3D12_GPU_VIRTUAL_ADDRESS address)
    {
        m_CommandList->SetComputeRootConstantBufferView(rootIndex, address);
    }

    void CommandList::SetComputeRootSRV(u32 rootIndex, D3D12_GPU_VIRTUAL_ADDRESS address)
    {
        m_CommandList->SetComputeRootShaderResourceView(rootIndex, address);
    }

    void CommandList::SetComputeRootUAV(u32 rootIndex, D3D12_GPU_VIRTUAL_ADDRESS address)
    {
        m_CommandList->SetComputeRootUnorderedAccessView(rootIndex, address);
    }

    void CommandList::SetComputeRootDescriptorTable(u32 rootIndex, D3D12_GPU_DESCRIPTOR_HANDLE baseHandle)
    {
        m_CommandList->SetComputeRootDescriptorTable(rootIndex, baseHandle);
    }

    void CommandList::SetVertexBuffer(u32 slot, const D3D12_VERTEX_BUFFER_VIEW& view)
    {
        m_CommandList->IASetVertexBuffers(slot, 1, &view);
    }

    void CommandList::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& view)
    {
        m_CommandList->IASetIndexBuffer(&view);
    }

    void CommandList::SetPrimitiveTopology(PrimitiveTopology topology)
    {
        m_CommandList->IASetPrimitiveTopology(static_cast<D3D12_PRIMITIVE_TOPOLOGY>(topology));
    }

    void CommandList::Draw(u32 vertexCount, u32 instanceCount, u32 startVertex, u32 startInstance)
    {
        m_CommandList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
    }

    void CommandList::DrawIndexed(u32 indexCount, u32 instanceCount, u32 startIndex, 
                                  i32 baseVertex, u32 startInstance)
    {
        m_CommandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
    }

    void CommandList::Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ)
    {
        m_CommandList->Dispatch(groupCountX, groupCountY, groupCountZ);
    }

    void CommandList::CopyBuffer(ID3D12Resource* dest, ID3D12Resource* src, u64 size)
    {
        m_CommandList->CopyBufferRegion(dest, 0, src, 0, size);
    }

    void CommandList::CopyBufferRegion(ID3D12Resource* dest, u64 destOffset, 
                                       ID3D12Resource* src, u64 srcOffset, u64 size)
    {
        m_CommandList->CopyBufferRegion(dest, destOffset, src, srcOffset, size);
    }

    void CommandList::CopyTexture(ID3D12Resource* dest, ID3D12Resource* src)
    {
        m_CommandList->CopyResource(dest, src);
    }

    void CommandList::CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION& dest,
                                        const D3D12_TEXTURE_COPY_LOCATION& src)
    {
        m_CommandList->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);
    }
}
