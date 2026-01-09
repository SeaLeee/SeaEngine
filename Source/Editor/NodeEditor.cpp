#include "Editor/NodeEditor.h"
#include "Core/Log.h"
#include "Core/FileSystem.h"
#include <imgui.h>
#include <imnodes.h>

namespace Sea
{
    NodeEditor::NodeEditor(RenderGraph& graph) : m_Graph(graph) {}
    NodeEditor::~NodeEditor() { Shutdown(); }

    void NodeEditor::Initialize()
    {
        m_Context = ImNodes::CreateContext();
        ImNodes::SetCurrentContext(m_Context);
        
        // 配置样式
        ImNodesStyle& style = ImNodes::GetStyle();
        style.Colors[ImNodesCol_TitleBar] = IM_COL32(50, 100, 150, 255);
        style.Colors[ImNodesCol_TitleBarHovered] = IM_COL32(70, 120, 170, 255);
        style.Colors[ImNodesCol_TitleBarSelected] = IM_COL32(90, 140, 190, 255);
        style.Colors[ImNodesCol_NodeBackground] = IM_COL32(40, 40, 40, 255);
        style.Colors[ImNodesCol_Link] = IM_COL32(100, 200, 100, 255);
        style.Colors[ImNodesCol_Pin] = IM_COL32(150, 150, 250, 255);
        
        SEA_CORE_INFO("Node Editor initialized");
    }

    void NodeEditor::Shutdown()
    {
        if (m_Context)
        {
            ImNodes::DestroyContext(m_Context);
            m_Context = nullptr;
        }
    }

