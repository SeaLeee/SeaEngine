#include "Graphics/Fence.h"
#include "Graphics/Device.h"
#include "Core/Log.h"

namespace Sea
{
    Fence::Fence(Device& device)
        : m_Device(device)
    {
    }

    Fence::~Fence()
    {
        Shutdown();
    }

    bool Fence::Initialize(u64 initialValue)
    {
        m_CurrentValue = initialValue;

        HRESULT hr = m_Device.GetDevice()->CreateFence(
            initialValue,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&m_Fence)
        );

        if (FAILED(hr))
        {
            SEA_CORE_ERROR("Failed to create fence");
            return false;
        }

        m_Event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        if (!m_Event)
        {
            SEA_CORE_ERROR("Failed to create fence event");
            return false;
        }

        SEA_CORE_TRACE("Fence initialized with value {}", initialValue);
        return true;
    }

    void Fence::Shutdown()
    {
        if (m_Event)
        {
            CloseHandle(m_Event);
            m_Event = nullptr;
        }
        m_Fence.Reset();
    }

    u64 Fence::GetCompletedValue() const
    {
        return m_Fence ? m_Fence->GetCompletedValue() : 0;
    }

    u64 Fence::Signal(ID3D12CommandQueue* queue)
    {
        ++m_CurrentValue;
        HRESULT hr = queue->Signal(m_Fence.Get(), m_CurrentValue);
        if (FAILED(hr))
        {
            SEA_CORE_ERROR("Failed to signal fence");
        }
        return m_CurrentValue;
    }

    void Fence::WaitForValue(u64 value)
    {
        if (m_Fence->GetCompletedValue() < value)
        {
            HRESULT hr = m_Fence->SetEventOnCompletion(value, m_Event);
            if (SUCCEEDED(hr))
            {
                WaitForSingleObject(m_Event, INFINITE);
            }
        }
    }

    void Fence::WaitForValue(u64 value, u32 timeoutMs)
    {
        if (m_Fence->GetCompletedValue() < value)
        {
            HRESULT hr = m_Fence->SetEventOnCompletion(value, m_Event);
            if (SUCCEEDED(hr))
            {
                WaitForSingleObject(m_Event, timeoutMs);
            }
        }
    }

    bool Fence::IsComplete(u64 value) const
    {
        return m_Fence->GetCompletedValue() >= value;
    }

    void Fence::Sync()
    {
        WaitForValue(m_CurrentValue);
    }
}
