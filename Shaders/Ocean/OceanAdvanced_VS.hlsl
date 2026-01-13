// Ocean Advanced Vertex Shader - AAA Quality Wave Simulation
// Features:
// - Multi-cascade Gerstner waves (World of Warships technique)
// - Distance-based LOD for wave detail
// - Proper tangent space for normal mapping
// - Jacobian-based foam generation

cbuffer OceanCB : register(b0)
{
    row_major float4x4 g_ViewProj;
    row_major float4x4 g_World;
    float4 g_CameraPos;
    float4 g_OceanParams;       // x = patch size, y = grid size, z = time, w = choppiness
    float4 g_SunDirection;
    float4 g_OceanColor;
    float4 g_SkyColor;
    float4 g_ScatterColor;
    float4 g_FoamParams;        // x = foam intensity, y = foam scale, z = whitecap threshold
    float4 g_AtmosphereParams;
};

// ============================================================================
// Wave Parameters - Multi-Cascade System (World of Warships Style)
// 使用基于物理的波浪参数，更真实的大洋波浪
// ============================================================================

// Cascade 0: Large swells (ocean-scale waves) - 远洋大浪
static const int CASCADE0_WAVES = 4;
static const float4 WaveParamsC0[CASCADE0_WAVES] = {
    float4(0.707, 0.707, 250.0, 8.0),    // Direction XZ, Wavelength, Amplitude - 主浪
    float4(-0.866, 0.5, 180.0, 5.5),     // 次主浪
    float4(0.5, -0.866, 120.0, 3.5),     // 交叉浪
    float4(-0.259, 0.966, 80.0, 2.2)     // 补充浪
};

// Cascade 1: Medium waves - 中等波浪
static const int CASCADE1_WAVES = 4;
static const float4 WaveParamsC1[CASCADE1_WAVES] = {
    float4(0.8, 0.6, 45.0, 1.4),
    float4(-0.6, 0.8, 32.0, 1.0),
    float4(0.3, -0.95, 22.0, 0.7),
    float4(-0.95, -0.3, 15.0, 0.5)
};

// Cascade 2: Small detail waves (close-up only) - 近距离细节波纹
static const int CASCADE2_WAVES = 4;
static const float4 WaveParamsC2[CASCADE2_WAVES] = {
    float4(0.9, 0.4, 8.0, 0.25),
    float4(-0.4, 0.9, 5.5, 0.18),
    float4(0.6, -0.8, 3.5, 0.12),
    float4(-0.8, -0.6, 2.2, 0.08)
};

static const float PI = 3.14159265359;
static const float GRAVITY = 9.81;

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
    float4 color : COLOR;
};

struct VSOutput
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

// ============================================================================
// Gerstner Wave with Jacobian for Foam
// ============================================================================
struct WaveResult
{
    float3 displacement;
    float3 tangent;
    float3 bitangent;
    float jacobian;     // For foam calculation
};

WaveResult GerstnerWave(float4 wave, float2 pos, float time, float steepness)
{
    WaveResult result;
    
    float2 dir = normalize(wave.xy);
    float wavelength = wave.z;
    float amplitude = wave.w * g_OceanParams.w;
    
    // Wave number and angular frequency
    float k = 2.0 * PI / wavelength;
    float omega = sqrt(GRAVITY * k);   // Deep water dispersion
    
    // Steepness (Q parameter in Gerstner)
    float Q = saturate(steepness) / (k * amplitude * 8.0);  // Limit to prevent loops
    Q = min(Q, 1.0 / (k * amplitude + 0.0001));
    
    // Phase
    float phase = k * dot(dir, pos) - omega * time;
    float sinP = sin(phase);
    float cosP = cos(phase);
    
    // Displacement
    result.displacement.x = Q * amplitude * dir.x * cosP;
    result.displacement.y = amplitude * sinP;
    result.displacement.z = Q * amplitude * dir.y * cosP;
    
    // Tangent derivatives (for normal calculation)
    float WA = k * amplitude;
    float QWA = Q * WA;
    
    result.tangent = float3(
        1.0 - QWA * dir.x * dir.x * sinP,
        WA * dir.x * cosP,
        -QWA * dir.x * dir.y * sinP
    );
    
    result.bitangent = float3(
        -QWA * dir.x * dir.y * sinP,
        WA * dir.y * cosP,
        1.0 - QWA * dir.y * dir.y * sinP
    );
    
    // Jacobian for foam (measures compression/stretching)
    // When Jacobian < 1, surface is compressed (foam forms)
    float Jxx = 1.0 - QWA * dir.x * dir.x * sinP;
    float Jyy = 1.0 - QWA * dir.y * dir.y * sinP;
    float Jxy = -QWA * dir.x * dir.y * sinP;
    
    result.jacobian = Jxx * Jyy - Jxy * Jxy;
    
    return result;
}

