#include "RenderGraph/PassTemplate.h"
#include "RenderGraph/RenderGraph.h"
#include "Graphics/Device.h"
#include "Shader/ShaderCompiler.h"
#include "Core/Log.h"

namespace Sea
{
    std::unordered_map<std::string, PassTemplate> PassTemplateLibrary::s_Templates;

    void PassTemplateLibrary::Initialize()
    {
        // 全屏后处理模板
        PassTemplate fullscreen;
        fullscreen.name = "Fullscreen Quad";
        fullscreen.type = PassTemplateType::FullscreenQuad;
        fullscreen.passType = PassType::Graphics;
        fullscreen.inputSlots = { "Input" };
        fullscreen.outputSlots = { "Output" };
        fullscreen.vertexShaderPath = "Shaders/Fullscreen_VS.hlsl";
        fullscreen.depthEnable = false;
        fullscreen.depthWrite = false;
        fullscreen.outputFormats = { Format::R8G8B8A8_UNORM };
        s_Templates["Fullscreen Quad"] = fullscreen;

        // GBuffer 填充模板
        PassTemplate gbuffer;
        gbuffer.name = "GBuffer";
        gbuffer.type = PassTemplateType::GBuffer;
        gbuffer.passType = PassType::Graphics;
        gbuffer.inputSlots = {};
        gbuffer.outputSlots = { "Albedo", "Normal", "Depth" };
        gbuffer.vertexShaderPath = "Shaders/GBuffer_VS.hlsl";
        gbuffer.pixelShaderPath = "Shaders/GBuffer_PS.hlsl";
        gbuffer.depthEnable = true;
        gbuffer.depthWrite = true;
        gbuffer.outputFormats = { Format::R8G8B8A8_UNORM, Format::R16G16B16A16_FLOAT };
        gbuffer.depthFormat = Format::D32_FLOAT;
        s_Templates["GBuffer"] = gbuffer;

        // 延迟光照模板
        PassTemplate deferredLighting;
        deferredLighting.name = "Deferred Lighting";
        deferredLighting.type = PassTemplateType::DeferredLighting;
        deferredLighting.passType = PassType::Graphics;
        deferredLighting.inputSlots = { "Albedo", "Normal", "Depth" };
        deferredLighting.outputSlots = { "HDR" };
        deferredLighting.vertexShaderPath = "Shaders/Fullscreen_VS.hlsl";
        deferredLighting.pixelShaderPath = "Shaders/DeferredLighting_PS.hlsl";
        deferredLighting.depthEnable = false;
        deferredLighting.outputFormats = { Format::R16G16B16A16_FLOAT };
        s_Templates["Deferred Lighting"] = deferredLighting;

        // 色调映射模板
        PassTemplate tonemap;
        tonemap.name = "Tonemap";
        tonemap.type = PassTemplateType::Tonemap;
        tonemap.passType = PassType::Graphics;
        tonemap.inputSlots = { "HDR" };
        tonemap.outputSlots = { "LDR" };
        tonemap.vertexShaderPath = "Shaders/Fullscreen_VS.hlsl";
        tonemap.pixelShaderPath = "Shaders/Tonemap_PS.hlsl";
        tonemap.depthEnable = false;
        tonemap.outputFormats = { Format::R8G8B8A8_UNORM };
        s_Templates["Tonemap"] = tonemap;

        // Bloom 阈值提取模板
        PassTemplate bloomThreshold;
        bloomThreshold.name = "Bloom Threshold";
        bloomThreshold.type = PassTemplateType::Bloom;
        bloomThreshold.passType = PassType::Graphics;
        bloomThreshold.inputSlots = { "HDR" };
        bloomThreshold.outputSlots = { "Bloom0" };
        bloomThreshold.vertexShaderPath = "Shaders/Fullscreen_VS.hlsl";
        bloomThreshold.pixelShaderPath = "Shaders/PostProcess/Bloom_Threshold_PS.hlsl";
        bloomThreshold.depthEnable = false;
        bloomThreshold.outputFormats = { Format::R16G16B16A16_FLOAT };
        s_Templates["Bloom Threshold"] = bloomThreshold;

        // Bloom 降采样模板
        PassTemplate bloomDownsample;
        bloomDownsample.name = "Bloom Downsample";
        bloomDownsample.type = PassTemplateType::Bloom;
        bloomDownsample.passType = PassType::Graphics;
        bloomDownsample.inputSlots = { "Input" };
        bloomDownsample.outputSlots = { "Output" };
        bloomDownsample.vertexShaderPath = "Shaders/Fullscreen_VS.hlsl";
        bloomDownsample.pixelShaderPath = "Shaders/PostProcess/Bloom_Downsample_PS.hlsl";
        bloomDownsample.depthEnable = false;
        bloomDownsample.outputFormats = { Format::R16G16B16A16_FLOAT };
        s_Templates["Bloom Downsample"] = bloomDownsample;

        // Bloom 升采样模板
        PassTemplate bloomUpsample;
        bloomUpsample.name = "Bloom Upsample";
        bloomUpsample.type = PassTemplateType::Bloom;
        bloomUpsample.passType = PassType::Graphics;
        bloomUpsample.inputSlots = { "LowRes", "HighRes" };
        bloomUpsample.outputSlots = { "Output" };
        bloomUpsample.vertexShaderPath = "Shaders/Fullscreen_VS.hlsl";
        bloomUpsample.pixelShaderPath = "Shaders/PostProcess/Bloom_Upsample_PS.hlsl";
        bloomUpsample.depthEnable = false;
        bloomUpsample.outputFormats = { Format::R16G16B16A16_FLOAT };
        s_Templates["Bloom Upsample"] = bloomUpsample;

        // Bloom 合成模板
        PassTemplate bloomComposite;
        bloomComposite.name = "Bloom Composite";
        bloomComposite.type = PassTemplateType::Bloom;
        bloomComposite.passType = PassType::Graphics;
        bloomComposite.inputSlots = { "Scene", "Bloom" };
        bloomComposite.outputSlots = { "Output" };
        bloomComposite.vertexShaderPath = "Shaders/Fullscreen_VS.hlsl";
        bloomComposite.pixelShaderPath = "Shaders/PostProcess/Bloom_Composite_PS.hlsl";
        bloomComposite.depthEnable = false;
        bloomComposite.outputFormats = { Format::R16G16B16A16_FLOAT };
        s_Templates["Bloom Composite"] = bloomComposite;

        // 模糊计算模板
        PassTemplate blur;
        blur.name = "Blur (Compute)";
        blur.type = PassTemplateType::Compute;
        blur.passType = PassType::Compute;
        blur.inputSlots = { "Input" };
        blur.outputSlots = { "Output" };
        blur.computeShaderPath = "Shaders/Blur_CS.hlsl";
        s_Templates["Blur (Compute)"] = blur;

        // 正向渲染模板
        PassTemplate forward;
        forward.name = "Forward Opaque";
        forward.type = PassTemplateType::ForwardOpaque;
        forward.passType = PassType::Graphics;
        forward.inputSlots = {};
        forward.outputSlots = { "Color", "Depth" };
        forward.vertexShaderPath = "Shaders/Basic.hlsl";
        forward.pixelShaderPath = "Shaders/Basic.hlsl";
        forward.shaderEntryVS = "VSMain";
        forward.shaderEntryPS = "PSMain";
        forward.depthEnable = true;
        forward.depthWrite = true;
        forward.outputFormats = { Format::R8G8B8A8_UNORM };
        s_Templates["Forward Opaque"] = forward;

        SEA_CORE_INFO("PassTemplateLibrary initialized with {} templates", s_Templates.size());
    }

