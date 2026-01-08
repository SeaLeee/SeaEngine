#include "Editor/NodeEditor.h"
#include "RenderGraph/RenderPass.h"
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
                    AddResourceNode("NewResource", {});
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
        for (size_t i = 0; i < m_Graph.GetPasses().size(); ++i)
        {
            auto& pass = m_Graph.GetPasses()[i];
            int nodeId = static_cast<int>(i + 1);

            // 设置节点位置
            if (pass.nodeX != 0 || pass.nodeY != 0)
                ImNodes::SetNodeGridSpacePos(nodeId, ImVec2(pass.nodeX, pass.nodeY));

            // 根据Pass类型设置颜色
            ImU32 titleColor;
            switch (pass.type)
            {
                case PassType::Graphics: titleColor = IM_COL32(80, 120, 200, 255); break;
                case PassType::Compute:  titleColor = IM_COL32(200, 120, 80, 255); break;
                case PassType::Copy:     titleColor = IM_COL32(120, 200, 80, 255); break;
            }
            ImNodes::PushColorStyle(ImNodesCol_TitleBar, titleColor);

            ImNodes::BeginNode(nodeId);
            
            ImNodes::BeginNodeTitleBar();
            ImGui::TextUnformatted(pass.name.c_str());
            ImNodes::EndNodeTitleBar();

            // 输入引脚
            for (size_t j = 0; j < pass.inputs.size(); ++j)
            {
                int pinId = nodeId * 100 + static_cast<int>(j);
                ImNodes::BeginInputAttribute(pinId);
                ImGui::Text("In %zu", j);
                ImNodes::EndInputAttribute();
            }

            // 添加空输入槽
            int emptyInputPin = nodeId * 100 + static_cast<int>(pass.inputs.size());
            ImNodes::BeginInputAttribute(emptyInputPin);
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "+ Input");
            ImNodes::EndInputAttribute();

            // 输出引脚
            for (size_t j = 0; j < pass.outputs.size(); ++j)
            {
                int pinId = nodeId * 100 + 50 + static_cast<int>(j);
                ImNodes::BeginOutputAttribute(pinId);
                ImGui::Text("Out %zu", j);
                ImNodes::EndOutputAttribute();
            }

            ImNodes::EndNode();
            ImNodes::PopColorStyle();

            // 保存节点位置
            ImVec2 pos = ImNodes::GetNodeGridSpacePos(nodeId);
            pass.nodeX = pos.x;
            pass.nodeY = pos.y;
        }

        // 渲染资源节点
        for (size_t i = 0; i < m_Graph.GetResources().size(); ++i)
        {
            auto& res = m_Graph.GetResources()[i];
            int nodeId = 1000 + static_cast<int>(i);

            ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(150, 80, 150, 255));
            ImNodes::BeginNode(nodeId);
            
            ImNodes::BeginNodeTitleBar();
            ImGui::Text("[R] %s", res.name.c_str());
            ImNodes::EndNodeTitleBar();

            ImGui::Text("%ux%u", res.width, res.height);

            int pinId = nodeId * 100;
            ImNodes::BeginOutputAttribute(pinId);
            ImGui::Text("Resource");
            ImNodes::EndOutputAttribute();

            ImNodes::EndNode();
            ImNodes::PopColorStyle();
        }
    }

    void NodeEditor::RenderLinks()
    {
        for (const auto& link : m_Links)
        {
            ImNodes::Link(link.linkId, link.startPin, link.endPin);
        }
    }

    void NodeEditor::HandleNewLinks()
    {
        int startPin, endPin;
        if (ImNodes::IsLinkCreated(&startPin, &endPin))
        {
            LinkInfo link;
            link.linkId = GetNextLinkId();
            link.startPin = startPin;
            link.endPin = endPin;
            m_Links.push_back(link);

            SEA_CORE_INFO("Link created: {} -> {}", startPin, endPin);
        }
    }

    void NodeEditor::HandleDeletedLinks()
    {
        int linkId;
        if (ImNodes::IsLinkDestroyed(&linkId))
        {
            auto it = std::find_if(m_Links.begin(), m_Links.end(),
                [linkId](const LinkInfo& l) { return l.linkId == linkId; });
            if (it != m_Links.end())
                m_Links.erase(it);
        }
    }

    void NodeEditor::RenderContextMenu()
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImNodes::IsEditorHovered())
        {
            m_ShowContextMenu = true;
            m_ContextMenuPos = ImGui::GetMousePos();
        }

        if (m_ShowContextMenu)
        {
            ImGui::SetNextWindowPos(m_ContextMenuPos);
            if (ImGui::BeginPopup("NodeContextMenu"))
            {
                if (ImGui::BeginMenu("Add Pass"))
                {
                    if (ImGui::MenuItem("Graphics Pass"))
                        AddPassNode("GraphicsPass", PassType::Graphics);
                    if (ImGui::MenuItem("Compute Pass"))
                        AddPassNode("ComputePass", PassType::Compute);
                    if (ImGui::MenuItem("Copy Pass"))
                        AddPassNode("CopyPass", PassType::Copy);
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Add Resource"))
                {
                    if (ImGui::MenuItem("Render Target"))
                    {
                        RGResourceDesc desc;
                        desc.name = "RenderTarget";
                        desc.width = 1920; desc.height = 1080;
                        desc.usage = TextureUsage::RenderTarget | TextureUsage::ShaderResource;
                        AddResourceNode("RenderTarget", desc);
                    }
                    if (ImGui::MenuItem("Depth Buffer"))
                    {
                        RGResourceDesc desc;
                        desc.name = "DepthBuffer";
                        desc.width = 1920; desc.height = 1080;
                        desc.format = Format::D32_FLOAT;
                        desc.usage = TextureUsage::DepthStencil;
                        AddResourceNode("DepthBuffer", desc);
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Delete Selected"))
                    DeleteSelectedNodes();
                ImGui::EndPopup();
            }
            else
            {
                ImGui::OpenPopup("NodeContextMenu");
            }
        }
    }

    void NodeEditor::AddPassNode(const std::string& name, PassType type)
    {
        RenderPassDesc desc;
        desc.name = name;
        desc.type = type;
        desc.nodeX = m_ContextMenuPos.x;
        desc.nodeY = m_ContextMenuPos.y;
        
        // 默认添加一个输出
        RGResourceHandle output;
        output.id = static_cast<u32>(m_Graph.GetResources().size());
        
        RGResourceDesc resDesc;
        resDesc.name = name + "_Output";
        resDesc.width = 1920; resDesc.height = 1080;
        m_Graph.CreateResource(resDesc);
        desc.outputs.push_back(output);

        m_Graph.AddPass(desc);
        SEA_CORE_INFO("Added pass node: {}", name);
    }

    void NodeEditor::AddResourceNode(const std::string& name, const RGResourceDesc& desc)
    {
        RGResourceDesc resDesc = desc;
        resDesc.name = name;
        m_Graph.CreateResource(resDesc);
        SEA_CORE_INFO("Added resource node: {}", name);
    }

    void NodeEditor::DeleteSelectedNodes()
    {
        int numSelected = ImNodes::NumSelectedNodes();
        if (numSelected > 0)
        {
            std::vector<int> selected(numSelected);
            ImNodes::GetSelectedNodes(selected.data());
            // 删除逻辑...
        }
    }

    void NodeEditor::ClearAll()
    {
        m_Graph.GetPasses().clear();
        m_Graph.GetResources().clear();
        m_Links.clear();
        m_Nodes.clear();
    }

    void NodeEditor::SaveToFile(const std::string& path)
    {
        auto json = m_Graph.Serialize();
        FileSystem::WriteTextFile(path, json.dump(2));
        SEA_CORE_INFO("Graph saved to: {}", path);
    }

    void NodeEditor::LoadFromFile(const std::string& path)
    {
        std::string content = FileSystem::ReadTextFile(path);
        if (!content.empty())
        {
            auto json = nlohmann::json::parse(content);
            m_Graph.Deserialize(json);
            SEA_CORE_INFO("Graph loaded from: {}", path);
        }
    }
}
