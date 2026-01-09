#pragma once
#include "Core/Types.h"
#include "Graphics/GraphicsTypes.h"

namespace Sea
{
    class Device;

    // GPU Fence - 用于CPU-GPU同步
    class Fence : public NonCopyable
    {
    public:
        Fence(Device& device);
        ~Fence();

        bool Initialize(u64 initialValue = 0);
        void Shutdown();

        // 获取当前fence值
        u64 GetCurrentValue() const { return m_CurrentValue; }

        // 获取已完成的fence值
        u64 GetCompletedValue() const;

        // 信号 - 返回新的fence值
        u64 Signal(ID3D12CommandQueue* queue);

        // 等待直到fence达到指定值
        void WaitForValue(u64 value);
        void WaitForValue(u64 value, u32 timeoutMs);

        // 检查是否完成
        bool IsComplete(u64 value) const;

        // 同步等待
        void Sync();

        // 获取底层fence对象
        ID3D12Fence* GetFence() const { return m_Fence.Get(); }
        HANDLE GetEvent() const { return m_Event; }

    private:
        Device& m_Device;
        ComPtr<ID3D12Fence> m_Fence;
        HANDLE m_Event = nullptr;
        u64 m_CurrentValue = 0;
    };
}
