#pragma once
#include "Core/Types.h"
#include "Graphics/GraphicsTypes.h"
#include <string>

namespace Sea
{
    // GPU资源状态
    enum class GPUResourceState
    {
        Unknown,
        Created,
        Uploading,
        Ready,
        Destroyed
    };

    // GPU资源基类
    class GPUResource : public NonCopyable
    {
    public:
        GPUResource() = default;
        virtual ~GPUResource() = default;

        // 获取底层D3D12资源
        virtual ID3D12Resource* GetResource() const { return m_Resource.Get(); }

        // 获取资源GPU虚拟地址
        D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const
        {
            return m_Resource ? m_Resource->GetGPUVirtualAddress() : 0;
        }

        // 获取资源描述
        D3D12_RESOURCE_DESC GetResourceDesc() const
        {
            return m_Resource ? m_Resource->GetDesc() : D3D12_RESOURCE_DESC{};
        }

        // 资源名称
        void SetName(const std::string& name);
        const std::string& GetName() const { return m_Name; }

        // 资源状态
        GPUResourceState GetState() const { return m_State; }
        void SetState(GPUResourceState state) { m_State = state; }

        // 当前资源状态（用于barrier）
        D3D12_RESOURCE_STATES GetCurrentResourceState() const { return m_CurrentState; }
        void SetCurrentResourceState(D3D12_RESOURCE_STATES state) { m_CurrentState = state; }

        // 资源是否有效
        bool IsValid() const { return m_Resource != nullptr; }

    protected:
        ComPtr<ID3D12Resource> m_Resource;
        std::string m_Name;
        GPUResourceState m_State = GPUResourceState::Unknown;
        D3D12_RESOURCE_STATES m_CurrentState = D3D12_RESOURCE_STATE_COMMON;
    };
}
