// Ocean Common - 海洋模拟公共定义
#ifndef OCEAN_COMMON_HLSLI
#define OCEAN_COMMON_HLSLI

#define PI 3.14159265359f
#define G 9.81f  // 重力加速度

// 海洋参数
cbuffer OceanParams : register(b0)
{
    float4 g_WindDirection;     // xy = wind dir, z = wind speed, w = amplitude
    float4 g_TimeParams;        // x = time, y = choppiness, z = grid size, w = patch size
    uint4 g_Resolution;         // x = N (FFT size), y = log2(N), zw = unused
};

// 复数运算
struct Complex
{
    float real;
    float imag;
};

Complex ComplexMake(float r, float i)
{
    Complex c;
    c.real = r;
    c.imag = i;
    return c;
}

Complex ComplexAdd(Complex a, Complex b)
{
    return ComplexMake(a.real + b.real, a.imag + b.imag);
}

Complex ComplexSub(Complex a, Complex b)
{
    return ComplexMake(a.real - b.real, a.imag - b.imag);
}

Complex ComplexMul(Complex a, Complex b)
{
    return ComplexMake(a.real * b.real - a.imag * b.imag, 
                       a.real * b.imag + a.imag * b.real);
}

Complex ComplexConj(Complex c)
{
    return ComplexMake(c.real, -c.imag);
}

Complex ComplexExp(float theta)
{
    return ComplexMake(cos(theta), sin(theta));
}

// Phillips 频谱
float Phillips(float2 k, float2 windDir, float windSpeed, float amplitude)
{
    float kLen = length(k);
    if (kLen < 0.0001f) return 0.0f;
    
    float L = windSpeed * windSpeed / G;  // 最大波长
    float kLen2 = kLen * kLen;
    float kLen4 = kLen2 * kLen2;
    float kL2 = kLen2 * L * L;
    
    float2 kNorm = k / kLen;
    float kDotW = dot(kNorm, windDir);
    float kDotW2 = kDotW * kDotW;
    
    // 抑制垂直于风向的波
    float dirDamp = kDotW2;
    
    // 抑制小波长
    float l = L / 1000.0f;
    float suppression = exp(-kLen2 * l * l);
    
    float phillips = amplitude * exp(-1.0f / kL2) / kLen4 * dirDamp * suppression;
    return phillips;
}

// Box-Muller 变换生成高斯随机数
float2 GaussianRandom(float2 uv, float seed)
{
    float u1 = frac(sin(dot(uv + seed, float2(12.9898, 78.233))) * 43758.5453);
    float u2 = frac(sin(dot(uv + seed, float2(93.9898, 67.345))) * 24634.6345);
    
    u1 = max(u1, 0.0001f);
    
    float r = sqrt(-2.0f * log(u1));
    float theta = 2.0f * PI * u2;
    
    return float2(r * cos(theta), r * sin(theta));
}

#endif // OCEAN_COMMON_HLSLI
