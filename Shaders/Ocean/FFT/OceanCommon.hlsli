// OceanCommon.hlsli
// Common functions for FFT-based ocean simulation
// Based on Jerry Tessendorf's "Simulating Ocean Water"
//         and GodotOceanWaves by 2Retr0

#ifndef OCEAN_COMMON_HLSLI
#define OCEAN_COMMON_HLSLI

// ============================================================================
// Constants
// ============================================================================
static const float PI = 3.14159265358979323846f;
static const float TAU = 2.0f * PI;
static const float GRAVITY = 9.81f;

static const uint MAP_SIZE = 256;  // Must match CPU-side value
static const uint LOG2_MAP_SIZE = 8;
static const uint NUM_CASCADES = 4;

// ============================================================================
// Complex Number Operations
// ============================================================================
struct Complex
{
    float re;
    float im;
};

Complex ComplexNew(float re, float im)
{
    Complex c;
    c.re = re;
    c.im = im;
    return c;
}

Complex ComplexAdd(Complex a, Complex b)
{
    return ComplexNew(a.re + b.re, a.im + b.im);
}

Complex ComplexSub(Complex a, Complex b)
{
    return ComplexNew(a.re - b.re, a.im - b.im);
}

Complex ComplexMul(Complex a, Complex b)
{
    return ComplexNew(
        a.re * b.re - a.im * b.im,
        a.re * b.im + a.im * b.re
    );
}

Complex ComplexMulReal(Complex a, float s)
{
    return ComplexNew(a.re * s, a.im * s);
}

Complex ComplexConj(Complex a)
{
    return ComplexNew(a.re, -a.im);
}

Complex ComplexExp(float theta)
{
    float s, c;
    sincos(theta, s, c);
    return ComplexNew(c, s);
}

// ============================================================================
// Random Number Generation
// ============================================================================