    void NodeEditor::Render()
    {
        ImGui::Begin("Render Graph Editor", nullptr, ImGuiWindowFlags_MenuBar);

        // 菜单栏
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Graph"))
            {
                if (ImGui::MenuItem("Add Pass Node"))
                    AddPassNode("NewPass", PassType::Graphics);
                if (ImGui::MenuItem("Add Resource Node"))
                    AddResourceNode("NewResource", ResourceNodeType::Texture2D);
                ImGui::Separator();
                if (ImGui::MenuItem("Compile Graph"))
                    m_Graph.Compile();
                if (ImGui::MenuItem("Clear All"))
                    ClearAll();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save..."))
                    SaveToFile("graph.json");
                if (ImGui::MenuItem("Load..."))
                    LoadFromFile("graph.json");
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // 节点编辑区域
        ImNodes::BeginNodeEditor();
        
        RenderNodes();
        RenderLinks();
        RenderContextMenu();

        ImNodes::EndNodeEditor();

        HandleNewLinks();
        HandleDeletedLinks();

        ImGui::End();
    }

    void NodeEditor::RenderNodes()
    {
        // 渲染Pass节点
        for (auto& pass : m_Graph.GetPasses())
        {
            int nodeId = static_cast<int>(pass.GetId() + 1);

            // 设置节点位置
            if (pass.GetPosX() != 0 || pass.GetPosY() != 0)
                ImNodes::SetNodeGridSpacePos(nodeId, ImVec2(pass.GetPosX(), pass.GetPosY()));

            // 根据Pass类型设置颜色
            ImU32 titleColor = IM_COL32(80, 120, 200, 255);
            switch (pass.GetType())
            {
                case PassType::Graphics:     titleColor = IM_COL32(80, 120, 200, 255); break;
                case PassType::Compute:      titleColor = IM_COL32(200, 120, 80, 255); break;
                case PassType::Copy:         titleColor = IM_COL32(120, 200, 80, 255); break;
                case PassType::AsyncCompute: titleColor = IM_COL32(200, 80, 200, 255); break;
            }
            ImNodes::PushColorStyle(ImNodesCol_TitleBar, titleColor);

            ImNodes::BeginNode(nodeId);
            
            ImNodes::BeginNodeTitleBar();
            ImGui::TextUnformatted(pass.GetName().c_str());
            ImNodes::EndNodeTitleBar();

            // 输入引脚
            const auto& inputs = pass.GetInputs();
            for (size_t j = 0; j < inputs.size(); ++j)
            {
                int pinId = nodeId * 100 + static_cast<int>(j);
                ImNodes::BeginInputAttribute(pinId);
                ImGui::Text("%s", inputs[j].name.c_str());
                ImNodes::EndInputAttribute();
            }

            // 输出引脚
            const auto& outputs = pass.GetOutputs();
            for (size_t j = 0; j < outputs.size(); ++j)
            {
                int pinId = nodeId * 100 + 50 + static_cast<int>(j);
                ImNodes::BeginOutputAttribute(pinId);
                ImGui::Text("%s", outputs[j].name.c_str());
                ImNodes::EndOutputAttribute();
            }

            ImNodes::EndNode();
            ImNodes::PopColorStyle();

            // 保存节点位置
            ImVec2 pos = ImNodes::GetNodeGridSpacePos(nodeId);
            pass.SetPosition(pos.x, pos.y);
        }

        // 渲染资源节点
        for (auto& res : m_Graph.GetResources())
        {
            int nodeId = 1000 + static_cast<int>(res.GetId());

            if (res.GetPosX() != 0 || res.GetPosY() != 0)
                ImNodes::SetNodeGridSpacePos(nodeId, ImVec2(res.GetPosX(), res.GetPosY()));

            // 资源节点使用绿色系
            ImU32 titleColor = res.IsExternal() ? IM_COL32(200, 150, 50, 255) : IM_COL32(50, 150, 100, 255);
            ImNodes::PushColorStyle(ImNodesCol_TitleBar, titleColor);

            ImNodes::BeginNode(nodeId);
            
            ImNodes::BeginNodeTitleBar();
            ImGui::TextUnformatted(res.GetName().c_str());
            ImNodes::EndNodeTitleBar();

            // 资源信息
            ImGui::Text("%s", ResourceNode::GetTypeString(res.GetType()));
            if (res.GetWidth() > 0 && res.GetHeight() > 0)
            {
                ImGui::Text("%ux%u", res.GetWidth(), res.GetHeight());
            }

            // 输出属性（资源可以被读取）
            int outPinId = nodeId * 100;
            ImNodes::BeginOutputAttribute(outPinId);
            ImGui::Text("→");
            ImNodes::EndOutputAttribute();

            ImNodes::EndNode();
            ImNodes::PopColorStyle();

            // 保存节点位置
            ImVec2 pos = ImNodes::GetNodeGridSpacePos(nodeId);
            res.SetPosition(pos.x, pos.y);
        }
    }

    void NodeEditor::RenderLinks()
    {
        int linkId = 0;
        for (const auto& pass : m_Graph.GetPasses())
        {
            int nodeId = static_cast<int>(pass.GetId() + 1);
            const auto& inputs = pass.GetInputs();
            
            for (size_t i = 0; i < inputs.size(); ++i)
            {
                if (inputs[i].IsConnected())
                {
                    int endPin = nodeId * 100 + static_cast<int>(i);
                    // 找到输出这个资源的节点
                    int startPin = (1000 + static_cast<int>(inputs[i].resourceId)) * 100;
                    ImNodes::Link(linkId++, startPin, endPin);
                }
            }
        }
    }

    void NodeEditor::HandleNewLinks()
    {
        int startPin, endPin;
        if (ImNodes::IsLinkCreated(&startPin, &endPin))
        {
            // 解析pin ID
            int startNodeId = startPin / 100;
            int endNodeId = endPin / 100;
            int inputSlot = endPin % 100;

            // 连接资源到Pass输入
            if (startNodeId >= 1000) // 资源节点
            {
                u32 resourceId = static_cast<u32>(startNodeId - 1000);
                u32 passId = static_cast<u32>(endNodeId - 1);
                
                PassNode* pass = m_Graph.GetPass(passId);
                if (pass && inputSlot < static_cast<int>(pass->GetInputs().size()))
                {
                    pass->SetInput(static_cast<u32>(inputSlot), resourceId);
                    m_Graph.MarkDirty();
                }
            }
        }
    }

    void NodeEditor::HandleDeletedLinks()
    {
        int linkId;
        if (ImNodes::IsLinkDestroyed(&linkId))
        {
            // 简单处理：重新编译图
            m_Graph.MarkDirty();
        }
    }

    void NodeEditor::RenderContextMenu()
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsWindowHovered())
        {
            m_ShowContextMenu = true;
            m_ContextMenuPos = ImGui::GetMousePos();
        }

