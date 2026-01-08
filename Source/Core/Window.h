#pragma once

#include "Core/Types.h"
#include <Windows.h>
#include <functional>

namespace Sea
{
    struct WindowDesc
    {
        std::string title = "SeaEngine";
        u32 width = 1920;
        u32 height = 1080;
        bool resizable = true;
        bool fullscreen = false;
    };

    class Window : public NonCopyable
    {
    public:
        using ResizeCallback = std::function<void(u32, u32)>;
        using CloseCallback = std::function<void()>;

        Window(const WindowDesc& desc);
        ~Window();

        bool Initialize();
        void Shutdown();
        void ProcessMessages();

        bool ShouldClose() const { return m_ShouldClose; }
        void SetShouldClose(bool close) { m_ShouldClose = close; }

        u32 GetWidth() const { return m_Width; }
        u32 GetHeight() const { return m_Height; }
        HWND GetHandle() const { return m_Handle; }
        f32 GetAspectRatio() const { return static_cast<f32>(m_Width) / static_cast<f32>(m_Height); }

        void SetResizeCallback(ResizeCallback callback) { m_ResizeCallback = std::move(callback); }
        void SetCloseCallback(CloseCallback callback) { m_CloseCallback = std::move(callback); }

        void SetTitle(const std::string& title);
        void Resize(u32 width, u32 height);

    private:
        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    private:
        HWND m_Handle = nullptr;
        HINSTANCE m_Instance = nullptr;
        std::string m_Title;
        u32 m_Width = 0;
        u32 m_Height = 0;
        bool m_Resizable = true;
        bool m_Fullscreen = false;
        bool m_ShouldClose = false;
        bool m_Minimized = false;

        ResizeCallback m_ResizeCallback;
        CloseCallback m_CloseCallback;
    };
}
