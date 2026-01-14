// OceanSpectrum_CS.hlsl
// Generates initial wave spectrum h0(k) using JONSWAP/TMA
// Matches GodotOceanWaves spectrum_compute.glsl EXACTLY

#include "OceanCommon.hlsli"

// ============================================================================
// Constant Buffer
// ============================================================================
cbuffer SpectrumConstants : register(b0)
{
    int2 g_Seed;               // Random seed for reproducibility
    float2 g_TileLength;       // Tile length (meters)
    float g_Alpha;             // JONSWAP alpha parameter
    float g_PeakFrequency;     // Peak angular frequency (rad/s)
    float g_WindSpeed;         // Wind speed (m/s)
    float g_WindAngle;         // Wind direction (radians)
    float g_Depth;             // Water depth (meters)
    float g_Swell;             // Swell parameter (wave elongation)
    float g_Detail;            // Detail parameter (high-freq suppression)
    float g_Spread;            // Spread parameter (directionality)
    uint g_CascadeIndex;       // Current cascade index
    float3 _padding;
}

// ============================================================================
// Resources
// ============================================================================

// Output: Initial spectrum - packed as (h0(k).xy, conj(h0(-k)).zw)
// Format: RGBA16F to store both h0(k) and h0(-k) conjugate
RWTexture2DArray<float4> g_OutputSpectrum : register(u0);

// ============================================================================
// Hash function (matches GodotOceanWaves)
// ============================================================================
float2 Hash(uint2 x)
{
    x = 1103515245U * ((x >> 1U) ^ x.yx);
    uint h32 = x.y + 374761393U + x.x * 3266489917U;
    h32 = 2246822519U * (h32 ^ (h32 >> 15));
    h32 = 3266489917U * (h32 ^ (h32 >> 13));
    uint n = h32 ^ (h32 >> 16);
    uint2 rz = uint2(n, n * 48271U);
    return float2((rz.xy >> 1) & uint2(0x7FFFFFFF, 0x7FFFFFFF)) / float(0x7FFFFFFF);
}

// Box-Muller transform: uniform -> Gaussian
float2 Gaussian(float2 x)
{
    float r = sqrt(-2.0f * log(max(x.x, 1e-10f)));
    float theta = TAU * x.y;
    return float2(r * cos(theta), r * sin(theta));
}

// ============================================================================
// Dispersion relation with derivative (matches GodotOceanWaves)
// ============================================================================
float2 DispersionRelationWithDerivative(float k, float depth)
{
    float a = k * depth;
    float b = tanh(a);
    float dr = sqrt(GRAVITY * k * b);
    float ddr = 0.5f * GRAVITY * (b + a * (1.0f - b * b)) / dr;
    return float2(dr, ddr);  // (omega, d_omega/dk)
}

// ============================================================================
// Longuet-Higgins normalization (matches GodotOceanWaves)
// ============================================================================
float LonguetHigginsNormalization(float s)
{
    float a = sqrt(s);
    return (s < 0.4f) 
        ? (0.5f / PI) + s * (0.220636f + s * (-0.109f + s * 0.090f)) 
        : rsqrt(PI) * (a * 0.5f + (1.0f / a) * 0.0625f);
}

float LonguetHigginsFunction(float s, float theta)
{
    return LonguetHigginsNormalization(s) * pow(abs(cos(theta * 0.5f)), 2.0f * s);
}

// ============================================================================
// Hasselmann directional spread (matches GodotOceanWaves)
// ============================================================================
float HasselmannDirectionalSpread(float omega, float omega_p, float windSpeed, float theta)
{
    float p = omega / omega_p;
    float s = (omega <= omega_p) 
        ? 6.97f * pow(abs(p), 4.06f) 
        : 9.77f * pow(abs(p), -2.33f - 1.45f * (windSpeed * omega_p / GRAVITY - 1.17f));
    float s_xi = 16.0f * tanh(omega_p / omega) * g_Swell * g_Swell;
    return LonguetHigginsFunction(s + s_xi, theta - g_WindAngle);
}

// ============================================================================
// TMA Spectrum (matches GodotOceanWaves)
// ============================================================================
float TMASpectrum(float omega, float omega_p, float alpha, float depth)
{
    const float beta = 1.25f;
    const float gamma = 3.3f;
    
    float sigma = (omega <= omega_p) ? 0.07f : 0.09f;
    float r = exp(-(omega - omega_p) * (omega - omega_p) / (2.0f * sigma * sigma * omega_p * omega_p));
    float jonswap = (alpha * GRAVITY * GRAVITY) / pow(omega, 5) * exp(-beta * pow(omega_p / omega, 4)) * pow(gamma, r);
    
    float omega_h = min(omega * sqrt(depth / GRAVITY), 2.0f);
    float tma_atten = (omega_h <= 1.0f) 
        ? 0.5f * omega_h * omega_h 
        : 1.0f - 0.5f * (2.0f - omega_h) * (2.0f - omega_h);
    
    return jonswap * tma_atten;
}

// ============================================================================
// Get spectrum amplitude (matches GodotOceanWaves get_spectrum_amplitude)
// ============================================================================
float2 GetSpectrumAmplitude(int2 id, int2 mapSize)
{
    float2 dk = TAU / g_TileLength;
    float2 k_vec = (float2(id) - float2(mapSize) * 0.5f) * dk;
    float k = length(k_vec) + 1e-6f;
    float theta = atan2(k_vec.x, k_vec.y);
    
    float2 disp = DispersionRelationWithDerivative(k, g_Depth);
    float omega = disp.x;
    float omega_norm = disp.y / k * dk.x * dk.y;
    
    float S = TMASpectrum(omega, g_PeakFrequency, g_Alpha, g_Depth);
    float D = lerp(0.5f / PI, HasselmannDirectionalSpread(omega, g_PeakFrequency, g_WindSpeed, theta), 1.0f - g_Spread)
            * exp(-(1.0f - g_Detail) * (1.0f - g_Detail) * k * k);
    
    return Gaussian(Hash(uint2(id + g_Seed))) * sqrt(2.0f * S * D * omega_norm);
}

// ============================================================================
// Main Compute Shader (matches GodotOceanWaves spectrum_compute.glsl)
// ============================================================================
[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    int2 pixelCoord = int2(DTid.xy);
    
    if (any(DTid.xy >= MAP_SIZE))
        return;
    
    int2 dims = int2(MAP_SIZE, MAP_SIZE);
    int2 id0 = pixelCoord;
    int2 id1 = ((-id0) % dims + dims) % dims;  // Wrap -k coordinate
    
    // Get spectrum amplitudes for k and -k
    float2 h0_k = GetSpectrumAmplitude(id0, dims);
    float2 h0_neg = GetSpectrumAmplitude(id1, dims);
    float2 h0_neg_conj = float2(h0_neg.x, -h0_neg.y);  // Complex conjugate
    
    // Pack both h0(k) and conj(h0(-k)) for use in modulation stage
    g_OutputSpectrum[uint3(DTid.xy, g_CascadeIndex)] = float4(h0_k, h0_neg_conj);
}
