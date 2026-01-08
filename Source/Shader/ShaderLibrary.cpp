#include "Shader/ShaderLibrary.h"
#include "Core/Log.h"

namespace Sea
{
    void ShaderLibrary::Add(const std::string& name, const ShaderData& shader)
    {
        m_Shaders[name] = shader;
    }

    bool ShaderLibrary::Load(const std::string& name, const std::filesystem::path& vsPath, const std::filesystem::path& psPath)
    {
        ShaderCompileDesc vsDesc, psDesc;
        vsDesc.filePath = vsPath; vsDesc.stage = ShaderStage::Vertex;
        psDesc.filePath = psPath; psDesc.stage = ShaderStage::Pixel;

        auto vsResult = ShaderCompiler::Compile(vsDesc);
        auto psResult = ShaderCompiler::Compile(psDesc);

        if (!vsResult.success || !psResult.success)
        {
            SEA_CORE_ERROR("Failed to load shader '{}': VS:{} PS:{}", name, vsResult.errors, psResult.errors);
            return false;
        }

        ShaderData data;
        data.name = name;
        data.vsCode = std::move(vsResult.bytecode);
        data.psCode = std::move(psResult.bytecode);
        m_Shaders[name] = std::move(data);

        SEA_CORE_INFO("Shader '{}' loaded", name);
        return true;
    }

    bool ShaderLibrary::LoadCompute(const std::string& name, const std::filesystem::path& csPath)
    {
        ShaderCompileDesc desc;
        desc.filePath = csPath;
        desc.stage = ShaderStage::Compute;

        auto result = ShaderCompiler::Compile(desc);
        if (!result.success)
        {
            SEA_CORE_ERROR("Failed to load compute shader '{}': {}", name, result.errors);
            return false;
        }

        ShaderData data;
        data.name = name;
        data.csCode = std::move(result.bytecode);
        m_Shaders[name] = std::move(data);
        return true;
    }

    ShaderData* ShaderLibrary::Get(const std::string& name)
    {
        auto it = m_Shaders.find(name);
        return it != m_Shaders.end() ? &it->second : nullptr;
    }

    bool ShaderLibrary::Contains(const std::string& name) const { return m_Shaders.contains(name); }
    void ShaderLibrary::Remove(const std::string& name) { m_Shaders.erase(name); }
    void ShaderLibrary::Clear() { m_Shaders.clear(); }
}
