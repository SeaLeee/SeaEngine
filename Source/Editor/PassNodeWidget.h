#pragma once
#include "Core/Types.h"
#include "RenderGraph/PassNode.h"
#include <imgui.h>
#include <imnodes.h>

namespace Sea
{
    class RenderGraph;

    // Pass节点可视化组件 - 在节点编辑器中渲染Pass节点
    class PassNodeWidget
    {
    public:
        PassNodeWidget() = default;
        ~PassNodeWidget() = default;

        // 渲染Pass节点
        void Render(PassNode& pass, int nodeId);

        // 渲染Pass节点的输入输出引脚
        void RenderInputPins(PassNode& pass, int nodeId);
        void RenderOutputPins(PassNode& pass, int nodeId);

        // 获取节点颜色
        static ImU32 GetNodeColor(PassType type);
        static ImU32 GetNodeTitleColor(PassType type);

        // 获取引脚ID
        static int GetInputPinId(int nodeId, int pinIndex);
        static int GetOutputPinId(int nodeId, int pinIndex);

        // 从引脚ID解析节点和引脚索引
        static void ParsePinId(int pinId, int& outNodeId, int& outPinIndex, bool& outIsInput);

    private:
        void RenderNodeContent(PassNode& pass);
    };
}
