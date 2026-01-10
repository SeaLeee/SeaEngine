#pragma once
#include "Core/Types.h"
#include "RenderGraph/PassNode.h"
#include "RenderGraph/ResourceNode.h"
#include "Shader/Shader.h"
#include <string>
#include <vector>
#include <memory>

namespace Sea
{
    class Device;
    class RenderGraph;
    class PipelineState;
    class RootSignature;
    class ShaderData;

    // Pass 模板类型
    enum class PassTemplateType
    {
        Custom,         // 自定义 Pass
        FullscreenQuad, // 全屏后处理
        GBuffer,        // GBuffer 填充
        DeferredLighting, // 延迟光照
        ForwardOpaque,  // 正向不透明
        ForwardTransparent, // 正向透明
        Shadow,         // 阴影
        SSAO,           // 屏幕空间环境光遮蔽
        Bloom,          // 泛光
        Tonemap,        // 色调映射
        Compute         // 通用计算
    };

    // Pass 模板定义 - 描述一个可复用的 Pass 配置
    struct PassTemplate
    {
        std::string name;
        PassTemplateType type = PassTemplateType::Custom;
        PassType passType = PassType::Graphics;
        
        // 输入输出槽定义
        std::vector<std::string> inputSlots;
        std::vector<std::string> outputSlots;
        
        // Shader 配置
        std::string vertexShaderPath;
        std::string pixelShaderPath;
        std::string computeShaderPath;
        std::string shaderEntryVS = "VSMain";
        std::string shaderEntryPS = "PSMain";
        std::string shaderEntryCS = "CSMain";
        
        // 渲染状态
        bool depthEnable = true;
        bool depthWrite = true;
        bool blendEnable = false;
        D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_BACK;
        D3D12_COMPARISON_FUNC depthFunc = D3D12_COMPARISON_FUNC_LESS;
        
        // 输出格式
        std::vector<Format> outputFormats;
        Format depthFormat = Format::D32_FLOAT;
    };

    // Pass 模板库 - 提供预定义的 Pass 模板
    class PassTemplateLibrary
    {
    public:
        static void Initialize();
        static void Shutdown();
        
        static const PassTemplate* GetTemplate(const std::string& name);
        static const PassTemplate* GetTemplate(PassTemplateType type);
        static std::vector<std::string> GetTemplateNames();
        
        static void RegisterTemplate(const std::string& name, PassTemplate templ);

    private:
        static std::unordered_map<std::string, PassTemplate> s_Templates;
    };

    // Pass Builder - 用于从模板创建 Pass
    class PassBuilder
    {
    public:
        PassBuilder(RenderGraph& graph, Device& device);
        
        // 从模板创建 Pass
        u32 CreatePassFromTemplate(const std::string& templateName, const std::string& passName);
        u32 CreatePassFromTemplate(PassTemplateType type, const std::string& passName);
        
        // 自定义创建
        u32 CreateFullscreenPass(const std::string& name, 
                                 const std::string& shaderPath,
                                 const std::vector<std::string>& inputs,
                                 const std::string& output);
        
        u32 CreateComputePass(const std::string& name,
                              const std::string& shaderPath,
                              const std::vector<std::string>& inputs,
                              const std::vector<std::string>& outputs);

        // 设置 Pass 的 Shader
        bool SetPassShader(u32 passId, const std::string& vsPath, const std::string& psPath);
        bool SetPassComputeShader(u32 passId, const std::string& csPath);

    private:
        RenderGraph& m_Graph;
        Device& m_Device;
    };
}
