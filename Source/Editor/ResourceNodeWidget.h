#pragma once
#include "Core/Types.h"
#include "RenderGraph/ResourceNode.h"
#include <imgui.h>
#include <imnodes.h>

namespace Sea
{
    class RenderGraph;

    // 资源节点可视化组件 - 在节点编辑器中渲染资源节点
    class ResourceNodeWidget
    {
    public:
        ResourceNodeWidget() = default;
        ~ResourceNodeWidget() = default;

        // 渲染资源节点
        void Render(ResourceNode& resource, int nodeId);

        // 获取节点颜色
        static ImU32 GetNodeColor(ResourceNodeType type);
        static ImU32 GetNodeTitleColor(ResourceNodeType type);

        // 资源引脚ID
        static int GetResourcePinId(int nodeId);

    private:
        void RenderNodeContent(ResourceNode& resource);
        void RenderFormatSelector(ResourceNode& resource);
    };
}
