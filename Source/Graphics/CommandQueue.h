#pragma once

#include "Graphics/GraphicsTypes.h"
#include "Core/Types.h"

namespace Sea
{
    class Device;
    class Fence;
    class CommandList;

    class CommandQueue : public NonCopyable
    {
    public:
        CommandQueue(Device& device, CommandQueueType type);
        ~CommandQueue();

        bool Initialize();
        void Shutdown();

        void ExecuteCommandLists(std::span<CommandList*> cmdLists);
        void ExecuteCommandList(CommandList* cmdList);

        u64 Signal();
        void WaitForFence(u64 fenceValue);
        void WaitForIdle();
        bool IsFenceComplete(u64 fenceValue) const;

        ID3D12CommandQueue* GetQueue() const { return m_Queue.Get(); }
        CommandQueueType GetType() const { return m_Type; }
        u64 GetCompletedFenceValue() const;

    private:
        Device& m_Device;
        CommandQueueType m_Type;

        ComPtr<ID3D12CommandQueue> m_Queue;
        ComPtr<ID3D12Fence> m_Fence;
        HANDLE m_FenceEvent = nullptr;
        u64 m_NextFenceValue = 1;
    };
}