    void PassTemplateLibrary::Shutdown()
    {
        s_Templates.clear();
    }

    const PassTemplate* PassTemplateLibrary::GetTemplate(const std::string& name)
    {
        auto it = s_Templates.find(name);
        return it != s_Templates.end() ? &it->second : nullptr;
    }

    const PassTemplate* PassTemplateLibrary::GetTemplate(PassTemplateType type)
    {
        for (const auto& [name, templ] : s_Templates)
        {
            if (templ.type == type)
                return &templ;
        }
        return nullptr;
    }

    std::vector<std::string> PassTemplateLibrary::GetTemplateNames()
    {
        std::vector<std::string> names;
        names.reserve(s_Templates.size());
        for (const auto& [name, templ] : s_Templates)
        {
            names.push_back(name);
        }
        return names;
    }

    void PassTemplateLibrary::RegisterTemplate(const std::string& name, PassTemplate templ)
    {
        templ.name = name;
        s_Templates[name] = std::move(templ);
    }

    // PassBuilder
    PassBuilder::PassBuilder(RenderGraph& graph, Device& device)
        : m_Graph(graph), m_Device(device)
    {
    }

    u32 PassBuilder::CreatePassFromTemplate(const std::string& templateName, const std::string& passName)
    {
        const PassTemplate* templ = PassTemplateLibrary::GetTemplate(templateName);
        if (!templ)
        {
            SEA_CORE_ERROR("PassBuilder: Template '{}' not found", templateName);
            return UINT32_MAX;
        }

        u32 passId = m_Graph.AddPass(passName, templ->passType);
        PassNode* pass = m_Graph.GetPass(passId);
        if (!pass)
            return UINT32_MAX;

        // 添加输入槽
        for (const auto& input : templ->inputSlots)
        {
            pass->AddInput(input, true);
        }

        // 添加输出槽
        for (const auto& output : templ->outputSlots)
        {
            pass->AddOutput(output);
        }

        SEA_CORE_INFO("Created pass '{}' from template '{}'", passName, templateName);
        return passId;
    }

