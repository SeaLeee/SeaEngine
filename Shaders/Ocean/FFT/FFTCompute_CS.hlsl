// FFTCompute_CS.hlsl
// Stockham FFT compute shader - performs one row of 1D FFT
// Run horizontally, transpose, run again for 2D FFT
// Based on GodotOceanWaves fft_compute.glsl

#include "OceanCommon.hlsli"

// ============================================================================
// Constant Buffer
// ============================================================================
cbuffer FFTConstants : register(b0)
{
    uint g_Stage;              // Current FFT stage (0 to log2(N)-1)
    uint g_Direction;          // 0 = forward (DFT), 1 = inverse (IDFT)
    uint g_SpectrumIndex;      // Which spectrum we're processing
    uint g_CascadeIndex;       // Which cascade
    uint g_MapSize;            // FFT size
    uint g_LogN;               // log2(MapSize)
    uint g_PingPong;           // 0 or 1 for buffer selection
    float _padding;
}

// ============================================================================
// Resources
// ============================================================================

// Butterfly lookup table
StructuredBuffer<float4> g_ButterflyFactors : register(t0);

// Ping-pong FFT buffers (read from one, write to other)
RWStructuredBuffer<float2> g_FFTInput : register(u0);
RWStructuredBuffer<float2> g_FFTOutput : register(u1);

// ============================================================================
// Shared Memory for Row FFT
// ============================================================================
groupshared Complex g_SharedData[512];  // Max 512 elements per row

// ============================================================================
// Helper Functions
// ============================================================================

uint GetBufferIndex(uint cascade, uint spectrum, uint row, uint col)
{
    // Layout: [cascade][spectrum][row][col]
    return cascade * 4 * g_MapSize * g_MapSize 
         + spectrum * g_MapSize * g_MapSize 
         + row * g_MapSize 
         + col;
}

// ============================================================================
// Main: Process one row
// ============================================================================
[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    uint row = Gid.x;           // Which row we're processing
    uint col = GTid.x;          // Column index within row
    
    if (row >= g_MapSize || col >= g_MapSize)
        return;
    
    // ========== Load data into shared memory ==========
    uint inputIdx = GetBufferIndex(g_CascadeIndex, g_SpectrumIndex, row, col);
    
    float2 loaded = g_FFTInput[inputIdx];
    g_SharedData[col] = UnpackComplex(loaded);
    
    GroupMemoryBarrierWithGroupSync();
    
    // ========== Perform all FFT stages in shared memory ==========
    [unroll]
    for (uint stage = 0; stage < g_LogN; ++stage)
    {
        uint butterflyWidth = 1u << (stage + 1);
        uint halfWidth = butterflyWidth >> 1;
        
        // Which butterfly are we in?
        uint butterflyIndex = col / butterflyWidth;
        uint localIndex = col % butterflyWidth;
        
        // Indices within butterfly
        uint idx0 = butterflyIndex * butterflyWidth + (localIndex % halfWidth);
        uint idx1 = idx0 + halfWidth;
        
        // Twiddle factor
        float angle = -TAU * float(localIndex % halfWidth) / float(butterflyWidth);
        if (g_Direction == 1) angle = -angle;  // IDFT
        
        Complex twiddle = ComplexExp(angle);
        
        // Load butterfly pair
        Complex a = g_SharedData[idx0];
        Complex b = g_SharedData[idx1];
        
        GroupMemoryBarrierWithGroupSync();
        
        // Butterfly operation: out0 = a + W*b, out1 = a - W*b
        Complex wb = ComplexMul(twiddle, b);
        
        if (localIndex < halfWidth)
        {
            g_SharedData[col] = ComplexAdd(a, wb);
        }
        else
        {
            g_SharedData[col] = ComplexSub(a, wb);
        }
        
        GroupMemoryBarrierWithGroupSync();
    }
    
    // ========== Write result ==========
    Complex result = g_SharedData[col];
    
    // Scale for inverse FFT
    if (g_Direction == 1)
    {
        result = ComplexMulReal(result, 1.0f / float(g_MapSize));
    }
    
    uint outputIdx = GetBufferIndex(g_CascadeIndex, g_SpectrumIndex, row, col);
    g_FFTOutput[outputIdx] = PackComplex(result);
}
