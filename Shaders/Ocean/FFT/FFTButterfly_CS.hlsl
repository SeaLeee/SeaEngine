// FFTButterfly_CS.hlsl
// Precomputes butterfly factors for Stockham FFT
// Only needs to run once when map size changes
// Based on GodotOceanWaves fft_butterfly.glsl

#include "OceanCommon.hlsli"

// ============================================================================
// Constant Buffer
// ============================================================================
cbuffer ButterflyConstants : register(b0)
{
    uint g_MapSize;            // FFT size (power of 2)
    uint g_LogN;               // log2(MapSize)
    float2 _padding;
}

// ============================================================================
// Resources
// ============================================================================

// Output: Butterfly factors
// Layout: [stage][element]
// Each element is: (twiddle.re, twiddle.im, index0, index1)
RWStructuredBuffer<float4> g_ButterflyFactors : register(u0);

// ============================================================================
// Main Compute Shader
// ============================================================================
[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint stage = DTid.x / g_MapSize;
    uint element = DTid.x % g_MapSize;
    
    if (stage >= g_LogN)
        return;
    
    // Stockham FFT: no bit-reversal needed!
    // For stage s (0-indexed), butterfly width = 2^(s+1)
    uint butterflyWidth = 1u << (stage + 1);
    uint halfWidth = butterflyWidth >> 1;
    
    // Which butterfly pair are we in?
    uint butterflyIndex = element / halfWidth;
    uint localIndex = element % halfWidth;
    
    // Is this the top or bottom of the butterfly?
    bool isTop = (localIndex < halfWidth);
    
    // Input indices for this stage
    // In Stockham, we read from even/odd pattern
    uint baseIndex = butterflyIndex * butterflyWidth;
    uint index0 = baseIndex + localIndex;
    uint index1 = baseIndex + localIndex + halfWidth;
    
    // Twiddle factor: W_N^k = exp(-2πik/N)
    // k = localIndex for this butterfly
    float twiddleExponent = -TAU * float(localIndex) / float(butterflyWidth);
    Complex twiddle = ComplexExp(twiddleExponent);
    
    // Store butterfly factor
    uint outputIndex = stage * g_MapSize + element;
    g_ButterflyFactors[outputIndex] = float4(
        twiddle.re,
        twiddle.im,
        asfloat(index0),
        asfloat(index1)
    );
}
