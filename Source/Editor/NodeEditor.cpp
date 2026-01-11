#include "Editor/NodeEditor.h"
#include "Core/Log.h"
#include "Core/FileSystem.h"
#include "Graphics/Device.h"
#include <imgui.h>
#include <imnodes.h>
#include <algorithm>

namespace Sea
{
    NodeEditor::NodeEditor(RenderGraph* graph, Device* device) 
        : m_Graph(graph), m_Device(device) 
    {
    }
    
    NodeEditor::~NodeEditor() 
    { 
        Shutdown(); 
    }

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
        style.Colors[ImNodesCol_NodeBackgroundHovered] = IM_COL32(50, 50, 50, 255);
        style.Colors[ImNodesCol_NodeBackgroundSelected] = IM_COL32(60, 60, 60, 255);
        style.Colors[ImNodesCol_Link] = IM_COL32(100, 200, 100, 255);
        style.Colors[ImNodesCol_LinkHovered] = IM_COL32(150, 255, 150, 255);
        style.Colors[ImNodesCol_LinkSelected] = IM_COL32(200, 255, 200, 255);
        style.Colors[ImNodesCol_Pin] = IM_COL32(150, 150, 250, 255);
        style.Colors[ImNodesCol_PinHovered] = IM_COL32(200, 200, 255, 255);
        style.NodeCornerRounding = 4.0f;
        style.PinCircleRadius = 4.0f;
        style.PinLineThickness = 2.0f;
        style.LinkThickness = 3.0f;
        
