#pragma once
#include "Core/Types.h"
#include <nlohmann/json.hpp>
#include <string>

namespace Sea
{
    class RenderGraph;

    // 图序列化器 - 保存和加载RenderGraph
    class GraphSerializer
    {
    public:
        GraphSerializer() = default;
        ~GraphSerializer() = default;

        // 序列化到JSON
        static nlohmann::json Serialize(const RenderGraph& graph);

        // 从JSON反序列化
        static bool Deserialize(RenderGraph& graph, const nlohmann::json& json);

        // 保存到文件
        static bool SaveToFile(const RenderGraph& graph, const std::string& filePath);

        // 从文件加载
        static bool LoadFromFile(RenderGraph& graph, const std::string& filePath);

        // 导出为可读格式（用于调试）
        static std::string ExportToString(const RenderGraph& graph);

        // 验证JSON结构
        static bool ValidateJson(const nlohmann::json& json, std::string& outError);

        // 获取文件扩展名
        static const char* GetFileExtension() { return ".seagraph"; }
        static const char* GetFileFilter() { return "SeaEngine Graph (*.seagraph)\0*.seagraph\0All Files (*.*)\0*.*\0"; }

    private:
        static nlohmann::json SerializeResource(const class ResourceNode& resource);
        static nlohmann::json SerializePass(const class PassNode& pass);
        static bool DeserializeResource(class ResourceNode& resource, const nlohmann::json& json);
        static bool DeserializePass(class PassNode& pass, const nlohmann::json& json);
    };
}
