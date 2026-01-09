#include "Editor/PassNodeWidget.h"
#include "RenderGraph/PassNode.h"

namespace Sea
{
    void PassNodeWidget::Render(PassNode& pass, int nodeId)
    {
        ImNodes::BeginNode(nodeId);

        // 标题栏
        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(pass.GetName().c_str());
        ImNodes::EndNodeTitleBar();

        // 输入引脚
        RenderInputPins(pass, nodeId);

        // 节点内容
        RenderNodeContent(pass);

        // 输出引脚
        RenderOutputPins(pass, nodeId);

        ImNodes::EndNode();

        // 设置节点位置（首次）
        ImVec2 pos = ImNodes::GetNodeGridSpacePos(nodeId);
        if (pos.x == 0 && pos.y == 0)
        {
            ImNodes::SetNodeGridSpacePos(nodeId, ImVec2(pass.GetPosX(), pass.GetPosY()));
        }
    }

    void PassNodeWidget::RenderInputPins(PassNode& pass, int nodeId)
    {
        const auto& inputs = pass.GetInputs();
        for (size_t i = 0; i < inputs.size(); ++i)
        {
            int pinId = GetInputPinId(nodeId, static_cast<int>(i));
            ImNodes::BeginInputAttribute(pinId);
            
            // 显示连接状态
            bool connected = inputs[i].IsConnected();
            if (connected)
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s", inputs[i].name.c_str());
            else if (inputs[i].isRequired)
                ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "%s*", inputs[i].name.c_str());
            else
                ImGui::TextUnformatted(inputs[i].name.c_str());

            ImNodes::EndInputAttribute();
        }
    }

    void PassNodeWidget::RenderOutputPins(PassNode& pass, int nodeId)
    {
        const auto& outputs = pass.GetOutputs();
        for (size_t i = 0; i < outputs.size(); ++i)
        {
            int pinId = GetOutputPinId(nodeId, static_cast<int>(i));
            ImNodes::BeginOutputAttribute(pinId);
            
            float indent = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(outputs[i].name.c_str()).x;
            ImGui::Indent(indent);
            ImGui::TextUnformatted(outputs[i].name.c_str());

            ImNodes::EndOutputAttribute();
        }
    }

    ImU32 PassNodeWidget::GetNodeColor(PassType type)
    {
        switch (type)
        {
            case PassType::Graphics:     return IM_COL32(50, 80, 120, 255);
            case PassType::Compute:      return IM_COL32(120, 80, 50, 255);
            case PassType::Copy:         return IM_COL32(80, 120, 50, 255);
            case PassType::AsyncCompute: return IM_COL32(120, 50, 120, 255);
            default:                     return IM_COL32(80, 80, 80, 255);
        }
    }

    ImU32 PassNodeWidget::GetNodeTitleColor(PassType type)
    {
        switch (type)
        {
            case PassType::Graphics:     return IM_COL32(70, 120, 180, 255);
            case PassType::Compute:      return IM_COL32(180, 120, 70, 255);
            case PassType::Copy:         return IM_COL32(120, 180, 70, 255);
            case PassType::AsyncCompute: return IM_COL32(180, 70, 180, 255);
            default:                     return IM_COL32(120, 120, 120, 255);
        }
    }

    int PassNodeWidget::GetInputPinId(int nodeId, int pinIndex)
    {
        // 格式: 高16位=节点ID, 位14=0表示输入, 低14位=引脚索引
        return (nodeId << 16) | (0 << 14) | (pinIndex & 0x3FFF);
    }

    int PassNodeWidget::GetOutputPinId(int nodeId, int pinIndex)
    {
        // 格式: 高16位=节点ID, 位14=1表示输出, 低14位=引脚索引
        return (nodeId << 16) | (1 << 14) | (pinIndex & 0x3FFF);
    }

    void PassNodeWidget::ParsePinId(int pinId, int& outNodeId, int& outPinIndex, bool& outIsInput)
    {
        outNodeId = (pinId >> 16) & 0xFFFF;
        outIsInput = ((pinId >> 14) & 1) == 0;
        outPinIndex = pinId & 0x3FFF;
    }

    void PassNodeWidget::RenderNodeContent(PassNode& pass)
    {
        // 显示Pass类型
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[%s]", PassNode::GetTypeString(pass.GetType()));
        
        // 显示启用状态
        bool enabled = pass.IsEnabled();
        if (ImGui::Checkbox("##enabled", &enabled))
        {
            pass.SetEnabled(enabled);
        }
        ImGui::SameLine();
        ImGui::TextColored(enabled ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                          enabled ? "Enabled" : "Disabled");
    }
}
