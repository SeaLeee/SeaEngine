#pragma once

#include "Core/Types.h"
#include "Core/Window.h"

namespace Sea
{
    struct ApplicationConfig
    {
        WindowDesc window;
        bool enableValidation = true;
        bool enableRenderDoc = true;
    };

    class Application : public NonCopyable
    {
    public:
        Application(const ApplicationConfig& config);
        virtual ~Application();

        void Run();
        void Quit() { m_Running = false; }

        Window& GetWindow() { return *m_Window; }
        const Window& GetWindow() const { return *m_Window; }

        static Application& Get() { return *s_Instance; }

    protected:
        virtual bool OnInitialize() { return true; }
        virtual void OnShutdown() {}
        virtual void OnUpdate(f32 deltaTime) {}
        virtual void OnRender() {}
        virtual void OnImGui() {}
        virtual void OnResize(u32 width, u32 height) {}

    private:
        bool Initialize();
        void Shutdown();
        void MainLoop();

    protected:
        ApplicationConfig m_Config;
        Scope<Window> m_Window;
        bool m_Running = false;
        bool m_Paused = false;

    private:
        static Application* s_Instance;
    };
}
