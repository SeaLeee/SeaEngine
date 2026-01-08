#pragma once
#include "Core/Types.h"
#include "Shader/ShaderCompiler.h"
#include <unordered_map>

namespace Sea
{
    struct ShaderData
    {
        std::string name;
        std::filesystem::path filePath;
        std::vector<u8> vsCode, psCode, gsCode, hsCode, dsCode, csCode;
    };

    class ShaderLibrary
    {
    public:
        void Add(const std::string& name, const ShaderData& shader);
        bool Load(const std::string& name, const std::filesystem::path& vsPath, const std::filesystem::path& psPath);
        bool LoadCompute(const std::string& name, const std::filesystem::path& csPath);
        ShaderData* Get(const std::string& name);
        bool Contains(const std::string& name) const;
        void Remove(const std::string& name);
        void Clear();

        const auto& GetAll() const { return m_Shaders; }

    private:
        std::unordered_map<std::string, ShaderData> m_Shaders;
    };
}
