// Sky Pixel Shader
// Procedural atmosphere with Rayleigh/Mie scattering

// ========== Inline functions from SkyCommon.hlsli ==========
static const float PI = 3.14159265359f;
static const float MIE_G = 0.76f;

// Rayleigh 相位函数
float RayleighPhase(float cosTheta)
{
    return 3.0f / (16.0f * PI) * (1.0f + cosTheta * cosTheta);
}

// Mie 相位函数 (Henyey-Greenstein)
float MiePhase(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = 1.0f + g2 - 2.0f * g * cosTheta;
    return 3.0f / (8.0f * PI) * ((1.0f - g2) / (2.0f + g2)) * (1.0f + cosTheta * cosTheta) / pow(denom, 1.5f);
}
// ============================================================

cbuffer SkyConstants : register(b0)
{
    row_major float4x4 InvViewProj;
    float3 CameraPosition;
    float Time;
    float3 SunDirection;
    float SunIntensity;
    float3 SunColor;
    float AtmosphereScale;
    float3 GroundColor;
    float CloudCoverage;
    float CloudDensity;
    float CloudHeight;
    float2 Padding;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float3 ViewDir : TEXCOORD1;
};

// 简化的大气散射
float3 ComputeAtmosphericScattering(float3 viewDir, float3 sunDir)
{
    // 基本颜色
    float3 skyColorZenith = float3(0.15, 0.35, 0.65);  // 天顶蓝色
    float3 skyColorHorizon = float3(0.6, 0.75, 0.9);   // 地平线淡蓝色
    float3 sunsetColor = float3(1.0, 0.5, 0.2);        // 日落颜色
    
    // 视线与太阳的夹角
    float cosTheta = dot(viewDir, sunDir);
    float sunViewAngle = saturate((cosTheta + 1.0) * 0.5);
    
    // 高度混合 (0 = 地平线, 1 = 天顶)
    float horizon = saturate(viewDir.y);
    float horizonFade = pow(1.0 - horizon, 3.0);
    
    // 基础天空颜色
    float3 skyColor = lerp(skyColorHorizon, skyColorZenith, saturate(viewDir.y * 2.0));
    
    // 太阳高度影响
    float sunHeight = sunDir.y;
    float sunsetFactor = saturate(1.0 - abs(sunHeight) * 3.0);
    
    // 日落/日出效果
    float sunAngle = saturate((cosTheta * 0.5 + 0.5));
    float3 sunsetGlow = sunsetColor * sunsetFactor * pow(sunAngle, 4.0) * horizonFade;
    
    // Rayleigh 散射 (天空蓝色)
    float rayleigh = RayleighPhase(cosTheta);
    float3 rayleighColor = skyColor * (1.0 + rayleigh * 0.3);
    
    // Mie 散射 (太阳周围光晕)
    float mie = MiePhase(cosTheta, MIE_G);
    float3 mieColor = SunColor * mie * 0.3;
    
    // 组合
    float3 color = rayleighColor + mieColor + sunsetGlow;
    
    // 太阳圆盘
    float sunDisk = smoothstep(0.9995, 0.9999, cosTheta);
    color += SunColor * sunDisk * SunIntensity;
    
    // 地面颜色 (在地平线以下)
    if (viewDir.y < 0.0)
    {
        float groundBlend = saturate(-viewDir.y * 10.0);
        color = lerp(color, GroundColor * 0.5, groundBlend);
    }
    
    return color * AtmosphereScale;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 viewDir = normalize(input.ViewDir);
    float3 sunDir = normalize(SunDirection);
    
    float3 color = ComputeAtmosphericScattering(viewDir, sunDir);
    
    return float4(color, 1.0);
}
