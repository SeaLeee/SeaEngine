// Ocean Quadtree Vertex Shader - Instanced Rendering with LOD
// Based on Unreal Engine's ocean rendering approach
// Features:
// - Instance-based rendering for quadtree nodes
// - LOD morphing to prevent popping
// - Edge stitching to prevent T-junction cracks
// - Multi-cascade Gerstner waves

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
    float4 g_FoamParams;
    float4 g_AtmosphereParams;
};

// 实例数据 (每个四叉树节点)
struct InstanceData
{
    float4 positionScale;   // xyz = 世界位置, w = 缩放
    float4 lodMorph;        // x = LOD, y = morph factor, zw = reserved
    float4 neighborLOD;     // 四个邻居的 LOD (左, 右, 下, 上)
};

StructuredBuffer<InstanceData> g_Instances : register(t0);

// ============================================================================
// Wave Parameters - 与 OceanAdvanced_VS.hlsl 相同
// ============================================================================

static const int CASCADE0_WAVES = 4;
static const float4 WaveParamsC0[CASCADE0_WAVES] = {
    float4(0.707, 0.707, 250.0, 8.0),
    float4(-0.866, 0.5, 180.0, 5.5),
    float4(0.5, -0.866, 120.0, 3.5),
    float4(-0.259, 0.966, 80.0, 2.2)
};

static const int CASCADE1_WAVES = 4;
static const float4 WaveParamsC1[CASCADE1_WAVES] = {
    float4(0.8, 0.6, 45.0, 1.4),
    float4(-0.6, 0.8, 32.0, 1.0),
    float4(0.3, -0.95, 22.0, 0.7),
    float4(-0.95, -0.3, 15.0, 0.5)
};

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
    uint instanceID : SV_InstanceID;
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
    float lod : LOD;
};

// ============================================================================
// Gerstner Wave
// ============================================================================
struct WaveResult
{
    float3 displacement;
    float3 tangent;
    float3 bitangent;
    float jacobian;
};

WaveResult GerstnerWave(float4 wave, float2 pos, float time, float steepness)
{
    WaveResult result;
    
    float2 dir = normalize(wave.xy);
    float wavelength = wave.z;
    float amplitude = wave.w * g_OceanParams.w;
    
    float k = 2.0 * PI / wavelength;
    float omega = sqrt(GRAVITY * k);
    
    float Q = saturate(steepness) / (k * amplitude * 8.0);
    Q = min(Q, 1.0 / (k * amplitude + 0.0001));
    
    float phase = k * dot(dir, pos) - omega * time;
    float sinP = sin(phase);
    float cosP = cos(phase);
    
    result.displacement.x = Q * amplitude * dir.x * cosP;
    result.displacement.y = amplitude * sinP;
    result.displacement.z = Q * amplitude * dir.y * cosP;
    
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
    
    float Jxx = 1.0 - QWA * dir.x * dir.x * sinP;
    float Jyy = 1.0 - QWA * dir.y * dir.y * sinP;
    float Jxy = -QWA * dir.x * dir.y * sinP;
    
    result.jacobian = Jxx * Jyy - Jxy * Jxy;
    
    return result;
}

// ============================================================================
// Edge Morphing - 防止 LOD 边界裂缝
// ============================================================================
float3 ApplyEdgeMorph(float3 localPos, float2 uv, float4 neighborLOD, float currentLOD)
{
    // 边缘阈值
    float edgeThreshold = 0.02;
    
    // 检查是否在边缘
    bool leftEdge = uv.x < edgeThreshold;
    bool rightEdge = uv.x > (1.0 - edgeThreshold);
    bool bottomEdge = uv.y < edgeThreshold;
    bool topEdge = uv.y > (1.0 - edgeThreshold);
    
    // 如果邻居 LOD 更粗糙，需要变形边缘顶点以匹配
    float3 morphedPos = localPos;
    
    // 左边缘
    if (leftEdge && neighborLOD.x > currentLOD)
    {
        float lodDiff = neighborLOD.x - currentLOD;
        float snapFactor = pow(2.0, lodDiff);
        morphedPos.z = floor(morphedPos.z * snapFactor + 0.5) / snapFactor;
    }
    
    // 右边缘
    if (rightEdge && neighborLOD.y > currentLOD)
    {
        float lodDiff = neighborLOD.y - currentLOD;
        float snapFactor = pow(2.0, lodDiff);
        morphedPos.z = floor(morphedPos.z * snapFactor + 0.5) / snapFactor;
    }
    
    // 下边缘
    if (bottomEdge && neighborLOD.z > currentLOD)
    {
        float lodDiff = neighborLOD.z - currentLOD;
        float snapFactor = pow(2.0, lodDiff);
        morphedPos.x = floor(morphedPos.x * snapFactor + 0.5) / snapFactor;
    }
    
    // 上边缘
    if (topEdge && neighborLOD.w > currentLOD)
    {
        float lodDiff = neighborLOD.w - currentLOD;
        float snapFactor = pow(2.0, lodDiff);
        morphedPos.x = floor(morphedPos.x * snapFactor + 0.5) / snapFactor;
    }
    
    return morphedPos;
}

