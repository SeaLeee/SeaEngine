#pragma once

#include "Core/Types.h"
#include <chrono>

namespace Sea
{
    class Timer
    {
    public:
        Timer();

        void Reset();
        void Tick();
        void Start();
        void Stop();

        f32 GetDeltaTime() const { return static_cast<f32>(m_DeltaTime); }
        f64 GetTotalTime() const { return m_TotalTime; }
        f64 GetElapsedSeconds() const;
        u64 GetFrameCount() const { return m_FrameCount; }
        f32 GetFPS() const { return m_FPS; }

    private:
        using Clock = std::chrono::high_resolution_clock;
        using TimePoint = std::chrono::time_point<Clock>;

        TimePoint m_BaseTime;
        TimePoint m_PrevTime;
        TimePoint m_CurrentTime;
        TimePoint m_StopTime;

        f64 m_DeltaTime = 0.0;
        f64 m_TotalTime = 0.0;
        f64 m_PausedTime = 0.0;

        u64 m_FrameCount = 0;
        f32 m_FPS = 0.0f;
        f32 m_FPSAccumulator = 0.0f;
        u32 m_FPSFrameCount = 0;

        bool m_Stopped = false;
    };
}
