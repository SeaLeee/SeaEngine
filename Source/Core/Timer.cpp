#include "Core/Timer.h"

namespace Sea
{
    Timer::Timer()
    {
        Reset();
    }

    void Timer::Reset()
    {
        m_BaseTime = Clock::now();
        m_PrevTime = m_BaseTime;
        m_StopTime = m_BaseTime;
        m_DeltaTime = 0.0;
        m_TotalTime = 0.0;
        m_PausedTime = 0.0;
        m_FrameCount = 0;
        m_FPS = 0.0f;
        m_FPSAccumulator = 0.0f;
        m_FPSFrameCount = 0;
        m_Stopped = false;
    }

    void Timer::Tick()
    {
        if (m_Stopped)
        {
            m_DeltaTime = 0.0;
            return;
        }

        m_CurrentTime = Clock::now();
        
        std::chrono::duration<f64> delta = m_CurrentTime - m_PrevTime;
        m_DeltaTime = delta.count();

        m_PrevTime = m_CurrentTime;

        // 防止负值
        if (m_DeltaTime < 0.0)
        {
            m_DeltaTime = 0.0;
        }

        std::chrono::duration<f64> total = m_CurrentTime - m_BaseTime;
        m_TotalTime = total.count() - m_PausedTime;

        m_FrameCount++;

        // 计算FPS
        m_FPSAccumulator += static_cast<f32>(m_DeltaTime);
        m_FPSFrameCount++;

        if (m_FPSAccumulator >= 1.0f)
        {
            m_FPS = static_cast<f32>(m_FPSFrameCount) / m_FPSAccumulator;
            m_FPSAccumulator = 0.0f;
            m_FPSFrameCount = 0;
        }
    }

    void Timer::Start()
    {
        if (m_Stopped)
        {
            auto startTime = Clock::now();
            std::chrono::duration<f64> pausedDuration = startTime - m_StopTime;
            m_PausedTime += pausedDuration.count();
            m_PrevTime = startTime;
            m_Stopped = false;
        }
    }

    void Timer::Stop()
    {
        if (!m_Stopped)
        {
            m_StopTime = Clock::now();
            m_Stopped = true;
        }
    }

    f64 Timer::GetElapsedSeconds() const
    {
        if (m_Stopped)
        {
            std::chrono::duration<f64> elapsed = m_StopTime - m_BaseTime;
            return elapsed.count() - m_PausedTime;
        }
        else
        {
            auto now = Clock::now();
            std::chrono::duration<f64> elapsed = now - m_BaseTime;
            return elapsed.count() - m_PausedTime;
        }
    }
}
