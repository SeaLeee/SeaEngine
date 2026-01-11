// Bloom Composite Pass - Final compositing with scene
// Unreal Engine style bloom with tint and intensity control

#include "Bloom_Common.hlsli"

Texture2D<float4> g_Scene : register(t0);    // Original HDR scene
Texture2D<float4> g_Bloom : register(t1);    // Final blurred bloom

float4 main(VSOutput input) : SV_TARGET
{
    // Sample original scene
    float3 sceneColor = g_Scene.Sample(g_LinearClampSampler, input.TexCoord).rgb;
    
    // Sample bloom result
    float3 bloomColor = g_Bloom.Sample(g_LinearClampSampler, input.TexCoord).rgb;
    
    // Apply bloom tint (Unreal style - allows colored bloom)
    float3 bloomTint = float3(g_BloomTint_R, g_BloomTint_G, g_BloomTint_B);
    bloomColor *= bloomTint;
    
    // Apply intensity
    bloomColor *= g_BloomIntensity;
    
    // Additive blend (standard bloom compositing)
    float3 finalColor = sceneColor + bloomColor;
    
    return float4(finalColor, 1.0f);
}
