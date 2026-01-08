#include "Core/Input.h"

namespace Sea
{
    HWND Input::s_Hwnd = nullptr;
    bool Input::s_CurrentKeyState[256] = {};
    bool Input::s_PreviousKeyState[256] = {};
    i32 Input::s_MouseX = 0;
    i32 Input::s_MouseY = 0;
    i32 Input::s_PrevMouseX = 0;
    i32 Input::s_PrevMouseY = 0;
    f32 Input::s_MouseWheelDelta = 0.0f;

    void Input::Initialize(HWND hwnd)
    {
        s_Hwnd = hwnd;
        memset(s_CurrentKeyState, 0, sizeof(s_CurrentKeyState));
        memset(s_PreviousKeyState, 0, sizeof(s_PreviousKeyState));
    }

    void Input::Update()
    {
        // 保存上一帧状态
        memcpy(s_PreviousKeyState, s_CurrentKeyState, sizeof(s_CurrentKeyState));

        // 更新当前键盘状态
        for (int i = 0; i < 256; ++i)
        {
            s_CurrentKeyState[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
        }

        // 更新鼠标位置
        s_PrevMouseX = s_MouseX;
        s_PrevMouseY = s_MouseY;

        POINT point;
        if (GetCursorPos(&point))
        {
            if (s_Hwnd && ScreenToClient(s_Hwnd, &point))
            {
                s_MouseX = point.x;
                s_MouseY = point.y;
            }
        }

        // 重置滚轮增量
        s_MouseWheelDelta = 0.0f;
    }

    bool Input::IsKeyDown(KeyCode key)
    {
        return s_CurrentKeyState[static_cast<u32>(key)];
    }

    bool Input::IsKeyPressed(KeyCode key)
    {
        u32 k = static_cast<u32>(key);
        return s_CurrentKeyState[k] && !s_PreviousKeyState[k];
    }

    bool Input::IsKeyReleased(KeyCode key)
    {
        u32 k = static_cast<u32>(key);
        return !s_CurrentKeyState[k] && s_PreviousKeyState[k];
    }

    bool Input::IsMouseButtonDown(KeyCode button)
    {
        return IsKeyDown(button);
    }

    bool Input::IsMouseButtonPressed(KeyCode button)
    {
        return IsKeyPressed(button);
    }

    bool Input::IsMouseButtonReleased(KeyCode button)
    {
        return IsKeyReleased(button);
    }

    std::pair<i32, i32> Input::GetMousePosition()
    {
        return { s_MouseX, s_MouseY };
    }

    std::pair<f32, f32> Input::GetMouseDelta()
    {
        return { 
            static_cast<f32>(s_MouseX - s_PrevMouseX), 
            static_cast<f32>(s_MouseY - s_PrevMouseY) 
        };
    }

    f32 Input::GetMouseWheelDelta()
    {
        return s_MouseWheelDelta;
    }

    void Input::SetMouseWheelDelta(f32 delta)
    {
        s_MouseWheelDelta = delta;
    }
}
