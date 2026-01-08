#include "Graphics/RootSignature.h"
#include "Graphics/Device.h"
#include "Core/Log.h"

namespace Sea
{
    RootSignature::RootSignature(Device& device, const RootSignatureDesc& desc) : m_Device(device), m_Desc(desc) {}
    RootSignature::~RootSignature() { m_RootSignature.Reset(); }

    bool RootSignature::Initialize()
    {
        std::vector<D3D12_ROOT_PARAMETER> params(m_Desc.parameters.size());
        std::vector<D3D12_DESCRIPTOR_RANGE> ranges(m_Desc.parameters.size());

        for (size_t i = 0; i < m_Desc.parameters.size(); ++i)
        {
            auto& p = m_Desc.parameters[i];
            params[i].ShaderVisibility = p.visibility;

            if (p.type == RootParameterDesc::Constants)
            {
                params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
                params[i].Constants = { p.shaderRegister, p.registerSpace, p.num32BitValues };
            }
            else if (p.type == RootParameterDesc::DescriptorTable)
            {
                ranges[i] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, p.shaderRegister, p.registerSpace, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
                params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                params[i].DescriptorTable = { 1, &ranges[i] };
            }
            else
            {
                params[i].ParameterType = static_cast<D3D12_ROOT_PARAMETER_TYPE>(p.type - 1);
                params[i].Descriptor = { p.shaderRegister, p.registerSpace };
            }
        }

        D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
        rootSigDesc.NumParameters = static_cast<UINT>(params.size());
        rootSigDesc.pParameters = params.data();
        rootSigDesc.NumStaticSamplers = static_cast<UINT>(m_Desc.staticSamplers.size());
        rootSigDesc.pStaticSamplers = m_Desc.staticSamplers.data();
        rootSigDesc.Flags = m_Desc.flags;

        ComPtr<ID3DBlob> signature, error;
        if (FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)))
        {
            SEA_CORE_ERROR("Failed to serialize root signature: {}", error ? (char*)error->GetBufferPointer() : "");
            return false;
        }

        return SUCCEEDED(m_Device.GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(),
            signature->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));
    }
}