// PCG hash for quality randomness
uint PCGHash(uint input)
{
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// Hash-based random for reproducible results
float HashRandom(uint seed)
{
    uint hash = PCGHash(seed);
    return float(hash) / float(0xFFFFFFFFu);
}

// Box-Muller for Gaussian distribution
float2 BoxMuller(float2 u)
{
    float r = sqrt(-2.0f * log(max(u.x, 1e-8f)));
    float theta = TAU * u.y;
    float s, c;
    sincos(theta, s, c);
    return float2(r * c, r * s);
}

// Get two Gaussian random numbers from pixel coordinate
float2 GaussianRandom(uint2 pixelCoord, uint2 seedOffset)
{
    uint seed1 = PCGHash(pixelCoord.x + seedOffset.x * 65536u + pixelCoord.y * 4096u + seedOffset.y);
    uint seed2 = PCGHash(seed1);
    float u1 = HashRandom(seed1);
    float u2 = HashRandom(seed2);
    return BoxMuller(float2(u1, u2));
}

// ============================================================================
// Dispersion Relation
// ============================================================================

// Dispersion relation: ω = sqrt(g * k * tanh(k * depth))
float DispersionRelation(float k, float depth)
{
    return sqrt(GRAVITY * k * tanh(k * depth));
}

// Deep water approximation: ω = sqrt(g * k)
float DispersionDeepWater(float k)
{
    return sqrt(GRAVITY * k);
}

// ============================================================================
// Wave Vector Helpers
// ============================================================================

// Get wave vector k from pixel coordinates
float2 GetWaveVector(uint2 pixelCoord, float tileLength)
{
    float2 k;
    // 映射到 [-N/2, N/2) 范围
    int halfN = (int)MAP_SIZE / 2;
    int nx = ((int)pixelCoord.x < halfN) ? (int)pixelCoord.x : (int)pixelCoord.x - (int)MAP_SIZE;
    int ny = ((int)pixelCoord.y < halfN) ? (int)pixelCoord.y : (int)pixelCoord.y - (int)MAP_SIZE;
    
    k.x = TAU * (float)nx / tileLength;
    k.y = TAU * (float)ny / tileLength;
    return k;
}

float2 GetWaveVectorXY(uint2 pixelCoord, float tileLengthX, float tileLengthY)
{
    float2 k;
    int halfN = (int)MAP_SIZE / 2;
    int nx = ((int)pixelCoord.x < halfN) ? (int)pixelCoord.x : (int)pixelCoord.x - (int)MAP_SIZE;
    int ny = ((int)pixelCoord.y < halfN) ? (int)pixelCoord.y : (int)pixelCoord.y - (int)MAP_SIZE;
    
    k.x = TAU * (float)nx / tileLengthX;
    k.y = TAU * (float)ny / tileLengthY;
    return k;
}

// ============================================================================
// JONSWAP/TMA Spectrum
// ============================================================================

// Standard JONSWAP shape parameter gamma
static const float JONSWAP_GAMMA = 3.3f;

// Pierson-Moskowitz spectrum S(ω)
float PiersonMoskowitz(float omega, float peakFrequency)
{
    float omegaP = peakFrequency;
    float omegaRatio = omegaP / omega;
    float omegaRatio4 = omegaRatio * omegaRatio * omegaRatio * omegaRatio;
    
    return 0.0081f * GRAVITY * GRAVITY / pow(omega, 5.0f) 
           * exp(-1.25f * omegaRatio4);
}

// JONSWAP spectrum enhancement
float JONSWAPEnhancement(float omega, float peakFrequency)
{
    float sigma = (omega <= peakFrequency) ? 0.07f : 0.09f;
    float omegaP = peakFrequency;
    
    float x = (omega - omegaP) / (sigma * omegaP);
    return pow(JONSWAP_GAMMA, exp(-0.5f * x * x));
}

// Full JONSWAP spectrum
float JONSWAPSpectrum(float omega, float alpha, float peakFrequency)
{
    if (omega <= 0.0001f) return 0.0f;
    
    float pm = PiersonMoskowitz(omega, peakFrequency);
    float enhancement = JONSWAPEnhancement(omega, peakFrequency);
    
    // 用 alpha 替代标准 0.0081
    return alpha / 0.0081f * pm * enhancement;
}

// TMA depth attenuation (Kitaigorodskii)
float TMADepthAttenuation(float omega, float depth)
{
    float k = omega * omega / GRAVITY; // 近似
    float kh = k * depth;
    
    if (kh > 1.0f)
    {
        return 1.0f;
    }
    else if (kh > 0.5f)
    {
        float t = kh - 0.5f;
        return 0.5f + 0.5f * t;
    }
    else
    {
        return 0.5f * kh * kh;
    }
}

// ============================================================================
// Directional Spread
// ============================================================================

// Hasselmann et al. (1980) directional spread
// D(θ) = cos^(2s)(θ/2) / normalization
float DirectionalSpread(float theta, float omega, float peakFrequency, float swell, float spread)
{
    // 计算频率相关的幂次 s
    float omegaRatio = omega / peakFrequency;
    float s;
    if (omegaRatio <= 1.0f)
    {
        s = 6.97f * pow(omegaRatio, 4.06f);
    }
    else
    {
        s = 9.77f * pow(omegaRatio, -2.33f - 1.45f * (omegaRatio - 1.17f));
    }
    
    // Swell 调整
    s = max(s * (1.0f + swell * 16.0f), 1.0f);
    
    // Spread 调整
    s = lerp(s, 0.5f, spread);
    
    // cos^(2s)(θ/2)
    float halfAngle = theta * 0.5f;
    float cosHalf = cos(halfAngle);
    
    return pow(max(cosHalf, 0.0f), 2.0f * s);
}

// ============================================================================
// Phillips Spectrum (Alternative)
// ============================================================================
float PhillipsSpectrum(float2 k, float2 windDir, float windSpeed, float detail)
{
    float kLen = length(k);
    if (kLen < 0.0001f) return 0.0f;
    
    float2 kNorm = k / kLen;
    
    // L = V^2 / g (最大波长)
    float L = windSpeed * windSpeed / GRAVITY;
    float kL = kLen * L;
    float kL2 = kL * kL;
    
    // 方向性
    float kDotW = dot(kNorm, windDir);
    float kDotW2 = kDotW * kDotW;
    
    // 抑制逆风波
    if (kDotW < 0.0f) kDotW2 *= 0.25f;
    
    // Phillips spectrum
    float phillips = exp(-1.0f / kL2) / (kLen * kLen * kLen * kLen) * kDotW2;
    
    // 高频衰减 (detail)
    float l = L * 0.001f * detail;
    phillips *= exp(-kLen * kLen * l * l);
    
    return phillips;
}

// ============================================================================
// Pack/Unpack Helpers
// ============================================================================

// Pack complex to float2
float2 PackComplex(Complex c)
{
    return float2(c.re, c.im);
}

// Unpack float2 to complex
Complex UnpackComplex(float2 v)
{
    return ComplexNew(v.x, v.y);
}

// Pack normal to R8G8B8A8_UNORM
float4 PackNormal(float3 normal, float foam)
{
    return float4(normal * 0.5f + 0.5f, foam);
}

// Unpack normal from R8G8B8A8_UNORM
float3 UnpackNormal(float4 packed)
{
    return packed.xyz * 2.0f - 1.0f;
}

#endif // OCEAN_COMMON_HLSLI
