// OceanFFT_PS.hlsl
// Pixel shader for FFT-based ocean rendering
// Features: GGX specular, subsurface scattering, foam, Fresnel
// Based on GodotOceanWaves water.gdshader
// Reference: https://github.com/2Retr0/GodotOceanWaves

// ============================================================================
// Constants
// ============================================================================
static const float PI = 3.14159265358979323846f;
static const float REFLECTANCE = 0.02f; // Reflectance from air to water (eta=1.33)

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
    float4 g_WaterColor;      // Default: (0.1, 0.15, 0.18)
    float4 g_FoamColor;       // Default: (0.73, 0.67, 0.62)
    float g_Roughness;        // Default: 0.4
    float g_NormalStrength;   // Default: 1.0
    uint g_NumCascades;
    float _padding1;
    float4 g_MapScales[4];    // [uv_scale.x, uv_scale.y, displacement_scale, normal_scale]
}

// ============================================================================
// Textures
// ============================================================================
Texture2DArray<float4> g_DisplacementMaps : register(t0);
Texture2DArray<float4> g_NormalFoamMaps : register(t1);  // Format: (gradient.x, gradient.y, dhx_dx, foam)
TextureCube<float4> g_EnvironmentMap : register(t2);

SamplerState g_LinearWrap : register(s0);
SamplerState g_LinearClamp : register(s1);

// ============================================================================
// Input
// ============================================================================
struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float2 texcoord : TEXCOORD1;
    float3 normal : TEXCOORD2;
    float waveHeight : TEXCOORD3;
};

// ============================================================================
// Smith Masking-Shadowing (from GodotOceanWaves)
// ============================================================================
float SmithMaskingShadowing(float cosTheta, float alpha)
{
    // Approximate: 1.0 / (alpha * tan(acos(cos_theta)))
    float sinTheta = sqrt(max(1.0f - cosTheta * cosTheta, 0.0001f));
    float a = cosTheta / (alpha * sinTheta + 0.0001f);
    float a_sq = a * a;
    return (a < 1.6f) ? (1.0f - 1.259f * a + 0.396f * a_sq) / (3.535f * a + 2.181f * a_sq) : 0.0f;
}

// ============================================================================
// GGX Distribution (from GodotOceanWaves)
// ============================================================================
float GGXDistribution(float cosTheta, float alpha)
{
    float a_sq = alpha * alpha;
    float d = 1.0f + (a_sq - 1.0f) * cosTheta * cosTheta;
    return a_sq / (PI * d * d);
}

// ============================================================================
// Fresnel (from GodotOceanWaves)
// ============================================================================
float WaterFresnel(float NdotV, float roughness)
{
    // This matches GodotOceanWaves' fresnel calculation
    float power = 5.0f * exp(-2.69f * roughness);
    float base = pow(1.0f - NdotV, power);
    float denom = 1.0f + 22.7f * pow(roughness, 1.5f);
    return lerp(base / denom, 1.0f, REFLECTANCE);
}

// ============================================================================
// Main Pixel Shader (matches GodotOceanWaves water.gdshader)
// ============================================================================
float4 main(PSInput input) : SV_TARGET
{
    float3 worldPos = input.worldPos;
    float3 V = normalize(g_CameraPos - worldPos);
    float3 L = normalize(-g_SunDirection);
    float dist = length(worldPos.xz - g_CameraPos.xz);
    
    // ========== Sample gradient/foam from all cascades ==========
    // Normal map format: (gradient.x, gradient.y, dhx_dx, foam)
    // This matches GodotOceanWaves' fft_unpack.glsl output
    float3 gradient = float3(0.0f, 0.0f, 0.0f);
    
    for (uint i = 0; i < g_NumCascades && i < 4; ++i)
    {
        float2 uvScale = g_MapScales[i].xy;
        float normalScale = g_MapScales[i].w;
        
        float2 uv = worldPos.xz * uvScale;
        
        // Sample normal/foam map: (gradient.x, gradient.y, dhx_dx, foam)
        float4 normalFoam = g_NormalFoamMaps.SampleLevel(g_LinearWrap, float3(uv, (float)i), 0);
        
        // Accumulate gradients weighted by normal scale
        gradient += float3(normalFoam.xy * normalScale, normalFoam.w);
    }
    
    // ========== Foam calculation (from GodotOceanWaves) ==========
    float foamFactor = smoothstep(0.0f, 1.0f, gradient.z * 0.75f) * exp(-dist * 0.0075f);
    
    // ========== Base albedo (blend water/foam color) ==========
    float3 waterColor = g_WaterColor.rgb;
    float3 foamColor = g_FoamColor.rgb;
    float3 albedo = lerp(waterColor, foamColor, foamFactor);
    
    // ========== Calculate normal from gradient (GodotOceanWaves style) ==========
    // Blend normal strength with distance
    float normalBlend = lerp(0.015f, g_NormalStrength, exp(-dist * 0.0175f));
    float2 normalGrad = gradient.xy * normalBlend;
    float3 N = normalize(float3(-normalGrad.x, 1.0f, -normalGrad.y));
    
    // ========== Fresnel (GodotOceanWaves style) ==========
    float NdotV = max(dot(N, V), 0.00002f);
    float fresnel = WaterFresnel(NdotV, g_Roughness);
    
    // Roughness increases with foam/fog
    float roughness = (1.0f - fresnel) * foamFactor + 0.4f;
    
    // ========== Lighting (GodotOceanWaves style) ==========
    float3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.00002f);
    float NdotH = dot(N, H);
    
    // Smith masking-shadowing
    float lightMask = SmithMaskingShadowing(NdotV, roughness);
    float viewMask = SmithMaskingShadowing(NdotL, roughness);
    float microfacetDistribution = GGXDistribution(NdotH, roughness);
    float geometricAttenuation = 1.0f / (1.0f + lightMask + viewMask);
    
    // Specular (from GodotOceanWaves)
    float3 sunColor = float3(1.0f, 0.95f, 0.9f) * g_SunIntensity;
    float3 specular = fresnel * microfacetDistribution * geometricAttenuation / (4.0f * NdotV + 0.1f) * sunColor;
    
    // ========== Subsurface Scattering (from GodotOceanWaves) ==========
    // SSS modifier produces a 'greener' color
    float3 sssModifier = float3(0.9f, 1.15f, 0.85f);
    
    float waveHeight = input.waveHeight;
    
    // Height-based SSS: thin wave crests let light through
    float sssHeight = 1.0f * max(0.0f, waveHeight + 2.5f) 
                    * pow(max(dot(L, -V), 0.0f), 4.0f) 
                    * pow(0.5f - 0.5f * dot(L, N), 3.0f);
    
    // Near-view SSS
    float sssNear = 0.5f * pow(NdotV, 2.0f);
    
    // Lambertian diffuse
    float lambertian = 0.5f * NdotL;
    
    // Combine diffuse components
    float3 diffuse = lerp(
        (sssHeight + sssNear) * sssModifier / (1.0f + lightMask) + lambertian, 
        foamColor, 
        foamFactor
    ) * (1.0f - fresnel) * sunColor;
    
    // ========== Environment Reflection ==========
    float3 R = reflect(-V, N);
    float3 envColor = float3(0.4f, 0.6f, 0.9f); // Sky blue fallback
    float3 reflection = envColor * fresnel * 0.3f;
    
    // ========== Final Color ==========
    float3 finalColor = albedo * 0.1f + diffuse + specular + reflection;
    
    // Tone mapping will be applied later
    return float4(finalColor, 1.0f);
}
