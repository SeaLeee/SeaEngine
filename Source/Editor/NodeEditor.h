#pragma once
#include "Core/Types.h"
#include "RenderGraph/RenderPass.h"
#include <imnodes.h>

namespace Sea
{
    class RenderGraph;

    // 核心节点编辑器 - 类似Falcor的可视化管线编辑
    class NodeEditor : public NonCopyable
    {
    public:
        NodeEditor(RenderGraph& graph);
        ~NodeEditor();

        void Initialize();
        void Shutdown();
        void Render();

        // 节点操作
        void AddPassNode(const std::string& name, PassType type);
        void AddResourceNode(const std::string& name, const RGResourceDesc& desc);
        void DeleteSelectedNodes();
        void ClearAll();

        // 序列化
        void SaveToFile(const std::string& path);
        void LoadFromFile(const std::string& path);

    private:
        void RenderNodes();
        void RenderLinks();
        void HandleNewLinks();
        void HandleDeletedLinks();
        void RenderContextMenu();

        int GetNextNodeId() { return m_NextNodeId++; }
        int GetNextPinId() { return m_NextPinId++; }
        int GetNextLinkId() { return m_NextLinkId++; }

    private:
        RenderGraph& m_Graph;
        ImNodesContext* m_Context = nullptr;

        struct NodeInfo
        {
            int nodeId;
            std::string name;
            enum Type { Pass, Resource } type;
            u32 passIndex = UINT32_MAX;
            u32 resourceIndex = UINT32_MAX;
            std::vector<int> inputPins;
            std::vector<int> outputPins;
        };

        struct LinkInfo
        {
            int linkId;
            int startPin, endPin;
        };

        std::vector<NodeInfo> m_Nodes;
        std::vector<LinkInfo> m_Links;

        int m_NextNodeId = 1;
        int m_NextPinId = 1000;
        int m_NextLinkId = 10000;

        bool m_ShowContextMenu = false;
        ImVec2 m_ContextMenuPos;
    };
}
