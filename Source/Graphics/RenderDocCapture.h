#pragma once
#include "Core/Types.h"

namespace Sea
{
    // RenderDoc API integration for frame capture debugging
    class RenderDocCapture
    {
    public:
        static bool Initialize();
        static void Shutdown();
        static void StartCapture();
        static void EndCapture();
        static bool IsAvailable();
        static void TriggerCapture(); // Capture current frame

    private:
        static void* s_RenderDocAPI;
        static bool s_Available;
    };
}
