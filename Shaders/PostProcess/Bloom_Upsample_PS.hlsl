// Bloom Upsample Pass - Progressive upsampling with tent filter
// Unreal Engine style: upsample lower mip and blend with current downsample mip
// Uses additive blending for progressive blur accumulation

#include "Bloom_Common.hlsli"

Texture2D<float4> g_LowRes : register(t0);  // Lower resolution mip (upsampled)
Texture2D<float4> g_HighRes : register(t1); // Current resolution downsample mip (to blend)

// 9-tap tent filter for high quality upsampling
float3 UpsampleTent9(float2 uv, float2 texelSize, float radius)
{
    float4 offset = texelSize.xyxy * float4(1.0f, 1.0f, -1.0f, 0.0f) * radius;
    
    float3 s0 = g_LowRes.Sample(g_LinearClampSampler, uv - offset.xy).rgb;
    float3 s1 = g_LowRes.Sample(g_LinearClampSampler, uv - offset.wy).rgb;
    float3 s2 = g_LowRes.Sample(g_LinearClampSampler, uv - offset.zy).rgb;
    float3 s3 = g_LowRes.Sample(g_LinearClampSampler, uv + offset.zw).rgb;
    float3 s4 = g_LowRes.Sample(g_LinearClampSampler, uv).rgb;
    float3 s5 = g_LowRes.Sample(g_LinearClampSampler, uv + offset.xw).rgb;
    float3 s6 = g_LowRes.Sample(g_LinearClampSampler, uv + offset.zy).rgb;
    float3 s7 = g_LowRes.Sample(g_LinearClampSampler, uv + offset.wy).rgb;
    float3 s8 = g_LowRes.Sample(g_LinearClampSampler, uv + offset.xy).rgb;
    
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

float4 main(VSOutput input) : SV_TARGET
{
    // Upsample from low-res with tent filter
    float3 lowResBloom = UpsampleTent9(input.TexCoord, g_TexelSize, g_BloomRadius);
    
    // For the smallest mip (last mip), just pass through - no blending needed
    if (g_IsLastMip > 0.5f)
    {
        return float4(lowResBloom, 1.0f);
    }
    
    // Sample current resolution downsample result
    float3 highResBloom = g_HighRes.Sample(g_LinearClampSampler, input.TexCoord).rgb;
    
    // Additive blend for progressive blur accumulation (Unreal style)
    float3 result = lowResBloom + highResBloom;
    
    return float4(result, 1.0f);
}
