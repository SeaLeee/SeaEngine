// Bloom Composite Pass - Final compositing with scene
// Unreal Engine style bloom with tint and intensity control
// Note: This simplified version only outputs bloom (composite is done differently)

#include "Bloom_Common.hlsli"

Texture2D<float4> g_Bloom : register(t0);    // Final blurred bloom

float4 main(VSOutput input) : SV_TARGET
{
    // Sample bloom result
    float3 bloomColor = g_Bloom.Sample(g_LinearClampSampler, input.TexCoord).rgb;
    
    // Apply bloom tint (Unreal style - allows colored bloom)
    float3 bloomTint = float3(g_BloomTint_R, g_BloomTint_G, g_BloomTint_B);
    bloomColor *= bloomTint;
    
    // Apply intensity
    bloomColor *= g_BloomIntensity;
    
    return float4(bloomColor, 1.0f);
}
