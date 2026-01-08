#pragma once

#include "Core/Types.h"
#include <Windows.h>

namespace Sea
{
    enum class KeyCode : u32
    {
        // 鼠标按键
        MouseLeft = VK_LBUTTON,
        MouseRight = VK_RBUTTON,
        MouseMiddle = VK_MBUTTON,

        // 字母键
        A = 'A', B = 'B', C = 'C', D = 'D', E = 'E', F = 'F', G = 'G',
        H = 'H', I = 'I', J = 'J', K = 'K', L = 'L', M = 'M', N = 'N',
        O = 'O', P = 'P', Q = 'Q', R = 'R', S = 'S', T = 'T', U = 'U',
        V = 'V', W = 'W', X = 'X', Y = 'Y', Z = 'Z',

        // 数字键
        Num0 = '0', Num1 = '1', Num2 = '2', Num3 = '3', Num4 = '4',
        Num5 = '5', Num6 = '6', Num7 = '7', Num8 = '8', Num9 = '9',

        // 功能键
        F1 = VK_F1, F2 = VK_F2, F3 = VK_F3, F4 = VK_F4,
        F5 = VK_F5, F6 = VK_F6, F7 = VK_F7, F8 = VK_F8,
        F9 = VK_F9, F10 = VK_F10, F11 = VK_F11, F12 = VK_F12,

        // 控制键
        Escape = VK_ESCAPE,
        Tab = VK_TAB,
        CapsLock = VK_CAPITAL,
        Shift = VK_SHIFT,
        Control = VK_CONTROL,
        Alt = VK_MENU,
        Space = VK_SPACE,
        Enter = VK_RETURN,
        Backspace = VK_BACK,
        Delete = VK_DELETE,
        Insert = VK_INSERT,
        Home = VK_HOME,
        End = VK_END,
        PageUp = VK_PRIOR,
        PageDown = VK_NEXT,

        // 方向键
        Left = VK_LEFT,
        Right = VK_RIGHT,
        Up = VK_UP,
        Down = VK_DOWN,
    };

    class Input
    {
    public:
        static void Initialize(HWND hwnd);
        static void Update();

        // 键盘
        static bool IsKeyDown(KeyCode key);
        static bool IsKeyPressed(KeyCode key);
        static bool IsKeyReleased(KeyCode key);

        // 鼠标
        static bool IsMouseButtonDown(KeyCode button);
        static bool IsMouseButtonPressed(KeyCode button);
        static bool IsMouseButtonReleased(KeyCode button);

        static std::pair<i32, i32> GetMousePosition();
        static std::pair<f32, f32> GetMouseDelta();
        static f32 GetMouseWheelDelta();

        static void SetMouseWheelDelta(f32 delta);

    private:
        static HWND s_Hwnd;
        static bool s_CurrentKeyState[256];
        static bool s_PreviousKeyState[256];
        static i32 s_MouseX, s_MouseY;
        static i32 s_PrevMouseX, s_PrevMouseY;
        static f32 s_MouseWheelDelta;
    };
}
