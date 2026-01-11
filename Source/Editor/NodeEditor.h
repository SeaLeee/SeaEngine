#pragma once
#include "Core/Types.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/PassTemplate.h"
#include <imnodes.h>
#include <functional>
#include <unordered_set>

namespace Sea
{
    class Device;
    class ShaderLibrary;

    // 节点选择回调
    using NodeSelectionCallback = std::function<void(int nodeId, bool isPass, u32 index)>;

    // 核心节点编辑器 - 类似Falcor的可视化管线编辑
    class NodeEditor : public NonCopyable
    {
    public:
        NodeEditor(RenderGraph* graph, Device* device = nullptr);
        ~NodeEditor();

        void Initialize();
        void Shutdown();
        void Render();

        // 设置设备引用（用于创建资源）
        void SetDevice(Device* device) { m_Device = device; }
        
        // 设置RenderGraph（用于管线切换时更新）
        void SetRenderGraph(RenderGraph* graph) { m_Graph = graph; m_InitializedNodes.clear(); }

        // 节点操作
        void AddPassNode(const std::string& name, PassType type);
        void AddPassFromTemplate(const std::string& templateName);
        void AddResourceNode(const std::string& name, ResourceNodeType type);
        void DeleteSelectedNodes();
        void DuplicateSelectedNodes();
        void ClearAll();

        // 获取选中的节点信息
        bool HasSelection() const { return m_SelectedPassId != UINT32_MAX || m_SelectedResourceId != UINT32_MAX; }
        u32 GetSelectedPassId() const { return m_SelectedPassId; }
        u32 GetSelectedResourceId() const { return m_SelectedResourceId; }
        
        // 选择回调
        void SetSelectionCallback(NodeSelectionCallback callback) { m_SelectionCallback = std::move(callback); }

        // 序列化
        void SaveToFile(const std::string& path);
        void LoadFromFile(const std::string& path);

        // 自动布局
        void AutoLayout();

        // 居中视图
        void CenterView();

    private:
        void RenderNodes();
        void RenderLinks();
        void RenderMiniMap();
        void HandleInput();
        void HandleNewLinks();
        void HandleDeletedLinks();
        void HandleSelection();
        void RenderContextMenu();
        void RenderPassTemplateMenu();
        void RenderNodeProperties();

        int GetNodeIdForPass(u32 passId) const { return static_cast<int>(passId + 1); }
        int GetNodeIdForResource(u32 resId) const { return static_cast<int>(1000 + resId); }
        u32 GetPassIdFromNode(int nodeId) const { return static_cast<u32>(nodeId - 1); }
        u32 GetResourceIdFromNode(int nodeId) const { return static_cast<u32>(nodeId - 1000); }
        bool IsResourceNode(int nodeId) const { return nodeId >= 1000; }

    private:
        RenderGraph* m_Graph = nullptr;
        Device* m_Device = nullptr;
        ImNodesContext* m_Context = nullptr;

        // 选择状态
        u32 m_SelectedPassId = UINT32_MAX;
        u32 m_SelectedResourceId = UINT32_MAX;
        NodeSelectionCallback m_SelectionCallback;

        // 右键菜单
        bool m_ShowContextMenu = false;
        ImVec2 m_ContextMenuPos;
        
        // 编辑状态
        bool m_IsDragging = false;
        bool m_NeedsAutoLayout = false;
        
        // 节点位置初始化跟踪
        std::unordered_set<int> m_InitializedNodes;
    };
}
