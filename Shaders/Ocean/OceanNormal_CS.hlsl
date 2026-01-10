// Ocean Normal Generation - 从位移图生成法线
#include "OceanCommon.hlsli"

Texture2D<float4> g_Displacement : register(t0);  // xyz = displacement
RWTexture2D<float4> g_Normal : register(u0);      // xyz = normal, w = foam

[numthreads(16, 16, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    uint N = g_Resolution.x;
    if (DTid.x >= N || DTid.y >= N) return;
    
    float texelSize = g_TimeParams.z / float(N);  // gridSize / N
    
    // 采样周围4个点计算梯度
    uint2 left = uint2((DTid.x + N - 1) % N, DTid.y);
    uint2 right = uint2((DTid.x + 1) % N, DTid.y);
    uint2 up = uint2(DTid.x, (DTid.y + N - 1) % N);
    uint2 down = uint2(DTid.x, (DTid.y + 1) % N);
    
    float3 dispL = g_Displacement[left].xyz;
    float3 dispR = g_Displacement[right].xyz;
    float3 dispU = g_Displacement[up].xyz;
    float3 dispD = g_Displacement[down].xyz;
    
    // 计算法线 (使用Sobel-like梯度)
    float3 dx = (dispR - dispL) / (2.0f * texelSize);
    float3 dz = (dispD - dispU) / (2.0f * texelSize);
    
    // 法线 = normalize(cross(tangent, bitangent))
    // tangent = (1, dx.y, 0) + (dx.x, 0, dx.z)
    // bitangent = (0, dz.y, 1) + (dz.x, 0, dz.z)
    float3 normal = normalize(float3(-dx.y, 1.0f, -dz.y));
    
    // 计算泡沫（基于Jacobian - 波峰处水面收缩）
    // J = (1 + dDx/dx) * (1 + dDz/dz) - (dDx/dz) * (dDz/dx)
    float Jxx = (dispR.x - dispL.x) / (2.0f * texelSize);
    float Jzz = (dispD.z - dispU.z) / (2.0f * texelSize);
    float Jxz = (dispR.z - dispL.z) / (2.0f * texelSize);
    float Jzx = (dispD.x - dispU.x) / (2.0f * texelSize);
    
    float jacobian = (1.0f + Jxx) * (1.0f + Jzz) - Jxz * Jzx;
    
    // 负Jacobian表示波峰翻卷 -> 泡沫
    float foam = saturate(-jacobian);
    
    g_Normal[DTid.xy] = float4(normal * 0.5f + 0.5f, foam);
}
