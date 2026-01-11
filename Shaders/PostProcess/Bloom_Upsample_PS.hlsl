// Bloom Upsample Pass - Progressive upsampling with tent filter
// Unreal Engine style 9-tap tent filter for smooth upsampling

#include "Bloom_Common.hlsli"

Texture2D<float4> g_LowRes : register(t0);  // Lower resolution mip
Texture2D<float4> g_HighRes : register(t1); // Higher resolution mip to blend with

// 9-tap tent filter for high quality upsampling
// Better than bilinear, avoids blocky artifacts
float3 UpsampleTent9(float2 uv, float2 texelSize, float radius)
{
    float4 offset = texelSize.xyxy * float4(1.0f, 1.0f, -1.0f, 0.0f) * radius;
    
    // 3x3 tent filter samples
    float3 s0 = g_LowRes.Sample(g_LinearClampSampler, uv - offset.xy).rgb; // -1, -1
    float3 s1 = g_LowRes.Sample(g_LinearClampSampler, uv - offset.wy).rgb; //  0, -1
    float3 s2 = g_LowRes.Sample(g_LinearClampSampler, uv - offset.zy).rgb; //  1, -1
    
    float3 s3 = g_LowRes.Sample(g_LinearClampSampler, uv + offset.zw).rgb; // -1,  0
    float3 s4 = g_LowRes.Sample(g_LinearClampSampler, uv).rgb;              //  0,  0
    float3 s5 = g_LowRes.Sample(g_LinearClampSampler, uv + offset.xw).rgb; //  1,  0
    
    float3 s6 = g_LowRes.Sample(g_LinearClampSampler, uv + offset.zy).rgb; // -1,  1
    float3 s7 = g_LowRes.Sample(g_LinearClampSampler, uv + offset.wy).rgb; //  0,  1
    float3 s8 = g_LowRes.Sample(g_LinearClampSampler, uv + offset.xy).rgb; //  1,  1
    
    // Tent filter weights (1-2-1 separable kernel normalized)
    // Corners: 1/16, Edges: 2/16, Center: 4/16
    float3 result = s0 * (1.0f / 16.0f);
    result += s1 * (2.0f / 16.0f);
    result += s2 * (1.0f / 16.0f);
    result += s3 * (2.0f / 16.0f);
    result += s4 * (4.0f / 16.0f);
    result += s5 * (2.0f / 16.0f);
    result += s6 * (1.0f / 16.0f);
    result += s7 * (2.0f / 16.0f);
    result += s8 * (1.0f / 16.0f);
    
    return result;
}

// Simple bilinear upsample with additive blend
float3 UpsampleBilinear(float2 uv)
{
    return g_LowRes.Sample(g_LinearClampSampler, uv).rgb;
}

float4 main(VSOutput input) : SV_TARGET
{
    // Get upsampled low-res bloom
    float3 lowResBloom = UpsampleTent9(input.TexCoord, g_TexelSize, g_BloomRadius);
    
    // Get existing high-res data
    float3 highResBloom = g_HighRes.Sample(g_LinearClampSampler, input.TexCoord).rgb;
    
    // Additive blend (progressive blur accumulation)
    float3 result = lowResBloom + highResBloom;
    
    return float4(result, 1.0f);
}
