#pragma once
#include "Graphics/GraphicsTypes.h"
#include "Core/Types.h"

namespace Sea
{
    class Device;

    struct RootParameterDesc
    {
        enum Type { Constants, CBV, SRV, UAV, DescriptorTable } type;
        u32 shaderRegister = 0, registerSpace = 0, num32BitValues = 0;
        D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL;
    };

    struct RootSignatureDesc
    {
        std::vector<RootParameterDesc> parameters;
        std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;
        D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    };

    class RootSignature : public NonCopyable
    {
    public:
        RootSignature(Device& device, const RootSignatureDesc& desc);
        ~RootSignature();

        bool Initialize();
        ID3D12RootSignature* GetRootSignature() const { return m_RootSignature.Get(); }

    private:
        Device& m_Device;
        RootSignatureDesc m_Desc;
        ComPtr<ID3D12RootSignature> m_RootSignature;
    };
}
