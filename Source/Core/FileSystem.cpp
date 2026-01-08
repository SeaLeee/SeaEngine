#include "Core/FileSystem.h"
#include "Core/Log.h"
#include <fstream>
#include <Windows.h>

namespace Sea
{
    std::string FileSystem::ReadTextFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::in);
        if (!file.is_open())
        {
            SEA_CORE_ERROR("Failed to open file: {}", path.string());
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::vector<u8> FileSystem::ReadBinaryFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            SEA_CORE_ERROR("Failed to open file: {}", path.string());
            return {};
        }

        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<u8> buffer(fileSize);

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

        return buffer;
    }

    bool FileSystem::WriteTextFile(const std::filesystem::path& path, const std::string& content)
    {
        // 确保目录存在
        if (path.has_parent_path())
        {
            CreateDirectories(path.parent_path());
        }

        std::ofstream file(path, std::ios::out);
        if (!file.is_open())
        {
            SEA_CORE_ERROR("Failed to create file: {}", path.string());
            return false;
        }

        file << content;
        return true;
    }

    bool FileSystem::WriteBinaryFile(const std::filesystem::path& path, const std::vector<u8>& data)
    {
        if (path.has_parent_path())
        {
            CreateDirectories(path.parent_path());
        }

        std::ofstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            SEA_CORE_ERROR("Failed to create file: {}", path.string());
            return false;
        }

        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        return true;
    }

    bool FileSystem::Exists(const std::filesystem::path& path)
    {
        return std::filesystem::exists(path);
    }

    bool FileSystem::IsDirectory(const std::filesystem::path& path)
    {
        return std::filesystem::is_directory(path);
    }

    bool FileSystem::CreateDirectories(const std::filesystem::path& path)
    {
        if (std::filesystem::exists(path))
            return true;

        std::error_code ec;
        bool result = std::filesystem::create_directories(path, ec);
        if (ec)
        {
            SEA_CORE_ERROR("Failed to create directory: {} - {}", path.string(), ec.message());
        }
        return result;
    }

    std::filesystem::path FileSystem::GetExecutablePath()
    {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        return std::filesystem::path(path).parent_path();
    }

    std::filesystem::path FileSystem::GetWorkingDirectory()
    {
        return std::filesystem::current_path();
    }

    std::filesystem::path FileSystem::GetShadersDirectory()
    {
        return GetWorkingDirectory() / "Shaders";
    }

    std::filesystem::path FileSystem::GetAssetsDirectory()
    {
        return GetWorkingDirectory() / "Assets";
    }

    std::vector<std::filesystem::path> FileSystem::GetFilesInDirectory(
        const std::filesystem::path& directory,
        const std::string& extension)
    {
        std::vector<std::filesystem::path> files;

        if (!std::filesystem::exists(directory))
            return files;

        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            if (entry.is_regular_file())
            {
                if (extension.empty() || entry.path().extension() == extension)
                {
                    files.push_back(entry.path());
                }
            }
        }

        return files;
    }

    std::filesystem::file_time_type FileSystem::GetLastWriteTime(const std::filesystem::path& path)
    {
        if (!std::filesystem::exists(path))
        {
            return std::filesystem::file_time_type::min();
        }
        return std::filesystem::last_write_time(path);
    }
}
