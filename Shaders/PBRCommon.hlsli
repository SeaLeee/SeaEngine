// PBR Common utilities
// PBRCommon.hlsli

#ifndef PBR_COMMON_HLSLI
#define PBR_COMMON_HLSLI

// Constants
static const float PI = 3.14159265359f;
static const float INV_PI = 0.31830988618f;
static const float EPSILON = 1e-6f;
static const float MIN_ROUGHNESS = 0.04f;

// Fresnel-Schlick 近似
// F0: 垂直入射时的反射率
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

// 带粗糙度的 Fresnel-Schlick (用于 IBL)
float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float invRoughness = 1.0 - roughness;
    return F0 + (max(float3(invRoughness, invRoughness, invRoughness), F0) - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

// GGX/Trowbridge-Reitz 法线分布函数 (NDF)
// 描述微表面法线分布
float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;
    
    float nom = a2;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = PI * denom * denom;
    
    return nom / max(denom, EPSILON);
}

// Schlick-GGX 几何遮蔽函数
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;  // 直接光照的 k 值
    
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return nom / max(denom, EPSILON);
}

// Smith 几何函数
// 同时考虑视线和光线的遮蔽
float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// IBL 用的几何函数 (k 值不同)
float GeometrySchlickGGX_IBL(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0;  // IBL 的 k 值
    
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return nom / max(denom, EPSILON);
}

float GeometrySmith_IBL(float NdotV, float NdotL, float roughness)
{
    float ggx1 = GeometrySchlickGGX_IBL(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX_IBL(NdotL, roughness);
    return ggx1 * ggx2;
}

// Cook-Torrance BRDF
// 返回镜面反射分量
float3 CookTorranceBRDF(float3 N, float3 V, float3 L, float3 F0, float roughness)
{
    float3 H = normalize(V + L);
    
    float NdotH = max(dot(N, H), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float HdotV = max(dot(H, V), 0.0);
    
    // 各项
    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    float3 F = FresnelSchlick(HdotV, F0);
    
    // Cook-Torrance 镜面项
    float3 nom = D * G * F;
    float denom = 4.0 * NdotV * NdotL + EPSILON;
    float3 specular = nom / denom;
    
    return specular;
}

// 完整的 PBR 光照计算
// albedo: 基础颜色
// metallic: 金属度
// roughness: 粗糙度
// N: 法线
// V: 视线方向
// L: 光线方向
// lightColor: 光源颜色
// lightIntensity: 光源强度
float3 PBRDirectLighting(
    float3 albedo,
    float metallic,
    float roughness,
    float3 N,
    float3 V,
    float3 L,
    float3 lightColor,
    float lightIntensity)
{
    // 确保粗糙度不会太低
    roughness = max(roughness, MIN_ROUGHNESS);
    
    // 计算 F0 (反射率)
    // 非金属使用 0.04，金属使用 albedo 颜色
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    
    float3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float HdotV = max(dot(H, V), 0.0);
    
    // Fresnel
    float3 F = FresnelSchlick(HdotV, F0);
    
    // 漫反射系数 (金属没有漫反射)
    float3 kD = (1.0 - F) * (1.0 - metallic);
    
    // 漫反射 (Lambertian)
    float3 diffuse = kD * albedo * INV_PI;
    
    // 镜面反射 (Cook-Torrance)
    float3 specular = CookTorranceBRDF(N, V, L, F0, roughness);
    
    // 最终结果
    float3 radiance = lightColor * lightIntensity;
    return (diffuse + specular) * radiance * NdotL;
}

// 简化的环境光 IBL (无环境贴图时使用)
// 使用球谐函数近似天空光
float3 AmbientIBL(
    float3 albedo,
    float metallic,
    float roughness,
    float ao,
    float3 N,
    float3 V,
    float3 ambientColor)
{
    roughness = max(roughness, MIN_ROUGHNESS);
    
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float NdotV = max(dot(N, V), 0.0);
    
    float3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kD = (1.0 - F) * (1.0 - metallic);
    
    // 简化的漫反射 IBL
    float3 diffuseIBL = kD * albedo * ambientColor;
    
    // 简化的镜面 IBL (使用环境色的亮度)
    float3 R = reflect(-V, N);
    float mipLevel = roughness * 4.0;  // 模拟 mip 级别
    float3 envColor = ambientColor * (1.0 - roughness * 0.5);
    float3 specularIBL = F * envColor;
    
    return (diffuseIBL + specularIBL) * ao;
}

// 法线贴图解码
float3 UnpackNormal(float3 packedNormal, float normalScale)
{
    float3 n = packedNormal * 2.0 - 1.0;
    n.xy *= normalScale;
    return normalize(n);
}

// TBN 矩阵变换法线到世界空间
float3 TransformNormalToWorld(float3 tangentNormal, float3 worldNormal, float3 worldTangent, float3 worldBitangent)
{
    float3x3 TBN = float3x3(worldTangent, worldBitangent, worldNormal);
    return normalize(mul(tangentNormal, TBN));
}

// 色调映射 - ACES (Academy Color Encoding System)
float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// Reinhard 色调映射
float3 Reinhard(float3 color)
{
    return color / (color + 1.0);
}

// 曝光调整
float3 Exposure(float3 color, float exposure)
{
    return color * pow(2.0, exposure);
}

// Gamma 校正
float3 LinearToGamma(float3 color)
{
    return pow(max(color, 0.0), 1.0 / 2.2);
}

float3 GammaToLinear(float3 color)
{
    return pow(max(color, 0.0), 2.2);
}

// sRGB 精确转换
float3 LinearToSRGB(float3 color)
{
    float3 result;
    result.r = color.r <= 0.0031308 ? 12.92 * color.r : 1.055 * pow(color.r, 1.0 / 2.4) - 0.055;
    result.g = color.g <= 0.0031308 ? 12.92 * color.g : 1.055 * pow(color.g, 1.0 / 2.4) - 0.055;
    result.b = color.b <= 0.0031308 ? 12.92 * color.b : 1.055 * pow(color.b, 1.0 / 2.4) - 0.055;
    return result;
}

float3 SRGBToLinear(float3 color)
{
    float3 result;
    result.r = color.r <= 0.04045 ? color.r / 12.92 : pow((color.r + 0.055) / 1.055, 2.4);
    result.g = color.g <= 0.04045 ? color.g / 12.92 : pow((color.g + 0.055) / 1.055, 2.4);
    result.b = color.b <= 0.04045 ? color.b / 12.92 : pow((color.b + 0.055) / 1.055, 2.4);
    return result;
}

#endif // PBR_COMMON_HLSLI
