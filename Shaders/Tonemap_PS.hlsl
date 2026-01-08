// Tonemap Pass Pixel Shader
// Converts HDR to LDR with various tonemapping operators

#include "Common.hlsli"

Texture2D<float4> g_HDRBuffer : register(t0);
SamplerState g_LinearSampler : register(s0);

cbuffer TonemapConstants : register(b1)
{
    float g_Exposure;
    float g_Gamma;
    int g_TonemapOperator;  // 0: Reinhard, 1: ACES, 2: Uncharted2
    float g_Padding;
}

// Reinhard tonemapping
float3 TonemapReinhard(float3 color)
{
    return color / (color + 1.0);
}

// ACES Filmic tonemapping
float3 TonemapACES(float3 color)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

// Uncharted 2 tonemapping
float3 Uncharted2Tonemap(float3 x)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 TonemapUncharted2(float3 color)
{
    float W = 11.2;
    float3 curr = Uncharted2Tonemap(color * 2.0);
    float3 whiteScale = 1.0 / Uncharted2Tonemap(float3(W, W, W));
    return curr * whiteScale;
}

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET
{
    float3 hdrColor = g_HDRBuffer.Sample(g_LinearSampler, input.TexCoord).rgb;
    
    // Apply exposure
    hdrColor *= g_Exposure;
    
    // Apply tonemapping
    float3 ldrColor;
    switch (g_TonemapOperator)
    {
        case 0:
            ldrColor = TonemapReinhard(hdrColor);
            break;
        case 1:
            ldrColor = TonemapACES(hdrColor);
            break;
        case 2:
            ldrColor = TonemapUncharted2(hdrColor);
            break;
        default:
            ldrColor = TonemapACES(hdrColor);
            break;
    }
    
    // Apply gamma correction
    ldrColor = pow(ldrColor, 1.0 / g_Gamma);
    
    return float4(ldrColor, 1.0);
}
