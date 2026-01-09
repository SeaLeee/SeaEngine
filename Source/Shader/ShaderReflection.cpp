#include "Shader/ShaderReflection.h"
#include "Core/Log.h"
#include <d3d12shader.h>
#include <dxcapi.h>

#pragma comment(lib, "dxcompiler.lib")

namespace Sea
{
    bool ShaderReflection::Reflect(const void* bytecode, size_t bytecodeSize)
    {
        if (!bytecode || bytecodeSize == 0)
        {
            SEA_CORE_ERROR("Invalid bytecode for reflection");
            return false;
        }

        m_Data = ShaderReflectionData{};

        // 尝试使用DXC反射（DXIL）
        if (ReflectDXC(bytecode, bytecodeSize))
        {
            return true;
        }

        // 回退到FXC反射（DXBC）
        return ReflectFXC(bytecode, bytecodeSize);
    }

    bool ShaderReflection::ReflectDXC(const void* bytecode, size_t bytecodeSize)
    {
        // 创建DXC工具
        ComPtr<IDxcUtils> utils;
        HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
        if (FAILED(hr))
        {
            return false;
        }

        // 创建反射接口
        DxcBuffer reflectionData;
        reflectionData.Ptr = bytecode;
        reflectionData.Size = bytecodeSize;
        reflectionData.Encoding = DXC_CP_ACP;

        ComPtr<ID3D12ShaderReflection> reflection;
        hr = utils->CreateReflection(&reflectionData, IID_PPV_ARGS(&reflection));
        if (FAILED(hr))
        {
            return false;
        }

        D3D12_SHADER_DESC shaderDesc;
        reflection->GetDesc(&shaderDesc);

        // 获取常量缓冲区信息
        for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i)
        {
            ID3D12ShaderReflectionConstantBuffer* cbReflection = reflection->GetConstantBufferByIndex(i);
            D3D12_SHADER_BUFFER_DESC cbDesc;
            cbReflection->GetDesc(&cbDesc);

            ConstantBufferInfo cbInfo;
            cbInfo.name = cbDesc.Name;
            cbInfo.size = cbDesc.Size;

            // 获取变量
            for (UINT v = 0; v < cbDesc.Variables; ++v)
            {
                ID3D12ShaderReflectionVariable* varReflection = cbReflection->GetVariableByIndex(v);
                D3D12_SHADER_VARIABLE_DESC varDesc;
                varReflection->GetDesc(&varDesc);

                ShaderVariable var;
                var.name = varDesc.Name;
                var.offset = varDesc.StartOffset;
                var.size = varDesc.Size;

                ID3D12ShaderReflectionType* typeReflection = varReflection->GetType();
                D3D12_SHADER_TYPE_DESC typeDesc;
                typeReflection->GetDesc(&typeDesc);

                // 推断类型
                if (typeDesc.Class == D3D_SVC_SCALAR)
                {
                    if (typeDesc.Type == D3D_SVT_FLOAT) var.type = ShaderVariableType::Float;
                    else if (typeDesc.Type == D3D_SVT_INT) var.type = ShaderVariableType::Int;
                    else if (typeDesc.Type == D3D_SVT_UINT) var.type = ShaderVariableType::UInt;
                    else if (typeDesc.Type == D3D_SVT_BOOL) var.type = ShaderVariableType::Bool;
                }
                else if (typeDesc.Class == D3D_SVC_VECTOR)
                {
                    if (typeDesc.Type == D3D_SVT_FLOAT)
                    {
                        switch (typeDesc.Columns)
                        {
                            case 2: var.type = ShaderVariableType::Float2; break;
                            case 3: var.type = ShaderVariableType::Float3; break;
                            case 4: var.type = ShaderVariableType::Float4; break;
                        }
                    }
                }
                else if (typeDesc.Class == D3D_SVC_MATRIX_COLUMNS || typeDesc.Class == D3D_SVC_MATRIX_ROWS)
                {
                    if (typeDesc.Rows == 4 && typeDesc.Columns == 4)
                        var.type = ShaderVariableType::Float4x4;
                    else if (typeDesc.Rows == 3 && typeDesc.Columns == 3)
                        var.type = ShaderVariableType::Float3x3;
                }

                cbInfo.variables.push_back(var);
            }

            m_Data.constantBuffers.push_back(cbInfo);
        }

