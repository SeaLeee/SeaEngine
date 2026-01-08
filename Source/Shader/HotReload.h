#pragma once
#include "Core/Types.h"
#include "Shader/ShaderLibrary.h"
#include <filesystem>

namespace Sea
{
    class HotReload
    {
    public:
        HotReload(ShaderLibrary& library);
        void WatchDirectory(const std::filesystem::path& dir);
        void Update(); // 检查文件变化并重新编译
        void SetEnabled(bool enabled) { m_Enabled = enabled; }

    private:
        ShaderLibrary& m_Library;
        std::unordered_map<std::string, std::filesystem::file_time_type> m_FileTimestamps;
        bool m_Enabled = true;
    };
}
