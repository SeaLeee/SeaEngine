// PBR Simple Shader - 不使用纹理的 PBR 渲染
// PBRSimple.hlsl
// Uses Cook-Torrance BRDF with GGX distribution

#include "PBRCommon.hlsli"

// 帧常量
cbuffer PerFrameData : register(b0)
{
    row_major float4x4 ViewProjection;
    row_major float4x4 View;
    row_major float4x4 Projection;
    float3 CameraPosition;
    float Time;
    float3 LightDirection;
    float _Padding1;
    float3 LightColor;
    float LightIntensity;
    float3 AmbientColor;
    float _Padding2;
};

// 物体常量
cbuffer PerObjectData : register(b1)
{
    row_major float4x4 World;
    row_major float4x4 WorldInvTranspose;
    float4 BaseColor;
    float Metallic;
    float Roughness;
    float AO;
    float EmissiveIntensity;
    float3 EmissiveColor;
    float NormalScale;
    uint TextureFlags;  // 位标记 (此简化版不使用纹理)
    float3 _Padding3;
};

// 顶点输入 (不需要切线)
struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float4 Color : COLOR0;
};

// 像素着色器输入
struct PSInput
{
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
    float4 Color : COLOR0;
};

// 顶点着色器
PSInput VSMain(VSInput input)
{
    PSInput output;
    
    float4 worldPos = mul(float4(input.Position, 1.0), World);
    output.WorldPos = worldPos.xyz;
    output.Position = mul(worldPos, ViewProjection);
    
    // 变换法线
    output.Normal = normalize(mul(float4(input.Normal, 0.0), WorldInvTranspose).xyz);
    output.TexCoord = input.TexCoord;
    output.Color = input.Color;
    
    return output;
}

// 像素着色器
float4 PSMain(PSInput input) : SV_TARGET
{
    // 使用材质参数 (无纹理)
    float4 albedo = BaseColor * input.Color;
    float metallic = Metallic;
    float roughness = max(Roughness, MIN_ROUGHNESS);
    float ao = AO;
    float3 emissive = EmissiveColor * EmissiveIntensity;
    float3 N = normalize(input.Normal);
    
    // 向量计算
    float3 V = normalize(CameraPosition - input.WorldPos);
    float3 L = normalize(-LightDirection);
    
    // 直接光照 (PBR)
    float3 directLighting = PBRDirectLighting(
        albedo.rgb,
        metallic,
        roughness,
        N, V, L,
        LightColor,
        LightIntensity
    );
    
    // 环境光照 (简单近似)
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo.rgb, metallic);
    float NdotV = max(dot(N, V), 0.0);
    float3 kS = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kD = (1.0 - kS) * (1.0 - metallic);
    
    float3 ambientDiffuse = kD * albedo.rgb * AmbientColor;
    float3 ambientSpecular = kS * AmbientColor * 0.3;  // 简化的镜面环境
    float3 ambientLighting = (ambientDiffuse + ambientSpecular) * ao;
    
    // 最终颜色
    float3 color = directLighting + ambientLighting + emissive;
    
    // 色调映射 (ACES)
    color = ACESFilm(color);
    
    // Gamma 校正
    color = LinearToGamma(color);
    
    return float4(color, albedo.a);
}
