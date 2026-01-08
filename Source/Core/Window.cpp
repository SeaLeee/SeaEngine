#include "Core/Window.h"
#include "Core/Log.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Sea
{
    Window::Window(const WindowDesc& desc)
        : m_Title(desc.title)
        , m_Width(desc.width)
        , m_Height(desc.height)
        , m_Resizable(desc.resizable)
        , m_Fullscreen(desc.fullscreen)
    {
    }

    Window::~Window()
    {
        Shutdown();
    }

    bool Window::Initialize()
    {
        m_Instance = GetModuleHandle(nullptr);

        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = m_Instance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"SeaEngineWindowClass";

        if (!RegisterClassEx(&wc))
        {
            SEA_CORE_ERROR("Failed to register window class");
            return false;
        }

        DWORD style = WS_OVERLAPPEDWINDOW;
        if (!m_Resizable)
        {
            style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
        }

        RECT windowRect = { 0, 0, static_cast<LONG>(m_Width), static_cast<LONG>(m_Height) };
        AdjustWindowRect(&windowRect, style, FALSE);

        int windowWidth = windowRect.right - windowRect.left;
        int windowHeight = windowRect.bottom - windowRect.top;

        // 居中显示
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        int posX = (screenWidth - windowWidth) / 2;
        int posY = (screenHeight - windowHeight) / 2;

        std::wstring wTitle(m_Title.begin(), m_Title.end());
        m_Handle = CreateWindowEx(
            0,
            L"SeaEngineWindowClass",
            wTitle.c_str(),
            style,
            posX, posY,
            windowWidth, windowHeight,
            nullptr,
            nullptr,
            m_Instance,
            this
        );

        if (!m_Handle)
        {
            SEA_CORE_ERROR("Failed to create window");
            return false;
        }

        ShowWindow(m_Handle, SW_SHOW);
        UpdateWindow(m_Handle);

        SEA_CORE_INFO("Window created: {} ({}x{})", m_Title, m_Width, m_Height);
        return true;
    }

    void Window::Shutdown()
    {
        if (m_Handle)
        {
            DestroyWindow(m_Handle);
            m_Handle = nullptr;
        }
        UnregisterClass(L"SeaEngineWindowClass", m_Instance);
    }

    void Window::ProcessMessages()
    {
        MSG msg = {};
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                m_ShouldClose = true;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    void Window::SetTitle(const std::string& title)
    {
        m_Title = title;
        std::wstring wTitle(title.begin(), title.end());
        SetWindowText(m_Handle, wTitle.c_str());
    }

    void Window::Resize(u32 width, u32 height)
    {
        m_Width = width;
        m_Height = height;
    }

    LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        Window* window = nullptr;

        if (msg == WM_CREATE)
        {
            CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            window = static_cast<Window*>(pCreate->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        }
        else
        {
            window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        // ImGui消息处理
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        {
            return true;
        }

        if (window)
        {
            return window->HandleMessage(msg, wParam, lParam);
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    LRESULT Window::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_CLOSE:
            m_ShouldClose = true;
            if (m_CloseCallback) m_CloseCallback();
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_SIZE:
        {
            UINT width = LOWORD(lParam);
            UINT height = HIWORD(lParam);

            if (wParam == SIZE_MINIMIZED)
            {
                m_Minimized = true;
            }
            else if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)
            {
                m_Minimized = false;
                if (width != m_Width || height != m_Height)
                {
                    m_Width = width;
                    m_Height = height;
                    if (m_ResizeCallback) m_ResizeCallback(width, height);
                }
            }
            return 0;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE)
            {
                m_ShouldClose = true;
            }
            return 0;
        }

        return DefWindowProc(m_Handle, msg, wParam, lParam);
    }
}
