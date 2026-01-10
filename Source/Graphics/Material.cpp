#include "Graphics/Material.h"

namespace Sea
{
    PBRMaterial::PBRMaterial(const std::string& name)
        : m_Name(name)
    {
    }

    void PBRMaterial::SetEmissive(const XMFLOAT3& color, float intensity)
    {
        m_Params.emissiveColor = color;
        m_Params.emissiveIntensity = intensity;
        m_Dirty = true;
    }

    void PBRMaterial::SetTexture(PBRTextureType type, Ref<Texture> texture)
    {
        u32 index = static_cast<u32>(type);
        if (index < static_cast<u32>(PBRTextureType::Count))
        {
            m_Textures[index] = texture;
            m_Dirty = true;
        }
    }

    Ref<Texture> PBRMaterial::GetTexture(PBRTextureType type) const
    {
        u32 index = static_cast<u32>(type);
        if (index < static_cast<u32>(PBRTextureType::Count))
        {
            return m_Textures[index];
        }
        return nullptr;
    }

    bool PBRMaterial::HasTexture(PBRTextureType type) const
    {
        u32 index = static_cast<u32>(type);
        if (index < static_cast<u32>(PBRTextureType::Count))
        {
            return m_Textures[index] != nullptr;
        }
        return false;
    }

    Ref<PBRMaterial> PBRMaterial::CreateDefault()
    {
        auto mat = MakeRef<PBRMaterial>("Default");
        mat->SetAlbedo(0.8f, 0.8f, 0.8f, 1.0f);
        mat->SetMetallic(0.0f);
        mat->SetRoughness(0.5f);
        return mat;
    }

    Ref<PBRMaterial> PBRMaterial::CreateMetal(float roughness)
    {
        auto mat = MakeRef<PBRMaterial>("Metal");
        mat->SetAlbedo(0.9f, 0.9f, 0.9f, 1.0f);
        mat->SetMetallic(1.0f);
        mat->SetRoughness(roughness);
        return mat;
    }

    Ref<PBRMaterial> PBRMaterial::CreatePlastic(const XMFLOAT3& color, float roughness)
    {
        auto mat = MakeRef<PBRMaterial>("Plastic");
        mat->SetAlbedo(color.x, color.y, color.z, 1.0f);
        mat->SetMetallic(0.0f);
        mat->SetRoughness(roughness);
        return mat;
    }

    Ref<PBRMaterial> PBRMaterial::CreateGold()
    {
        auto mat = MakeRef<PBRMaterial>("Gold");
        // 金色的 F0 值 (线性空间)
        mat->SetAlbedo(1.0f, 0.766f, 0.336f, 1.0f);
        mat->SetMetallic(1.0f);
        mat->SetRoughness(0.3f);
        return mat;
    }

    Ref<PBRMaterial> PBRMaterial::CreateSilver()
    {
        auto mat = MakeRef<PBRMaterial>("Silver");
        mat->SetAlbedo(0.972f, 0.960f, 0.915f, 1.0f);
        mat->SetMetallic(1.0f);
        mat->SetRoughness(0.2f);
        return mat;
    }

    Ref<PBRMaterial> PBRMaterial::CreateCopper()
    {
        auto mat = MakeRef<PBRMaterial>("Copper");
        mat->SetAlbedo(0.955f, 0.638f, 0.538f, 1.0f);
        mat->SetMetallic(1.0f);
        mat->SetRoughness(0.35f);
        return mat;
    }

    Ref<PBRMaterial> PBRMaterial::CreateRustedMetal()
    {
        auto mat = MakeRef<PBRMaterial>("Rusted Metal");
        mat->SetAlbedo(0.45f, 0.25f, 0.15f, 1.0f);
        mat->SetMetallic(0.6f);
        mat->SetRoughness(0.75f);
        return mat;
    }

    Ref<PBRMaterial> PBRMaterial::CreateCeramic(const XMFLOAT3& color)
    {
        auto mat = MakeRef<PBRMaterial>("Ceramic");
        mat->SetAlbedo(color.x, color.y, color.z, 1.0f);
        mat->SetMetallic(0.0f);
        mat->SetRoughness(0.15f);  // 光滑的陶瓷
        return mat;
    }

    Ref<PBRMaterial> PBRMaterial::CreateGlass()
    {
        auto mat = MakeRef<PBRMaterial>("Glass");
        mat->SetAlbedo(0.95f, 0.95f, 0.95f, 0.3f);  // 半透明
        mat->SetMetallic(0.0f);
        mat->SetRoughness(0.05f);  // 非常光滑
        return mat;
    }
}
