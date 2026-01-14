// OceanFFT_VS.hlsl
// Vertex shader for FFT-based ocean rendering
// Samples displacement maps from multiple cascades

// ============================================================================
// Constant Buffer
// ============================================================================
cbuffer OceanConstants : register(b0)
{
    float4x4 g_ViewProj;
    float4x4 g_World;
    float3 g_CameraPos;
    float g_Time;
    float3 g_SunDirection;
    float g_SunIntensity;
    float4 g_WaterColor;
    float4 g_FoamColor;
    float g_Roughness;
    float g_NormalStrength;
    uint g_NumCascades;
    float _padding1;
    // Map scales: [uvScale.x, uvScale.y, dispScale, normalScale]
    float4 g_MapScales[4];
}

// ============================================================================
// Textures
// ============================================================================
Texture2DArray<float4> g_DisplacementMaps : register(t0);
SamplerState g_LinearWrap : register(s0);

// ============================================================================
// Vertex Input/Output
// ============================================================================
struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float2 texcoord : TEXCOORD1;
    float3 normal : TEXCOORD2;
    float waveHeight : TEXCOORD3;  // Pass wave height for SSS calculation
};

// ============================================================================
// Main Vertex Shader (matches GodotOceanWaves vertex displacement)
// ============================================================================
VSOutput main(VSInput input)
{
    VSOutput output;
    
    // World position of base mesh
    float3 worldPos = mul(float4(input.position, 1.0f), g_World).xyz;
    
    // Distance-based displacement falloff (from GodotOceanWaves)
    float distFromCamera = length(worldPos.xz - g_CameraPos.xz);
    float distanceFactor = min(exp(-(distFromCamera - 150.0f) * 0.007f), 1.0f);
    
    // Sample displacement from all cascades
    float3 totalDisplacement = float3(0.0f, 0.0f, 0.0f);
    
    for (uint i = 0; i < g_NumCascades && i < 4; ++i)
    {
        float2 uvScale = g_MapScales[i].xy;
        float dispScale = g_MapScales[i].z;
        
        // Calculate UV for this cascade
        float2 uv = worldPos.xz * uvScale;
        
        // Sample displacement
        float4 disp = g_DisplacementMaps.SampleLevel(g_LinearWrap, float3(uv, (float)i), 0);
        
        // Accumulate displacement
        totalDisplacement += disp.xyz * dispScale;
    }
    
    // Apply displacement with distance falloff
    worldPos += totalDisplacement * distanceFactor;
    
    // Pass wave height for SSS
    output.waveHeight = totalDisplacement.y;
    
    // Transform to clip space
    output.position = mul(float4(worldPos, 1.0f), g_ViewProj);
    output.worldPos = worldPos;
    output.texcoord = input.texcoord;
    
    // Normal will be computed in pixel shader from normal maps
    output.normal = float3(0.0f, 1.0f, 0.0f);
    
    return output;
}
