// FFTUnpack_CS.hlsl
// Unpacks FFT results to displacement and normal maps
// Matches GodotOceanWaves fft_unpack.glsl EXACTLY

#include "OceanCommon.hlsli"

// ============================================================================
// Constant Buffer
// ============================================================================
cbuffer UnpackConstants : register(b0)
{
    uint g_MapSize;            // Texture size
    uint g_CascadeIndex;       // Which cascade
    float g_Whitecap;          // Foam threshold (Jacobian)
    float g_FoamGrowRate;      // Foam growth rate
    float g_FoamDecayRate;     // Foam decay rate
    float3 _padding;
}

// ============================================================================
// Resources
// ============================================================================

// FFT buffer with computed IFFTs (packed format from modulate stage)
// After FFT:
// [0].x = hx (displacement X), [0].y = hy (displacement Y / height)
// [1].x = hz (displacement Z), [1].y = dhy_dx
// [2].x = dhy_dz, [2].y = dhx_dx
// [3].x = dhz_dz, [3].y = dhz_dx
// Using RWStructuredBuffer so we can use the same UAV heap as other compute shaders
RWStructuredBuffer<float2> g_FFTBuffer : register(u0);

// Output displacement map (RGB = XYZ displacement)
RWTexture2DArray<float4> g_DisplacementMap : register(u1);

// Output normal/foam map: Format = (gradient.x, gradient.y, dhx_dx, foam)
RWTexture2DArray<float4> g_NormalFoamMap : register(u2);

// ============================================================================
// Helper Functions
// ============================================================================

// We access FFT buffer at offset of NUM_SPECTRA*map_size*map_size because
// FFT does not transpose a second time (matching GodotOceanWaves)
uint GetFFTIndex(uint cascade, uint spectrum, uint2 coord)
{
    return cascade * g_MapSize * g_MapSize * 4 * 2 
         + 4 * g_MapSize * g_MapSize  // Offset to second half (post-FFT)
         + spectrum * g_MapSize * g_MapSize 
         + coord.y * g_MapSize 
         + coord.x;
}

// ============================================================================
// Main Compute Shader (matches GodotOceanWaves fft_unpack.glsl)
// ============================================================================
[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 coord = DTid.xy;
    
    if (any(coord >= g_MapSize))
        return;
    
    // Sign shift for ifftshift: (-1)^(x+y)
    float signShift = ((coord.x & 1) ^ (coord.y & 1)) ? -1.0f : 1.0f;
    
    // ========== Read FFT results (packed format) ==========
    float2 data0 = g_FFTBuffer[GetFFTIndex(g_CascadeIndex, 0, coord)];
    float2 data1 = g_FFTBuffer[GetFFTIndex(g_CascadeIndex, 1, coord)];
    float2 data2 = g_FFTBuffer[GetFFTIndex(g_CascadeIndex, 2, coord)];
    float2 data3 = g_FFTBuffer[GetFFTIndex(g_CascadeIndex, 3, coord)];
    
    // Unpack with sign shift
    float hx = data0.x * signShift;      // Displacement X
    float hy = data0.y * signShift;      // Displacement Y (height)
    float hz = data1.x * signShift;      // Displacement Z
    float dhy_dx = data1.y * signShift;  // Height gradient X
    float dhy_dz = data2.x * signShift;  // Height gradient Z
    float dhx_dx = data2.y * signShift;  // For Jacobian
    float dhz_dz = data3.x * signShift;  // For Jacobian
    float dhz_dx = data3.y * signShift;  // For Jacobian
    
    // ========== Write displacement ==========
    float3 displacement = float3(hx, hy, hz);
    g_DisplacementMap[uint3(coord, g_CascadeIndex)] = float4(displacement, 0.0f);
    
    // ========== Calculate Jacobian for foam ==========
    // J = (1 + dhx/dx)(1 + dhz/dz) - (dhz/dx)^2
    float jacobian = (1.0f + dhx_dx) * (1.0f + dhz_dz) - dhz_dx * dhz_dx;
    
    // Foam appears where Jacobian is negative (wave folding)
    float foamFactor = -min(0.0f, jacobian - g_Whitecap);
    
    // Simple foam calculation (no temporal accumulation for now)
    float foam = saturate(foamFactor * g_FoamGrowRate);
    
    // ========== Calculate gradient (matches GodotOceanWaves) ==========
    // gradient = (dhy/dx, dhy/dz) / (1 + abs(dhx/dx, dhz/dz))
    float2 gradient = float2(dhy_dx, dhy_dz) / (1.0f + abs(float2(dhx_dx, dhz_dz)));
    
    // ========== Write normal/foam map ==========
    // Format: (gradient.x, gradient.y, dhx_dx, foam)
    // This is used directly in the pixel shader for lighting
    g_NormalFoamMap[uint3(coord, g_CascadeIndex)] = float4(gradient, dhx_dx, foam);
}
