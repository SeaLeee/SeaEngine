// PBR (Physically Based Rendering) Shader
// PBR.hlsl
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
    uint TextureFlags;  // 位标记: bit0=Albedo, bit1=Normal, bit2=MetallicRoughness, bit3=AO, bit4=Emissive
    float3 _Padding3;
};

// 贴图
Texture2D AlbedoTexture : register(t0);
Texture2D NormalTexture : register(t1);
Texture2D MetallicRoughnessTexture : register(t2);  // G=Roughness, B=Metallic (glTF 格式)
Texture2D AOTexture : register(t3);
Texture2D EmissiveTexture : register(t4);

// 采样器
SamplerState LinearSampler : register(s0);

// 顶点输入
struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float4 Tangent : TANGENT;
    float4 Color : COLOR0;
};

// 简化版顶点输入 (不需要切线)
struct VSInputSimple
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
    float3 Tangent : TEXCOORD3;
    float3 Bitangent : TEXCOORD4;
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
    
    // 变换切线
    float3 tangent = normalize(mul(float4(input.Tangent.xyz, 0.0), World).xyz);
    output.Tangent = tangent;
    
    // 计算副切线 (考虑手性)
    output.Bitangent = cross(output.Normal, tangent) * input.Tangent.w;
    
    output.TexCoord = input.TexCoord;
    output.Color = input.Color;
    
    return output;
}

// 像素着色器
float4 PSMain(PSInput input) : SV_TARGET
{
    // 采样或使用默认值
    float4 albedo = BaseColor * input.Color;
    float metallic = Metallic;
    float roughness = Roughness;
    float ao = AO;
    float3 emissive = EmissiveColor * EmissiveIntensity;
    float3 N = normalize(input.Normal);
    
    // Albedo 贴图
    if (TextureFlags & 0x01)
    {
        float4 texAlbedo = AlbedoTexture.Sample(LinearSampler, input.TexCoord);
        texAlbedo.rgb = SRGBToLinear(texAlbedo.rgb);  // 转到线性空间
        albedo *= texAlbedo;
    }
    
    // 法线贴图
    if (TextureFlags & 0x02)
    {
        float3 tangentNormal = NormalTexture.Sample(LinearSampler, input.TexCoord).xyz;
        tangentNormal = UnpackNormal(tangentNormal, NormalScale);
        N = TransformNormalToWorld(tangentNormal, normalize(input.Normal), 
                                   normalize(input.Tangent), normalize(input.Bitangent));
    }
    
    // MetallicRoughness 贴图 (glTF 格式: G=Roughness, B=Metallic)
    if (TextureFlags & 0x04)
    {
        float4 mr = MetallicRoughnessTexture.Sample(LinearSampler, input.TexCoord);
        roughness *= mr.g;
        metallic *= mr.b;
    }
    
    // AO 贴图
    if (TextureFlags & 0x08)
    {
        ao *= AOTexture.Sample(LinearSampler, input.TexCoord).r;
    }
    
    // Emissive 贴图
    if (TextureFlags & 0x10)
    {
        float3 texEmissive = EmissiveTexture.Sample(LinearSampler, input.TexCoord).rgb;
        texEmissive = SRGBToLinear(texEmissive);
        emissive *= texEmissive;
    }
    
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
    
    // 环境光 (简化 IBL)
    float3 ambientLighting = AmbientIBL(
        albedo.rgb,
        metallic,
        roughness,
        ao,
        N, V,
        AmbientColor
    );
    
    // 最终颜色
    float3 color = directLighting + ambientLighting + emissive;
    
    // 色调映射 (ACES)
    color = ACESFilm(color);
    
    // Gamma 校正
    color = LinearToGamma(color);
    
    return float4(color, albedo.a);
}

// 简化版顶点着色器 (没有切线输入时使用)
PSInput VSMainSimple(VSInputSimple input)
{
    PSInput output;
    
    float4 worldPos = mul(float4(input.Position, 1.0), World);
    output.WorldPos = worldPos.xyz;
    output.Position = mul(worldPos, ViewProjection);
    output.Normal = normalize(mul(float4(input.Normal, 0.0), WorldInvTranspose).xyz);
    output.TexCoord = input.TexCoord;
    output.Color = input.Color;
    
    // 生成切线空间 (使用法线生成)
    float3 up = abs(output.Normal.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
    output.Tangent = normalize(cross(up, output.Normal));
    output.Bitangent = cross(output.Normal, output.Tangent);
    
    return output;
}
