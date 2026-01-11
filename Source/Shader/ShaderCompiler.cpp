#include "Shader/ShaderCompiler.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include <d3dcompiler.h>
#include <dxcapi.h>
#include <wrl/client.h>
#include <filesystem>

using Microsoft::WRL::ComPtr;

namespace Sea
{
    static ComPtr<IDxcUtils> s_DxcUtils;
    static ComPtr<IDxcCompiler3> s_DxcCompiler;
    static ComPtr<IDxcIncludeHandler> s_DxcIncludeHandler;

    bool ShaderCompiler::Initialize()
    {
        // 尝试初始化DXC
        if (SUCCEEDED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&s_DxcUtils))) &&
            SUCCEEDED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&s_DxcCompiler))))
        {
            s_DxcUtils->CreateDefaultIncludeHandler(&s_DxcIncludeHandler);
            SEA_CORE_INFO("DXC Shader Compiler initialized");
            return true;
        }
        SEA_CORE_WARN("DXC not available, falling back to FXC");
        return true;
    }

    void ShaderCompiler::Shutdown()
    {
        s_DxcIncludeHandler.Reset();
        s_DxcCompiler.Reset();
        s_DxcUtils.Reset();
    }

    ShaderCompileResult ShaderCompiler::Compile(const ShaderCompileDesc& desc)
    {
        if (desc.model >= ShaderModel::SM_6_0 && s_DxcCompiler)
            return CompileDXC(desc);
        return CompileFXC(desc);
    }

    ShaderCompileResult ShaderCompiler::CompileFromSource(const std::string& source, const ShaderCompileDesc& desc)
    {
        ShaderCompileResult result;
        std::string target = GetTargetProfile(desc.stage, desc.model);

        UINT flags = 0;
        if (desc.debug) flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
        if (desc.optimize) flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;

        ComPtr<ID3DBlob> shaderBlob, errorBlob;
        HRESULT hr = D3DCompile(source.data(), source.size(), nullptr, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            desc.entryPoint.c_str(), target.c_str(), flags, 0, &shaderBlob, &errorBlob);

        if (FAILED(hr))
        {
            result.success = false;
            if (errorBlob) result.errors = static_cast<char*>(errorBlob->GetBufferPointer());
        }
        else
        {
            result.success = true;
            result.bytecode.resize(shaderBlob->GetBufferSize());
            memcpy(result.bytecode.data(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());
        }
        return result;
    }

    ShaderCompileResult ShaderCompiler::CompileFXC(const ShaderCompileDesc& desc)
    {
        std::string source = FileSystem::ReadTextFile(desc.filePath);
        if (source.empty())
        {
            ShaderCompileResult result;
            result.errors = "Failed to read shader file: " + desc.filePath.string();
            SEA_CORE_ERROR("Failed to read shader file: {}", desc.filePath.string());
            return result;
        }
        return CompileFromSource(source, desc);
    }

    ShaderCompileResult ShaderCompiler::CompileDXC(const ShaderCompileDesc& desc)
    {
        ShaderCompileResult result;
        if (!s_DxcCompiler) { result.errors = "DXC not initialized"; return result; }

        std::string source = FileSystem::ReadTextFile(desc.filePath);
        if (source.empty())
        {
            result.errors = "Failed to read shader file: " + desc.filePath.string();
            SEA_CORE_ERROR("Failed to read shader file: {}", desc.filePath.string());
            return result;
        }
        
        std::wstring wSource(source.begin(), source.end());
        std::wstring wEntry(desc.entryPoint.begin(), desc.entryPoint.end());
        
        // 先保存到临时变量，避免迭代器来自不同的临时对象
        std::string targetProfile = GetTargetProfile(desc.stage, desc.model);
        std::wstring target(targetProfile.begin(), targetProfile.end());

        std::vector<LPCWSTR> args;
        args.push_back(L"-E"); args.push_back(wEntry.c_str());
        args.push_back(L"-T"); args.push_back(target.c_str());
        
        // 添加 include 路径 - 使用 shader 文件所在目录
        std::wstring shaderDir = desc.filePath.parent_path().wstring();
        std::wstring shadersRoot = std::filesystem::path("Shaders").wstring();
        args.push_back(L"-I"); args.push_back(shaderDir.c_str());
        args.push_back(L"-I"); args.push_back(shadersRoot.c_str());
        
        if (desc.debug) { args.push_back(L"-Zi"); args.push_back(L"-Od"); }
        if (desc.optimize) args.push_back(L"-O3");

        DxcBuffer sourceBuffer = { source.c_str(), source.size(), CP_UTF8 };
        ComPtr<IDxcResult> results;

        HRESULT hr = s_DxcCompiler->Compile(&sourceBuffer, args.data(), static_cast<UINT32>(args.size()),
            s_DxcIncludeHandler.Get(), IID_PPV_ARGS(&results));

        HRESULT status;
        results->GetStatus(&status);

        ComPtr<IDxcBlobUtf8> errors;
        results->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        if (errors && errors->GetStringLength() > 0)
            result.errors = errors->GetStringPointer();

        if (SUCCEEDED(status))
        {
            ComPtr<IDxcBlob> shaderBlob;
            results->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
            result.bytecode.resize(shaderBlob->GetBufferSize());
            memcpy(result.bytecode.data(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());
            result.success = true;
        }
        return result;
    }

    std::string ShaderCompiler::GetTargetProfile(ShaderStage stage, ShaderModel model)
    {
        const char* stageStr[] = { "vs", "ps", "gs", "hs", "ds", "cs" };
        const char* modelStr[] = { "5_0", "5_1", "6_0", "6_1", "6_2", "6_3", "6_4", "6_5", "6_6" };
        return std::string(stageStr[static_cast<int>(stage)]) + "_" + modelStr[static_cast<int>(model)];
    }
}
