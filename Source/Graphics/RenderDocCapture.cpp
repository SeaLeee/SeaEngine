#include "Graphics/RenderDocCapture.h"
#include "Core/Log.h"
#include <Windows.h>

// RenderDoc API header (inline definition to avoid dependency)
typedef void* RENDERDOC_API_1_0_0;
typedef int (*pRENDERDOC_GetAPI)(int version, void** outAPIPointers);

namespace Sea
{
    void* RenderDocCapture::s_RenderDocAPI = nullptr;
    bool RenderDocCapture::s_Available = false;

    bool RenderDocCapture::Initialize()
    {
#ifdef SEA_ENABLE_RENDERDOC
        // Try to load RenderDoc if it's injected
        if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
        {
            pRENDERDOC_GetAPI getAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
            if (getAPI && getAPI(10000, &s_RenderDocAPI) == 1)
            {
                s_Available = true;
                SEA_CORE_INFO("RenderDoc integration enabled");
                return true;
            }
        }
        SEA_CORE_INFO("RenderDoc not detected - frame capture disabled");
#endif
        return false;
    }

    void RenderDocCapture::Shutdown() { s_RenderDocAPI = nullptr; s_Available = false; }
    void RenderDocCapture::StartCapture() { /* API call would go here */ }
    void RenderDocCapture::EndCapture() { /* API call would go here */ }
    bool RenderDocCapture::IsAvailable() { return s_Available; }
    void RenderDocCapture::TriggerCapture() { /* Trigger single frame capture */ }
}
