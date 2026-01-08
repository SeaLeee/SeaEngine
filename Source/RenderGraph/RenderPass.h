#pragma once
#include "Core/Types.h"
#include "Graphics/GraphicsTypes.h"
#include <nlohmann/json.hpp>

namespace Sea
{
    class CommandList;
    class RenderGraph;

    // 资源描述
    struct RGResourceDesc
    {
        std::string name;
        enum Type { Texture, Buffer } type = Texture;
        u32 width = 0, height = 0;
        Format format = Format::R8G8B8A8_UNORM;
        TextureUsage usage = TextureUsage::ShaderResource | TextureUsage::RenderTarget;
        bool isExternal = false; // 外部传入的资源(如backbuffer)
    };

    // 资源句柄
    struct RGResourceHandle
    {
        u32 id = UINT32_MAX;
        bool IsValid() const { return id != UINT32_MAX; }
    };

    // Pass类型
    enum class PassType { Graphics, Compute, Copy };

    // Pass执行回调
    using PassExecuteFunc = std::function<void(CommandList& cmdList, class RenderPassContext& ctx)>;

    // Pass描述
    struct RenderPassDesc
    {
        std::string name;
        PassType type = PassType::Graphics;
        std::vector<RGResourceHandle> inputs;   // 读取的资源
        std::vector<RGResourceHandle> outputs;  // 写入的资源
        PassExecuteFunc executeFunc;

        // 用于节点编辑器的位置
        float nodeX = 0, nodeY = 0;
    };

    // Pass上下文 - 在执行时提供资源访问
    class RenderPassContext
    {
    public:
        ID3D12Resource* GetInputResource(u32 index) const;
        ID3D12Resource* GetOutputResource(u32 index) const;
        D3D12_CPU_DESCRIPTOR_HANDLE GetOutputRTV(u32 index) const;
        D3D12_CPU_DESCRIPTOR_HANDLE GetOutputDSV() const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetInputSRV(u32 index) const;

        void SetResources(const std::vector<ID3D12Resource*>& inputs,
                         const std::vector<ID3D12Resource*>& outputs,
                         const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& rtvs,
                         D3D12_CPU_DESCRIPTOR_HANDLE dsv,
                         const std::vector<D3D12_GPU_DESCRIPTOR_HANDLE>& srvs);

    private:
        std::vector<ID3D12Resource*> m_Inputs, m_Outputs;
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_RTVs;
        D3D12_CPU_DESCRIPTOR_HANDLE m_DSV = {};
        std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> m_SRVs;
    };

    // RenderGraph - 管线连接的核心
    class RenderGraph : public NonCopyable
    {
    public:
        RenderGraph();
        ~RenderGraph();

        // 资源管理
        RGResourceHandle CreateResource(const RGResourceDesc& desc);
        RGResourceHandle ImportExternalResource(const std::string& name, ID3D12Resource* resource);
        const RGResourceDesc* GetResourceDesc(RGResourceHandle handle) const;

        // Pass管理
        u32 AddPass(const RenderPassDesc& desc);
        void RemovePass(u32 passId);
        RenderPassDesc* GetPass(u32 passId);
        const std::vector<RenderPassDesc>& GetPasses() const { return m_Passes; }
        std::vector<RenderPassDesc>& GetPasses() { return m_Passes; }

        // 连接管理
        void Connect(u32 srcPass, u32 srcOutput, u32 dstPass, u32 dstInput);
        void Disconnect(u32 dstPass, u32 dstInput);

        // 编译和执行
        bool Compile();
        void Execute(CommandList& cmdList);

        // 获取编译后的执行顺序
        const std::vector<u32>& GetExecutionOrder() const { return m_ExecutionOrder; }

        // 序列化
        nlohmann::json Serialize() const;
        bool Deserialize(const nlohmann::json& json);

        // 资源列表
        const std::vector<RGResourceDesc>& GetResources() const { return m_Resources; }
        std::vector<RGResourceDesc>& GetResources() { return m_Resources; }

    private:
        std::vector<RGResourceDesc> m_Resources;
        std::vector<RenderPassDesc> m_Passes;
        std::vector<u32> m_ExecutionOrder;
        bool m_IsDirty = true;

        // 外部资源映射
        std::unordered_map<u32, ID3D12Resource*> m_ExternalResources;
    };
}
