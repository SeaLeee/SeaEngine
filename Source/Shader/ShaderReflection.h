#pragma once
#include "Core/Types.h"
#include "Graphics/GraphicsTypes.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace Sea
{
    // Shader变量类型
    enum class ShaderVariableType
    {
        Unknown,
        Bool,
        Int, Int2, Int3, Int4,
        UInt, UInt2, UInt3, UInt4,
        Float, Float2, Float3, Float4,
        Float3x3, Float4x4,
        Texture1D, Texture2D, Texture3D, TextureCube,
        Sampler,
        StructuredBuffer,
        RWTexture2D, RWStructuredBuffer
    };

    // Shader变量信息
    struct ShaderVariable
    {
        std::string name;
        ShaderVariableType type = ShaderVariableType::Unknown;
        u32 offset = 0;
        u32 size = 0;
        u32 bindPoint = 0;
        u32 bindSpace = 0;
    };

    // 常量缓冲区信息
    struct ConstantBufferInfo
    {
        std::string name;
        u32 bindPoint = 0;
        u32 bindSpace = 0;
        u32 size = 0;
        std::vector<ShaderVariable> variables;
    };

    // 资源绑定信息
    struct ResourceBindingInfo
    {
        std::string name;
        ShaderVariableType type = ShaderVariableType::Unknown;
        u32 bindPoint = 0;
        u32 bindSpace = 0;
        u32 bindCount = 1;  // 数组大小
    };

    // Shader输入/输出签名
    struct ShaderSignatureElement
    {
        std::string semanticName;
        u32 semanticIndex = 0;
        Format format = Format::Unknown;
        u32 register_ = 0;
    };

    // Shader反射数据
    struct ShaderReflectionData
    {
        // Shader类型
        std::string shaderType;  // "vs", "ps", "cs", etc.
        std::string entryPoint;

        // 常量缓冲区
        std::vector<ConstantBufferInfo> constantBuffers;

        // 纹理/采样器绑定
        std::vector<ResourceBindingInfo> srvBindings;   // Shader Resource Views
        std::vector<ResourceBindingInfo> uavBindings;   // Unordered Access Views
        std::vector<ResourceBindingInfo> samplerBindings;

        // 输入/输出签名
        std::vector<ShaderSignatureElement> inputSignature;
        std::vector<ShaderSignatureElement> outputSignature;

        // Compute shader特有
        u32 threadGroupSizeX = 0;
        u32 threadGroupSizeY = 0;
        u32 threadGroupSizeZ = 0;
    };

    // Shader反射器 - 分析编译后的Shader字节码
    class ShaderReflection
    {
    public:
        ShaderReflection() = default;
        ~ShaderReflection() = default;

        // 从编译后的字节码反射
        bool Reflect(const void* bytecode, size_t bytecodeSize);

        // 获取反射数据
        const ShaderReflectionData& GetReflectionData() const { return m_Data; }

        // 便捷访问
        const std::vector<ConstantBufferInfo>& GetConstantBuffers() const { return m_Data.constantBuffers; }
        const std::vector<ResourceBindingInfo>& GetSRVBindings() const { return m_Data.srvBindings; }
        const std::vector<ResourceBindingInfo>& GetUAVBindings() const { return m_Data.uavBindings; }
        const std::vector<ResourceBindingInfo>& GetSamplerBindings() const { return m_Data.samplerBindings; }

        // 查找资源
        const ConstantBufferInfo* FindConstantBuffer(const std::string& name) const;
        const ResourceBindingInfo* FindSRV(const std::string& name) const;
        const ResourceBindingInfo* FindUAV(const std::string& name) const;

        // 计算根签名需要的槽位数
        u32 GetRequiredRootParameterCount() const;

        // 获取类型字符串
        static const char* GetVariableTypeString(ShaderVariableType type);
        static u32 GetVariableTypeSize(ShaderVariableType type);

    private:
        bool ReflectDXC(const void* bytecode, size_t bytecodeSize);
        bool ReflectFXC(const void* bytecode, size_t bytecodeSize);

        ShaderReflectionData m_Data;
    };
}