// ============================================================================
// Main Vertex Shader
// ============================================================================
VSOutput VSMain(VSInput input)
{
    VSOutput output;
    
    // 获取实例数据
    InstanceData instance = g_Instances[input.instanceID];
    float3 instancePos = instance.positionScale.xyz;
    float instanceScale = instance.positionScale.w;
    float currentLOD = instance.lodMorph.x;
    float morphFactor = instance.lodMorph.y;
    float4 neighborLOD = instance.neighborLOD;
    
    // 应用边缘变形
    float3 localPos = ApplyEdgeMorph(input.position, input.texcoord, neighborLOD, currentLOD);
    
    // 变换到世界空间
    float3 worldPos = instancePos + localPos * instanceScale;
    float2 worldXZ = worldPos.xz;
    
    float time = g_OceanParams.z;
    
    // 基于距离的 LOD
    float distToCamera = length(worldPos - g_CameraPos.xyz);
    
    float lod0 = 1.0;
    float lod1 = saturate(1.0 - distToCamera / 1000.0);
    float lod2 = saturate(1.0 - distToCamera / 300.0);
    
    float baseSteepness = 0.75;
    
    // 累积波浪
    float3 totalDisplacement = float3(0, 0, 0);
    float3 totalTangent = float3(1, 0, 0);
    float3 totalBitangent = float3(0, 0, 1);
    float totalJacobian = 1.0;
    
    // Cascade 0: Large Swells
    [unroll]
    for (int i = 0; i < CASCADE0_WAVES; i++)
    {
        WaveResult wave = GerstnerWave(WaveParamsC0[i], worldXZ, time * 0.8, baseSteepness);
        totalDisplacement += wave.displacement * lod0;
        totalTangent += (wave.tangent - float3(1, 0, 0)) * lod0;
        totalBitangent += (wave.bitangent - float3(0, 0, 1)) * lod0;
        totalJacobian *= lerp(1.0, wave.jacobian, lod0);
    }
    
    // Cascade 1: Medium Waves
    [unroll]
    for (int j = 0; j < CASCADE1_WAVES; j++)
    {
        WaveResult wave = GerstnerWave(WaveParamsC1[j], worldXZ, time * 1.2, baseSteepness * 0.8);
        totalDisplacement += wave.displacement * lod1;
        totalTangent += (wave.tangent - float3(1, 0, 0)) * lod1;
        totalBitangent += (wave.bitangent - float3(0, 0, 1)) * lod1;
        totalJacobian *= lerp(1.0, wave.jacobian, lod1);
    }
    
    // Cascade 2: Detail Waves
    [unroll]
    for (int k = 0; k < CASCADE2_WAVES; k++)
    {
        WaveResult wave = GerstnerWave(WaveParamsC2[k], worldXZ, time * 1.5, baseSteepness * 0.5);
        totalDisplacement += wave.displacement * lod2;
        totalTangent += (wave.tangent - float3(1, 0, 0)) * lod2;
        totalBitangent += (wave.bitangent - float3(0, 0, 1)) * lod2;
        totalJacobian *= lerp(1.0, wave.jacobian, lod2);
    }
    
    // 应用 LOD 变形 (平滑过渡到更粗糙的 LOD)
    totalDisplacement = lerp(totalDisplacement, totalDisplacement * 0.5, morphFactor * 0.5);
    
    // 应用位移
    float3 displacedPos = worldPos + totalDisplacement;
    
    // 计算法线
    float3 normal = normalize(cross(totalBitangent, totalTangent));
    if (normal.y < 0) normal = -normal;
    
    // 输出
    output.worldPos = displacedPos;
    output.position = mul(float4(displacedPos, 1.0), g_ViewProj);
    output.normal = normal;
    output.tangent = normalize(totalTangent);
    output.bitangent = normalize(totalBitangent);
    output.texcoord = input.texcoord;
    output.texcoord2 = input.texcoord * 4.0 + float2(time * 0.02, time * 0.01);
    
    // 泡沫
    float foamFromJacobian = saturate(1.0 - totalJacobian);
    float foamFromHeight = saturate(totalDisplacement.y * 0.15 - 0.2);
    output.foam = saturate(foamFromJacobian * 2.0 + foamFromHeight);
    
    output.lod = currentLOD;
    
    return output;
}
