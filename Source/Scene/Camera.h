#pragma once

#include "Core/Types.h"
#include <DirectXMath.h>

namespace Sea
{
    using namespace DirectX;

    class Camera
    {
    public:
        Camera();
        ~Camera() = default;

        void SetPerspective(f32 fovY, f32 aspectRatio, f32 nearZ, f32 farZ);
        void SetOrthographic(f32 width, f32 height, f32 nearZ, f32 farZ);

        void SetPosition(const XMFLOAT3& position);
        void SetRotation(f32 pitch, f32 yaw, f32 roll);
        void LookAt(const XMFLOAT3& target, const XMFLOAT3& up = { 0, 1, 0 });

        void Update();

        // 相机控制
        void ProcessKeyboard(f32 forward, f32 right, f32 up, f32 deltaTime);
        void ProcessMouseMovement(f32 xOffset, f32 yOffset);
        void ProcessMouseScroll(f32 yOffset);

        // Getters
        const XMFLOAT3& GetPosition() const { return m_Position; }
        const XMFLOAT3& GetForward() const { return m_Forward; }
        const XMFLOAT3& GetRight() const { return m_Right; }
        const XMFLOAT3& GetUp() const { return m_Up; }

        const XMFLOAT4X4& GetViewMatrix() const { return m_ViewMatrix; }
        const XMFLOAT4X4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
        XMFLOAT4X4 GetViewProjectionMatrix() const;

        f32 GetFOV() const { return m_FOV; }
        f32 GetNearZ() const { return m_NearZ; }
        f32 GetFarZ() const { return m_FarZ; }

        f32 GetMoveSpeed() const { return m_MoveSpeed; }
        void SetMoveSpeed(f32 speed) { m_MoveSpeed = speed; }

    private:
        void UpdateVectors();

    private:
        XMFLOAT3 m_Position = { 0, 0, -5 };
        XMFLOAT3 m_Forward = { 0, 0, 1 };
        XMFLOAT3 m_Right = { 1, 0, 0 };
        XMFLOAT3 m_Up = { 0, 1, 0 };

        f32 m_Pitch = 0.0f;  // 俯仰角
        f32 m_Yaw = 0.0f;    // 偏航角
        f32 m_Roll = 0.0f;   // 翻滚角

        f32 m_FOV = 45.0f;
        f32 m_AspectRatio = 16.0f / 9.0f;
        f32 m_NearZ = 0.1f;
        f32 m_FarZ = 1000.0f;

        f32 m_MoveSpeed = 5.0f;
        f32 m_MouseSensitivity = 0.1f;

        XMFLOAT4X4 m_ViewMatrix;
        XMFLOAT4X4 m_ProjectionMatrix;

        bool m_IsPerspective = true;
    };
}
