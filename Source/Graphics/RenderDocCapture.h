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
        static void TriggerCapture(); // Start capturing current frame
        static void EndCaptureAndOpen(); // End capture and open RenderDoc UI
        static uint32_t GetNumCaptures(); // Get number of captures made
        static void LaunchReplayUI(); // Open RenderDoc UI with latest capture
        static bool IsFrameCapturing(); // Check if currently capturing

    private:
        static void* s_RenderDocAPI;
        static bool s_Available;
    };
}
