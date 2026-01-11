#pragma once
#include "Core/Types.h"
#include "Graphics/Graphics.h"
#include "Scene/Camera.h"
#include <DirectXMath.h>

namespace Sea
{
    using namespace DirectX;

    // 天空渲染参数
    struct SkySettings
    {
        // 太阳参数
        XMFLOAT3 SunDirection = { 0.5f, 0.5f, 0.5f };
        float SunIntensity = 10.0f;
        XMFLOAT3 SunColor = { 1.0f, 0.95f, 0.85f };
        float AtmosphereScale = 1.0f;
        
        // 地面颜色
        XMFLOAT3 GroundColor = { 0.3f, 0.25f, 0.2f };
        
        // 云参数
        float CloudCoverage = 0.5f;     // 0-1
        float CloudDensity = 1.0f;      // 密度乘数
        float CloudHeight = 2000.0f;    // 云层高度 (m)
        
        // 开关
        bool EnableSky = true;
        bool EnableClouds = true;
        bool EnableAtmosphere = true;
    };

    // 天空常量缓冲区结构
    struct SkyConstants
    {
        XMFLOAT4X4 InvViewProj;
        XMFLOAT3 CameraPosition;
        float Time;
        XMFLOAT3 SunDirection;
        float SunIntensity;
        XMFLOAT3 SunColor;
        float AtmosphereScale;
        XMFLOAT3 GroundColor;
        float CloudCoverage;
        float CloudDensity;
        float CloudHeight;
        float Padding[2];
    };

    class SkyRenderer : public NonCopyable
    {
    public:
        SkyRenderer(Device& device);
        ~SkyRenderer();

        bool Initialize();
        void Shutdown();

        void Update(f32 deltaTime);
        void Render(CommandList& cmdList, Camera& camera);

        // 获取/设置参数
        SkySettings& GetSettings() { return m_Settings; }
        const SkySettings& GetSettings() const { return m_Settings; }
        void SetSettings(const SkySettings& settings) { m_Settings = settings; }

        // 太阳方向控制
        void SetSunAzimuth(float degrees);
        void SetSunElevation(float degrees);
        float GetSunAzimuth() const { return m_SunAzimuth; }
        float GetSunElevation() const { return m_SunElevation; }
        
        // 时间控制
        void SetTimeOfDay(float hours); // 0-24
        float GetTimeOfDay() const { return m_TimeOfDay; }
        void SetAutoTimeProgress(bool enable) { m_AutoTimeProgress = enable; }
        bool GetAutoTimeProgress() const { return m_AutoTimeProgress; }

    private:
        bool CreatePipelines();
        bool CreateConstantBuffer();
        void UpdateSunDirection();

        Device& m_Device;
        SkySettings m_Settings;
        
        // 渲染资源
        Ref<PipelineState> m_SkyPSO;
        Ref<PipelineState> m_CloudsPSO;
        Scope<RootSignature> m_RootSignature;
        Scope<Buffer> m_ConstantBuffer;

        // 太阳控制
        float m_SunAzimuth = 45.0f;   // 水平角度 (0-360)
        float m_SunElevation = 45.0f; // 仰角 (-90 to 90)
        
        // 时间系统
        float m_TimeOfDay = 12.0f;    // 小时 (0-24)
        bool m_AutoTimeProgress = false;
        float m_TotalTime = 0.0f;
    };
}
