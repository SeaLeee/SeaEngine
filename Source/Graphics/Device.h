#pragma once

#include "Graphics/GraphicsTypes.h"
#include "Core/Types.h"

namespace Sea
{
    struct DeviceDesc
    {
        bool enableDebugLayer = true;
        bool enableGPUValidation = false;
        bool preferHighPerformanceAdapter = true;
    };

    class Device : public NonCopyable
    {
    public:
        Device(const DeviceDesc& desc = {});
        ~Device();

        bool Initialize();
        void Shutdown();

        ID3D12Device* GetDevice() const { return m_Device.Get(); }
        IDXGIFactory6* GetFactory() const { return m_Factory.Get(); }
        IDXGIAdapter4* GetAdapter() const { return m_Adapter.Get(); }

        std::string GetAdapterName() const { return m_AdapterName; }
        u64 GetDedicatedVideoMemory() const { return m_DedicatedVideoMemory; }

        // 资源创建辅助
        ComPtr<ID3D12Resource> CreateCommittedResource(
            const D3D12_HEAP_PROPERTIES& heapProps,
            D3D12_HEAP_FLAGS heapFlags,
            const D3D12_RESOURCE_DESC& resourceDesc,
            D3D12_RESOURCE_STATES initialState,
            const D3D12_CLEAR_VALUE* clearValue = nullptr
        );

        void WaitForIdle();

        static Device& Get() { return *s_Instance; }

    private:
        bool CreateFactory();
        bool SelectAdapter();
        bool CreateDevice();
        void EnableDebugLayer();

    private:
        DeviceDesc m_Desc;
        
        ComPtr<IDXGIFactory6> m_Factory;
        ComPtr<IDXGIAdapter4> m_Adapter;
        ComPtr<ID3D12Device> m_Device;
        ComPtr<ID3D12Debug> m_DebugController;
        ComPtr<ID3D12InfoQueue> m_InfoQueue;

        std::string m_AdapterName;
        u64 m_DedicatedVideoMemory = 0;

        static Device* s_Instance;
    };
}
