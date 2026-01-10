// Ocean Pixel Shader - 海面PBR着色
#include "../Common.hlsli"

cbuffer OceanCB : register(b0)
{
    row_major float4x4 g_ViewProj;
    row_major float4x4 g_World;
    float4 g_CameraPos;
    float4 g_OceanParams;   // x = patch size, y = grid size, z = time, w = unused
    float4 g_SunDirection;
    float4 g_OceanColor;    // 海洋深水颜色
    float4 g_SkyColor;      // 天空颜色 (用于菲涅尔反射)
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLDPOS;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
    float foam : FOAM;
};

// 菲涅尔项 - Schlick近似
float FresnelSchlick(float cosTheta, float F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - saturate(cosTheta), 5.0f);
}

// 水的折射颜色 (SSS近似)
float3 SubsurfaceScattering(float3 viewDir, float3 lightDir, float3 normal, float3 waterColor)
{
    // 简化的次表面散射
    float3 H = normalize(lightDir + normal * 0.2f);
    float VdotH = pow(saturate(dot(viewDir, -H)), 3.0f);
    
    return waterColor * VdotH * 0.3f;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 N = normalize(input.normal);
    float3 V = normalize(g_CameraPos.xyz - input.worldPos);
    float3 L = normalize(-g_SunDirection.xyz);
    float3 H = normalize(V + L);
    
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float NdotH = saturate(dot(N, H));
    
    // 水的折射率 n = 1.33, F0 ≈ 0.02
    float F0 = 0.02f;
    float fresnel = FresnelSchlick(NdotV, F0);
    
    // 深水颜色
    float3 deepColor = g_OceanColor.rgb;
    
    // 浅水/散射颜色
    float3 scatterColor = float3(0.0f, 0.3f, 0.4f);
    
    // 基于视角的水体颜色
    float3 waterColor = lerp(scatterColor, deepColor, saturate(NdotV));
    
    // 天空反射
    float3 reflectDir = reflect(-V, N);
    // 简化的天空颜色 (理想情况应采样天空盒)
    float3 skyReflection = g_SkyColor.rgb * pow(saturate(reflectDir.y * 0.5f + 0.5f), 0.5f);
    
    // 太阳高光
    float sunSpec = pow(NdotH, 256.0f) * 2.0f;
    float3 sunColor = float3(1.0f, 0.95f, 0.8f) * sunSpec * NdotL;
    
    // 次表面散射
    float3 sss = SubsurfaceScattering(V, L, N, scatterColor);
    
    // 混合
    float3 diffuse = waterColor * (0.3f + NdotL * 0.5f);
    float3 reflection = skyReflection;
    
    // 根据菲涅尔混合折射和反射
    float3 color = lerp(diffuse + sss, reflection, fresnel) + sunColor;
    
    // 泡沫
    float3 foamColor = float3(0.9f, 0.95f, 1.0f);
    color = lerp(color, foamColor, input.foam * 0.8f);
    
    // 简单的大气散射/雾
    float dist = length(input.worldPos - g_CameraPos.xyz);
    float fogFactor = 1.0f - exp(-dist * 0.002f);
    float3 fogColor = g_SkyColor.rgb * 0.8f;
    color = lerp(color, fogColor, saturate(fogFactor));
    
    return float4(color, 1.0f);
}
