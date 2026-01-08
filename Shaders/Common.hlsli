// Common shader utilities for SeaEngine
// This file can be included in other shaders using #include "Common.hlsli"

#ifndef COMMON_HLSLI
#define COMMON_HLSLI

// Constants
static const float PI = 3.14159265359f;
static const float INV_PI = 0.31830988618f;
static const float EPSILON = 1e-6f;

// Common constant buffer
cbuffer FrameConstants : register(b0)
{
    float4x4 g_ViewMatrix;
    float4x4 g_ProjMatrix;
    float4x4 g_ViewProjMatrix;
    float4x4 g_InvViewProjMatrix;
    float3 g_CameraPosition;
    float g_Time;
    float2 g_ScreenSize;
    float2 g_InvScreenSize;
}

// Vertex input structures
struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float4 Tangent : TANGENT;
};

// Common output structures
struct PSInput
{
    float4 Position : SV_POSITION;
    float3 WorldPos : WORLDPOS;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 Tangent : TANGENT;
    float3 Bitangent : BITANGENT;
};

// Utility functions
float3 ReconstructWorldPosition(float2 uv, float depth)
{
    float4 clipPos = float4(uv * 2.0 - 1.0, depth, 1.0);
    clipPos.y = -clipPos.y;
    float4 worldPos = mul(g_InvViewProjMatrix, clipPos);
    return worldPos.xyz / worldPos.w;
}

float LinearizeDepth(float depth, float nearZ, float farZ)
{
    return nearZ * farZ / (farZ - depth * (farZ - nearZ));
}

float3 GammaToLinear(float3 color)
{
    return pow(color, 2.2);
}

float3 LinearToGamma(float3 color)
{
    return pow(color, 1.0 / 2.2);
}

// Normal map decoding
float3 DecodeNormal(float2 encodedNormal)
{
    float2 n = encodedNormal * 2.0 - 1.0;
    float z = sqrt(max(0.0, 1.0 - dot(n, n)));
    return float3(n, z);
}

float3 TransformNormalToWorld(float3 normal, float3 worldNormal, float3 worldTangent, float3 worldBitangent)
{
    float3x3 TBN = float3x3(worldTangent, worldBitangent, worldNormal);
    return normalize(mul(normal, TBN));
}

#endif // COMMON_HLSLI
