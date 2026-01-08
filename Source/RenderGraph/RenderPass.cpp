#include "RenderGraph/RenderPass.h"
#include "Core/Log.h"
#include <queue>
#include <algorithm>

namespace Sea
{
    // RenderPassContext implementation
    ID3D12Resource* RenderPassContext::GetInputResource(u32 index) const
    {
        return index < m_Inputs.size() ? m_Inputs[index] : nullptr;
    }

    ID3D12Resource* RenderPassContext::GetOutputResource(u32 index) const
    {
        return index < m_Outputs.size() ? m_Outputs[index] : nullptr;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE RenderPassContext::GetOutputRTV(u32 index) const
    {
        return index < m_RTVs.size() ? m_RTVs[index] : D3D12_CPU_DESCRIPTOR_HANDLE{};
    }

    D3D12_CPU_DESCRIPTOR_HANDLE RenderPassContext::GetOutputDSV() const { return m_DSV; }

    D3D12_GPU_DESCRIPTOR_HANDLE RenderPassContext::GetInputSRV(u32 index) const
    {
        return index < m_SRVs.size() ? m_SRVs[index] : D3D12_GPU_DESCRIPTOR_HANDLE{};
    }

    void RenderPassContext::SetResources(const std::vector<ID3D12Resource*>& inputs,
                                         const std::vector<ID3D12Resource*>& outputs,
                                         const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& rtvs,
                                         D3D12_CPU_DESCRIPTOR_HANDLE dsv,
                                         const std::vector<D3D12_GPU_DESCRIPTOR_HANDLE>& srvs)
    {
        m_Inputs = inputs; m_Outputs = outputs; m_RTVs = rtvs; m_DSV = dsv; m_SRVs = srvs;
    }

    // RenderGraph implementation
    RenderGraph::RenderGraph() = default;
    RenderGraph::~RenderGraph() = default;

    RGResourceHandle RenderGraph::CreateResource(const RGResourceDesc& desc)
    {
        RGResourceHandle handle;
        handle.id = static_cast<u32>(m_Resources.size());
        m_Resources.push_back(desc);
        m_IsDirty = true;
        return handle;
    }

    RGResourceHandle RenderGraph::ImportExternalResource(const std::string& name, ID3D12Resource* resource)
    {
        RGResourceDesc desc;
        desc.name = name;
        desc.isExternal = true;
        
        RGResourceHandle handle;
        handle.id = static_cast<u32>(m_Resources.size());
        m_Resources.push_back(desc);
        m_ExternalResources[handle.id] = resource;
        m_IsDirty = true;
        return handle;
    }

    const RGResourceDesc* RenderGraph::GetResourceDesc(RGResourceHandle handle) const
    {
        if (handle.id < m_Resources.size())
            return &m_Resources[handle.id];
        return nullptr;
    }

    u32 RenderGraph::AddPass(const RenderPassDesc& desc)
    {
        u32 id = static_cast<u32>(m_Passes.size());
        m_Passes.push_back(desc);
        m_IsDirty = true;
        return id;
    }

    void RenderGraph::RemovePass(u32 passId)
    {
        if (passId < m_Passes.size())
        {
            m_Passes.erase(m_Passes.begin() + passId);
            m_IsDirty = true;
        }
    }

    RenderPassDesc* RenderGraph::GetPass(u32 passId)
    {
        return passId < m_Passes.size() ? &m_Passes[passId] : nullptr;
    }

    void RenderGraph::Connect(u32 srcPass, u32 srcOutput, u32 dstPass, u32 dstInput)
    {
        if (srcPass >= m_Passes.size() || dstPass >= m_Passes.size()) return;
        if (srcOutput >= m_Passes[srcPass].outputs.size()) return;

        // 确保目标pass有足够的input slot
        while (m_Passes[dstPass].inputs.size() <= dstInput)
            m_Passes[dstPass].inputs.push_back({});

        m_Passes[dstPass].inputs[dstInput] = m_Passes[srcPass].outputs[srcOutput];
        m_IsDirty = true;
    }

    void RenderGraph::Disconnect(u32 dstPass, u32 dstInput)
    {
        if (dstPass < m_Passes.size() && dstInput < m_Passes[dstPass].inputs.size())
        {
            m_Passes[dstPass].inputs[dstInput] = {};
            m_IsDirty = true;
        }
    }

    bool RenderGraph::Compile()
    {
        if (!m_IsDirty) return true;

        m_ExecutionOrder.clear();
        
        // 拓扑排序
        std::vector<u32> inDegree(m_Passes.size(), 0);
        std::vector<std::vector<u32>> adjList(m_Passes.size());

        // 构建依赖图
        for (u32 i = 0; i < m_Passes.size(); ++i)
        {
            for (const auto& input : m_Passes[i].inputs)
            {
                if (!input.IsValid()) continue;
                
                // 找到哪个pass输出了这个资源
                for (u32 j = 0; j < m_Passes.size(); ++j)
                {
                    if (i == j) continue;
                    for (const auto& output : m_Passes[j].outputs)
                    {
                        if (output.id == input.id)
                        {
                            adjList[j].push_back(i);
                            inDegree[i]++;
                        }
                    }
                }
            }
        }

        // Kahn算法
        std::queue<u32> queue;
        for (u32 i = 0; i < m_Passes.size(); ++i)
        {
            if (inDegree[i] == 0) queue.push(i);
        }

        while (!queue.empty())
        {
            u32 node = queue.front();
            queue.pop();
            m_ExecutionOrder.push_back(node);

            for (u32 neighbor : adjList[node])
            {
                if (--inDegree[neighbor] == 0)
                    queue.push(neighbor);
            }
        }

        if (m_ExecutionOrder.size() != m_Passes.size())
        {
            SEA_CORE_ERROR("RenderGraph has cyclic dependencies!");
            return false;
        }

        m_IsDirty = false;
        SEA_CORE_INFO("RenderGraph compiled: {} passes", m_ExecutionOrder.size());
        return true;
    }

    void RenderGraph::Execute(CommandList& cmdList)
    {
        if (m_IsDirty) Compile();

        RenderPassContext ctx;
        for (u32 passId : m_ExecutionOrder)
        {
            auto& pass = m_Passes[passId];
            if (pass.executeFunc)
            {
                pass.executeFunc(cmdList, ctx);
            }
        }
    }

    nlohmann::json RenderGraph::Serialize() const
    {
        nlohmann::json j;
        j["resources"] = nlohmann::json::array();
        for (const auto& res : m_Resources)
        {
            j["resources"].push_back({
                {"name", res.name}, {"type", static_cast<int>(res.type)},
                {"width", res.width}, {"height", res.height},
                {"format", static_cast<int>(res.format)}, {"isExternal", res.isExternal}
            });
        }

        j["passes"] = nlohmann::json::array();
        for (const auto& pass : m_Passes)
        {
            nlohmann::json p;
            p["name"] = pass.name;
            p["type"] = static_cast<int>(pass.type);
            p["nodeX"] = pass.nodeX; p["nodeY"] = pass.nodeY;
            p["inputs"] = nlohmann::json::array();
            for (const auto& in : pass.inputs) p["inputs"].push_back(in.id);
            p["outputs"] = nlohmann::json::array();
            for (const auto& out : pass.outputs) p["outputs"].push_back(out.id);
            j["passes"].push_back(p);
        }
        return j;
    }

    bool RenderGraph::Deserialize(const nlohmann::json& j)
    {
        try {
            m_Resources.clear(); m_Passes.clear();
            
            for (const auto& r : j["resources"])
            {
                RGResourceDesc desc;
                desc.name = r["name"]; desc.type = static_cast<RGResourceDesc::Type>(r["type"].get<int>());
                desc.width = r["width"]; desc.height = r["height"];
                desc.format = static_cast<Format>(r["format"].get<int>());
                desc.isExternal = r["isExternal"];
                m_Resources.push_back(desc);
            }

            for (const auto& p : j["passes"])
            {
                RenderPassDesc pass;
                pass.name = p["name"]; pass.type = static_cast<PassType>(p["type"].get<int>());
                pass.nodeX = p["nodeX"]; pass.nodeY = p["nodeY"];
                for (const auto& i : p["inputs"]) pass.inputs.push_back({i.get<u32>()});
                for (const auto& o : p["outputs"]) pass.outputs.push_back({o.get<u32>()});
                m_Passes.push_back(pass);
            }

            m_IsDirty = true;
            return true;
        } catch (...) { return false; }
    }
}
