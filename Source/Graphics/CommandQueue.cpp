#include "Graphics/CommandQueue.h"
#include "Graphics/Device.h"
#include "Graphics/CommandList.h"
#include "Core/Log.h"

namespace Sea
{
    CommandQueue::CommandQueue(Device& device, CommandQueueType type)
        : m_Device(device)
        , m_Type(type)
    {
    }

    CommandQueue::~CommandQueue()
    {
        Shutdown();
    }

    bool CommandQueue::Initialize()
    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = static_cast<D3D12_COMMAND_LIST_TYPE>(m_Type);
        desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 0;

        HRESULT hr = m_Device.GetDevice()->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_Queue));
        if (FAILED(hr))
        {
            SEA_CORE_ERROR("Failed to create command queue");
            return false;
        }

        hr = m_Device.GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));
        if (FAILED(hr))
        {
            SEA_CORE_ERROR("Failed to create fence");
            return false;
        }

        m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!m_FenceEvent)
        {
            SEA_CORE_ERROR("Failed to create fence event");
            return false;
        }

        return true;
    }

    void CommandQueue::Shutdown()
    {
        if (m_Queue && m_Fence && m_FenceEvent)
        {
            WaitForIdle();
        }

        if (m_FenceEvent)
        {
            CloseHandle(m_FenceEvent);
            m_FenceEvent = nullptr;
        }

        m_Fence.Reset();
        m_Queue.Reset();
    }

    void CommandQueue::ExecuteCommandLists(std::span<CommandList*> cmdLists)
    {
        std::vector<ID3D12CommandList*> d3dCmdLists;
        d3dCmdLists.reserve(cmdLists.size());

        for (auto* cmdList : cmdLists)
        {
            d3dCmdLists.push_back(cmdList->GetCommandList());
        }

        m_Queue->ExecuteCommandLists(
            static_cast<UINT>(d3dCmdLists.size()),
            d3dCmdLists.data()
        );
    }

    void CommandQueue::ExecuteCommandList(CommandList* cmdList)
    {
        ID3D12CommandList* d3dCmdList = cmdList->GetCommandList();
        m_Queue->ExecuteCommandLists(1, &d3dCmdList);
    }

    u64 CommandQueue::Signal()
    {
        u64 fenceValue = m_NextFenceValue++;
        m_Queue->Signal(m_Fence.Get(), fenceValue);
        return fenceValue;
    }

    void CommandQueue::WaitForFence(u64 fenceValue)
    {
        if (IsFenceComplete(fenceValue))
            return;

        m_Fence->SetEventOnCompletion(fenceValue, m_FenceEvent);
        WaitForSingleObject(m_FenceEvent, INFINITE);
    }

    void CommandQueue::WaitForIdle()
    {
        u64 fenceValue = Signal();
        WaitForFence(fenceValue);
    }

    bool CommandQueue::IsFenceComplete(u64 fenceValue) const
    {
        return m_Fence->GetCompletedValue() >= fenceValue;
    }

    u64 CommandQueue::GetCompletedFenceValue() const
    {
        return m_Fence->GetCompletedValue();
    }
}
