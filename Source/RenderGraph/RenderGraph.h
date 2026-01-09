#pragma once
#include "Core/Types.h"
#include "RenderGraph/ResourceNode.h"
#include "RenderGraph/PassNode.h"
#include "RenderGraph/GraphCompiler.h"
#include "RenderGraph/ResourcePool.h"
#include "RenderGraph/FrameResource.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <memory>
#include <unordered_map>

namespace Sea
{
    class Device;
    class CommandList;

    // RenderGraph上下文 - Pass执行时访问资源
    class RenderGraphContext
    {
    public:
        void SetDevice(Device* device) { m_Device = device; }
        Device* GetDevice() const { return m_Device; }

        void SetInputResources(const std::vector<ID3D12Resource*>& inputs) { m_Inputs = inputs; }
        void SetOutputResources(const std::vector<ID3D12Resource*>& outputs) { m_Outputs = outputs; }
        void SetInputSRVs(const std::vector<D3D12_GPU_DESCRIPTOR_HANDLE>& srvs) { m_InputSRVs = srvs; }
        void SetOutputRTVs(const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& rtvs) { m_OutputRTVs = rtvs; }
        void SetDSV(D3D12_CPU_DESCRIPTOR_HANDLE dsv) { m_DSV = dsv; }

        ID3D12Resource* GetInput(u32 index) const { return index < m_Inputs.size() ? m_Inputs[index] : nullptr; }
        ID3D12Resource* GetOutput(u32 index) const { return index < m_Outputs.size() ? m_Outputs[index] : nullptr; }
        D3D12_GPU_DESCRIPTOR_HANDLE GetInputSRV(u32 index) const { return index < m_InputSRVs.size() ? m_InputSRVs[index] : D3D12_GPU_DESCRIPTOR_HANDLE{}; }
        D3D12_CPU_DESCRIPTOR_HANDLE GetOutputRTV(u32 index) const { return index < m_OutputRTVs.size() ? m_OutputRTVs[index] : D3D12_CPU_DESCRIPTOR_HANDLE{}; }
        D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const { return m_DSV; }

    private:
        Device* m_Device = nullptr;
        std::vector<ID3D12Resource*> m_Inputs;
        std::vector<ID3D12Resource*> m_Outputs;
        std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> m_InputSRVs;
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_OutputRTVs;
        D3D12_CPU_DESCRIPTOR_HANDLE m_DSV = {};
    };

    // Pass执行回调 (使用RenderGraphContext)
    using RenderPassExecuteFunc = std::function<void(CommandList& cmdList, RenderGraphContext& ctx)>;

    // RenderGraph - 渲染管线图
    class RenderGraph
    {
    public:
        RenderGraph();
        ~RenderGraph();

        void Initialize(Device* device);
        void Shutdown();

        // 资源管理
        u32 CreateResource(const std::string& name, ResourceNodeType type = ResourceNodeType::Texture2D);
        u32 ImportExternalResource(const std::string& name, ID3D12Resource* resource);
        ResourceNode* GetResource(u32 id);
        const ResourceNode* GetResource(u32 id) const;
        const std::vector<ResourceNode>& GetResources() const { return m_Resources; }
        std::vector<ResourceNode>& GetResources() { return m_Resources; }

        // Pass管理
        u32 AddPass(const std::string& name, PassType type = PassType::Graphics);
        void RemovePass(u32 id);
        PassNode* GetPass(u32 id);
        const PassNode* GetPass(u32 id) const;
        const std::vector<PassNode>& GetPasses() const { return m_Passes; }
        std::vector<PassNode>& GetPasses() { return m_Passes; }

        // 设置Pass执行回调
        void SetPassExecuteCallback(u32 passId, RenderPassExecuteFunc callback);

        // 连接管理
        void Connect(u32 srcPassId, u32 srcOutputSlot, u32 dstPassId, u32 dstInputSlot);
        void Disconnect(u32 passId, u32 inputSlot);

        // 编译和执行
        bool Compile();
        void Execute(CommandList& cmdList);
        bool IsDirty() const { return m_IsDirty; }
        void MarkDirty() { m_IsDirty = true; }

        // 获取编译结果
        const CompileResult& GetLastCompileResult() const { return m_LastCompileResult; }
        const std::vector<u32>& GetExecutionOrder() const { return m_LastCompileResult.executionOrder; }

        // 序列化
        nlohmann::json Serialize() const;
        bool Deserialize(const nlohmann::json& json);
        bool SaveToFile(const std::string& path) const;
        bool LoadFromFile(const std::string& path);

        // 清空图
        void Clear();

        // 资源池
        ResourcePool& GetResourcePool() { return m_ResourcePool; }

    private:
        Device* m_Device = nullptr;
        std::vector<ResourceNode> m_Resources;
        std::vector<PassNode> m_Passes;
        std::unordered_map<u32, RenderPassExecuteFunc> m_ExecuteCallbacks;
        std::unordered_map<u32, ID3D12Resource*> m_ExternalResources;

        GraphCompiler m_Compiler;
        ResourcePool m_ResourcePool;
        CompileResult m_LastCompileResult;
        
        bool m_IsDirty = true;
        u32 m_NextResourceId = 0;
        u32 m_NextPassId = 0;
    };
}
