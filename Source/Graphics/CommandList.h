#pragma once

#include "Graphics/GraphicsTypes.h"
#include "Core/Types.h"

namespace Sea
{
    class Device;
    class Buffer;
    class Texture;
    class PipelineState;
    class RootSignature;
    class DescriptorHeap;

    class CommandList : public NonCopyable
    {
    public:
        CommandList(Device& device, CommandQueueType type);
        ~CommandList();

        bool Initialize();
        void Shutdown();

        void Reset();
        void Close();

        // 资源屏障
        void TransitionBarrier(ID3D12Resource* resource, ResourceState before, ResourceState after);
        void UAVBarrier(ID3D12Resource* resource);
        void FlushBarriers();

        // 清除操作
        void ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv, const f32* color);
        void ClearDepthStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv, f32 depth = 1.0f, u8 stencil = 0);

        // 渲染目标
        void SetRenderTargets(std::span<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs, 
                             const D3D12_CPU_DESCRIPTOR_HANDLE* dsv = nullptr);
        void SetViewport(const Viewport& viewport);
        void SetScissorRect(const ScissorRect& rect);

        // 管线状态
        void SetPipelineState(PipelineState* pso);
        void SetGraphicsRootSignature(RootSignature* rootSig);
        void SetComputeRootSignature(RootSignature* rootSig);

        // 描述符堆
        void SetDescriptorHeaps(std::span<ID3D12DescriptorHeap*> heaps);

        // 根参数
        void SetGraphicsRootConstant(u32 rootIndex, u32 value, u32 offset);
        void SetGraphicsRootConstants(u32 rootIndex, const void* data, u32 count);
        void SetGraphicsRootCBV(u32 rootIndex, D3D12_GPU_VIRTUAL_ADDRESS address);
        void SetGraphicsRootSRV(u32 rootIndex, D3D12_GPU_VIRTUAL_ADDRESS address);
        void SetGraphicsRootUAV(u32 rootIndex, D3D12_GPU_VIRTUAL_ADDRESS address);
        void SetGraphicsRootDescriptorTable(u32 rootIndex, D3D12_GPU_DESCRIPTOR_HANDLE baseHandle);

        void SetComputeRootConstant(u32 rootIndex, u32 value, u32 offset);
        void SetComputeRootConstants(u32 rootIndex, const void* data, u32 count);
        void SetComputeRootCBV(u32 rootIndex, D3D12_GPU_VIRTUAL_ADDRESS address);
        void SetComputeRootSRV(u32 rootIndex, D3D12_GPU_VIRTUAL_ADDRESS address);
        void SetComputeRootUAV(u32 rootIndex, D3D12_GPU_VIRTUAL_ADDRESS address);
        void SetComputeRootDescriptorTable(u32 rootIndex, D3D12_GPU_DESCRIPTOR_HANDLE baseHandle);

        // 顶点/索引缓冲
        void SetVertexBuffer(u32 slot, const D3D12_VERTEX_BUFFER_VIEW& view);
        void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& view);
        void SetPrimitiveTopology(PrimitiveTopology topology);

        // 绘制命令
        void Draw(u32 vertexCount, u32 instanceCount = 1, u32 startVertex = 0, u32 startInstance = 0);
        void DrawIndexed(u32 indexCount, u32 instanceCount = 1, u32 startIndex = 0, 
                        i32 baseVertex = 0, u32 startInstance = 0);

        // 调度命令
        void Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ);

        // 拷贝命令
        void CopyBuffer(ID3D12Resource* dest, ID3D12Resource* src, u64 size);
        void CopyBufferRegion(ID3D12Resource* dest, u64 destOffset, 
                             ID3D12Resource* src, u64 srcOffset, u64 size);
        void CopyTexture(ID3D12Resource* dest, ID3D12Resource* src);
        void CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION& dest,
                              const D3D12_TEXTURE_COPY_LOCATION& src);

        ID3D12GraphicsCommandList* GetCommandList() const { return m_CommandList.Get(); }
        CommandQueueType GetType() const { return m_Type; }

    private:
        Device& m_Device;
        CommandQueueType m_Type;

        ComPtr<ID3D12CommandAllocator> m_Allocator;
        ComPtr<ID3D12GraphicsCommandList> m_CommandList;

        std::vector<D3D12_RESOURCE_BARRIER> m_PendingBarriers;
        static constexpr u32 MaxPendingBarriers = 16;
    };
}