        // 获取绑定资源
        for (UINT i = 0; i < shaderDesc.BoundResources; ++i)
        {
            D3D12_SHADER_INPUT_BIND_DESC bindDesc;
            reflection->GetResourceBindingDesc(i, &bindDesc);

            ResourceBindingInfo binding;
            binding.name = bindDesc.Name;
            binding.bindPoint = bindDesc.BindPoint;
            binding.bindSpace = bindDesc.Space;
            binding.bindCount = bindDesc.BindCount;

            switch (bindDesc.Type)
            {
                case D3D_SIT_CBUFFER:
                    // 已在常量缓冲区中处理
                    for (auto& cb : m_Data.constantBuffers)
                    {
                        if (cb.name == binding.name)
                        {
                            cb.bindPoint = binding.bindPoint;
                            cb.bindSpace = binding.bindSpace;
                        }
                    }
                    break;
                case D3D_SIT_TEXTURE:
                    if (bindDesc.Dimension == D3D_SRV_DIMENSION_TEXTURE2D)
                        binding.type = ShaderVariableType::Texture2D;
                    else if (bindDesc.Dimension == D3D_SRV_DIMENSION_TEXTURE3D)
                        binding.type = ShaderVariableType::Texture3D;
                    else if (bindDesc.Dimension == D3D_SRV_DIMENSION_TEXTURECUBE)
                        binding.type = ShaderVariableType::TextureCube;
                    m_Data.srvBindings.push_back(binding);
                    break;
                case D3D_SIT_SAMPLER:
                    binding.type = ShaderVariableType::Sampler;
                    m_Data.samplerBindings.push_back(binding);
                    break;
                case D3D_SIT_UAV_RWTYPED:
                    binding.type = ShaderVariableType::RWTexture2D;
                    m_Data.uavBindings.push_back(binding);
                    break;
                case D3D_SIT_STRUCTURED:
                    binding.type = ShaderVariableType::StructuredBuffer;
                    m_Data.srvBindings.push_back(binding);
                    break;
                case D3D_SIT_UAV_RWSTRUCTURED:
                    binding.type = ShaderVariableType::RWStructuredBuffer;
                    m_Data.uavBindings.push_back(binding);
                    break;
                default:
                    break;
            }
        }

        // Compute shader线程组大小
        reflection->GetThreadGroupSize(
            &m_Data.threadGroupSizeX,
            &m_Data.threadGroupSizeY,
            &m_Data.threadGroupSizeZ
        );

        SEA_CORE_TRACE("Shader reflected: {} CBs, {} SRVs, {} UAVs, {} Samplers",
            m_Data.constantBuffers.size(),
            m_Data.srvBindings.size(),
            m_Data.uavBindings.size(),
            m_Data.samplerBindings.size());

