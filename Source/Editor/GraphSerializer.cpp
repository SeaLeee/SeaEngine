#include "Editor/GraphSerializer.h"
#include "RenderGraph/RenderGraph.h"
#include "Core/Log.h"
#include <fstream>
#include <sstream>

namespace Sea
{
    nlohmann::json GraphSerializer::Serialize(const RenderGraph& graph)
    {
        nlohmann::json j;
        j["version"] = "1.0";
        j["type"] = "SeaEngine.RenderGraph";

        // 序列化资源
        j["resources"] = nlohmann::json::array();
        for (const auto& resource : graph.GetResources())
        {
            j["resources"].push_back(SerializeResource(resource));
        }

        // 序列化Pass
        j["passes"] = nlohmann::json::array();
        for (const auto& pass : graph.GetPasses())
        {
            j["passes"].push_back(SerializePass(pass));
        }

        return j;
    }

    bool GraphSerializer::Deserialize(RenderGraph& graph, const nlohmann::json& json)
    {
        std::string error;
        if (!ValidateJson(json, error))
        {
            SEA_CORE_ERROR("Invalid graph JSON: {}", error);
            return false;
        }

        graph.Clear();

        try
        {
            // 反序列化资源
            if (json.contains("resources"))
            {
                for (const auto& rj : json["resources"])
                {
                    u32 id = graph.CreateResource(rj["name"], 
                        static_cast<ResourceNodeType>(rj["type"].get<int>()));
                    auto* resource = graph.GetResource(id);
                    if (resource)
                    {
                        DeserializeResource(*resource, rj);
                    }
                }
            }

            // 反序列化Pass
            if (json.contains("passes"))
            {
                for (const auto& pj : json["passes"])
                {
                    u32 id = graph.AddPass(pj["name"],
                        static_cast<PassType>(pj["passType"].get<int>()));
                    auto* pass = graph.GetPass(id);
                    if (pass)
                    {
                        DeserializePass(*pass, pj);
                    }
                }
            }

            graph.MarkDirty();
            return true;
        }
        catch (const std::exception& e)
        {
            SEA_CORE_ERROR("Failed to deserialize graph: {}", e.what());
            return false;
        }
    }

    bool GraphSerializer::SaveToFile(const RenderGraph& graph, const std::string& filePath)
    {
        try
        {
            nlohmann::json j = Serialize(graph);
            std::ofstream file(filePath);
            if (!file.is_open())
            {
                SEA_CORE_ERROR("Failed to open file for writing: {}", filePath);
                return false;
            }
            file << j.dump(2);
            SEA_CORE_INFO("Graph saved to: {}", filePath);
            return true;
        }
        catch (const std::exception& e)
        {
            SEA_CORE_ERROR("Failed to save graph: {}", e.what());
            return false;
        }
    }

    bool GraphSerializer::LoadFromFile(RenderGraph& graph, const std::string& filePath)
    {
        try
        {
            std::ifstream file(filePath);
            if (!file.is_open())
            {
                SEA_CORE_ERROR("Failed to open file: {}", filePath);
                return false;
            }
            nlohmann::json j;
            file >> j;
            bool result = Deserialize(graph, j);
            if (result)
            {
                SEA_CORE_INFO("Graph loaded from: {}", filePath);
            }
            return result;
        }
        catch (const std::exception& e)
        {
            SEA_CORE_ERROR("Failed to load graph: {}", e.what());
            return false;
        }
    }

    std::string GraphSerializer::ExportToString(const RenderGraph& graph)
    {
        std::stringstream ss;
        ss << "=== RenderGraph ===\n";
        ss << "Resources: " << graph.GetResources().size() << "\n";
        for (const auto& res : graph.GetResources())
        {
            ss << "  [" << res.GetId() << "] " << res.GetName() 
               << " (" << ResourceNode::GetTypeString(res.GetType()) << ")\n";
        }
        ss << "Passes: " << graph.GetPasses().size() << "\n";
        for (const auto& pass : graph.GetPasses())
        {
            ss << "  [" << pass.GetId() << "] " << pass.GetName()
               << " (" << PassNode::GetTypeString(pass.GetType()) << ")\n";
            ss << "    Inputs: " << pass.GetInputs().size() << "\n";
            ss << "    Outputs: " << pass.GetOutputs().size() << "\n";
        }
        return ss.str();
    }

    bool GraphSerializer::ValidateJson(const nlohmann::json& json, std::string& outError)
    {
        if (!json.contains("version"))
        {
            outError = "Missing 'version' field";
            return false;
        }
        if (!json.contains("type") || json["type"] != "SeaEngine.RenderGraph")
        {
            outError = "Invalid or missing 'type' field";
            return false;
        }
        return true;
    }

    nlohmann::json GraphSerializer::SerializeResource(const ResourceNode& resource)
    {
        nlohmann::json j;
        j["id"] = resource.GetId();
        j["name"] = resource.GetName();
        j["type"] = static_cast<int>(resource.GetType());
        j["width"] = resource.GetWidth();
        j["height"] = resource.GetHeight();
        j["depth"] = resource.GetDepth();
        j["format"] = static_cast<int>(resource.GetFormat());
        j["mipLevels"] = resource.GetMipLevels();
        j["posX"] = resource.GetPosX();
        j["posY"] = resource.GetPosY();
        j["external"] = resource.IsExternal();
        return j;
    }

    nlohmann::json GraphSerializer::SerializePass(const PassNode& pass)
    {
        nlohmann::json j;
        j["id"] = pass.GetId();
        j["name"] = pass.GetName();
        j["passType"] = static_cast<int>(pass.GetType());
        j["posX"] = pass.GetPosX();
        j["posY"] = pass.GetPosY();
        j["enabled"] = pass.IsEnabled();

        j["inputs"] = nlohmann::json::array();
        for (const auto& input : pass.GetInputs())
        {
            nlohmann::json ij;
            ij["name"] = input.name;
            ij["resourceId"] = input.resourceId;
            ij["required"] = input.isRequired;
            j["inputs"].push_back(ij);
        }

        j["outputs"] = nlohmann::json::array();
        for (const auto& output : pass.GetOutputs())
        {
            nlohmann::json oj;
            oj["name"] = output.name;
            oj["resourceId"] = output.resourceId;
            j["outputs"].push_back(oj);
        }

        return j;
    }

    bool GraphSerializer::DeserializeResource(ResourceNode& resource, const nlohmann::json& json)
    {
        resource.SetDimensions(json["width"], json["height"], json["depth"]);
        resource.SetFormat(static_cast<Format>(json["format"].get<int>()));
        resource.SetMipLevels(json["mipLevels"]);
        resource.SetPosition(json["posX"], json["posY"]);
        resource.SetExternal(json["external"]);
        return true;
    }

    bool GraphSerializer::DeserializePass(PassNode& pass, const nlohmann::json& json)
    {
        pass.SetPosition(json["posX"], json["posY"]);
        pass.SetEnabled(json["enabled"]);

        for (const auto& ij : json["inputs"])
        {
            u32 slot = pass.AddInput(ij["name"], ij["required"]);
            pass.SetInput(slot, ij["resourceId"]);
        }

        for (const auto& oj : json["outputs"])
        {
            u32 slot = pass.AddOutput(oj["name"]);
            pass.SetOutput(slot, oj["resourceId"]);
        }

        return true;
    }
}
