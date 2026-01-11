// Bloom Upsample Final Pass - Combines all mip levels with per-mip weights
// This is Unreal's approach: each mip level has its own intensity weight

#include "Bloom_Common.hlsli"

// Each mip level as separate texture
Texture2D<float4> g_Bloom1 : register(t0);  // 1/2 res
Texture2D<float4> g_Bloom2 : register(t1);  // 1/4 res
Texture2D<float4> g_Bloom3 : register(t2);  // 1/8 res
Texture2D<float4> g_Bloom4 : register(t3);  // 1/16 res
Texture2D<float4> g_Bloom5 : register(t4);  // 1/32 res
Texture2D<float4> g_Bloom6 : register(t5);  // 1/64 res

// 9-tap tent upsample
float3 TentUpsample(Texture2D<float4> tex, float2 uv, float2 texelSize)
{
    float4 offset = texelSize.xyxy * float4(1.0f, 1.0f, -1.0f, 0.0f);
    
    float3 s0 = tex.Sample(g_LinearClampSampler, uv - offset.xy).rgb;
    float3 s1 = tex.Sample(g_LinearClampSampler, uv - offset.wy).rgb;
    float3 s2 = tex.Sample(g_LinearClampSampler, uv - offset.zy).rgb;
    float3 s3 = tex.Sample(g_LinearClampSampler, uv + offset.zw).rgb;
    float3 s4 = tex.Sample(g_LinearClampSampler, uv).rgb;
    float3 s5 = tex.Sample(g_LinearClampSampler, uv + offset.xw).rgb;
    float3 s6 = tex.Sample(g_LinearClampSampler, uv + offset.zy).rgb;
    float3 s7 = tex.Sample(g_LinearClampSampler, uv + offset.wy).rgb;
    float3 s8 = tex.Sample(g_LinearClampSampler, uv + offset.xy).rgb;
    
    return (s0 + s2 + s6 + s8) * (1.0f / 16.0f) +
           (s1 + s3 + s5 + s7) * (2.0f / 16.0f) +
           s4 * (4.0f / 16.0f);
}

float4 main(VSOutput input) : SV_TARGET
{
    float2 uv = input.TexCoord;
    
    // Sample each mip with tent filter and apply weights
    // Each mip contributes different bloom "size"
    // Mip 1 = sharp/small bloom, Mip 6 = large/soft bloom
    
    float3 bloom = float3(0.0f, 0.0f, 0.0f);
    
    // Mip level texel sizes (each is 2x larger than previous)
    float2 ts1 = g_TexelSize * 2.0f;
    float2 ts2 = g_TexelSize * 4.0f;
    float2 ts3 = g_TexelSize * 8.0f;
    float2 ts4 = g_TexelSize * 16.0f;
    float2 ts5 = g_TexelSize * 32.0f;
    float2 ts6 = g_TexelSize * 64.0f;
    
    // Accumulate weighted bloom from each mip
    if (g_Bloom1Weight > 0.0f)
        bloom += TentUpsample(g_Bloom1, uv, ts1) * g_Bloom1Weight;
    
    if (g_Bloom2Weight > 0.0f)
        bloom += TentUpsample(g_Bloom2, uv, ts2) * g_Bloom2Weight;
    
    if (g_Bloom3Weight > 0.0f)
        bloom += TentUpsample(g_Bloom3, uv, ts3) * g_Bloom3Weight;
    
    if (g_Bloom4Weight > 0.0f)
        bloom += TentUpsample(g_Bloom4, uv, ts4) * g_Bloom4Weight;
    
    if (g_Bloom5Weight > 0.0f)
        bloom += TentUpsample(g_Bloom5, uv, ts5) * g_Bloom5Weight;
    
    if (g_Bloom6Weight > 0.0f)
        bloom += TentUpsample(g_Bloom6, uv, ts6) * g_Bloom6Weight;
    
    return float4(bloom, 1.0f);
}
