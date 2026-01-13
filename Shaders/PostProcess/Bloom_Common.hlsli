// Bloom Common - Shared definitions for Unreal-style Bloom
// Reference: Unreal Engine 4/5 Post Process Bloom Implementation

#ifndef BLOOM_COMMON_HLSLI
#define BLOOM_COMMON_HLSLI

// Bloom constants
cbuffer BloomConstants : register(b0)
{
    float2 g_TexelSize;         // 1.0 / Resolution
    float g_BloomThreshold;     // Brightness threshold for bloom
    float g_BloomIntensity;     // Overall bloom strength
    
    float g_BloomRadius;        // Bloom blur radius multiplier
    float g_BloomTint_R;        // Bloom color tint
    float g_BloomTint_G;
    float g_BloomTint_B;
    
    // Per-mip weights (Unreal style - 6 mip levels)
    float g_Bloom1Weight;       // Mip 0 (1/2 res)
    float g_Bloom2Weight;       // Mip 1 (1/4 res)
    float g_Bloom3Weight;       // Mip 2 (1/8 res)
    float g_Bloom4Weight;       // Mip 3 (1/16 res)
    float g_Bloom5Weight;       // Mip 4 (1/32 res)
    float g_Bloom6Weight;       // Mip 5 (1/64 res)
    
    float g_CurrentMipLevel;    // Current mip level for upsample
    float g_IsLastMip;          // 1.0 if this is the smallest mip level
};

SamplerState g_LinearClampSampler : register(s0);

// Vertex shader output
struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

// Safe HDR luminance calculation
float Luminance(float3 color)
{
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

// Karis average for anti-flickering (used in downsample)
float KarisAverage(float3 c1, float3 c2, float3 c3, float3 c4)
{
    // Weight by inverse luminance to reduce fireflies
    float w1 = 1.0f / (1.0f + Luminance(c1));
    float w2 = 1.0f / (1.0f + Luminance(c2));
    float w3 = 1.0f / (1.0f + Luminance(c3));
    float w4 = 1.0f / (1.0f + Luminance(c4));
    return (w1 + w2 + w3 + w4);
}

#endif // BLOOM_COMMON_HLSLI
