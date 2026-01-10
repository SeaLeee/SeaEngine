#include "Graphics/RenderDocCapture.h"
#include "Core/Log.h"
#include <Windows.h>
#include <string>
#include <vector>

// RenderDoc API 定义
// 参考: https://renderdoc.org/docs/in_application_api.html
struct RENDERDOC_API_1_6_0
{
    void (*GetAPIVersion)(int* major, int* minor, int* patch);
    void (*SetCaptureOptionU32)(int option, unsigned int value);
    void (*SetCaptureOptionF32)(int option, float value);
    unsigned int (*GetCaptureOptionU32)(int option);
    float (*GetCaptureOptionF32)(int option);
    void (*SetFocusToggleKeys)(void* keys, int num);
    void (*SetCaptureKeys)(void* keys, int num);
    unsigned int (*GetOverlayBits)();
    void (*MaskOverlayBits)(unsigned int And, unsigned int Or);
    void (*RemoveHooks)();
    void (*UnloadCrashHandler)();
    void (*SetCaptureFilePathTemplate)(const char* pathtemplate);
    const char* (*GetCaptureFilePathTemplate)();
    unsigned int (*GetNumCaptures)();
    unsigned int (*GetCapture)(unsigned int idx, char* filename, unsigned int* pathlength, unsigned long long* timestamp);
    void (*TriggerCapture)();
    unsigned int (*IsTargetControlConnected)();
    unsigned int (*LaunchReplayUI)(unsigned int connectTargetControl, const char* cmdline);
    void (*SetActiveWindow)(void* device, void* wndHandle);
    void (*StartFrameCapture)(void* device, void* wndHandle);
    unsigned int (*IsFrameCapturing)();
    void (*EndFrameCapture)(void* device, void* wndHandle);
    unsigned int (*TriggerMultiFrameCapture)(unsigned int numFrames);
    void (*SetCaptureFileComments)(const char* filePath, const char* comments);
    unsigned int (*DiscardFrameCapture)(void* device, void* wndHandle);
    void (*ShowReplayUI)();
    void (*SetCaptureTitle)(const char* title);
};

typedef int (*pRENDERDOC_GetAPI)(int version, void** outAPIPointers);

#define RENDERDOC_API_VERSION_1_6_0 10600

namespace Sea
{
    void* RenderDocCapture::s_RenderDocAPI = nullptr;
    bool RenderDocCapture::s_Available = false;
    static HMODULE s_RenderDocModule = nullptr;

    // 尝试从常见路径加载 RenderDoc DLL
    static HMODULE TryLoadRenderDoc()
    {
        // 先检查是否已经被注入（从 RenderDoc 启动的情况）
        HMODULE mod = GetModuleHandleA("renderdoc.dll");
        if (mod)
        {
            SEA_CORE_INFO("RenderDoc already injected");
            return mod;
        }

        // 常见的 RenderDoc 安装路径
        std::vector<std::string> searchPaths = {
            "C:\\Program Files\\RenderDoc\\renderdoc.dll",
            "C:\\Program Files (x86)\\RenderDoc\\renderdoc.dll",
            "D:\\Program Files\\RenderDoc\\renderdoc.dll",
            "D:\\RenderDoc\\renderdoc.dll",
        };

        // 尝试从环境变量获取路径
        char envPath[MAX_PATH] = {};
        if (GetEnvironmentVariableA("RENDERDOC_PATH", envPath, MAX_PATH) > 0)
        {
            std::string path = std::string(envPath) + "\\renderdoc.dll";
            searchPaths.insert(searchPaths.begin(), path);
        }

        // 尝试从注册表获取安装路径
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Classes\\RenderDoc.RDCCapture.1\\DefaultIcon", 
                          0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            char regPath[MAX_PATH] = {};
            DWORD size = MAX_PATH;
            if (RegQueryValueExA(hKey, nullptr, nullptr, nullptr, (LPBYTE)regPath, &size) == ERROR_SUCCESS)
            {
                // 路径格式类似 "C:\Program Files\RenderDoc\qrenderdoc.exe,0"
                std::string path(regPath);
                size_t pos = path.rfind('\\');
                if (pos != std::string::npos)
                {
                    path = path.substr(0, pos) + "\\renderdoc.dll";
                    searchPaths.insert(searchPaths.begin(), path);
                }
            }
            RegCloseKey(hKey);
        }

        // 尝试加载
        for (const auto& path : searchPaths)
        {
            SEA_CORE_INFO("Trying to load RenderDoc from: {}", path);
            mod = LoadLibraryA(path.c_str());
            if (mod)
            {
                SEA_CORE_INFO("Successfully loaded RenderDoc from: {}", path);
                return mod;
            }
        }

