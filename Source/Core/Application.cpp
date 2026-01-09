#include "Core/Application.h"
#include "Core/Log.h"
#include "Core/Timer.h"

namespace Sea
{
    Application* Application::s_Instance = nullptr;

    Application::Application(const ApplicationConfig& config)
        : m_Config(config)
    {
        SEA_ASSERT(!s_Instance, "Application already exists!");
        s_Instance = this;
    }

    Application::~Application()
    {
        s_Instance = nullptr;
    }

    void Application::Run()
    {
        if (!Initialize())
        {
            SEA_CORE_ERROR("Application initialization failed");
            return;
        }

        MainLoop();
        Shutdown();
    }

    bool Application::Initialize()
    {
        Log::Initialize();
        SEA_CORE_INFO("SeaEngine Initializing...");

        // 创建窗口
        m_Window = MakeScope<Window>(m_Config.window);
        if (!m_Window->Initialize())
        {
            return false;
        }

        m_Window->SetResizeCallback([this](u32 width, u32 height) {
            OnResize(width, height);
        });

        m_Window->SetCloseCallback([this]() {
            m_Running = false;
        });

        // 调用派生类初始化
        if (!OnInitialize())
        {
            SEA_CORE_ERROR("OnInitialize failed");
            return false;
        }

        m_Running = true;
        SEA_CORE_INFO("SeaEngine Initialized Successfully");
        return true;
    }

    void Application::Shutdown()
    {
        SEA_CORE_INFO("SeaEngine Shutting down...");
        OnShutdown();
        m_Window->Shutdown();
        Log::Shutdown();
    }

    void Application::MainLoop()
    {
        SEA_CORE_INFO("Entering main loop, m_Running={}", m_Running);
        Timer timer;
        timer.Reset();

        while (m_Running)
        {
            timer.Tick();
            f32 deltaTime = timer.GetDeltaTime();

            m_Window->ProcessMessages();

            if (m_Window->ShouldClose())
            {
                SEA_CORE_INFO("Window should close");
                m_Running = false;
                break;
            }

            if (!m_Paused)
            {
                OnUpdate(deltaTime);
                OnRender();
            }
        }
        SEA_CORE_INFO("Exiting main loop");
    }
}
