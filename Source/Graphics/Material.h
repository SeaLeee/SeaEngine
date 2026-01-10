#pragma once

#include "Core/Types.h"
#include "Graphics/Texture.h"
#include <DirectXMath.h>
#include <string>
#include <memory>

namespace Sea
{
    using namespace DirectX;

    // PBR 材质参数
    struct PBRMaterialParams
    {
        XMFLOAT4 albedo = { 1.0f, 1.0f, 1.0f, 1.0f };  // 基础颜色 (RGB) + Alpha
        float metallic = 0.0f;                          // 金属度 [0, 1]
        float roughness = 0.5f;                         // 粗糙度 [0, 1]
        float ao = 1.0f;                                // 环境遮蔽
        float emissiveIntensity = 0.0f;                 // 自发光强度
        XMFLOAT3 emissiveColor = { 0.0f, 0.0f, 0.0f }; // 自发光颜色
        float normalScale = 1.0f;                       // 法线强度
        float _padding[2];
    };

    // 材质贴图类型
    enum class PBRTextureType : u32
    {
        Albedo = 0,         // 基础颜色贴图 (sRGB)
        Normal,             // 法线贴图 (Linear)
        MetallicRoughness,  // 金属度(B) + 粗糙度(G) 合并贴图
        AO,                 // 环境遮蔽贴图
        Emissive,           // 自发光贴图 (sRGB)
        Height,             // 高度/位移贴图
        Count
    };

    // PBR 材质类
    class PBRMaterial : public NonCopyable
    {
    public:
        PBRMaterial(const std::string& name = "Default");
        ~PBRMaterial() = default;

        // 参数设置
        void SetAlbedo(const XMFLOAT4& albedo) { m_Params.albedo = albedo; m_Dirty = true; }
        void SetAlbedo(float r, float g, float b, float a = 1.0f) { m_Params.albedo = { r, g, b, a }; m_Dirty = true; }
        void SetMetallic(float metallic) { m_Params.metallic = metallic; m_Dirty = true; }
        void SetRoughness(float roughness) { m_Params.roughness = roughness; m_Dirty = true; }
        void SetAO(float ao) { m_Params.ao = ao; m_Dirty = true; }
        void SetEmissive(const XMFLOAT3& color, float intensity = 1.0f);
        void SetNormalScale(float scale) { m_Params.normalScale = scale; m_Dirty = true; }

        // 参数获取
        const XMFLOAT4& GetAlbedo() const { return m_Params.albedo; }
        float GetMetallic() const { return m_Params.metallic; }
        float GetRoughness() const { return m_Params.roughness; }
        float GetAO() const { return m_Params.ao; }
        const XMFLOAT3& GetEmissiveColor() const { return m_Params.emissiveColor; }
        float GetEmissiveIntensity() const { return m_Params.emissiveIntensity; }
        float GetNormalScale() const { return m_Params.normalScale; }
        const PBRMaterialParams& GetParams() const { return m_Params; }

        // 贴图设置
        void SetTexture(PBRTextureType type, Ref<Texture> texture);
        Ref<Texture> GetTexture(PBRTextureType type) const;
        bool HasTexture(PBRTextureType type) const;

        // 属性
        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& name) { m_Name = name; }
        bool IsDirty() const { return m_Dirty; }
        void ClearDirty() { m_Dirty = false; }

        // 预设材质
        static Ref<PBRMaterial> CreateDefault();
        static Ref<PBRMaterial> CreateMetal(float roughness = 0.3f);
        static Ref<PBRMaterial> CreatePlastic(const XMFLOAT3& color, float roughness = 0.4f);
        static Ref<PBRMaterial> CreateGold();
        static Ref<PBRMaterial> CreateSilver();
        static Ref<PBRMaterial> CreateCopper();
        static Ref<PBRMaterial> CreateRustedMetal();
        static Ref<PBRMaterial> CreateCeramic(const XMFLOAT3& color);
        static Ref<PBRMaterial> CreateGlass();

    private:
        std::string m_Name;
        PBRMaterialParams m_Params;
        Ref<Texture> m_Textures[static_cast<u32>(PBRTextureType::Count)];
        bool m_Dirty = true;
    };

    // GPU 上传用的材质常量缓冲区结构
    struct alignas(16) PBRMaterialConstants
    {
        XMFLOAT4 albedo;                    // 16 bytes
        float metallic;                      // 4 bytes
        float roughness;                     // 4 bytes
        float ao;                            // 4 bytes
        float emissiveIntensity;             // 4 bytes
        XMFLOAT3 emissiveColor;              // 12 bytes
        float normalScale;                   // 4 bytes
        u32 textureFlags;                    // 4 bytes (位标记哪些贴图有效)
        float _padding[3];                   // 12 bytes padding
    };

    static_assert(sizeof(PBRMaterialConstants) % 16 == 0, "PBRMaterialConstants must be 16-byte aligned");
}