        return nullptr;
    }

    bool RenderDocCapture::Initialize()
    {
        SEA_CORE_INFO("RenderDocCapture::Initialize() called");
        
#ifdef SEA_ENABLE_RENDERDOC
        SEA_CORE_INFO("SEA_ENABLE_RENDERDOC is defined");
        
        // 尝试加载 RenderDoc DLL
        HMODULE mod = TryLoadRenderDoc();
        s_RenderDocModule = mod;
        
        SEA_CORE_INFO("RenderDoc module handle = {}", (void*)mod);
        
        if (mod)
        {
            pRENDERDOC_GetAPI getAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
            SEA_CORE_INFO("GetProcAddress for RENDERDOC_GetAPI = {}", (void*)getAPI);
            
            if (getAPI)
            {
                int ret = getAPI(RENDERDOC_API_VERSION_1_6_0, &s_RenderDocAPI);
                SEA_CORE_INFO("getAPI returned {}, s_RenderDocAPI = {}", ret, s_RenderDocAPI);
                
                if (ret == 1 && s_RenderDocAPI)
                {
                    s_Available = true;
                    
                    // 获取版本信息
                    RENDERDOC_API_1_6_0* api = (RENDERDOC_API_1_6_0*)s_RenderDocAPI;
                    int major, minor, patch;
                    api->GetAPIVersion(&major, &minor, &patch);
                    
                    SEA_CORE_INFO("RenderDoc {}.{}.{} integration enabled - Press F12 to capture!", major, minor, patch);
                    
                    // 设置捕获选项
                    api->SetCaptureOptionU32(0, 1);  // eRENDERDOC_Option_AllowVSync = 0
                    api->SetCaptureOptionU32(1, 1);  // eRENDERDOC_Option_AllowFullscreen = 1
                    api->SetCaptureOptionU32(6, 1);  // eRENDERDOC_Option_CaptureAllCmdLists = 6
                    api->SetCaptureOptionU32(10, 1); // eRENDERDOC_Option_RefAllResources = 10
                    
                    return true;
                }
                else
                {
                    SEA_CORE_WARN("RenderDoc API call failed: ret={}", ret);
                }
            }
            else
            {
                SEA_CORE_WARN("Failed to get RENDERDOC_GetAPI function");
            }
        }
        else
        {
            SEA_CORE_WARN("===========================================");
            SEA_CORE_WARN("RenderDoc DLL not found!");
            SEA_CORE_WARN("Please install RenderDoc or set RENDERDOC_PATH env variable");
            SEA_CORE_WARN("Download from: https://renderdoc.org/");
            SEA_CORE_WARN("===========================================");
        }
#else
        SEA_CORE_WARN("SEA_ENABLE_RENDERDOC is NOT defined - RenderDoc support disabled at compile time");
#endif
        return false;
    }

    void RenderDocCapture::Shutdown()
    {
        SEA_CORE_INFO("RenderDocCapture::Shutdown()");
        s_RenderDocAPI = nullptr;
        s_Available = false;
        // 注意：不要 FreeLibrary，因为 RenderDoc 可能还在追踪资源
        // if (s_RenderDocModule) FreeLibrary(s_RenderDocModule);
        s_RenderDocModule = nullptr;
    }

    void RenderDocCapture::StartCapture()
    {
        SEA_CORE_INFO("RenderDocCapture::StartCapture() - Available={}, API={}", s_Available, s_RenderDocAPI);
#ifdef SEA_ENABLE_RENDERDOC
        if (s_Available && s_RenderDocAPI)
        {
            RENDERDOC_API_1_6_0* api = (RENDERDOC_API_1_6_0*)s_RenderDocAPI;
            api->StartFrameCapture(nullptr, nullptr);
            SEA_CORE_INFO("RenderDoc: Frame capture started");
        }
        else
        {
            SEA_CORE_WARN("RenderDoc: Cannot start capture - not available");
        }
#else
        SEA_CORE_WARN("RenderDoc: StartCapture called but SEA_ENABLE_RENDERDOC not defined");
#endif
    }

    void RenderDocCapture::EndCapture()
    {
        SEA_CORE_INFO("RenderDocCapture::EndCapture() - Available={}, API={}", s_Available, s_RenderDocAPI);
#ifdef SEA_ENABLE_RENDERDOC
        if (s_Available && s_RenderDocAPI)
        {
            RENDERDOC_API_1_6_0* api = (RENDERDOC_API_1_6_0*)s_RenderDocAPI;
            api->EndFrameCapture(nullptr, nullptr);
            SEA_CORE_INFO("RenderDoc: Frame capture ended");
        }
        else
        {
            SEA_CORE_WARN("RenderDoc: Cannot end capture - not available");
        }
#else
        SEA_CORE_WARN("RenderDoc: EndCapture called but SEA_ENABLE_RENDERDOC not defined");
#endif
    }

    bool RenderDocCapture::IsAvailable()
    {
        return s_Available;
    }

    void RenderDocCapture::TriggerCapture()
    {
        SEA_CORE_INFO("RenderDocCapture::TriggerCapture() - Available={}, API={}", s_Available, s_RenderDocAPI);
#ifdef SEA_ENABLE_RENDERDOC
        if (s_Available && s_RenderDocAPI)
        {
            RENDERDOC_API_1_6_0* api = (RENDERDOC_API_1_6_0*)s_RenderDocAPI;
            
            // 开始截帧
            api->StartFrameCapture(nullptr, nullptr);
            SEA_CORE_INFO("RenderDoc: Frame capture started...");
        }
        else
        {
            SEA_CORE_WARN("RenderDoc: Cannot trigger capture - not available (Available={}, API={})", 
                         s_Available, s_RenderDocAPI);
        }
#else
        SEA_CORE_WARN("RenderDoc: TriggerCapture called but SEA_ENABLE_RENDERDOC not defined");
#endif
    }

    void RenderDocCapture::EndCaptureAndOpen()
    {
#ifdef SEA_ENABLE_RENDERDOC
        if (s_Available && s_RenderDocAPI)
        {
            RENDERDOC_API_1_6_0* api = (RENDERDOC_API_1_6_0*)s_RenderDocAPI;
            
            // 结束截帧
            api->EndFrameCapture(nullptr, nullptr);
            
            uint32_t numCaptures = api->GetNumCaptures();
            SEA_CORE_INFO("RenderDoc: Frame captured! Total captures: {}", numCaptures);
            
            // 获取最新截帧路径并打开
            if (numCaptures > 0)
            {
                char path[512] = {};
                uint32_t pathLen = 512;
                uint64_t timestamp = 0;
                api->GetCapture(numCaptures - 1, path, &pathLen, &timestamp);
                
                SEA_CORE_INFO("RenderDoc: Opening capture: {}", path);
                api->LaunchReplayUI(1, path);
            }
        }
#endif
    }

    uint32_t RenderDocCapture::GetNumCaptures()
    {
#ifdef SEA_ENABLE_RENDERDOC
        if (s_Available && s_RenderDocAPI)
        {
            RENDERDOC_API_1_6_0* api = (RENDERDOC_API_1_6_0*)s_RenderDocAPI;
            return api->GetNumCaptures();
        }
#endif
        return 0;
    }

    void RenderDocCapture::LaunchReplayUI()
    {
#ifdef SEA_ENABLE_RENDERDOC
        if (s_Available && s_RenderDocAPI)
        {
            RENDERDOC_API_1_6_0* api = (RENDERDOC_API_1_6_0*)s_RenderDocAPI;
            uint32_t numCaptures = api->GetNumCaptures();
            if (numCaptures > 0)
            {
                // 获取最新的截帧文件路径
                char path[512] = {};
                uint32_t pathLen = 512;
                uint64_t timestamp = 0;
                api->GetCapture(numCaptures - 1, path, &pathLen, &timestamp);
                
                SEA_CORE_INFO("RenderDoc: Opening capture: {}", path);
                
                // 启动 RenderDoc UI
                api->LaunchReplayUI(1, path);
            }
            else
            {
                SEA_CORE_WARN("RenderDoc: No captures available to view");
                // 仍然启动 UI
                api->LaunchReplayUI(1, nullptr);
            }
        }
        else
        {
            SEA_CORE_WARN("RenderDoc: Cannot launch UI - not available");
        }
#else
        SEA_CORE_WARN("RenderDoc: LaunchReplayUI called but SEA_ENABLE_RENDERDOC not defined");
#endif
    }

    bool RenderDocCapture::IsFrameCapturing()
    {
#ifdef SEA_ENABLE_RENDERDOC
        if (s_Available && s_RenderDocAPI)
        {
            RENDERDOC_API_1_6_0* api = (RENDERDOC_API_1_6_0*)s_RenderDocAPI;
            return api->IsFrameCapturing() != 0;
        }
#endif
        return false;
    }
}