        return true;
    }

    bool ShaderReflection::ReflectFXC(const void* bytecode, size_t bytecodeSize)
    {
        // FXC使用D3DReflect
        ComPtr<ID3D12ShaderReflection> reflection;
        HRESULT hr = D3DReflect(bytecode, bytecodeSize, IID_PPV_ARGS(&reflection));
        if (FAILED(hr))
        {
            return false;
        }

        // 与DXC类似的反射逻辑...
        D3D12_SHADER_DESC shaderDesc;
        reflection->GetDesc(&shaderDesc);

        // 简化实现 - 只获取基本信息
        for (UINT i = 0; i < shaderDesc.BoundResources; ++i)
        {
            D3D12_SHADER_INPUT_BIND_DESC bindDesc;
            reflection->GetResourceBindingDesc(i, &bindDesc);

            ResourceBindingInfo binding;
            binding.name = bindDesc.Name;
            binding.bindPoint = bindDesc.BindPoint;
            binding.bindSpace = bindDesc.Space;

            if (bindDesc.Type == D3D_SIT_TEXTURE)
            {
                binding.type = ShaderVariableType::Texture2D;
                m_Data.srvBindings.push_back(binding);
            }
            else if (bindDesc.Type == D3D_SIT_SAMPLER)
            {
                binding.type = ShaderVariableType::Sampler;
                m_Data.samplerBindings.push_back(binding);
            }
        }

        return true;
    }

    const ConstantBufferInfo* ShaderReflection::FindConstantBuffer(const std::string& name) const
    {
        for (const auto& cb : m_Data.constantBuffers)
        {
            if (cb.name == name) return &cb;
        }
        return nullptr;
    }

    const ResourceBindingInfo* ShaderReflection::FindSRV(const std::string& name) const
    {
        for (const auto& srv : m_Data.srvBindings)
        {
            if (srv.name == name) return &srv;
        }
        return nullptr;
    }

    const ResourceBindingInfo* ShaderReflection::FindUAV(const std::string& name) const
    {
        for (const auto& uav : m_Data.uavBindings)
        {
            if (uav.name == name) return &uav;
        }
        return nullptr;
    }

    u32 ShaderReflection::GetRequiredRootParameterCount() const
    {
        return static_cast<u32>(
            m_Data.constantBuffers.size() +
            m_Data.srvBindings.size() +
            m_Data.uavBindings.size() +
            m_Data.samplerBindings.size()
        );
    }

    const char* ShaderReflection::GetVariableTypeString(ShaderVariableType type)
    {
        switch (type)
        {
            case ShaderVariableType::Bool: return "bool";
            case ShaderVariableType::Int: return "int";
            case ShaderVariableType::Int2: return "int2";
            case ShaderVariableType::Int3: return "int3";
            case ShaderVariableType::Int4: return "int4";
            case ShaderVariableType::UInt: return "uint";
            case ShaderVariableType::UInt2: return "uint2";
            case ShaderVariableType::UInt3: return "uint3";
            case ShaderVariableType::UInt4: return "uint4";
            case ShaderVariableType::Float: return "float";
            case ShaderVariableType::Float2: return "float2";
            case ShaderVariableType::Float3: return "float3";
            case ShaderVariableType::Float4: return "float4";
            case ShaderVariableType::Float3x3: return "float3x3";
            case ShaderVariableType::Float4x4: return "float4x4";
            case ShaderVariableType::Texture1D: return "Texture1D";
            case ShaderVariableType::Texture2D: return "Texture2D";
            case ShaderVariableType::Texture3D: return "Texture3D";
            case ShaderVariableType::TextureCube: return "TextureCube";
            case ShaderVariableType::Sampler: return "SamplerState";
            case ShaderVariableType::StructuredBuffer: return "StructuredBuffer";
            case ShaderVariableType::RWTexture2D: return "RWTexture2D";
            case ShaderVariableType::RWStructuredBuffer: return "RWStructuredBuffer";
            default: return "unknown";
        }
    }

    u32 ShaderReflection::GetVariableTypeSize(ShaderVariableType type)
    {
        switch (type)
        {
            case ShaderVariableType::Bool:
            case ShaderVariableType::Int:
            case ShaderVariableType::UInt:
            case ShaderVariableType::Float: return 4;
            case ShaderVariableType::Int2:
            case ShaderVariableType::UInt2:
            case ShaderVariableType::Float2: return 8;
            case ShaderVariableType::Int3:
            case ShaderVariableType::UInt3:
            case ShaderVariableType::Float3: return 12;
            case ShaderVariableType::Int4:
            case ShaderVariableType::UInt4:
            case ShaderVariableType::Float4: return 16;
            case ShaderVariableType::Float3x3: return 36;
            case ShaderVariableType::Float4x4: return 64;
            default: return 0;
        }
    }
}