        // 启用属�?
        ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkDetachWithDragClick);
        
        SEA_CORE_INFO("Node Editor initialized");
    }

    void NodeEditor::Shutdown()
    {
        if (m_Context)
        {
            ImNodes::PopAttributeFlag();
            ImNodes::DestroyContext(m_Context);
            m_Context = nullptr;
        }
    }

    void NodeEditor::Render()
    {
        ImGui::Begin("Render Graph Editor", nullptr, ImGuiWindowFlags_MenuBar);

        // 菜单�?
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Graph"))
            {
                if (ImGui::BeginMenu("Add Pass"))
                {
                    RenderPassTemplateMenu();
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Add Resource"))
                {
                    if (ImGui::MenuItem("Texture2D"))
                        AddResourceNode("Texture2D", ResourceNodeType::Texture2D);
                    if (ImGui::MenuItem("DepthStencil"))
                        AddResourceNode("Depth", ResourceNodeType::DepthStencil);
                    if (ImGui::MenuItem("Buffer"))
                        AddResourceNode("Buffer", ResourceNodeType::Buffer);
                    if (ImGui::MenuItem("TextureCube"))
                        AddResourceNode("Cubemap", ResourceNodeType::TextureCube);
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Compile", "Ctrl+B"))
                {
                    if (m_Graph->Compile())
                        SEA_CORE_INFO("Graph compiled successfully");
                }
                if (ImGui::MenuItem("Auto Layout", "Ctrl+L"))
                    AutoLayout();
                if (ImGui::MenuItem("Center View", "Home"))
                    CenterView();
                ImGui::Separator();
                if (ImGui::MenuItem("Clear All"))
                    ClearAll();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Delete Selected", "Del"))
                    DeleteSelectedNodes();
                if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
                    DuplicateSelectedNodes();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save Graph...", "Ctrl+S"))
                    SaveToFile("RenderGraph.json");
                if (ImGui::MenuItem("Load Graph...", "Ctrl+O"))
                    LoadFromFile("RenderGraph.json");
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // 工具�?
        if (ImGui::Button("Compile"))
        {
            m_Graph->Compile();
        }
        ImGui::SameLine();
        if (ImGui::Button("Auto Layout"))
        {
            AutoLayout();
        }
        ImGui::SameLine();
        
        // 显示编译状�?
        const auto& result = m_Graph->GetLastCompileResult();
        if (result.success)
        {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Compiled (%zu passes)", result.executionOrder.size());
        }
        else if (!result.errorMessage.empty())
        {
            ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "Error: %s", result.errorMessage.c_str());
        }
        else
        {
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.3f, 1.0f), "Not compiled");
        }

        ImGui::Separator();

        // 节点编辑区域
        ImNodes::BeginNodeEditor();
        
        RenderNodes();
        RenderLinks();
        RenderMiniMap();
        RenderContextMenu();

        ImNodes::EndNodeEditor();

        HandleInput();
        HandleNewLinks();
        HandleDeletedLinks();
        HandleSelection();

        ImGui::End();
    }

    void NodeEditor::RenderPassTemplateMenu()
    {
        // 预定义模�?
        auto templates = PassTemplateLibrary::GetTemplateNames();
        for (const auto& name : templates)
        {
            if (ImGui::MenuItem(name.c_str()))
            {
                AddPassFromTemplate(name);
            }
        }
        
        ImGui::Separator();
        
        // 自定义类�?
        if (ImGui::MenuItem("Custom Graphics Pass"))
            AddPassNode("Graphics Pass", PassType::Graphics);
        if (ImGui::MenuItem("Custom Compute Pass"))
            AddPassNode("Compute Pass", PassType::Compute);
        if (ImGui::MenuItem("Custom Copy Pass"))
            AddPassNode("Copy Pass", PassType::Copy);
    }

    void NodeEditor::RenderNodes()
    {
        // 渲染Pass节点
        for (auto& pass : m_Graph->GetPasses())
        {
            int nodeId = GetNodeIdForPass(pass.GetId());

            // 只在首次渲染时设置位置，之后�?imnodes 管理拖动
            if (m_InitializedNodes.find(nodeId) == m_InitializedNodes.end())
            {
                if (pass.GetPosX() == 0 && pass.GetPosY() == 0)
                {
                    ImNodes::SetNodeGridSpacePos(nodeId, ImVec2(200.0f + pass.GetId() * 200.0f, 100.0f));
                }
                else
                {
                    ImNodes::SetNodeGridSpacePos(nodeId, ImVec2(pass.GetPosX(), pass.GetPosY()));
                }
                m_InitializedNodes.insert(nodeId);
            }

            // 根据Pass类型设置颜色
            ImU32 titleColor = IM_COL32(80, 120, 200, 255);
            switch (pass.GetType())
            {
                case PassType::Graphics:     titleColor = IM_COL32(80, 120, 200, 255); break;
                case PassType::Compute:      titleColor = IM_COL32(200, 120, 80, 255); break;
                case PassType::Copy:         titleColor = IM_COL32(120, 200, 80, 255); break;
                case PassType::AsyncCompute: titleColor = IM_COL32(200, 80, 200, 255); break;
            }
            
            if (!pass.IsEnabled())
            {
                titleColor = IM_COL32(100, 100, 100, 200);
            }
            
            ImNodes::PushColorStyle(ImNodesCol_TitleBar, titleColor);
            ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, titleColor + IM_COL32(20, 20, 20, 0));
            ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, titleColor + IM_COL32(40, 40, 40, 0));

            ImNodes::BeginNode(nodeId);
            
            ImNodes::BeginNodeTitleBar();
            ImGui::TextUnformatted(pass.GetName().c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", PassNode::GetTypeString(pass.GetType()));
            ImNodes::EndNodeTitleBar();

            // 输入引脚
            const auto& inputs = pass.GetInputs();
            for (size_t j = 0; j < inputs.size(); ++j)
            {
                int pinId = nodeId * 100 + static_cast<int>(j);
                
                ImU32 pinColor = inputs[j].IsConnected() ? IM_COL32(100, 200, 100, 255) : IM_COL32(150, 150, 150, 255);
                ImNodes::PushColorStyle(ImNodesCol_Pin, pinColor);
                
                ImNodes::BeginInputAttribute(pinId);
                ImGui::Text("%s", inputs[j].name.c_str());
                ImNodes::EndInputAttribute();
                
                ImNodes::PopColorStyle();
            }

            // 输出引脚
            const auto& outputs = pass.GetOutputs();
            for (size_t j = 0; j < outputs.size(); ++j)
            {
                int pinId = nodeId * 100 + 50 + static_cast<int>(j);
                
                ImU32 pinColor = outputs[j].IsConnected() ? IM_COL32(100, 200, 100, 255) : IM_COL32(200, 200, 100, 255);
                ImNodes::PushColorStyle(ImNodesCol_Pin, pinColor);
                
                ImNodes::BeginOutputAttribute(pinId);
                ImGui::Text("%s", outputs[j].name.c_str());
                ImNodes::EndOutputAttribute();
                
                ImNodes::PopColorStyle();
            }

            ImNodes::EndNode();
            ImNodes::PopColorStyle();
            ImNodes::PopColorStyle();
            ImNodes::PopColorStyle();

            // 保存节点位置
            ImVec2 pos = ImNodes::GetNodeGridSpacePos(nodeId);
            pass.SetPosition(pos.x, pos.y);
        }

        // 渲染资源节点
        for (auto& res : m_Graph->GetResources())
        {
            int nodeId = GetNodeIdForResource(res.GetId());

            // 只在首次渲染时设置位�?
            if (m_InitializedNodes.find(nodeId) == m_InitializedNodes.end())
            {
                if (res.GetPosX() == 0 && res.GetPosY() == 0)
                {
                    ImNodes::SetNodeGridSpacePos(nodeId, ImVec2(50.0f, 50.0f + res.GetId() * 100.0f));
                }
                else
                {
                    ImNodes::SetNodeGridSpacePos(nodeId, ImVec2(res.GetPosX(), res.GetPosY()));
                }
                m_InitializedNodes.insert(nodeId);
            }

            // 资源节点使用绿色系，外部资源使用橙色
            ImU32 titleColor = res.IsExternal() ? IM_COL32(200, 150, 50, 255) : IM_COL32(50, 150, 100, 255);
            
            ImNodes::PushColorStyle(ImNodesCol_TitleBar, titleColor);
            ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, titleColor + IM_COL32(20, 20, 20, 0));
            ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, titleColor + IM_COL32(40, 40, 40, 0));

            ImNodes::BeginNode(nodeId);
            
            ImNodes::BeginNodeTitleBar();
            ImGui::TextUnformatted(res.GetName().c_str());
            ImNodes::EndNodeTitleBar();

            // 资源信息
            ImGui::TextDisabled("%s", ResourceNode::GetTypeString(res.GetType()));
            if (res.GetWidth() > 0 && res.GetHeight() > 0)
            {
                ImGui::Text("%ux%u", res.GetWidth(), res.GetHeight());
            }
            if (res.IsExternal())
            {
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "[External]");
            }

            // 输出属性（资源可以被读取）
            int outPinId = nodeId * 100;
            ImNodes::BeginOutputAttribute(outPinId);
            ImGui::Text("Output");
            ImNodes::EndOutputAttribute();

            ImNodes::EndNode();
            ImNodes::PopColorStyle();
            ImNodes::PopColorStyle();
            ImNodes::PopColorStyle();

            // 保存节点位置
            ImVec2 pos = ImNodes::GetNodeGridSpacePos(nodeId);
            res.SetPosition(pos.x, pos.y);
        }
    }

    void NodeEditor::RenderLinks()
    {
        int linkId = 0;
        
        // Pass 输入连接到资�?
        for (const auto& pass : m_Graph->GetPasses())
        {
            int nodeId = GetNodeIdForPass(pass.GetId());
            const auto& inputs = pass.GetInputs();
            
            for (size_t i = 0; i < inputs.size(); ++i)
            {
                if (inputs[i].IsConnected())
                {
                    int endPin = nodeId * 100 + static_cast<int>(i);
                    int startPin = GetNodeIdForResource(inputs[i].resourceId) * 100;
                    
                    ImNodes::Link(linkId++, startPin, endPin);
                }
            }
        }
        
        // Pass 之间的连接（通过资源�?
        for (const auto& pass : m_Graph->GetPasses())
        {
            int nodeId = GetNodeIdForPass(pass.GetId());
            const auto& outputs = pass.GetOutputs();
            
            for (size_t i = 0; i < outputs.size(); ++i)
            {
                if (outputs[i].IsConnected())
                {
                    // 找到使用这个资源作为输入的所�?Pass
                    for (const auto& otherPass : m_Graph->GetPasses())
                    {
                        if (otherPass.GetId() == pass.GetId()) continue;
                        
                        const auto& otherInputs = otherPass.GetInputs();
                        for (size_t j = 0; j < otherInputs.size(); ++j)
                        {
                            if (otherInputs[j].resourceId == outputs[i].resourceId)
                            {
                                int startPin = nodeId * 100 + 50 + static_cast<int>(i);
                                int endPin = GetNodeIdForPass(otherPass.GetId()) * 100 + static_cast<int>(j);
                                
                                // 使用不同颜色表示 Pass 之间的连�?
                                ImNodes::PushColorStyle(ImNodesCol_Link, IM_COL32(150, 150, 255, 255));
                                ImNodes::Link(linkId++, startPin, endPin);
                                ImNodes::PopColorStyle();
                            }
                        }
                    }
                }
            }
        }
    }

    void NodeEditor::RenderMiniMap()
    {
        ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomRight);
    }

    void NodeEditor::HandleInput()
    {
        // 键盘快捷�?
        if (ImGui::IsWindowFocused())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Delete))
            {
                DeleteSelectedNodes();
            }
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D))
            {
                DuplicateSelectedNodes();
            }
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_B))
            {
                m_Graph->Compile();
            }
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_L))
            {
                AutoLayout();
            }
        }
    }

    void NodeEditor::HandleNewLinks()
    {
        int startPin, endPin;
        if (ImNodes::IsLinkCreated(&startPin, &endPin))
        {
            int startNodeId = startPin / 100;
            int endNodeId = endPin / 100;
            int inputSlot = endPin % 100;
            int outputSlot = (startPin % 100) - 50;

            // 资源 -> Pass 输入
            if (IsResourceNode(startNodeId) && !IsResourceNode(endNodeId))
            {
                u32 resourceId = GetResourceIdFromNode(startNodeId);
                u32 passId = GetPassIdFromNode(endNodeId);
                
                PassNode* pass = m_Graph->GetPass(passId);
                if (pass && inputSlot < static_cast<int>(pass->GetInputs().size()))
                {
                    pass->SetInput(static_cast<u32>(inputSlot), resourceId);
                    m_Graph->MarkDirty();
                    SEA_CORE_INFO("Connected resource {} to pass {} input {}", 
                                  resourceId, pass->GetName(), inputSlot);
                }
            }
            // Pass 输出 -> Pass 输入 (创建中间资源)
            else if (!IsResourceNode(startNodeId) && !IsResourceNode(endNodeId) && outputSlot >= 0)
            {
                u32 srcPassId = GetPassIdFromNode(startNodeId);
                u32 dstPassId = GetPassIdFromNode(endNodeId);
                
                PassNode* srcPass = m_Graph->GetPass(srcPassId);
                PassNode* dstPass = m_Graph->GetPass(dstPassId);
                
                if (srcPass && dstPass)
                {
                    const auto& outputs = srcPass->GetOutputs();
                    if (outputSlot < static_cast<int>(outputs.size()))
                    {
                        // 如果输出槽还没有关联资源，创建一�?
                        u32 resourceId = outputs[outputSlot].resourceId;
                        if (resourceId == UINT32_MAX)
                        {
                            std::string resName = srcPass->GetName() + "_" + outputs[outputSlot].name;
                            resourceId = m_Graph->CreateResource(resName, ResourceNodeType::Texture2D);
                            if (auto* res = m_Graph->GetResource(resourceId))
                            {
                                res->SetDimensions(1920, 1080);
                                res->SetFormat(Format::R8G8B8A8_UNORM);
                            }
                            srcPass->SetOutput(static_cast<u32>(outputSlot), resourceId);
                        }
                        
                        dstPass->SetInput(static_cast<u32>(inputSlot), resourceId);
                        m_Graph->MarkDirty();
                        SEA_CORE_INFO("Connected pass {} output {} to pass {} input {}", 
                                      srcPass->GetName(), outputSlot, dstPass->GetName(), inputSlot);
                    }
                }
            }
        }
    }

    void NodeEditor::HandleDeletedLinks()
    {
        int linkId;
        if (ImNodes::IsLinkDestroyed(&linkId))
        {
            // 重新编译�?
            m_Graph->MarkDirty();
            SEA_CORE_INFO("Link destroyed, graph marked dirty");
        }
    }

    void NodeEditor::HandleSelection()
    {
        int numSelected = ImNodes::NumSelectedNodes();
        if (numSelected == 1)
        {
            int selectedNode;
            ImNodes::GetSelectedNodes(&selectedNode);
            
            if (IsResourceNode(selectedNode))
            {
                m_SelectedResourceId = GetResourceIdFromNode(selectedNode);
                m_SelectedPassId = UINT32_MAX;
                
                if (m_SelectionCallback)
                    m_SelectionCallback(selectedNode, false, m_SelectedResourceId);
            }
            else
            {
                m_SelectedPassId = GetPassIdFromNode(selectedNode);
                m_SelectedResourceId = UINT32_MAX;
                
                if (m_SelectionCallback)
                    m_SelectionCallback(selectedNode, true, m_SelectedPassId);
            }
        }
        else
        {
            m_SelectedPassId = UINT32_MAX;
            m_SelectedResourceId = UINT32_MAX;
        }
    }

    void NodeEditor::RenderContextMenu()
    {
        // 右键打开上下文菜�?
        if (ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            m_ShowContextMenu = true;
            m_ContextMenuPos = ImGui::GetMousePos();
            ImGui::OpenPopup("NodeEditorContextMenu");
        }

        if (ImGui::BeginPopup("NodeEditorContextMenu"))
        {
            ImGui::Text("Add Node");
            ImGui::Separator();
            
            if (ImGui::BeginMenu("Pass (from Template)"))
            {
                RenderPassTemplateMenu();
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Pass (Custom)"))
            {
                if (ImGui::MenuItem("Graphics Pass"))
                    AddPassNode("Graphics Pass", PassType::Graphics);
                if (ImGui::MenuItem("Compute Pass"))
                    AddPassNode("Compute Pass", PassType::Compute);
                if (ImGui::MenuItem("Copy Pass"))
                    AddPassNode("Copy Pass", PassType::Copy);
                if (ImGui::MenuItem("Async Compute"))
                    AddPassNode("Async Compute", PassType::AsyncCompute);
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Resource"))
            {
                if (ImGui::MenuItem("Texture2D"))
                    AddResourceNode("Texture2D", ResourceNodeType::Texture2D);
                if (ImGui::MenuItem("DepthStencil"))
                    AddResourceNode("Depth", ResourceNodeType::DepthStencil);
                if (ImGui::MenuItem("Buffer"))
                    AddResourceNode("Buffer", ResourceNodeType::Buffer);
                if (ImGui::MenuItem("Texture3D"))
                    AddResourceNode("Volume", ResourceNodeType::Texture3D);
                if (ImGui::MenuItem("TextureCube"))
                    AddResourceNode("Cubemap", ResourceNodeType::TextureCube);
                ImGui::EndMenu();
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Delete Selected", "Del", false, HasSelection()))
                DeleteSelectedNodes();
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, HasSelection()))
                DuplicateSelectedNodes();
            
            ImGui::EndPopup();
        }
    }

    void NodeEditor::RenderNodeProperties()
    {
        // 在属性面板中渲染选中节点的属�?
    }

    void NodeEditor::AddPassNode(const std::string& name, PassType type)
    {
        u32 passId = m_Graph->AddPass(name, type);
        PassNode* pass = m_Graph->GetPass(passId);
        if (pass)
        {
            // 设置位置 - 使用简单偏�?
            float xOffset = 200.0f + (passId % 4) * 220.0f;
            float yOffset = 100.0f + (passId / 4) * 200.0f;
            pass->SetPosition(xOffset, yOffset);
            
            // 添加默认输入输出
            pass->AddInput("Input", false);
            pass->AddOutput("Output");
        }
        SEA_CORE_INFO("Added pass node: {} ({})", name, PassNode::GetTypeString(type));
    }

    void NodeEditor::AddPassFromTemplate(const std::string& templateName)
    {
        const PassTemplate* templ = PassTemplateLibrary::GetTemplate(templateName);
        if (!templ)
        {
            SEA_CORE_WARN("Template '{}' not found", templateName);
            return;
        }

        u32 passId = m_Graph->AddPass(templateName, templ->passType);
        PassNode* pass = m_Graph->GetPass(passId);
        if (pass)
        {
            // 计算节点位置 - 使用简单偏�?
            float xOffset = 200.0f + (passId % 4) * 220.0f;
            float yOffset = 100.0f + (passId / 4) * 200.0f;
            pass->SetPosition(xOffset, yOffset);

            for (const auto& input : templ->inputSlots)
            {
                pass->AddInput(input, true);
            }
            for (const auto& output : templ->outputSlots)
            {
                pass->AddOutput(output);
            }
        }
        SEA_CORE_INFO("Added pass from template: {}", templateName);
    }

    void NodeEditor::AddResourceNode(const std::string& name, ResourceNodeType type)
    {
        u32 resId = m_Graph->CreateResource(name, type);
        ResourceNode* res = m_Graph->GetResource(resId);
        if (res)
        {
            // 计算资源节点位置 - 左侧偏移
            float xOffset = 50.0f;
            float yOffset = 100.0f + resId * 120.0f;
            res->SetPosition(xOffset, yOffset);
            res->SetDimensions(1920, 1080);
            
            if (type == ResourceNodeType::DepthStencil)
                res->SetFormat(Format::D32_FLOAT);
            else
                res->SetFormat(Format::R8G8B8A8_UNORM);
        }
        SEA_CORE_INFO("Added resource node: {} ({})", name, ResourceNode::GetTypeString(type));
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
                if (!IsResourceNode(nodeId))
                {
                    u32 passId = GetPassIdFromNode(nodeId);
                    m_Graph->RemovePass(passId);
                    SEA_CORE_INFO("Deleted pass node: {}", passId);
                }
                // 资源节点暂时不删除，因为可能被引�?
            }
            
            ImNodes::ClearNodeSelection();
            m_SelectedPassId = UINT32_MAX;
            m_SelectedResourceId = UINT32_MAX;
        }
    }

    void NodeEditor::DuplicateSelectedNodes()
    {
        // TODO: 实现节点复制
        SEA_CORE_INFO("Duplicate not yet implemented");
    }

    void NodeEditor::ClearAll()
    {
        m_Graph->Clear();
        m_SelectedPassId = UINT32_MAX;
        m_SelectedResourceId = UINT32_MAX;
        m_InitializedNodes.clear();
        SEA_CORE_INFO("Cleared all nodes");
    }

    void NodeEditor::AutoLayout()
    {
        // 清除初始化状态，让位置重新应�?
        m_InitializedNodes.clear();
        
        // 简单的自动布局：按执行顺序排列
        const auto& passes = m_Graph->GetPasses();
        const auto& resources = m_Graph->GetResources();
        
        // 资源节点放在左边
        float yOffset = 50.0f;
        for (auto& res : m_Graph->GetResources())
        {
            res.SetPosition(50.0f, yOffset);
            yOffset += 120.0f;
        }
        
        // Pass 节点�?ID 顺序排列
        float xOffset = 300.0f;
        yOffset = 100.0f;
        for (auto& pass : m_Graph->GetPasses())
        {
            pass.SetPosition(xOffset, yOffset);
            xOffset += 220.0f;
            
            // 每行最�?�?
            if ((pass.GetId() + 1) % 4 == 0)
            {
                xOffset = 300.0f;
                yOffset += 200.0f;
            }
        }
        
        SEA_CORE_INFO("Auto layout applied");
    }

    void NodeEditor::CenterView()
    {
        // 计算所有节点的中心
        float minX = FLT_MAX, minY = FLT_MAX;
        float maxX = -FLT_MAX, maxY = -FLT_MAX;
        
        for (const auto& pass : m_Graph->GetPasses())
        {
            minX = std::min(minX, pass.GetPosX());
            minY = std::min(minY, pass.GetPosY());
            maxX = std::max(maxX, pass.GetPosX());
            maxY = std::max(maxY, pass.GetPosY());
        }
        for (const auto& res : m_Graph->GetResources())
        {
            minX = std::min(minX, res.GetPosX());
            minY = std::min(minY, res.GetPosY());
            maxX = std::max(maxX, res.GetPosX());
            maxY = std::max(maxY, res.GetPosY());
        }
        
        if (minX != FLT_MAX)
        {
            float centerX = (minX + maxX) / 2.0f;
            float centerY = (minY + maxY) / 2.0f;
            ImNodes::EditorContextResetPanning(ImVec2(-centerX + 400, -centerY + 300));
        }
    }

    void NodeEditor::SaveToFile(const std::string& path)
    {
        if (m_Graph->SaveToFile(path))
        {
            SEA_CORE_INFO("Graph saved to: {}", path);
        }
    }

    void NodeEditor::LoadFromFile(const std::string& path)
    {
        if (m_Graph->LoadFromFile(path))
        {
            SEA_CORE_INFO("Graph loaded from: {}", path);
        }
    }
}
