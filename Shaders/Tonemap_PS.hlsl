// Tonemap Pass Pixel Shader
// Converts HDR to LDR with various tonemapping operators
// Implements Unreal Engine style ACES tonemapping

#include "Common.hlsli"

Texture2D<float4> g_HDRBuffer : register(t0);
Texture2D<float4> g_BloomBuffer : register(t1);
SamplerState g_LinearSampler : register(s0);

cbuffer TonemapConstants : register(b1)
{
    float g_Exposure;
    float g_Gamma;
    int g_TonemapOperator;  // 0: ACES (Unreal), 1: Reinhard, 2: Uncharted2, 3: GT, 4: None
    float g_BloomIntensity;
    
    float g_BloomTintR;
    float g_BloomTintG;
    float g_BloomTintB;
    float g_BloomEnabled;
}

// ============================================================================
// ACES Tonemapping - Unreal Engine Style (Narkowicz 2015 Fitted Curve)
// This is the widely-used approximation that matches UE4's look
// ============================================================================

// ACES Filmic Tone Mapping Curve (Stephen Hill's fit)
// This curve closely matches UE4's default tonemapper
float3 ACESFitted(float3 color)
{
    // sRGB => ACES (AP0) input transform (simplified)
    static const float3x3 ACESInputMat = {
        {0.59719f, 0.35458f, 0.04823f},
        {0.07600f, 0.90834f, 0.01566f},
        {0.02840f, 0.13383f, 0.83777f}
    };
    
    // ACES (AP0) => sRGB output transform (simplified)  
    static const float3x3 ACESOutputMat = {
        { 1.60475f, -0.53108f, -0.07367f},
        {-0.10208f,  1.10813f, -0.00605f},
        {-0.00327f, -0.07276f,  1.07602f}
    };
    
    color = mul(ACESInputMat, color);
    
    // RRT and ODT fit (attempt at matching UE4)
    float3 a = color * (color + 0.0245786f) - 0.000090537f;
    float3 b = color * (0.983729f * color + 0.4329510f) + 0.238081f;
    color = a / b;
    
    color = mul(ACESOutputMat, color);
    
    return saturate(color);
}

// Simpler ACES approximation (Krzysztof Narkowicz)
// Very close to UE4's look, computationally cheaper
float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// ============================================================================
// Alternative: GT Tonemapper (Gran Turismo style, very similar to UE4)
// This is often preferred for games due to better color saturation
// ============================================================================

float3 GTTonemap(float3 x)
{
    float P = 1.0;   // max brightness
    float a = 1.0;   // contrast
    float m = 0.22;  // linear section start
    float l = 0.4;   // linear section length
    float c = 1.33;  // black tightness
    float b = 0.0;   // pedestal
    
    float3 result;
    
    // Calculate for each channel
    float l0 = ((P - m) * l) / a;
    float L0 = m - m / a;
    float L1 = m + (1.0 - m) / a;
    float S0 = m + l0;
    float S1 = m + a * l0;
    float C2 = (a * P) / (P - S1);
    float CP = -C2 / P;
    
    float3 w0 = 1.0 - smoothstep(0.0, m, x);
    float3 w2 = step(m + l0, x);
    float3 w1 = 1.0 - w0 - w2;
    
    float3 T = m * pow(x / m, c) + b;
    float3 S = P - (P - S1) * exp(CP * (x - S0));
    float3 L = m + a * (x - m);
    
    result = T * w0 + L * w1 + S * w2;
    return result;
}

// ============================================================================
// Reinhard Extended (better than simple Reinhard)
// ============================================================================

float3 TonemapReinhardExtended(float3 color, float maxWhite)
{
    float3 numerator = color * (1.0 + color / (maxWhite * maxWhite));
    return numerator / (1.0 + color);
}

// Simple Reinhard
float3 TonemapReinhard(float3 color)
{
    return color / (color + 1.0);
}

// ============================================================================
// Uncharted 2 Tonemapping (Filmic)
// ============================================================================

float3 Uncharted2Tonemap(float3 x)
{
    float A = 0.15;  // Shoulder Strength
    float B = 0.50;  // Linear Strength
    float C = 0.10;  // Linear Angle
    float D = 0.20;  // Toe Strength
    float E = 0.02;  // Toe Numerator
    float F = 0.30;  // Toe Denominator
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 TonemapUncharted2(float3 color)
{
    float W = 11.2;  // Linear White Point Value
    float exposureBias = 2.0;
    float3 curr = Uncharted2Tonemap(color * exposureBias);
    float3 whiteScale = 1.0 / Uncharted2Tonemap(float3(W, W, W));
    return curr * whiteScale;
}

// ============================================================================
// Main Shader
// ============================================================================

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET
{
    float3 hdrColor = g_HDRBuffer.Sample(g_LinearSampler, input.TexCoord).rgb;
    
    // Add bloom if enabled
    if (g_BloomEnabled > 0.5f)
    {
        float3 bloomColor = g_BloomBuffer.Sample(g_LinearSampler, input.TexCoord).rgb;
        float3 bloomTint = float3(g_BloomTintR, g_BloomTintG, g_BloomTintB);
        hdrColor += bloomColor * bloomTint * g_BloomIntensity;
    }
    
    // Apply exposure
    hdrColor *= g_Exposure;
    
    // Apply tonemapping
    float3 ldrColor;
    switch (g_TonemapOperator)
    {
        case 0:  // ACES (Unreal Engine style) - uses simple fitted curve
            ldrColor = ACESFilm(hdrColor);
            break;
        case 1:  // Reinhard Extended
            ldrColor = TonemapReinhardExtended(hdrColor, 4.0);
            break;
        case 2:  // Uncharted 2 (Filmic)
            ldrColor = TonemapUncharted2(hdrColor);
            break;
        case 3:  // GT Tonemap (Gran Turismo style)
            ldrColor = GTTonemap(hdrColor);
            break;
        case 4:  // None (linear clamp)
            ldrColor = saturate(hdrColor);
            break;
        default:
            ldrColor = ACESFilm(hdrColor);
            break;
    }
    
    // Apply gamma correction (sRGB approximation)
    // Note: If output is already sRGB, this converts linear to sRGB gamma
    ldrColor = pow(abs(ldrColor), 1.0 / g_Gamma);
    
    return float4(ldrColor, 1.0);
}
