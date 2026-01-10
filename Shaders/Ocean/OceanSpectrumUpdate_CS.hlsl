// Ocean Spectrum Update - 每帧更新频谱 h(k,t)
#include "OceanCommon.hlsli"

Texture2D<float4> g_H0 : register(t0);           // 初始频谱
Texture2D<float4> g_Omega : register(t1);        // 角频率

RWTexture2D<float4> g_Ht : register(u0);         // 时变频谱 h(k,t)
RWTexture2D<float2> g_DxDz : register(u1);       // 水平位移频谱 Dx, Dz
RWTexture2D<float2> g_DyDxy : register(u2);      // 垂直位移和Dxy

[numthreads(16, 16, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    uint N = g_Resolution.x;
    if (DTid.x >= N || DTid.y >= N) return;
    
    float time = g_TimeParams.x;
    float choppiness = g_TimeParams.y;
    float patchSize = g_TimeParams.w;
    
    // 读取初始频谱
    float4 h0Data = g_H0[DTid.xy];
    Complex h0 = ComplexMake(h0Data.x, h0Data.y);
    Complex h0Conj = ComplexMake(h0Data.z, h0Data.w);
    
    float omega = g_Omega[DTid.xy].x;
    
    // 计算 h(k,t) = h0(k) * exp(i*ω*t) + h0*(-k) * exp(-i*ω*t)
    float phase = omega * time;
    Complex expPos = ComplexExp(phase);
    Complex expNeg = ComplexExp(-phase);
    
    Complex ht = ComplexAdd(ComplexMul(h0, expPos), ComplexMul(h0Conj, expNeg));
    
    // 计算波矢量
    float2 x = float2(DTid.xy) - float2(N, N) * 0.5f;
    float2 k = x * (2.0f * PI / patchSize);
    float kLen = length(k);
    
    // 避免除零
    float2 kNorm = (kLen > 0.0001f) ? (k / kLen) : float2(0, 0);
    
    // 位移频谱
    // Dx(k,t) = -i * kx/|k| * h(k,t) (choppiness控制水平位移)
    // Dy(k,t) = h(k,t)
    // Dz(k,t) = -i * kz/|k| * h(k,t)
    
    // -i * ht = (-i) * (ht.real + i*ht.imag) = ht.imag - i*ht.real
    Complex minusIHt = ComplexMake(ht.imag, -ht.real);
    
    Complex Dx = ComplexMake(minusIHt.real * kNorm.x * choppiness, 
                             minusIHt.imag * kNorm.x * choppiness);
    Complex Dz = ComplexMake(minusIHt.real * kNorm.y * choppiness, 
                             minusIHt.imag * kNorm.y * choppiness);
    
    g_Ht[DTid.xy] = float4(ht.real, ht.imag, 0, 0);
    g_DxDz[DTid.xy] = float2(Dx.real, Dz.real);  // 只存实部用于后续FFT
    g_DyDxy[DTid.xy] = float2(ht.real, Dx.imag); // Dy 和额外数据
}
