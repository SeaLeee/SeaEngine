// Bloom Threshold Pass - Extract bright pixels
// Unreal Engine style threshold with soft knee

#include "Bloom_Common.hlsli"

Texture2D<float4> g_HDRInput : register(t0);

// Soft threshold function (prevents hard cutoff artifacts)
float3 SoftThreshold(float3 color, float threshold, float softKnee)
{
    float brightness = Luminance(color);
    float soft = brightness - threshold + softKnee;
    soft = clamp(soft, 0.0f, 2.0f * softKnee);
    soft = soft * soft / (4.0f * softKnee + 0.00001f);
    
    float contribution = max(soft, brightness - threshold);
    contribution /= max(brightness, 0.00001f);
    
    return color * contribution;
}

// Quadratic threshold (Unreal style)
float3 QuadraticThreshold(float3 color, float threshold, float3 curve)
{
    // curve = (threshold - knee, knee * 2, 0.25 / knee)
    float brightness = Luminance(color);
    
    // Under threshold
    float rq = clamp(brightness - curve.x, 0.0f, curve.y);
    rq = curve.z * rq * rq;
    
    // Combine
    return color * max(rq, brightness - threshold) / max(brightness, 0.0001f);
}

float4 main(VSOutput input) : SV_TARGET
{
    float3 color = g_HDRInput.Sample(g_LinearClampSampler, input.TexCoord).rgb;
    
    // Precompute curve parameters
    float knee = g_BloomThreshold * 0.5f; // Soft knee
    float3 curve = float3(
        g_BloomThreshold - knee,
        knee * 2.0f,
        0.25f / max(knee, 0.00001f)
    );
    
    // Apply threshold with soft knee
    float3 bloomColor = QuadraticThreshold(color, g_BloomThreshold, curve);
    
    // Clamp to prevent extreme values
    bloomColor = min(bloomColor, float3(65504.0f, 65504.0f, 65504.0f));
    
    return float4(bloomColor, 1.0f);
}
