// Sky and Atmosphere Common Functions
// Based on Nishita sky model with Rayleigh and Mie scattering

#ifndef SKY_COMMON_HLSLI
#define SKY_COMMON_HLSLI

static const float PI = 3.14159265359f;
static const float EARTH_RADIUS = 6371000.0f;       // 地球半径 (m)
static const float ATMOSPHERE_HEIGHT = 100000.0f;   // 大气层高度 (m)

// 大气散射参数
static const float3 RAYLEIGH_SCATTERING = float3(5.5e-6, 13.0e-6, 22.4e-6); // Rayleigh 散射系数
static const float MIE_SCATTERING = 21e-6;           // Mie 散射系数
static const float RAYLEIGH_SCALE_HEIGHT = 8500.0f;  // Rayleigh 尺度高度
static const float MIE_SCALE_HEIGHT = 1200.0f;       // Mie 尺度高度
static const float MIE_G = 0.76f;                    // Mie 各向异性系数

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

// 计算大气层密度
float2 GetAtmosphereDensity(float height)
{
    float rayleighDensity = exp(-height / RAYLEIGH_SCALE_HEIGHT);
    float mieDensity = exp(-height / MIE_SCALE_HEIGHT);
    return float2(rayleighDensity, mieDensity);
}

// 射线与球体相交
bool RaySphereIntersect(float3 rayOrigin, float3 rayDir, float3 sphereCenter, float sphereRadius, out float t0, out float t1)
{
    float3 oc = rayOrigin - sphereCenter;
    float a = dot(rayDir, rayDir);
    float b = 2.0f * dot(oc, rayDir);
    float c = dot(oc, oc) - sphereRadius * sphereRadius;
    float discriminant = b * b - 4.0f * a * c;
    
    if (discriminant < 0.0f)
    {
        t0 = t1 = 0.0f;
        return false;
    }
    
    float sqrtDisc = sqrt(discriminant);
    t0 = (-b - sqrtDisc) / (2.0f * a);
    t1 = (-b + sqrtDisc) / (2.0f * a);
    return true;
}

// 计算大气层光学深度
float3 ComputeOpticalDepth(float3 start, float3 end, int numSamples)
{
    float3 dir = normalize(end - start);
    float length = distance(start, end);
    float stepSize = length / float(numSamples);
    
    float3 opticalDepth = 0;
    float3 pos = start + dir * stepSize * 0.5f;
    
    for (int i = 0; i < numSamples; i++)
    {
        float height = distance(pos, float3(0, -EARTH_RADIUS, 0)) - EARTH_RADIUS;
        float2 density = GetAtmosphereDensity(height);
        
        opticalDepth.xy += density * stepSize;
        pos += dir * stepSize;
    }
    
    return opticalDepth;
}

#endif // SKY_COMMON_HLSLI