// ============================================================================
// Main Vertex Shader
// ============================================================================
VSOutput VSMain(VSInput input)
{
    VSOutput output;
    
    float time = g_OceanParams.z;
    float2 worldXZ = input.position.xz;
    
    // Distance from camera for LOD
    float3 worldPosApprox = mul(float4(input.position, 1.0), g_World).xyz;
    float distToCamera = length(worldPosApprox - g_CameraPos.xyz);
    
    // LOD factors - fade out detail waves at distance (基于距离的LOD)
    float lod0 = 1.0;                                          // Large waves always visible
    float lod1 = saturate(1.0 - distToCamera / 1000.0);        // Medium waves fade at 1000m
    float lod2 = saturate(1.0 - distToCamera / 300.0);         // Detail waves fade at 300m
    
    // Steepness based on sea state - 波浪陡峭度 (更高 = 更尖锐的波峰)
    float baseSteepness = 0.75;
    
    // Accumulate waves
    float3 totalDisplacement = float3(0, 0, 0);
    float3 totalTangent = float3(1, 0, 0);
    float3 totalBitangent = float3(0, 0, 1);
    float totalJacobian = 1.0;
    
    // ========== Cascade 0: Large Swells ==========
    [unroll]
    for (int i = 0; i < CASCADE0_WAVES; i++)
    {
        WaveResult wave = GerstnerWave(WaveParamsC0[i], worldXZ, time * 0.8, baseSteepness);
        totalDisplacement += wave.displacement * lod0;
        totalTangent += (wave.tangent - float3(1, 0, 0)) * lod0;
        totalBitangent += (wave.bitangent - float3(0, 0, 1)) * lod0;
        totalJacobian *= lerp(1.0, wave.jacobian, lod0);
    }
    
    // ========== Cascade 1: Medium Waves ==========
    [unroll]
    for (int j = 0; j < CASCADE1_WAVES; j++)
    {
        WaveResult wave = GerstnerWave(WaveParamsC1[j], worldXZ, time * 1.2, baseSteepness * 0.8);
        totalDisplacement += wave.displacement * lod1;
        totalTangent += (wave.tangent - float3(1, 0, 0)) * lod1;
        totalBitangent += (wave.bitangent - float3(0, 0, 1)) * lod1;
        totalJacobian *= lerp(1.0, wave.jacobian, lod1);
    }
    
    // ========== Cascade 2: Detail Waves ==========
    [unroll]
    for (int k = 0; k < CASCADE2_WAVES; k++)
    {
        WaveResult wave = GerstnerWave(WaveParamsC2[k], worldXZ, time * 1.5, baseSteepness * 0.5);
        totalDisplacement += wave.displacement * lod2;
        totalTangent += (wave.tangent - float3(1, 0, 0)) * lod2;
        totalBitangent += (wave.bitangent - float3(0, 0, 1)) * lod2;
        totalJacobian *= lerp(1.0, wave.jacobian, lod2);
    }
    
    // Apply displacement
    float3 displacedPos = input.position + totalDisplacement;
    
    // Calculate normal from tangent space
    float3 normal = normalize(cross(totalBitangent, totalTangent));
    
    // Ensure normal points up (flip if needed)
    if (normal.y < 0) normal = -normal;
    
    // World space transformation
    float4 worldPos = mul(float4(displacedPos, 1.0), g_World);
    output.worldPos = worldPos.xyz;
    
    // Clip space
    output.position = mul(worldPos, g_ViewProj);
    
    // Transform normal and tangent to world space
    float3x3 worldMatrix3x3 = (float3x3)g_World;
    output.normal = normalize(mul(normal, worldMatrix3x3));
    output.tangent = normalize(mul(totalTangent, worldMatrix3x3));
    output.bitangent = normalize(mul(totalBitangent, worldMatrix3x3));
    
    // Texture coordinates for detail normal map
    output.texcoord = input.texcoord;
    output.texcoord2 = input.texcoord * 4.0 + float2(time * 0.02, time * 0.01);
    
    // Foam based on Jacobian (wave breaking)
    // When Jacobian < 1, surface is compressed
    float foamFromJacobian = saturate(1.0 - totalJacobian);
    
    // Also add foam at wave peaks
    float foamFromHeight = saturate(totalDisplacement.y * 0.15 - 0.2);
    
    // Combine foam sources
    output.foam = saturate(foamFromJacobian * 2.0 + foamFromHeight);
    
    return output;
}
