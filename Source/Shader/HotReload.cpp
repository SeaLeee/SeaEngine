#include "Shader/HotReload.h"
#include "Core/Log.h"

namespace Sea
{
    HotReload::HotReload(ShaderLibrary& library) : m_Library(library) {}

    void HotReload::WatchDirectory(const std::filesystem::path& dir)
    {
        if (!std::filesystem::exists(dir)) return;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".hlsl")
            {
                m_FileTimestamps[entry.path().string()] = std::filesystem::last_write_time(entry);
            }
        }
    }

    void HotReload::Update()
    {
        if (!m_Enabled) return;
        
        for (auto& [path, lastTime] : m_FileTimestamps)
        {
            auto currentTime = std::filesystem::last_write_time(path);
            if (currentTime != lastTime)
            {
                lastTime = currentTime;
                SEA_CORE_INFO("Shader file changed: {}", path);
                // 触发重新编译逻辑
            }
        }
    }
}
