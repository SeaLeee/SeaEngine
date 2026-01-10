// Ocean Gerstner Pixel Shader - 海面PBR着色（独立版本，不依赖Common.hlsli）

cbuffer OceanCB : register(b0)
{
    row_major float4x4 g_ViewProj;
    row_major float4x4 g_World;
    float4 g_CameraPos;
    float4 g_OceanParams;   // x = patch size, y = grid size, z = time, w = choppiness
    float4 g_SunDirection;
    float4 g_OceanColor;    // 海洋深水颜色
    float4 g_SkyColor;      // 天空颜色
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
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

// 简化的次表面散射
float3 SubsurfaceScattering(float3 viewDir, float3 lightDir, float3 normal, float3 waterColor)
{
    float3 H = normalize(lightDir + normal * 0.3);
    float VdotH = pow(saturate(dot(viewDir, -H)), 4.0);
    return waterColor * VdotH * 0.4;
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
    float F0 = 0.02;
    float fresnel = FresnelSchlick(NdotV, F0);
    
    // 海洋颜色
    float3 deepColor = g_OceanColor.rgb;          // 深水颜色
    float3 shallowColor = float3(0.1, 0.4, 0.5);  // 浅水颜色
    float3 scatterColor = float3(0.0, 0.2, 0.3);  // 散射颜色
    
    // 基于法线和视角的颜色混合
    float depthFactor = saturate(1.0 - NdotV);
    float3 waterColor = lerp(shallowColor, deepColor, depthFactor);
    
    // 天空反射
    float3 R = reflect(-V, N);
    float skyGradient = saturate(R.y * 0.5 + 0.5);
    float3 skyColor = lerp(g_SkyColor.rgb * 0.5, g_SkyColor.rgb, skyGradient);
    
    // 太阳反射（高光）
    float sunSpec = pow(NdotH, 512.0) * 5.0;
    float3 sunColor = float3(1.0, 0.95, 0.85) * sunSpec * NdotL;
    
    // 次表面散射
    float3 sss = SubsurfaceScattering(V, L, N, scatterColor);
    
    // 漫反射
    float3 diffuse = waterColor * (0.2 + NdotL * 0.4);
    
    // 环境光
    float3 ambient = waterColor * 0.15;
    
    // 根据菲涅尔混合折射和反射
    float3 color = ambient + lerp(diffuse + sss, skyColor, fresnel) + sunColor;
    
    // 泡沫
    float foamIntensity = input.foam * input.foam;  // 平方使泡沫更集中
    float3 foamColor = float3(0.95, 0.98, 1.0);
    color = lerp(color, foamColor, foamIntensity * 0.7);
    
    // 大气雾效
    float dist = length(input.worldPos - g_CameraPos.xyz);
    float fogDensity = 0.001;
    float fogFactor = 1.0 - exp(-dist * fogDensity);
    float3 fogColor = g_SkyColor.rgb * 0.7;
    color = lerp(color, fogColor, saturate(fogFactor));
    
    // 简单色调映射
    color = color / (color + 1.0);
    
    // Gamma校正
    color = pow(color, 1.0 / 2.2);
    
    return float4(color, 1.0);
}
