// Ocean Advanced Pixel Shader - AAA Quality Ocean Rendering
// Based on:
// - "World of Warships: Technical Aspects of Rendering High-Quality Sea Till Horizon" (GDC)
// - "Rendering Rapids in Uncharted 4" (SIGGRAPH)
// - Real-time Water Rendering (NVIDIA)

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

// Textures
Texture2D<float4> g_NormalMap : register(t0);
Texture2D<float4> g_FoamTexture : register(t1);
TextureCube<float4> g_EnvironmentMap : register(t2);
SamplerState g_LinearWrap : register(s0);
SamplerState g_LinearClamp : register(s1);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLDPOS;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
    float2 texcoord2 : TEXCOORD1;   // Second UV for detail
    float foam : FOAM;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
};

// ============================================================================
// Constants - 更真实的水体参数
// ============================================================================
static const float PI = 3.14159265359;
static const float WATER_IOR = 1.333;           // Index of refraction
static const float WATER_F0 = 0.02;             // Fresnel reflectance at normal incidence
static const float3 WATER_ABSORPTION = float3(0.35, 0.045, 0.025);  // 调整吸收系数使水更蓝绿
static const float3 WATER_SCATTER = float3(0.0025, 0.005, 0.006);   // 增大散射

// ============================================================================
// Utility Functions
// ============================================================================

// Schlick Fresnel with roughness
float FresnelSchlickRoughness(float cosTheta, float F0, float roughness)
{
    return F0 + (max(1.0 - roughness, F0) - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

// GGX Distribution
float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return num / max(denom, 0.0001);
}

// Smith GGX Geometry function
float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    
    float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
    float ggx2 = NdotL / (NdotL * (1.0 - k) + k);
    
    return ggx1 * ggx2;
}

// ============================================================================
// Subsurface Scattering (Uncharted 4 Style)
// 次表面散射 - 模拟阳光穿透水面的效果
// ============================================================================
float3 SubsurfaceScattering(float3 V, float3 L, float3 N, float3 lightColor, float sssStrength)
{
    // Wrap lighting for SSS - 包裹光照使暗部也有散射
    float wrapNdotL = saturate((dot(N, L) + 0.6) / 1.6);
    
    // View-dependent SSS (light passing through waves)
    // 当观察方向与光线方向相对时，散射最强（逆光）
    float3 H = normalize(L + N * 0.6);
    float VdotH = saturate(dot(V, -H));
    float sssIntensity = pow(VdotH, 3.0) * 1.5;
    
    // 增加额外的边缘散射
    float rimSSS = pow(1.0 - saturate(dot(N, V)), 3.0) * 0.5;
    
    // Color based on water absorption - 散射颜色随深度变化
    float3 scatterColor = g_ScatterColor.rgb * (1.0 - WATER_ABSORPTION * 0.5);
    
    // Combine all SSS components
    float3 sss = scatterColor * (wrapNdotL * 0.5 + sssIntensity + rimSSS) * sssStrength;
    
    return sss * lightColor;
}

// ============================================================================
// Foam / Whitecaps (World of Warships Style)
// ============================================================================
float3 CalculateFoam(float2 uv, float foamFactor, float NdotL)
{
    // Multi-layer foam sampling for better coverage
    float2 foamUV1 = uv * g_FoamParams.y;
    float2 foamUV2 = uv * g_FoamParams.y * 2.3 + float2(0.3, 0.7);
    
    // Animate foam
    float time = g_OceanParams.z;
    foamUV1 += float2(time * 0.02, time * 0.01);
    foamUV2 += float2(-time * 0.015, time * 0.025);
    
    // Sample foam texture (or use procedural)
    float foam1 = 1.0; // g_FoamTexture.Sample(g_LinearWrap, foamUV1).r;
    float foam2 = 1.0; // g_FoamTexture.Sample(g_LinearWrap, foamUV2).r;
    
    // Combine foam layers
    float foamNoise = (foam1 * 0.6 + foam2 * 0.4);
    
    // Threshold based on wave steepness (Jacobian)
    float foamThreshold = g_FoamParams.z;
    float foamMask = smoothstep(foamThreshold - 0.2, foamThreshold + 0.2, foamFactor);
    
    // Final foam intensity
    float finalFoam = foamMask * foamNoise * g_FoamParams.x;
    
    // Foam color (slightly tinted by water and light)
    float3 foamColor = lerp(float3(0.9, 0.95, 1.0), float3(1.0, 1.0, 1.0), NdotL * 0.5);
    
    return foamColor * saturate(finalFoam);
}

// ============================================================================
// Atmospheric Scattering / Fog (World of Warships Style)
// ============================================================================
float3 ApplyAtmosphericFog(float3 color, float3 worldPos, float3 cameraPos, float3 sunDir)
{
    float3 rayDir = worldPos - cameraPos;
    float distance = length(rayDir);
    rayDir = normalize(rayDir);
    
    // Exponential fog
    float fogDensity = g_AtmosphereParams.x;
    float fogAmount = 1.0 - exp(-distance * fogDensity);
    
    // Height-based fog falloff
    float heightFalloff = g_AtmosphereParams.y;
    float heightFog = exp(-max(worldPos.y, 0.0) * heightFalloff);
    fogAmount *= heightFog;
    
    // Sun-aligned fog color (aerial perspective)
    float sunAmount = max(dot(rayDir, -sunDir), 0.0);
    sunAmount = pow(sunAmount, 8.0);
    
    float3 fogColor = lerp(g_SkyColor.rgb * 0.8, g_SkyColor.rgb * 1.2, sunAmount);
    
    // Apply fog
    return lerp(color, fogColor, saturate(fogAmount));
}

