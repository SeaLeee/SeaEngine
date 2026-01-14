// FFTTranspose_CS.hlsl
// Matrix transpose for 2D FFT
// Run FFT on rows, transpose, run FFT on "rows" again = column FFT
// Based on GodotOceanWaves transpose.glsl

#include "OceanCommon.hlsli"

// ============================================================================
// Constant Buffer
// ============================================================================
cbuffer TransposeConstants : register(b0)
{
    uint g_MapSize;            // Matrix size (N x N)
    uint g_SpectrumIndex;      // Which spectrum to transpose
    uint g_CascadeIndex;       // Which cascade
    float _padding;
}

// ============================================================================
// Resources
// ============================================================================

// In-place transpose using two buffers
RWStructuredBuffer<float2> g_Input : register(u0);
RWStructuredBuffer<float2> g_Output : register(u1);

// ============================================================================
// Shared Memory Tile
// ============================================================================
#define TILE_SIZE 16
groupshared float2 g_Tile[TILE_SIZE][TILE_SIZE + 1]; // +1 to avoid bank conflicts

// ============================================================================
// Main: Tiled transpose
// ============================================================================
[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    uint baseOffset = g_CascadeIndex * 4 * g_MapSize * g_MapSize 
                    + g_SpectrumIndex * g_MapSize * g_MapSize;
    
    // Source coordinates
    uint2 srcCoord = Gid.xy * TILE_SIZE + GTid.xy;
    
    // Load tile into shared memory
    if (srcCoord.x < g_MapSize && srcCoord.y < g_MapSize)
    {
        uint srcIdx = baseOffset + srcCoord.y * g_MapSize + srcCoord.x;
        g_Tile[GTid.y][GTid.x] = g_Input[srcIdx];
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    // Destination coordinates (transposed)
    uint2 dstCoord;
    dstCoord.x = Gid.y * TILE_SIZE + GTid.x;
    dstCoord.y = Gid.x * TILE_SIZE + GTid.y;
    
    // Write transposed data
    if (dstCoord.x < g_MapSize && dstCoord.y < g_MapSize)
    {
        uint dstIdx = baseOffset + dstCoord.y * g_MapSize + dstCoord.x;
        // Note: Read from transposed position in shared memory
        g_Output[dstIdx] = g_Tile[GTid.x][GTid.y];
    }
}