    u32 PassBuilder::CreatePassFromTemplate(PassTemplateType type, const std::string& passName)
    {
        const PassTemplate* templ = PassTemplateLibrary::GetTemplate(type);
        if (!templ)
        {
            SEA_CORE_ERROR("PassBuilder: Template type {} not found", static_cast<int>(type));
            return UINT32_MAX;
        }
        return CreatePassFromTemplate(templ->name, passName);
    }

    u32 PassBuilder::CreateFullscreenPass(const std::string& name,
                                          const std::string& shaderPath,
                                          const std::vector<std::string>& inputs,
                                          const std::string& output)
    {
        u32 passId = m_Graph.AddPass(name, PassType::Graphics);
        PassNode* pass = m_Graph.GetPass(passId);
        if (!pass)
            return UINT32_MAX;

        for (const auto& input : inputs)
        {
            pass->AddInput(input, true);
        }
        pass->AddOutput(output);

        SEA_CORE_INFO("Created fullscreen pass: {}", name);
        return passId;
    }

    u32 PassBuilder::CreateComputePass(const std::string& name,
                                       const std::string& shaderPath,
                                       const std::vector<std::string>& inputs,
                                       const std::vector<std::string>& outputs)
    {
        u32 passId = m_Graph.AddPass(name, PassType::Compute);
        PassNode* pass = m_Graph.GetPass(passId);
        if (!pass)
            return UINT32_MAX;

        for (const auto& input : inputs)
        {
            pass->AddInput(input, true);
        }
        for (const auto& output : outputs)
        {
            pass->AddOutput(output);
        }

        SEA_CORE_INFO("Created compute pass: {}", name);
        return passId;
    }

    bool PassBuilder::SetPassShader(u32 passId, const std::string& vsPath, const std::string& psPath)
    {
        // TODO: 编译 shader 并关联到 pass
        PassNode* pass = m_Graph.GetPass(passId);
        if (!pass)
            return false;

        SEA_CORE_INFO("Set shader for pass {}: VS={}, PS={}", pass->GetName(), vsPath, psPath);
        return true;
    }

    bool PassBuilder::SetPassComputeShader(u32 passId, const std::string& csPath)
    {
        PassNode* pass = m_Graph.GetPass(passId);
        if (!pass || pass->GetType() != PassType::Compute)
            return false;

        SEA_CORE_INFO("Set compute shader for pass {}: CS={}", pass->GetName(), csPath);
        return true;
    }
}
