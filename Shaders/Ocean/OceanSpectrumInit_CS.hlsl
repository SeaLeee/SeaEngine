// Ocean Spectrum Init - 初始化 Phillips 频谱 h0(k)
#include "OceanCommon.hlsli"

RWTexture2D<float4> g_H0 : register(u0);         // 初始频谱 h0(k) - RG = h0, BA = h0*
RWTexture2D<float4> g_Omega : register(u1);      // 角频率 ω(k)

[numthreads(16, 16, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    uint N = g_Resolution.x;
    if (DTid.x >= N || DTid.y >= N) return;
    
    float patchSize = g_TimeParams.w;
    float amplitude = g_WindDirection.w;
    float windSpeed = g_WindDirection.z;
    float2 windDir = normalize(g_WindDirection.xy);
    
    // 计算波矢量 k
    float2 x = float2(DTid.xy) - float2(N, N) * 0.5f;
    float2 k = x * (2.0f * PI / patchSize);
    
    float kLen = length(k);
    
    // 生成高斯随机数
    float2 xi = GaussianRandom(float2(DTid.xy) / float(N), 1.0f);
    
    // Phillips 频谱
    float ph = sqrt(Phillips(k, windDir, windSpeed, amplitude) * 0.5f);
    
    // h0(k) = 1/sqrt(2) * (ξr + i*ξi) * sqrt(Ph(k))
    Complex h0 = ComplexMake(xi.x * ph, xi.y * ph);
    
    // h0*(-k) - 使用共轭对称性
    float2 xi2 = GaussianRandom(float2(N - DTid.x, N - DTid.y) / float(N), 1.0f);
    float2 kNeg = -k;
    float phNeg = sqrt(Phillips(kNeg, windDir, windSpeed, amplitude) * 0.5f);
    Complex h0Conj = ComplexMake(xi2.x * phNeg, -xi2.y * phNeg);
    
    // 角频率 ω(k) = sqrt(g * |k|)
    float omega = sqrt(G * kLen);
    
    g_H0[DTid.xy] = float4(h0.real, h0.imag, h0Conj.real, h0Conj.imag);
    g_Omega[DTid.xy] = float4(omega, 0, 0, 0);
}
