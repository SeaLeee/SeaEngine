#include "RenderGraph/RenderGraph.h"
#include "Graphics/Device.h"
#include "Graphics/CommandList.h"
#include "Core/Log.h"
#include "Core/FileSystem.h"
#include <fstream>

namespace Sea
{
    RenderGraph::RenderGraph() = default;
    RenderGraph::~RenderGraph() { Shutdown(); }

    void RenderGraph::Initialize(Device* device)
    {
        m_Device = device;
        m_ResourcePool.Initialize(device);
        SEA_CORE_INFO("RenderGraph initialized");
    }

    void RenderGraph::Shutdown()
    {
        m_ResourcePool.Shutdown();
        Clear();
    }

    u32 RenderGraph::CreateResource(const std::string& name, ResourceNodeType type)
    {
        u32 id = m_NextResourceId++;
        m_Resources.emplace_back(id, name, type);
        m_IsDirty = true;
        return id;
    }

    u32 RenderGraph::ImportExternalResource(const std::string& name, ID3D12Resource* resource)
    {
        u32 id = m_NextResourceId++;
        ResourceNode node(id, name, ResourceNodeType::Texture2D);
        node.SetExternal(true);
        m_Resources.push_back(node);
        m_ExternalResources[id] = resource;
        m_IsDirty = true;
        return id;
    }

    ResourceNode* RenderGraph::GetResource(u32 id)
    {
        for (auto& res : m_Resources)
        {
            if (res.GetId() == id) return &res;
        }
        return nullptr;
    }

    const ResourceNode* RenderGraph::GetResource(u32 id) const
    {
        for (const auto& res : m_Resources)
        {
            if (res.GetId() == id) return &res;
        }
        return nullptr;
    }

    u32 RenderGraph::AddPass(const std::string& name, PassType type)
    {
        u32 id = m_NextPassId++;
        m_Passes.emplace_back(id, name, type);
        m_IsDirty = true;
        return id;
    }

    void RenderGraph::RemovePass(u32 id)
    {
        auto it = std::find_if(m_Passes.begin(), m_Passes.end(),
            [id](const PassNode& p) { return p.GetId() == id; });
        if (it != m_Passes.end())
        {
            m_Passes.erase(it);
            m_ExecuteCallbacks.erase(id);
            m_IsDirty = true;
        }
    }

    PassNode* RenderGraph::GetPass(u32 id)
    {
        for (auto& pass : m_Passes)
        {
            if (pass.GetId() == id) return &pass;
        }
        return nullptr;
    }

    const PassNode* RenderGraph::GetPass(u32 id) const
    {
        for (const auto& pass : m_Passes)
        {
            if (pass.GetId() == id) return &pass;
        }
        return nullptr;
    }

    void RenderGraph::SetPassExecuteCallback(u32 passId, RenderPassExecuteFunc callback)
    {
        m_ExecuteCallbacks[passId] = std::move(callback);
    }

    void RenderGraph::Connect(u32 srcPassId, u32 srcOutputSlot, u32 dstPassId, u32 dstInputSlot)
    {
        PassNode* srcPass = GetPass(srcPassId);
        PassNode* dstPass = GetPass(dstPassId);
        
        if (!srcPass || !dstPass) return;

        const auto& outputs = srcPass->GetOutputs();
        if (srcOutputSlot >= outputs.size()) return;

        u32 resourceId = outputs[srcOutputSlot].resourceId;
        dstPass->SetInput(dstInputSlot, resourceId);
        m_IsDirty = true;
    }

    void RenderGraph::Disconnect(u32 passId, u32 inputSlot)
    {
        PassNode* pass = GetPass(passId);
        if (pass)
        {
            pass->ClearInput(inputSlot);
            m_IsDirty = true;
        }
    }

    bool RenderGraph::Compile()
    {
        if (!m_IsDirty) return m_LastCompileResult.success;

        m_LastCompileResult = m_Compiler.Compile(*this);
        
        if (m_LastCompileResult.success)
        {
            m_IsDirty = false;
            SEA_CORE_INFO("RenderGraph compiled successfully");
        }
        else
        {
            SEA_CORE_ERROR("RenderGraph compilation failed: {}", m_LastCompileResult.errorMessage);
        }

        return m_LastCompileResult.success;
    }

    void RenderGraph::Execute(CommandList& cmdList)
    {
        if (m_IsDirty && !Compile())
        {
            return;
        }

        m_ResourcePool.BeginFrame(0); // TODO: 使用正确的帧索引

        RenderGraphContext ctx;
        ctx.SetDevice(m_Device);

        for (u32 passId : m_LastCompileResult.executionOrder)
        {
            PassNode* pass = GetPass(passId);
            if (!pass || !pass->IsEnabled()) continue;

            // 设置资源上下文
            std::vector<ID3D12Resource*> inputs, outputs;
            for (const auto& input : pass->GetInputs())
            {
                if (input.IsConnected())
                {
                    auto it = m_ExternalResources.find(input.resourceId);
                    inputs.push_back(it != m_ExternalResources.end() ? it->second : nullptr);
                }
            }
            for (const auto& output : pass->GetOutputs())
            {
                if (output.IsConnected())
                {
                    auto it = m_ExternalResources.find(output.resourceId);
                    outputs.push_back(it != m_ExternalResources.end() ? it->second : nullptr);
                }
            }
            ctx.SetInputResources(inputs);
            ctx.SetOutputResources(outputs);

            // 执行Pass
            auto it = m_ExecuteCallbacks.find(passId);
            if (it != m_ExecuteCallbacks.end() && it->second)
            {
                it->second(cmdList, ctx);
            }
        }

        m_ResourcePool.EndFrame();
    }

