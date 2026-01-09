#include "Editor/ResourceNodeWidget.h"
#include "RenderGraph/ResourceNode.h"

namespace Sea
{
    void ResourceNodeWidget::Render(ResourceNode& resource, int nodeId)
    {
        // 设置节点样式
        ImNodes::PushColorStyle(ImNodesCol_TitleBar, GetNodeTitleColor(resource.GetType()));
        ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, GetNodeTitleColor(resource.GetType()));
        ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, GetNodeTitleColor(resource.GetType()));

        ImNodes::BeginNode(nodeId);

        // 标题栏
        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(resource.GetName().c_str());
        ImNodes::EndNodeTitleBar();

        // 输出引脚（资源可以被读取）
        int pinId = GetResourcePinId(nodeId);
        ImNodes::BeginOutputAttribute(pinId);
        RenderNodeContent(resource);
        ImNodes::EndOutputAttribute();

        ImNodes::EndNode();

        ImNodes::PopColorStyle();
        ImNodes::PopColorStyle();
        ImNodes::PopColorStyle();

        // 设置节点位置
        ImVec2 pos = ImNodes::GetNodeGridSpacePos(nodeId);
        if (pos.x == 0 && pos.y == 0)
        {
            ImNodes::SetNodeGridSpacePos(nodeId, ImVec2(resource.GetPosX(), resource.GetPosY()));
        }
    }

    ImU32 ResourceNodeWidget::GetNodeColor(ResourceNodeType type)
    {
        switch (type)
        {
            case ResourceNodeType::Texture2D:    return IM_COL32(60, 100, 60, 255);
            case ResourceNodeType::Texture3D:    return IM_COL32(100, 60, 60, 255);
            case ResourceNodeType::TextureCube:  return IM_COL32(60, 60, 100, 255);
            case ResourceNodeType::Buffer:       return IM_COL32(100, 100, 60, 255);
            case ResourceNodeType::DepthStencil: return IM_COL32(100, 60, 100, 255);
            default:                             return IM_COL32(80, 80, 80, 255);
        }
    }

    ImU32 ResourceNodeWidget::GetNodeTitleColor(ResourceNodeType type)
    {
        switch (type)
        {
            case ResourceNodeType::Texture2D:    return IM_COL32(80, 140, 80, 255);
            case ResourceNodeType::Texture3D:    return IM_COL32(140, 80, 80, 255);
            case ResourceNodeType::TextureCube:  return IM_COL32(80, 80, 140, 255);
            case ResourceNodeType::Buffer:       return IM_COL32(140, 140, 80, 255);
            case ResourceNodeType::DepthStencil: return IM_COL32(140, 80, 140, 255);
            default:                             return IM_COL32(120, 120, 120, 255);
        }
    }

    int ResourceNodeWidget::GetResourcePinId(int nodeId)
    {
        // 资源节点使用特殊的引脚ID格式
        return (nodeId << 16) | 0x8000;  // 高位设置标记
    }

    void ResourceNodeWidget::RenderNodeContent(ResourceNode& resource)
    {
        // 显示类型
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[%s]", 
                          ResourceNode::GetTypeString(resource.GetType()));

        // 显示尺寸
        if (resource.GetType() != ResourceNodeType::Buffer)
        {
            ImGui::Text("%dx%d", resource.GetWidth(), resource.GetHeight());
            if (resource.GetDepth() > 1)
            {
                ImGui::SameLine();
                ImGui::Text("x%d", resource.GetDepth());
            }
        }
        else
        {
            ImGui::Text("%llu bytes", resource.GetBufferSize());
        }

        // 显示外部资源标记
        if (resource.IsExternal())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[External]");
        }
    }

    void ResourceNodeWidget::RenderFormatSelector(ResourceNode& resource)
    {
        // 简化的格式选择器
        static const char* formats[] = {
            "R8G8B8A8_UNORM",
            "R16G16B16A16_FLOAT",
            "R32G32B32A32_FLOAT",
            "D24_UNORM_S8_UINT",
            "D32_FLOAT"
        };
        
        int currentFormat = static_cast<int>(resource.GetFormat());
        if (ImGui::Combo("Format", &currentFormat, formats, IM_ARRAYSIZE(formats)))
        {
            resource.SetFormat(static_cast<Format>(currentFormat));
        }
    }
}
