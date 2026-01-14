// OceanModulate_CS.hlsl
// Time-modulates spectrum and prepares for FFT
// Matches GodotOceanWaves spectrum_modulate.glsl EXACTLY
// Real-valued outputs are packed in pairs for efficiency

#include "OceanCommon.hlsli"

// ============================================================================
// Constant Buffer
// ============================================================================
cbuffer ModulateConstants : register(b0)
{
    float2 g_TileLength;       // Tile length (meters)
    float g_Depth;             // Water depth (meters)
    float g_Time;              // Current time (seconds)
    uint g_CascadeIndex;       // Current cascade index
    float3 _padding;
}

// ============================================================================
// Resources
// ============================================================================

// Input: Initial spectrum h0(k) packed as (h0.xy, conj(h0(-k)))
Texture2DArray<float4> g_InputSpectrum : register(t0);

// Output: Modulated spectra for FFT (packed pairs)
// Layout matches GodotOceanWaves exactly:
// [0] = (hx.x - hy.y, hx.y + hy.x) - packed displacement X+Y
// [1] = (hz.x - dhy_dx.y, hz.y + dhy_dx.x) - packed disp Z + gradient
// [2] = (dhy_dz.x - dhx_dx.y, dhy_dz.y + dhx_dx.x) - packed gradients
// [3] = (dhz_dz.x - dhz_dx.y, dhz_dz.y + dhz_dx.x) - packed Jacobian terms
RWStructuredBuffer<float2> g_FFTBuffer : register(u0);

// ============================================================================
// Helpers
// ============================================================================
uint GetFFTIndex(uint2 coord, uint spectrumIndex)
{
    return spectrumIndex * MAP_SIZE * MAP_SIZE + coord.y * MAP_SIZE + coord.x;
}

// ============================================================================
// Main Compute Shader (matches GodotOceanWaves spectrum_modulate.glsl)
// ============================================================================
[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 pixelCoord = DTid.xy;
    
    if (any(pixelCoord >= MAP_SIZE))
        return;
    
    // Get wave vector k = (id - dims/2) * 2π / tile_length
    float2 k_vec = ((float2)pixelCoord - (float)MAP_SIZE * 0.5f) * 2.0f * PI / g_TileLength;
    float k = length(k_vec) + 1e-6f;
    float2 k_unit = k_vec / k;
    
    // Load h0 spectrum - format: (h0.xy, conj(h0(-k)))
    float4 h0_packed = g_InputSpectrum[uint3(pixelCoord, g_CascadeIndex)];
    float2 h0_k = h0_packed.xy;
    float2 h0_neg_conj = h0_packed.zw;
    
    // Time modulation: h(k,t) = h0(k) * exp(iωt) + h0*(-k) * exp(-iωt)
    float dispersion = DispersionRelation(k, g_Depth) * g_Time;
    float2 modulation = float2(cos(dispersion), sin(dispersion));
    float2 modulation_conj = float2(modulation.x, -modulation.y);
    
    // Complex multiplication for modulation
    float2 h = float2(
        h0_k.x * modulation.x - h0_k.y * modulation.y + h0_neg_conj.x * modulation_conj.x - h0_neg_conj.y * modulation_conj.y,
        h0_k.x * modulation.y + h0_k.y * modulation.x + h0_neg_conj.x * modulation_conj.y + h0_neg_conj.y * modulation_conj.x
    );
    
    // h_inv is used to simplify complex multiplication with i
    // i * h = (-h.y, h.x)
    float2 h_inv = float2(-h.y, h.x);
    
    // ========== Wave displacement calculation ==========
    // hx = -i * (kx/|k|) * h = h_inv * k_unit.x  (but GodotOceanWaves uses .y here due to coordinate swap)
    float2 hx = h_inv * k_unit.y;  // Matches: mul_complex(vec2(0, -k_unit.x), h)
    float2 hy = h;                  // Height displacement
    float2 hz = h_inv * k_unit.x;  // Matches: mul_complex(vec2(0, -k_unit.z), h)
    
    // ========== Wave gradient calculation ==========
    // dhy/dx = i * kx * h
    float2 dhy_dx = h_inv * k_vec.y;  // Note: GodotOceanWaves uses swapped indices
    float2 dhy_dz = h_inv * k_vec.x;
    
    // Jacobian partial derivatives
    float2 dhx_dx = -h * k_vec.y * k_unit.y;
    float2 dhz_dz = -h * k_vec.x * k_unit.x;
    float2 dhz_dx = -h * k_vec.y * k_unit.x;
    
    // ========== Pack outputs for FFT ==========
    // Because h respects the complex conjugation property (output will be real),
    // we can pack two waves into one complex number
    uint baseIdx = g_CascadeIndex * 4 * MAP_SIZE * MAP_SIZE;
    
    // Spectrum 0: packed (hx, hy) -> (hx.x - hy.y, hx.y + hy.x)
    // After FFT, real part = hx, imag part = hy
    g_FFTBuffer[baseIdx + GetFFTIndex(pixelCoord, 0)] = float2(hx.x - hy.y, hx.y + hy.x);
    
    // Spectrum 1: packed (hz, dhy_dx)
    g_FFTBuffer[baseIdx + GetFFTIndex(pixelCoord, 1)] = float2(hz.x - dhy_dx.y, hz.y + dhy_dx.x);
    
    // Spectrum 2: packed (dhy_dz, dhx_dx)
    g_FFTBuffer[baseIdx + GetFFTIndex(pixelCoord, 2)] = float2(dhy_dz.x - dhx_dx.y, dhy_dz.y + dhx_dx.x);
    
    // Spectrum 3: packed (dhz_dz, dhz_dx) - for Jacobian calculation
    g_FFTBuffer[baseIdx + GetFFTIndex(pixelCoord, 3)] = float2(dhz_dz.x - dhz_dx.y, dhz_dz.y + dhz_dx.x);
}