    nlohmann::json RenderGraph::Serialize() const
    {
        nlohmann::json j;
        
        // 序列化资源
        j["resources"] = nlohmann::json::array();
        for (const auto& res : m_Resources)
        {
            nlohmann::json rj;
            rj["id"] = res.GetId();
            rj["name"] = res.GetName();
            rj["type"] = static_cast<int>(res.GetType());
            rj["width"] = res.GetWidth();
            rj["height"] = res.GetHeight();
            rj["format"] = static_cast<int>(res.GetFormat());
            rj["posX"] = res.GetPosX();
            rj["posY"] = res.GetPosY();
            rj["external"] = res.IsExternal();
            j["resources"].push_back(rj);
        }

        // 序列化Pass
        j["passes"] = nlohmann::json::array();
        for (const auto& pass : m_Passes)
        {
            nlohmann::json pj;
            pj["id"] = pass.GetId();
            pj["name"] = pass.GetName();
            pj["type"] = static_cast<int>(pass.GetType());
            pj["posX"] = pass.GetPosX();
            pj["posY"] = pass.GetPosY();
            pj["enabled"] = pass.IsEnabled();

            pj["inputs"] = nlohmann::json::array();
            for (const auto& input : pass.GetInputs())
            {
                nlohmann::json ij;
                ij["name"] = input.name;
                ij["resourceId"] = input.resourceId;
                ij["required"] = input.isRequired;
                pj["inputs"].push_back(ij);
            }

            pj["outputs"] = nlohmann::json::array();
            for (const auto& output : pass.GetOutputs())
            {
                nlohmann::json oj;
                oj["name"] = output.name;
                oj["resourceId"] = output.resourceId;
                pj["outputs"].push_back(oj);
            }

            j["passes"].push_back(pj);
        }

        return j;
    }

    bool RenderGraph::Deserialize(const nlohmann::json& json)
    {
        try
        {
            Clear();

            // 反序列化资源
            if (json.contains("resources"))
            {
                for (const auto& rj : json["resources"])
                {
                    u32 id = rj["id"];
                    std::string name = rj["name"];
                    auto type = static_cast<ResourceNodeType>(rj["type"].get<int>());
                    
                    ResourceNode node(id, name, type);
                    node.SetDimensions(rj["width"], rj["height"]);
                    node.SetFormat(static_cast<Format>(rj["format"].get<int>()));
                    node.SetPosition(rj["posX"], rj["posY"]);
                    node.SetExternal(rj["external"]);
                    
                    m_Resources.push_back(node);
                    m_NextResourceId = std::max(m_NextResourceId, id + 1);
                }
            }

            // 反序列化Pass
            if (json.contains("passes"))
            {
                for (const auto& pj : json["passes"])
                {
                    u32 id = pj["id"];
                    std::string name = pj["name"];
                    auto type = static_cast<PassType>(pj["type"].get<int>());
                    
                    PassNode pass(id, name, type);
                    pass.SetPosition(pj["posX"], pj["posY"]);
                    pass.SetEnabled(pj["enabled"]);

                    for (const auto& ij : pj["inputs"])
                    {
                        u32 slot = pass.AddInput(ij["name"], ij["required"]);
                        pass.SetInput(slot, ij["resourceId"]);
                    }

                    for (const auto& oj : pj["outputs"])
                    {
                        u32 slot = pass.AddOutput(oj["name"]);
                        pass.SetOutput(slot, oj["resourceId"]);
                    }

                    m_Passes.push_back(pass);
                    m_NextPassId = std::max(m_NextPassId, id + 1);
                }
            }

            m_IsDirty = true;
            return true;
        }
        catch (const std::exception& e)
        {
            SEA_CORE_ERROR("Failed to deserialize RenderGraph: {}", e.what());
            return false;
        }
    }

    bool RenderGraph::SaveToFile(const std::string& path) const
    {
        try
        {
            nlohmann::json j = Serialize();
            std::ofstream file(path);
            if (!file.is_open()) return false;
            file << j.dump(2);
            SEA_CORE_INFO("RenderGraph saved to: {}", path);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool RenderGraph::LoadFromFile(const std::string& path)
    {
        try
        {
            std::ifstream file(path);
            if (!file.is_open()) return false;
            nlohmann::json j;
            file >> j;
            bool result = Deserialize(j);
            if (result)
            {
                SEA_CORE_INFO("RenderGraph loaded from: {}", path);
            }
            return result;
        }
        catch (...)
        {
            return false;
        }
    }

    void RenderGraph::Clear()
    {
        m_Resources.clear();
        m_Passes.clear();
        m_ExecuteCallbacks.clear();
        m_ExternalResources.clear();
        m_NextResourceId = 0;
        m_NextPassId = 0;
        m_IsDirty = true;
    }
}
