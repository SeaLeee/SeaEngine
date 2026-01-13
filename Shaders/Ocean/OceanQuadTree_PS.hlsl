// Ocean QuadTree Pixel Shader - 简化版无纹理渲染
// 用于 QuadTree LOD 系统，不需要外部纹理资源

cbuffer OceanCB : register(b0)
{
    row_major float4x4 g_ViewProj;
    row_major float4x4 g_World;
    float4 g_CameraPos;
    float4 g_OceanParams;       // x = patch size, y = grid size, z = time, w = choppiness
    float4 g_SunDirection;      // xyz = direction, w = intensity
    float4 g_OceanColor;        // Deep water absorption color
    float4 g_SkyColor;          // Sky/horizon color
    float4 g_ScatterColor;      // Subsurface scatter color
    float4 g_FoamParams;        // x = foam intensity, y = foam scale, z = whitecap threshold, w = shore blend
    float4 g_AtmosphereParams;  // x = fog density, y = fog height falloff, z = sun disk size, w = unused
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLDPOS;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
    float2 texcoord2 : TEXCOORD1;
    float foam : FOAM;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
};

// Constants
static const float PI = 3.14159265359;
static const float WATER_F0 = 0.02;
static const float3 WATER_ABSORPTION = float3(0.35, 0.045, 0.025);

// Schlick Fresnel
float FresnelSchlick(float cosTheta, float F0)
{
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

// Subsurface Scattering (simplified)
float3 SubsurfaceScattering(float3 V, float3 L, float3 N, float3 lightColor)
{
    float wrapNdotL = saturate((dot(N, L) + 0.6) / 1.6);
    float3 H = normalize(L + N * 0.6);
    float VdotH = saturate(dot(V, -H));
    float sssIntensity = pow(VdotH, 3.0) * 1.5;
    float rimSSS = pow(1.0 - saturate(dot(N, V)), 3.0) * 0.5;
    float3 scatterColor = g_ScatterColor.rgb * (1.0 - WATER_ABSORPTION * 0.5);
    return scatterColor * (wrapNdotL * 0.5 + sssIntensity + rimSSS) * 0.8 * lightColor;
}

// Procedural foam
float3 CalculateFoam(float2 uv, float foamFactor, float NdotL)
{
    float time = g_OceanParams.z;
    
    // Simple procedural foam pattern
    float2 foamUV1 = uv * g_FoamParams.y + time * 0.02;
    float2 foamUV2 = uv * g_FoamParams.y * 2.3 + time * 0.015;
    
    // Simple noise-like pattern
    float foam1 = frac(sin(dot(foamUV1, float2(12.9898, 78.233))) * 43758.5453);
    float foam2 = frac(sin(dot(foamUV2, float2(63.7264, 10.873))) * 43758.5453);
    float foamNoise = (foam1 * 0.6 + foam2 * 0.4);
    
    float foamThreshold = g_FoamParams.z;
    float foamMask = smoothstep(foamThreshold - 0.2, foamThreshold + 0.2, foamFactor);
    float finalFoam = foamMask * foamNoise * g_FoamParams.x;
    
    float3 foamColor = lerp(float3(0.9, 0.95, 1.0), float3(1.0, 1.0, 1.0), NdotL * 0.5);
    return foamColor * saturate(finalFoam);
}

// Atmospheric fog
float3 ApplyAtmosphericFog(float3 color, float3 worldPos, float3 cameraPos, float3 sunDir)
{
    float dist = length(worldPos - cameraPos);
    float fogDensity = g_AtmosphereParams.x;
    float fogAmount = 1.0 - exp(-dist * fogDensity * 0.0001);
    fogAmount = saturate(fogAmount);
    
    float3 fogColor = g_SkyColor.rgb;
    float sunAmount = max(0.0, dot(normalize(worldPos - cameraPos), -sunDir));
    fogColor = lerp(fogColor, g_SunDirection.w * float3(1.0, 0.9, 0.7), pow(sunAmount, 8.0) * 0.3);
    
    return lerp(color, fogColor, fogAmount);
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
    
    // Fresnel
    float fresnel = FresnelSchlick(NdotV, WATER_F0);
    
    // 反射颜色 (简化 - 使用天空颜色)
    float3 R = reflect(-V, N);
    float skyGradient = saturate(R.y * 0.5 + 0.5);
    float3 reflectionColor = lerp(g_OceanColor.rgb, g_SkyColor.rgb, skyGradient);
    
    // 太阳高光
    float sunSpec = pow(NdotH, 256.0) * g_SunDirection.w;
    reflectionColor += float3(1.0, 0.95, 0.8) * sunSpec;
    
    // 折射/水体颜色
    float3 refractionColor = g_OceanColor.rgb;
    
    // 次表面散射
    float3 sss = SubsurfaceScattering(V, L, N, float3(1.0, 0.98, 0.95) * g_SunDirection.w);
    refractionColor += sss;
    
    // 混合反射和折射
    float3 waterColor = lerp(refractionColor, reflectionColor, fresnel);
    
    // 泡沫
    float3 foam = CalculateFoam(input.texcoord, input.foam, NdotL);
    waterColor = lerp(waterColor, foam, saturate(input.foam * g_FoamParams.x));
    
    // 大气雾
    waterColor = ApplyAtmosphericFog(waterColor, input.worldPos, g_CameraPos.xyz, g_SunDirection.xyz);
    
    return float4(waterColor, 1.0);
}
