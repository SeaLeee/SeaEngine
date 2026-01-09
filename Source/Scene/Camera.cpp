#include "Scene/Camera.h"
#include <cmath>

namespace Sea
{
    Camera::Camera()
    {
        XMStoreFloat4x4(&m_ViewMatrix, XMMatrixIdentity());
        XMStoreFloat4x4(&m_ProjectionMatrix, XMMatrixIdentity());
        UpdateVectors();
    }

    void Camera::SetPerspective(f32 fovY, f32 aspectRatio, f32 nearZ, f32 farZ)
    {
        m_FOV = fovY;
        m_AspectRatio = aspectRatio;
        m_NearZ = nearZ;
        m_FarZ = farZ;
        m_IsPerspective = true;

        XMMATRIX proj = XMMatrixPerspectiveFovLH(
            XMConvertToRadians(fovY),
            aspectRatio,
            nearZ,
            farZ
        );
        XMStoreFloat4x4(&m_ProjectionMatrix, proj);
    }

    void Camera::SetOrthographic(f32 width, f32 height, f32 nearZ, f32 farZ)
    {
        m_NearZ = nearZ;
        m_FarZ = farZ;
        m_IsPerspective = false;

        XMMATRIX proj = XMMatrixOrthographicLH(width, height, nearZ, farZ);
        XMStoreFloat4x4(&m_ProjectionMatrix, proj);
    }

    void Camera::SetPosition(const XMFLOAT3& position)
    {
        m_Position = position;
    }

    void Camera::SetRotation(f32 pitch, f32 yaw, f32 roll)
    {
        m_Pitch = pitch;
        m_Yaw = yaw;
        m_Roll = roll;
        UpdateVectors();
    }

    void Camera::LookAt(const XMFLOAT3& target, const XMFLOAT3& up)
    {
        XMVECTOR pos = XMLoadFloat3(&m_Position);
        XMVECTOR tgt = XMLoadFloat3(&target);
        XMVECTOR upVec = XMLoadFloat3(&up);

        XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(tgt, pos));
        XMStoreFloat3(&m_Forward, forward);

        XMVECTOR right = XMVector3Normalize(XMVector3Cross(upVec, forward));
        XMStoreFloat3(&m_Right, right);

        XMVECTOR actualUp = XMVector3Cross(forward, right);
        XMStoreFloat3(&m_Up, actualUp);

        // 从方向向量计算欧拉角
        m_Pitch = XMConvertToDegrees(std::asin(-m_Forward.y));
        m_Yaw = XMConvertToDegrees(std::atan2(m_Forward.x, m_Forward.z));
    }

    void Camera::Update()
    {
        UpdateVectors();

        XMVECTOR pos = XMLoadFloat3(&m_Position);
        XMVECTOR forward = XMLoadFloat3(&m_Forward);
        XMVECTOR up = XMLoadFloat3(&m_Up);

        XMMATRIX view = XMMatrixLookAtLH(pos, XMVectorAdd(pos, forward), up);
        XMStoreFloat4x4(&m_ViewMatrix, view);
    }

    void Camera::ProcessKeyboard(f32 forward, f32 right, f32 up, f32 deltaTime)
    {
        f32 velocity = m_MoveSpeed * deltaTime;

        XMVECTOR pos = XMLoadFloat3(&m_Position);
        XMVECTOR fwd = XMLoadFloat3(&m_Forward);
        XMVECTOR rgt = XMLoadFloat3(&m_Right);
        XMVECTOR upVec = XMVectorSet(0, 1, 0, 0);

        pos = XMVectorAdd(pos, XMVectorScale(fwd, forward * velocity));
        pos = XMVectorAdd(pos, XMVectorScale(rgt, right * velocity));
        pos = XMVectorAdd(pos, XMVectorScale(upVec, up * velocity));

        XMStoreFloat3(&m_Position, pos);
    }

    void Camera::ProcessMouseMovement(f32 xOffset, f32 yOffset)
    {
        m_Yaw += xOffset * m_MouseSensitivity;
        m_Pitch += yOffset * m_MouseSensitivity;

        // 限制俯仰角
        if (m_Pitch > 89.0f) m_Pitch = 89.0f;
        if (m_Pitch < -89.0f) m_Pitch = -89.0f;

        UpdateVectors();
    }

    void Camera::ProcessMouseScroll(f32 yOffset)
    {
        m_FOV -= yOffset;
        if (m_FOV < 1.0f) m_FOV = 1.0f;
        if (m_FOV > 120.0f) m_FOV = 120.0f;

        if (m_IsPerspective)
        {
            SetPerspective(m_FOV, m_AspectRatio, m_NearZ, m_FarZ);
        }
    }

    void Camera::UpdateVectors()
    {
        f32 pitchRad = XMConvertToRadians(m_Pitch);
        f32 yawRad = XMConvertToRadians(m_Yaw);

        XMFLOAT3 forward;
        forward.x = std::cos(pitchRad) * std::sin(yawRad);
        forward.y = std::sin(pitchRad);
        forward.z = std::cos(pitchRad) * std::cos(yawRad);

        XMVECTOR fwd = XMVector3Normalize(XMLoadFloat3(&forward));
        XMStoreFloat3(&m_Forward, fwd);

        XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
        XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, fwd));
        XMStoreFloat3(&m_Right, right);

        XMVECTOR up = XMVector3Cross(fwd, right);
        XMStoreFloat3(&m_Up, up);
    }

    XMFLOAT4X4 Camera::GetViewProjectionMatrix() const
    {
        XMMATRIX view = XMLoadFloat4x4(&m_ViewMatrix);
        XMMATRIX proj = XMLoadFloat4x4(&m_ProjectionMatrix);
        XMFLOAT4X4 result;
        XMStoreFloat4x4(&result, XMMatrixMultiply(view, proj));
        return result;
    }
}
