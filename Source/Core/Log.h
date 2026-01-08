#pragma once

#include "Core/Types.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace Sea
{
    class Log
    {
    public:
        static void Initialize(const std::string& logFile = "SeaEngine.log");
        static void Shutdown();

        static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };
}

// 核心引擎日志宏
#define SEA_CORE_TRACE(...)    ::Sea::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define SEA_CORE_DEBUG(...)    ::Sea::Log::GetCoreLogger()->debug(__VA_ARGS__)
#define SEA_CORE_INFO(...)     ::Sea::Log::GetCoreLogger()->info(__VA_ARGS__)
#define SEA_CORE_WARN(...)     ::Sea::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define SEA_CORE_ERROR(...)    ::Sea::Log::GetCoreLogger()->error(__VA_ARGS__)
#define SEA_CORE_CRITICAL(...) ::Sea::Log::GetCoreLogger()->critical(__VA_ARGS__)

// 客户端日志宏
#define SEA_TRACE(...)    ::Sea::Log::GetClientLogger()->trace(__VA_ARGS__)
#define SEA_DEBUG(...)    ::Sea::Log::GetClientLogger()->debug(__VA_ARGS__)
#define SEA_INFO(...)     ::Sea::Log::GetClientLogger()->info(__VA_ARGS__)
#define SEA_WARN(...)     ::Sea::Log::GetClientLogger()->warn(__VA_ARGS__)
#define SEA_ERROR(...)    ::Sea::Log::GetClientLogger()->error(__VA_ARGS__)
#define SEA_CRITICAL(...) ::Sea::Log::GetClientLogger()->critical(__VA_ARGS__)

// 断言宏
#ifdef _DEBUG
#define SEA_ASSERT(condition, ...) \
    do { \
        if (!(condition)) { \
            SEA_CORE_CRITICAL("Assertion Failed: {}", __VA_ARGS__); \
            __debugbreak(); \
        } \
    } while(false)
#else
#define SEA_ASSERT(condition, ...)
#endif
