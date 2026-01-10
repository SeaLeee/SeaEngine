// Ocean FFT - Cooley-Tukey FFT 算法
#include "OceanCommon.hlsli"

// 输入/输出纹理 (ping-pong)
RWTexture2D<float2> g_Input : register(u0);
RWTexture2D<float2> g_Output : register(u1);

cbuffer FFTParams : register(b1)
{
    uint g_Step;        // 当前FFT步骤
    uint g_Direction;   // 0 = 水平, 1 = 垂直
    uint g_IsInverse;   // 0 = 正向FFT, 1 = 逆向FFT
    uint g_Padding;
};

groupshared float2 sharedData[512];

[numthreads(256, 1, 1)]
void CSMain(uint3 GroupId : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    uint N = g_Resolution.x;
    uint threadIdx = GTid.x;
    uint batchIdx = GroupId.x;
    
    // 确定当前处理的行/列
    uint lineIdx = batchIdx;
    if (lineIdx >= N) return;
    
    // 位反转索引（仅在第一步）
    uint idx1 = threadIdx;
    uint idx2 = threadIdx + 256;
    
    // 从纹理加载数据
    uint2 coord1, coord2;
    if (g_Direction == 0) // 水平
    {
        coord1 = uint2(idx1, lineIdx);
        coord2 = uint2(idx2, lineIdx);
    }
    else // 垂直
    {
        coord1 = uint2(lineIdx, idx1);
        coord2 = uint2(lineIdx, idx2);
    }
    
    if (idx1 < N) sharedData[idx1] = g_Input[coord1];
    if (idx2 < N) sharedData[idx2] = g_Input[coord2];
    
    GroupMemoryBarrierWithGroupSync();
    
    // 执行FFT蝶形运算
    uint logN = g_Resolution.y;
    
    for (uint s = 0; s < logN; ++s)
    {
        uint m = 1u << (s + 1);
        uint mHalf = m >> 1;
        
        float angle = (g_IsInverse ? 1.0f : -1.0f) * 2.0f * PI / float(m);
        
        for (uint k = threadIdx; k < N / 2; k += 256)
        {
            uint j = k % mHalf;
            uint i = (k / mHalf) * m + j;
            
            Complex w = ComplexExp(angle * float(j));
            
            Complex even = ComplexMake(sharedData[i].x, sharedData[i].y);
            Complex odd = ComplexMake(sharedData[i + mHalf].x, sharedData[i + mHalf].y);
            Complex t = ComplexMul(w, odd);
            
            Complex res0 = ComplexAdd(even, t);
            Complex res1 = ComplexSub(even, t);
            
            sharedData[i] = float2(res0.real, res0.imag);
            sharedData[i + mHalf] = float2(res1.real, res1.imag);
        }
        
        GroupMemoryBarrierWithGroupSync();
    }
    
    // 逆FFT需要除以N
    float scale = g_IsInverse ? (1.0f / float(N)) : 1.0f;
    
    // 写回纹理
    if (idx1 < N) 
    {
        g_Output[coord1] = sharedData[idx1] * scale;
    }
    if (idx2 < N) 
    {
        g_Output[coord2] = sharedData[idx2] * scale;
    }
}
