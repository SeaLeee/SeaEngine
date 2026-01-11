// Bloom Downsample Pass - Progressive downsampling with filtering
// Uses 13-tap filter for high quality (Unreal Engine style)

#include "Bloom_Common.hlsli"

Texture2D<float4> g_Input : register(t0);

// 13-tap downsample filter (better quality than simple bilinear)
// This pattern reduces aliasing and provides smoother blur
float3 Downsample13Tap(float2 uv, float2 texelSize)
{
    // Center sample
    float3 A = g_Input.Sample(g_LinearClampSampler, uv).rgb;
    
    // Inner samples (4 samples at half texel offset)
    float3 B = g_Input.Sample(g_LinearClampSampler, uv + texelSize * float2(-1.0f, -1.0f) * 0.5f).rgb;
    float3 C = g_Input.Sample(g_LinearClampSampler, uv + texelSize * float2( 1.0f, -1.0f) * 0.5f).rgb;
    float3 D = g_Input.Sample(g_LinearClampSampler, uv + texelSize * float2(-1.0f,  1.0f) * 0.5f).rgb;
    float3 E = g_Input.Sample(g_LinearClampSampler, uv + texelSize * float2( 1.0f,  1.0f) * 0.5f).rgb;
    
    // Outer samples (4 samples at 1 texel offset)
    float3 F = g_Input.Sample(g_LinearClampSampler, uv + texelSize * float2(-1.0f,  0.0f)).rgb;
    float3 G = g_Input.Sample(g_LinearClampSampler, uv + texelSize * float2( 1.0f,  0.0f)).rgb;
    float3 H = g_Input.Sample(g_LinearClampSampler, uv + texelSize * float2( 0.0f, -1.0f)).rgb;
    float3 I = g_Input.Sample(g_LinearClampSampler, uv + texelSize * float2( 0.0f,  1.0f)).rgb;
    
    // Corner samples (4 samples at 2 texel offset)
    float3 J = g_Input.Sample(g_LinearClampSampler, uv + texelSize * float2(-2.0f, -2.0f) * 0.5f).rgb;
    float3 K = g_Input.Sample(g_LinearClampSampler, uv + texelSize * float2( 2.0f, -2.0f) * 0.5f).rgb;
    float3 L = g_Input.Sample(g_LinearClampSampler, uv + texelSize * float2(-2.0f,  2.0f) * 0.5f).rgb;
    float3 M = g_Input.Sample(g_LinearClampSampler, uv + texelSize * float2( 2.0f,  2.0f) * 0.5f).rgb;
    
    // Weighted average
    // Center: 0.125
    // Inner 4: 0.125 each (total 0.5)
    // Edges 4: 0.0625 each (total 0.25)
    // Corners 4: 0.03125 each (total 0.125)
    float3 result = A * 0.125f;
    result += (B + C + D + E) * 0.125f;
    result += (F + G + H + I) * 0.0625f;
    result += (J + K + L + M) * 0.03125f;
    
    return result;
}

// Simple box downsample (faster, used for first pass with Karis average)
float3 DownsampleBox4Karis(float2 uv, float2 texelSize)
{
    float4 offset = texelSize.xyxy * float4(-0.5f, -0.5f, 0.5f, 0.5f);
    
    float3 c0 = g_Input.Sample(g_LinearClampSampler, uv + offset.xy).rgb;
    float3 c1 = g_Input.Sample(g_LinearClampSampler, uv + offset.zy).rgb;
    float3 c2 = g_Input.Sample(g_LinearClampSampler, uv + offset.xw).rgb;
    float3 c3 = g_Input.Sample(g_LinearClampSampler, uv + offset.zw).rgb;
    
    // Karis average to reduce fireflies
    float w0 = 1.0f / (1.0f + Luminance(c0));
    float w1 = 1.0f / (1.0f + Luminance(c1));
    float w2 = 1.0f / (1.0f + Luminance(c2));
    float w3 = 1.0f / (1.0f + Luminance(c3));
    
    float totalWeight = w0 + w1 + w2 + w3;
    return (c0 * w0 + c1 * w1 + c2 * w2 + c3 * w3) / totalWeight;
}

float4 main(VSOutput input) : SV_TARGET
{
    float3 color = Downsample13Tap(input.TexCoord, g_TexelSize);
    return float4(color, 1.0f);
}