        if (m_ShowContextMenu)
        {
            ImGui::SetNextWindowPos(m_ContextMenuPos);
            if (ImGui::BeginPopup("NodeContextMenu"))
            {
                if (ImGui::MenuItem("Add Graphics Pass"))
                    AddPassNode("Graphics Pass", PassType::Graphics);
                if (ImGui::MenuItem("Add Compute Pass"))
                    AddPassNode("Compute Pass", PassType::Compute);
                if (ImGui::MenuItem("Add Copy Pass"))
                    AddPassNode("Copy Pass", PassType::Copy);
                ImGui::Separator();
                if (ImGui::MenuItem("Add Texture2D"))
                    AddResourceNode("Texture2D", ResourceNodeType::Texture2D);
                if (ImGui::MenuItem("Add DepthStencil"))
                    AddResourceNode("Depth", ResourceNodeType::DepthStencil);
                if (ImGui::MenuItem("Add Buffer"))
                    AddResourceNode("Buffer", ResourceNodeType::Buffer);

                ImGui::EndPopup();
            }
            else
            {
                ImGui::OpenPopup("NodeContextMenu");
            }

            if (!ImGui::IsPopupOpen("NodeContextMenu"))
                m_ShowContextMenu = false;
        }
    }

    void NodeEditor::AddPassNode(const std::string& name, PassType type)
    {
        u32 passId = m_Graph.AddPass(name, type);
        PassNode* pass = m_Graph.GetPass(passId);
        if (pass)
        {
            pass->SetPosition(m_ContextMenuPos.x, m_ContextMenuPos.y);
            // 添加默认输入输出
            pass->AddInput("Input", false);
            pass->AddOutput("Output");
        }
        SEA_CORE_INFO("Added pass node: {}", name);
    }

    void NodeEditor::AddResourceNode(const std::string& name, ResourceNodeType type)
    {
        u32 resId = m_Graph.CreateResource(name, type);
        ResourceNode* res = m_Graph.GetResource(resId);
        if (res)
        {
            res->SetPosition(m_ContextMenuPos.x, m_ContextMenuPos.y);
            res->SetDimensions(1920, 1080);
            res->SetFormat(Format::R8G8B8A8_UNORM);
        }
        SEA_CORE_INFO("Added resource node: {}", name);
    }

    void NodeEditor::DeleteSelectedNodes()
    {
        int numSelected = ImNodes::NumSelectedNodes();
        if (numSelected > 0)
        {
            std::vector<int> selectedNodes(numSelected);
            ImNodes::GetSelectedNodes(selectedNodes.data());

            for (int nodeId : selectedNodes)
            {
                if (nodeId < 1000)
                {
                    m_Graph.RemovePass(static_cast<u32>(nodeId - 1));
                }
                // 资源节点通常不删除，因为可能被引用
            }
        }
    }

    void NodeEditor::ClearAll()
    {
        m_Graph.Clear();
        m_Nodes.clear();
        m_Links.clear();
        SEA_CORE_INFO("Cleared all nodes");
    }

    void NodeEditor::SaveToFile(const std::string& path)
    {
        m_Graph.SaveToFile(path);
    }

    void NodeEditor::LoadFromFile(const std::string& path)
    {
        m_Graph.LoadFromFile(path);
    }
}
