#pragma once

#include "Core/Types.h"
#include <filesystem>

namespace Sea
{
    class FileSystem
    {
    public:
        static std::string ReadTextFile(const std::filesystem::path& path);
        static std::vector<u8> ReadBinaryFile(const std::filesystem::path& path);
        
        static bool WriteTextFile(const std::filesystem::path& path, const std::string& content);
        static bool WriteBinaryFile(const std::filesystem::path& path, const std::vector<u8>& data);

        static bool Exists(const std::filesystem::path& path);
        static bool IsDirectory(const std::filesystem::path& path);
        static bool CreateDirectories(const std::filesystem::path& path);

        static std::filesystem::path GetExecutablePath();
        static std::filesystem::path GetWorkingDirectory();
        static std::filesystem::path GetShadersDirectory();
        static std::filesystem::path GetAssetsDirectory();

        static std::vector<std::filesystem::path> GetFilesInDirectory(
            const std::filesystem::path& directory,
            const std::string& extension = ""
        );

        static std::filesystem::file_time_type GetLastWriteTime(const std::filesystem::path& path);
    };
}