// ============================================================================
// Sun Disk Reflection
// ============================================================================
float3 SunDiskReflection(float3 R, float3 sunDir, float roughness)
{
    float sunDiskSize = g_AtmosphereParams.z * (1.0 + roughness * 2.0);
    float sunDot = saturate(dot(R, -sunDir));
    float sunIntensity = pow(sunDot, 1.0 / max(sunDiskSize, 0.001));
    
    // Sun color
    float3 sunColor = float3(1.0, 0.95, 0.85) * g_SunDirection.w;
    
    return sunColor * sunIntensity * 5.0;
}

// ============================================================================
// Main Pixel Shader
// ============================================================================
float4 PSMain(PSInput input) : SV_TARGET
{
    // Normalize interpolated values
    float3 N = normalize(input.normal);
    float3 T = normalize(input.tangent);
    float3 B = normalize(input.bitangent);
    float3 V = normalize(g_CameraPos.xyz - input.worldPos);
    float3 L = normalize(-g_SunDirection.xyz);
    
    // Sample normal map (if available) and transform to world space
    // For now, use vertex normal
    // float3 normalMap = g_NormalMap.Sample(g_LinearWrap, input.texcoord).xyz * 2.0 - 1.0;
    // float3x3 TBN = float3x3(T, B, N);
    // N = normalize(mul(normalMap, TBN));
    
    // Reflection vector
    float3 R = reflect(-V, N);
    float3 H = normalize(V + L);
    
    // Dot products
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));
    
    // Distance-based roughness (smoother at distance - World of Warships technique)
    float dist = length(input.worldPos - g_CameraPos.xyz);
    float distanceFade = saturate(dist / 800.0);  // 更远的平滑距离
    float roughness = lerp(0.02, 0.12, distanceFade);  // 近处更光滑
    
    // ========== Fresnel ==========
    float fresnel = FresnelSchlickRoughness(NdotV, WATER_F0, roughness);
    
    // ========== Deep Water Color (Absorption) ==========
    // Deeper viewing angle = more absorption = darker
    float3 absorptionColor = g_OceanColor.rgb;
    float depthFactor = 1.0 - NdotV;
    
    // 更丰富的水体颜色变化 - 基于菲涅尔效应
    float3 shallowColor = float3(0.02, 0.18, 0.25);  // 浅水区偏青绿
    float3 deepColor = absorptionColor * 0.4;         // 深水区更暗更蓝
    float3 waterBodyColor = lerp(deepColor, shallowColor, pow(NdotV, 2.0));
    
    // ========== Subsurface Scattering ==========
    float sssStrength = g_FoamParams.w;  // 从常量缓冲区获取 SSS 强度
    float3 sss = SubsurfaceScattering(V, L, N, float3(1.0, 0.98, 0.9), sssStrength);
    
    // 额外的 SSS 在波峰处 (阳光穿透薄薄的波峰)
    float waveTopSSS = saturate(input.foam * 0.8);
    sss += g_ScatterColor.rgb * waveTopSSS * sssStrength * 0.5;
    
    // ========== Sky Reflection ==========
    // Sample environment map or use procedural sky
    float3 skyReflection;
    
    // 更丰富的天空渐变
    float horizonFade = pow(saturate(1.0 - R.y), 4.0);
    float3 zenithColor = float3(0.25, 0.45, 0.85);   // 天顶深蓝
    float3 horizonColor = g_SkyColor.rgb * 1.1;       // 地平线略亮
    float3 sunHorizonColor = float3(1.0, 0.85, 0.6); // 太阳方向暖色
    
    // 太阳方向影响天空颜色
    float sunInfluence = max(dot(R, -L), 0.0);
    sunInfluence = pow(sunInfluence, 4.0) * 0.3;
    
    skyReflection = lerp(zenithColor, horizonColor, horizonFade);
    skyReflection = lerp(skyReflection, sunHorizonColor, sunInfluence * horizonFade);
    
    // Add sun disk to reflection
    skyReflection += SunDiskReflection(R, L, roughness);
    
    // Roughness affects reflection (blur at distance)
    skyReflection *= (1.0 - roughness * 0.3);
    
    // ========== Specular (Sun) ==========
    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    
    float3 numerator = D * G * fresnel;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    float3 specular = numerator / denominator;
    
    float3 sunColor = float3(1.0, 0.95, 0.85) * g_SunDirection.w;
    float3 sunSpecular = specular * sunColor * NdotL;
    
    // ========== Combine Lighting ==========
    // Refracted (underwater) component
    float3 refracted = waterBodyColor + sss;
    
    // Reflected component
    float3 reflected = skyReflection;
    
    // Blend based on Fresnel
    float3 color = lerp(refracted, reflected, fresnel) + sunSpecular;
    
    // ========== Foam / Whitecaps ==========
    float3 foamColor = CalculateFoam(input.texcoord, input.foam, NdotL);
    float foamBlend = saturate(length(foamColor));
    color = lerp(color, foamColor, foamBlend * 0.85);
    
    // ========== Atmospheric Fog ==========
    color = ApplyAtmosphericFog(color, input.worldPos, g_CameraPos.xyz, L);
    
    // ========== HDR Output (no tonemapping here - done in post) ==========
    return float4(color, 1.0);
}
