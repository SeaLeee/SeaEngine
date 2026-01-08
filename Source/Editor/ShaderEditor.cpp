#include "Editor/ShaderEditor.h"
#include "Shader/ShaderCompiler.h"
#include "Core/FileSystem.h"
#include <imgui.h>

namespace Sea
{
    void ShaderEditor::Render()
    {
        ImGui::Begin("Shader Editor", nullptr, ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open...")) {}
                if (ImGui::MenuItem("Save", "Ctrl+S", false, m_Modified)) SaveFile();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Build"))
            {
                if (ImGui::MenuItem("Compile", "F5")) Compile();
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // 代码编辑区
        ImGui::BeginChild("CodeEditor", ImVec2(0, -100), true);
        
        static char buffer[65536] = "";
        if (m_Source.size() < sizeof(buffer))
            strcpy_s(buffer, m_Source.c_str());

        if (ImGui::InputTextMultiline("##source", buffer, sizeof(buffer),
            ImVec2(-1, -1), ImGuiInputTextFlags_AllowTabInput))
        {
            m_Source = buffer;
            m_Modified = true;
        }
        ImGui::EndChild();

        // 编译输出
        ImGui::BeginChild("Output", ImVec2(0, 0), true);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Compiler Output:");
        ImGui::TextWrapped("%s", m_CompileOutput.c_str());
        ImGui::EndChild();

        ImGui::End();
    }

    void ShaderEditor::OpenFile(const std::string& path)
    {
        m_CurrentFile = path;
        m_Source = FileSystem::ReadTextFile(path);
        m_Modified = false;
    }

    void ShaderEditor::SaveFile()
    {
        if (!m_CurrentFile.empty())
        {
            FileSystem::WriteTextFile(m_CurrentFile, m_Source);
            m_Modified = false;
        }
    }

    void ShaderEditor::Compile()
    {
        if (m_CurrentFile.empty()) return;

        ShaderCompileDesc desc;
        desc.filePath = m_CurrentFile;
        
        // 根据文件名推断shader类型
        if (m_CurrentFile.find("VS") != std::string::npos || m_CurrentFile.find("_vs") != std::string::npos)
            desc.stage = ShaderStage::Vertex;
        else if (m_CurrentFile.find("PS") != std::string::npos || m_CurrentFile.find("_ps") != std::string::npos)
            desc.stage = ShaderStage::Pixel;
        else if (m_CurrentFile.find("CS") != std::string::npos || m_CurrentFile.find("_cs") != std::string::npos)
            desc.stage = ShaderStage::Compute;

        auto result = ShaderCompiler::CompileFromSource(m_Source, desc);
        
        if (result.success)
            m_CompileOutput = "Compilation successful! (" + std::to_string(result.bytecode.size()) + " bytes)";
        else
            m_CompileOutput = "Compilation failed:\n" + result.errors;
    }
}
