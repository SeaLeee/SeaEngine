#pragma once

// Deferred Rendering using FrameGraph
// This demonstrates how to build a complete deferred rendering pipeline with the FrameGraph system

#include "RenderGraph/FrameGraph.h"
#include "RHI/RHI.h"
#include <functional>

namespace Sea
{
    //=============================================================================
    // Deferred GBuffer Data
    //=============================================================================
    struct GBufferData
    {
        FrameGraphResourceHandle albedo;
        FrameGraphResourceHandle normal;
        FrameGraphResourceHandle material;      // Metallic, Roughness, AO
        FrameGraphResourceHandle emissive;
        FrameGraphResourceHandle depth;
        FrameGraphResourceHandle velocity;      // Motion vectors for TAA
    };

    //=============================================================================
    // Lighting Data
    //=============================================================================
    struct LightingData
    {
        FrameGraphResourceHandle hdrColor;
        FrameGraphResourceHandle depth;
    };

    //=============================================================================
    // Post Processing Data
    //=============================================================================
    struct PostProcessData
    {
        FrameGraphResourceHandle bloomBright;
        FrameGraphResourceHandle bloomBlur;
        FrameGraphResourceHandle ldrOutput;
    };

    //=============================================================================
    // Shadow Map Data
    //=============================================================================
    struct ShadowMapData
    {
        FrameGraphResourceHandle cascadedShadowMap;
        u32 cascadeCount = 4;
        u32 shadowMapSize = 2048;
    };

    //=============================================================================
    // Render Context - Data passed to passes
    //=============================================================================
    struct RenderContext
    {
        // Screen dimensions
        u32 screenWidth = 0;
        u32 screenHeight = 0;
        
        // Frame timing
        f32 deltaTime = 0.0f;
        f32 totalTime = 0.0f;
        u32 frameIndex = 0;
        
        // Camera matrices (should be per-view in multi-view rendering)
        // Matrix4x4 viewMatrix;
        // Matrix4x4 projMatrix;
        
        //=========================================================================
        // Scene Rendering Callbacks
        // 这些回调由用户提供，在相应的 Pass 中被调用
        //=========================================================================
        
        //! 渲染所有不透明物体到 GBuffer
        //! 在 GBuffer Pass 中调用
        //! 应该：设置每个物体的变换矩阵、材质参数，然后 DrawIndexed
        std::function<void(RHICommandList& cmdList)> renderOpaqueObjects;
        
        //! 渲染所有透明物体（前向渲染）
        //! 在 Forward Transparent Pass 中调用（如果有的话）
        std::function<void(RHICommandList& cmdList)> renderTransparentObjects;
        
        //! 渲染阴影投射物体
        //! 在 Shadow Pass 中调用，应该只渲染投射阴影的物体
        std::function<void(RHICommandList& cmdList)> renderShadowCasters;
        
        //! 渲染天空盒
        //! 在 Skybox Pass 中调用，在光照之后、后处理之前
        std::function<void(RHICommandList& cmdList)> renderSkybox;
        
        //=========================================================================
        // Pipeline State Objects
        // 预编译的管线状态，包含着色器、混合状态、深度状态等
        //=========================================================================
        
        //! GBuffer 渲染管线（输出到多个 RenderTarget）
        //! VS: 变换顶点到裁剪空间，输出世界位置/法线/UV
        //! PS: 输出 Albedo, Normal, Material, Emissive, Velocity 到 MRT
        RHIPipelineState* gbufferPipeline = nullptr;
        
        //! 延迟光照管线（全屏 Pass）
        //! VS: 生成全屏三角形
        //! PS: 读取 GBuffer，计算所有光源贡献，输出 HDR 颜色
        RHIPipelineState* lightingPipeline = nullptr;
        
        //! 天空盒渲染管线
        RHIPipelineState* skyboxPipeline = nullptr;
        
        //! 色调映射管线（HDR -> LDR）
        RHIPipelineState* tonemapPipeline = nullptr;
        
        //! Bloom 亮度提取管线
        RHIPipelineState* bloomBrightPipeline = nullptr;
        
        //! Bloom 模糊管线
        RHIPipelineState* bloomBlurPipeline = nullptr;
        
        //! FXAA 抗锯齿管线
        RHIPipelineState* fxaaPipeline = nullptr;
        
        RHIRootSignature* commonRootSignature = nullptr;
        RHIDescriptorHeap* srvHeap = nullptr;
    };

    //=============================================================================
    // DeferredFrameGraph
    //=============================================================================
    class DeferredFrameGraph
    {
    public:
        DeferredFrameGraph() = default;
        ~DeferredFrameGraph() = default;

        //! Build the deferred rendering frame graph
        void Setup(FrameGraph& fg, RenderContext& context);

        // Configuration
        void SetBloomEnabled(bool enabled) { m_BloomEnabled = enabled; }
        void SetFXAAEnabled(bool enabled) { m_FXAAEnabled = enabled; }
        void SetShadowsEnabled(bool enabled) { m_ShadowsEnabled = enabled; }
        void SetSSAOEnabled(bool enabled) { m_SSAOEnabled = enabled; }
        
        bool IsBloomEnabled() const { return m_BloomEnabled; }
        bool IsFXAAEnabled() const { return m_FXAAEnabled; }
        bool IsShadowsEnabled() const { return m_ShadowsEnabled; }
        bool IsSSAOEnabled() const { return m_SSAOEnabled; }

    private:
        // Add individual passes
        GBufferData AddGBufferPass(FrameGraph& fg, RenderContext& context);
        ShadowMapData AddShadowPass(FrameGraph& fg, RenderContext& context);
        LightingData AddLightingPass(FrameGraph& fg, RenderContext& context,
                                     const GBufferData& gbuffer,
                                     const ShadowMapData* shadows);
        FrameGraphResourceHandle AddSkyboxPass(FrameGraph& fg, RenderContext& context,
                                               const LightingData& lighting);
        PostProcessData AddBloomPass(FrameGraph& fg, RenderContext& context,
                                     FrameGraphResourceHandle hdrInput);
        FrameGraphResourceHandle AddTonemapPass(FrameGraph& fg, RenderContext& context,
                                                FrameGraphResourceHandle hdrInput,
                                                const PostProcessData* bloom);
        FrameGraphResourceHandle AddFXAAPass(FrameGraph& fg, RenderContext& context,
                                             FrameGraphResourceHandle ldrInput);

        // Feature flags
        bool m_BloomEnabled = true;
        bool m_FXAAEnabled = true;
        bool m_ShadowsEnabled = true;
        bool m_SSAOEnabled = false;
    };

    //=============================================================================
    // Implementation
    //=============================================================================

    inline void DeferredFrameGraph::Setup(FrameGraph& fg, RenderContext& context)
    {
        // 1. Shadow mapping (optional)
        ShadowMapData shadows;
        if (m_ShadowsEnabled)
        {
            shadows = AddShadowPass(fg, context);
        }

        // 2. GBuffer pass - render all opaque geometry
        GBufferData gbuffer = AddGBufferPass(fg, context);

        // 3. Deferred lighting
        LightingData lighting = AddLightingPass(fg, context, gbuffer,
                                                m_ShadowsEnabled ? &shadows : nullptr);

        // 4. Skybox (rendered after lighting, before post-process)
        FrameGraphResourceHandle hdrOutput = AddSkyboxPass(fg, context, lighting);

        // 5. Post-processing
        PostProcessData bloom;
        if (m_BloomEnabled)
        {
            bloom = AddBloomPass(fg, context, hdrOutput);
        }

        // 6. Tonemapping HDR -> LDR
        FrameGraphResourceHandle ldrOutput = AddTonemapPass(fg, context, hdrOutput,
                                                            m_BloomEnabled ? &bloom : nullptr);

        // 7. FXAA (optional)
        if (m_FXAAEnabled)
        {
            ldrOutput = AddFXAAPass(fg, context, ldrOutput);
        }

        // Final output is marked for presentation
        fg.MarkOutput(ldrOutput);
    }

    inline GBufferData DeferredFrameGraph::AddGBufferPass(FrameGraph& fg, RenderContext& context)
    {
        struct GBufferPassData
        {
            GBufferData gbuffer;
        };

        auto& pass = fg.AddPass<GBufferPassData>(
            "GBuffer",
            FrameGraphPassType::Graphics,
            [&context](FrameGraphBuilder& builder, GBufferPassData& data)
            {
                // Create GBuffer textures
                RHITextureDesc albedoDesc;
                albedoDesc.width = context.screenWidth;
                albedoDesc.height = context.screenHeight;
                albedoDesc.format = RHIFormat::R8G8B8A8_UNORM;
                albedoDesc.usage = RHITextureUsage::RenderTarget | RHITextureUsage::ShaderResource;
                albedoDesc.name = "GBuffer_Albedo";
                data.gbuffer.albedo = builder.CreateTexture("GBuffer_Albedo", albedoDesc);
                data.gbuffer.albedo = builder.Write(data.gbuffer.albedo);

                RHITextureDesc normalDesc;
                normalDesc.width = context.screenWidth;
                normalDesc.height = context.screenHeight;
                normalDesc.format = RHIFormat::R16G16B16A16_FLOAT;  // World-space normals
                normalDesc.usage = RHITextureUsage::RenderTarget | RHITextureUsage::ShaderResource;
                normalDesc.name = "GBuffer_Normal";
                data.gbuffer.normal = builder.CreateTexture("GBuffer_Normal", normalDesc);
                data.gbuffer.normal = builder.Write(data.gbuffer.normal);

                RHITextureDesc materialDesc;
                materialDesc.width = context.screenWidth;
                materialDesc.height = context.screenHeight;
                materialDesc.format = RHIFormat::R8G8B8A8_UNORM;  // Metallic, Roughness, AO, flags
                materialDesc.usage = RHITextureUsage::RenderTarget | RHITextureUsage::ShaderResource;
                materialDesc.name = "GBuffer_Material";
                data.gbuffer.material = builder.CreateTexture("GBuffer_Material", materialDesc);
                data.gbuffer.material = builder.Write(data.gbuffer.material);

                RHITextureDesc emissiveDesc;
                emissiveDesc.width = context.screenWidth;
                emissiveDesc.height = context.screenHeight;
                emissiveDesc.format = RHIFormat::R11G11B10_FLOAT;  // Emissive HDR
                emissiveDesc.usage = RHITextureUsage::RenderTarget | RHITextureUsage::ShaderResource;
                emissiveDesc.name = "GBuffer_Emissive";
                data.gbuffer.emissive = builder.CreateTexture("GBuffer_Emissive", emissiveDesc);
                data.gbuffer.emissive = builder.Write(data.gbuffer.emissive);

                RHITextureDesc velocityDesc;
                velocityDesc.width = context.screenWidth;
                velocityDesc.height = context.screenHeight;
                velocityDesc.format = RHIFormat::R16G16_FLOAT;  // Screen-space velocity
                velocityDesc.usage = RHITextureUsage::RenderTarget | RHITextureUsage::ShaderResource;
                velocityDesc.name = "GBuffer_Velocity";
                data.gbuffer.velocity = builder.CreateTexture("GBuffer_Velocity", velocityDesc);
                data.gbuffer.velocity = builder.Write(data.gbuffer.velocity);

                RHITextureDesc depthDesc;
                depthDesc.width = context.screenWidth;
                depthDesc.height = context.screenHeight;
                depthDesc.format = RHIFormat::D32_FLOAT;
                depthDesc.usage = RHITextureUsage::DepthStencil | RHITextureUsage::ShaderResource;
                depthDesc.name = "SceneDepth";
                depthDesc.clearValue = RHIClearValue::CreateDepthStencil(1.0f, 0);
                data.gbuffer.depth = builder.CreateTexture("SceneDepth", depthDesc);
                data.gbuffer.depth = builder.UseDepthStencil(data.gbuffer.depth, false);  // 写入深度

                builder.SetSideEffect(true);  // This pass modifies external state
            },
            [&context](RHICommandList& cmdList, const GBufferPassData& data)
            {
                // ========== 1. 设置视口和裁剪矩形 ==========
                RHIViewport viewport;
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = static_cast<f32>(context.screenWidth);
                viewport.height = static_cast<f32>(context.screenHeight);
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                cmdList.SetViewport(viewport);

                RHIScissorRect scissor;
                scissor.left = 0;
                scissor.top = 0;
                scissor.right = static_cast<i32>(context.screenWidth);
                scissor.bottom = static_cast<i32>(context.screenHeight);
                cmdList.SetScissorRect(scissor);

                // ========== 2. 清除 GBuffer 渲染目标 ==========
                // 注意：实际实现中需要从 FrameGraph 获取分配的物理资源的 RTV
                // 这里通过 context 中预分配的描述符来演示
                f32 clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                
                // GBuffer MRT 通常包含：
                // RT0: Albedo (RGB) + Alpha/Flags (A)
                // RT1: Normal (RGB) + Roughness (A) 或 World Normal (RG) + ...
                // RT2: Material (Metallic, Roughness, AO, ...)
                // RT3: Emissive (RGB)
                // RT4: Velocity (RG)
                
                // cmdList.ClearRenderTarget(albedoRTV, clearColor);
                // cmdList.ClearRenderTarget(normalRTV, clearColor);
                // cmdList.ClearRenderTarget(materialRTV, clearColor);
                // cmdList.ClearRenderTarget(emissiveRTV, clearColor);
                // cmdList.ClearRenderTarget(velocityRTV, clearColor);
                // cmdList.ClearDepthStencil(depthDSV, 1.0f, 0);

                // ========== 3. 设置 MRT 渲染目标 ==========
                // 注意：RHICommandList 需要扩展以支持多渲染目标
                // 在实际实现中，需要绑定所有 GBuffer RTV 和深度 DSV
                // cmdList.SetRenderTargets(gbufferRTVs, depthDSV);

                // ========== 4. 设置管线状态 ==========
                if (context.gbufferPipeline)
                {
                    cmdList.SetPipelineState(context.gbufferPipeline);
                }
                if (context.commonRootSignature)
                {
                    cmdList.SetRootSignature(context.commonRootSignature);
                }

                // ========== 5. 设置图元拓扑 ==========
                cmdList.SetPrimitiveTopology(RHIPrimitiveTopology::TriangleList);

                // ========== 6. 渲染所有不透明物体到 GBuffer ==========
                // 这是 GBuffer Pass 的核心：渲染场景中的所有不透明几何体
                // 每个物体的 GBuffer shader 会输出：
                //   - Albedo color
                //   - World/View space normal
                //   - Metallic, Roughness, AO
                //   - Emissive
                //   - Motion vectors
                if (context.renderOpaqueObjects)
                {
                    context.renderOpaqueObjects(cmdList);
                }
            }
        );

        return pass.gbuffer;
    }

    inline ShadowMapData DeferredFrameGraph::AddShadowPass(FrameGraph& fg, RenderContext& context)
    {
        struct ShadowPassData
        {
            ShadowMapData shadows;
        };

        auto& pass = fg.AddPass<ShadowPassData>(
            "ShadowMap",
            FrameGraphPassType::Graphics,
            [&context](FrameGraphBuilder& builder, ShadowPassData& data)
            {
                // Create cascaded shadow map array
                RHITextureDesc shadowDesc;
                shadowDesc.width = data.shadows.shadowMapSize;
                shadowDesc.height = data.shadows.shadowMapSize;
                shadowDesc.depth = data.shadows.cascadeCount;
                shadowDesc.format = RHIFormat::D32_FLOAT;
                shadowDesc.dimension = RHITextureDimension::Texture2D;  // 2D array
                shadowDesc.usage = RHITextureUsage::DepthStencil | RHITextureUsage::ShaderResource;
                shadowDesc.name = "CascadedShadowMap";

                data.shadows.cascadedShadowMap = builder.CreateTexture("CascadedShadowMap", shadowDesc);
                data.shadows.cascadedShadowMap = builder.UseDepthStencil(data.shadows.cascadedShadowMap, true);
            },
            [&context](RHICommandList& cmdList, const ShadowPassData& data)
            {
                // Render shadow cascades
                // For each cascade:
                //   - Set viewport to cascade region
                //   - Clear depth
                //   - Render shadow casters
            }
        );

        return pass.shadows;
    }

    inline LightingData DeferredFrameGraph::AddLightingPass(FrameGraph& fg, RenderContext& context,
                                                             const GBufferData& gbuffer,
                                                             const ShadowMapData* shadows)
    {
        struct LightingPassData
        {
            // Inputs
            FrameGraphResourceHandle albedoSRV;
            FrameGraphResourceHandle normalSRV;
            FrameGraphResourceHandle materialSRV;
            FrameGraphResourceHandle emissiveSRV;
            FrameGraphResourceHandle depthSRV;
            FrameGraphResourceHandle shadowSRV;
            
            // Outputs
            LightingData lighting;
        };

        auto& pass = fg.AddPass<LightingPassData>(
            "DeferredLighting",
            FrameGraphPassType::Graphics,
            [&context, &gbuffer, shadows](FrameGraphBuilder& builder, LightingPassData& data)
            {
                // Read GBuffer
                data.albedoSRV = builder.Read(gbuffer.albedo);
                data.normalSRV = builder.Read(gbuffer.normal);
                data.materialSRV = builder.Read(gbuffer.material);
                data.emissiveSRV = builder.Read(gbuffer.emissive);
                data.depthSRV = builder.Read(gbuffer.depth);
                
                // Read shadow map if available
                if (shadows)
                {
                    data.shadowSRV = builder.Read(shadows->cascadedShadowMap);
                }

                // Create HDR color output
                RHITextureDesc hdrDesc;
                hdrDesc.width = context.screenWidth;
                hdrDesc.height = context.screenHeight;
                hdrDesc.format = RHIFormat::R16G16B16A16_FLOAT;
                hdrDesc.usage = RHITextureUsage::RenderTarget | RHITextureUsage::ShaderResource;
                hdrDesc.name = "HDRSceneColor";
                data.lighting.hdrColor = builder.CreateTexture("HDRSceneColor", hdrDesc);
                data.lighting.hdrColor = builder.Write(data.lighting.hdrColor);

                // Preserve depth for later passes (skybox, transparent)
                data.lighting.depth = builder.Read(gbuffer.depth);
            },
            [&context](RHICommandList& cmdList, const LightingPassData& data)
            {
                // ========== 延迟光照 Pass ==========
                // 这个 Pass 不渲染任何几何体！
                // 它只是：
                //   1. 读取 GBuffer 纹理（作为 SRV）
                //   2. 对每个像素进行光照计算
                //   3. 输出到 HDR 颜色缓冲
                
                // 设置视口（全屏）
                RHIViewport viewport;
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = static_cast<f32>(context.screenWidth);
                viewport.height = static_cast<f32>(context.screenHeight);
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                cmdList.SetViewport(viewport);

                RHIScissorRect scissor;
                scissor.left = 0;
                scissor.top = 0;
                scissor.right = static_cast<i32>(context.screenWidth);
                scissor.bottom = static_cast<i32>(context.screenHeight);
                cmdList.SetScissorRect(scissor);

                // 设置光照管线状态
                if (context.lightingPipeline)
                {
                    cmdList.SetPipelineState(context.lightingPipeline);
                }
                if (context.commonRootSignature)
                {
                    cmdList.SetRootSignature(context.commonRootSignature);
                }

                // 绑定 GBuffer 纹理到着色器槽位
                // 光照着色器会读取这些纹理来重建像素的：
                //   - 世界位置（从深度 + 逆投影矩阵）
                //   - 法线
                //   - Albedo
                //   - Metallic/Roughness/AO
                // 然后对每个像素计算所有光源的贡献
                
                // 设置描述符堆（SRV heap）
                // cmdList.SetDescriptorHeaps(...);
                
                // 绑定 GBuffer SRV 描述符表
                // cmdList.SetGraphicsRootDescriptorTable(0, gbufferSRVTable);
                
                // 绑定光源常量缓冲
                // cmdList.SetGraphicsRootConstantBufferView(1, lightsCBV);
                
                // 绘制全屏三角形（3个顶点，顶点着色器生成全屏覆盖的三角形）
                cmdList.SetPrimitiveTopology(RHIPrimitiveTopology::TriangleList);
                cmdList.Draw(3, 1, 0, 0);
            }
        );

        return pass.lighting;
    }

    inline FrameGraphResourceHandle DeferredFrameGraph::AddSkyboxPass(FrameGraph& fg, RenderContext& context,
                                                                       const LightingData& lighting)
    {
        struct SkyboxPassData
        {
            FrameGraphResourceHandle hdrColor;
            FrameGraphResourceHandle depth;
        };

        auto& pass = fg.AddPass<SkyboxPassData>(
            "Skybox",
            FrameGraphPassType::Graphics,
            [&context, &lighting](FrameGraphBuilder& builder, SkyboxPassData& data)
            {
                // Read-write HDR color (we're adding to it)
                data.hdrColor = builder.ReadWrite(lighting.hdrColor);
                
                // Read depth for depth testing
                data.depth = builder.Read(lighting.depth);
            },
            [&context](RHICommandList& cmdList, const SkyboxPassData& data)
            {
                if (context.skyboxPipeline && context.renderSkybox)
                {
                    // cmdList.SetPipelineState(context.skyboxPipeline);
                    context.renderSkybox(cmdList);
                }
            }
        );

        return pass.hdrColor;
    }

    inline PostProcessData DeferredFrameGraph::AddBloomPass(FrameGraph& fg, RenderContext& context,
                                                             FrameGraphResourceHandle hdrInput)
    {
        struct BloomBrightPassData
        {
            FrameGraphResourceHandle hdrInput;
            FrameGraphResourceHandle brightOutput;
        };

        // Pass 1: Extract bright areas
        auto& brightPass = fg.AddPass<BloomBrightPassData>(
            "BloomBright",
            FrameGraphPassType::Compute,
            [&context, hdrInput](FrameGraphBuilder& builder, BloomBrightPassData& data)
            {
                data.hdrInput = builder.Read(hdrInput);

                // Quarter resolution for bloom
                RHITextureDesc brightDesc;
                brightDesc.width = context.screenWidth / 2;
                brightDesc.height = context.screenHeight / 2;
                brightDesc.format = RHIFormat::R11G11B10_FLOAT;
                brightDesc.usage = RHITextureUsage::UnorderedAccess | RHITextureUsage::ShaderResource;
                brightDesc.name = "BloomBright";
                data.brightOutput = builder.CreateTexture("BloomBright", brightDesc);
                data.brightOutput = builder.Write(data.brightOutput);
            },
            [&context](RHICommandList& cmdList, const BloomBrightPassData& data)
            {
                if (context.bloomBrightPipeline)
                {
                    // Dispatch compute shader for brightness extraction
                    // cmdList.Dispatch(groupsX, groupsY, 1);
                }
            }
        );

        // Pass 2: Blur
        struct BloomBlurPassData
        {
            FrameGraphResourceHandle brightInput;
            FrameGraphResourceHandle blurOutput;
        };

        auto& blurPass = fg.AddPass<BloomBlurPassData>(
            "BloomBlur",
            FrameGraphPassType::Compute,
            [&context, &brightPass](FrameGraphBuilder& builder, BloomBlurPassData& data)
            {
                data.brightInput = builder.Read(brightPass.brightOutput);

                RHITextureDesc blurDesc;
                blurDesc.width = context.screenWidth / 2;
                blurDesc.height = context.screenHeight / 2;
                blurDesc.format = RHIFormat::R11G11B10_FLOAT;
                blurDesc.usage = RHITextureUsage::UnorderedAccess | RHITextureUsage::ShaderResource;
                blurDesc.name = "BloomBlur";
                data.blurOutput = builder.CreateTexture("BloomBlur", blurDesc);
                data.blurOutput = builder.Write(data.blurOutput);
            },
            [&context](RHICommandList& cmdList, const BloomBlurPassData& data)
            {
                if (context.bloomBlurPipeline)
                {
                    // Multiple blur passes with ping-pong
                    // cmdList.Dispatch(...);
                }
            }
        );

        PostProcessData result;
        result.bloomBright = brightPass.brightOutput;
        result.bloomBlur = blurPass.blurOutput;
        return result;
    }

    inline FrameGraphResourceHandle DeferredFrameGraph::AddTonemapPass(FrameGraph& fg, RenderContext& context,
                                                                        FrameGraphResourceHandle hdrInput,
                                                                        const PostProcessData* bloom)
    {
        struct TonemapPassData
        {
            FrameGraphResourceHandle hdrInput;
            FrameGraphResourceHandle bloomInput;
            FrameGraphResourceHandle ldrOutput;
        };

        auto& pass = fg.AddPass<TonemapPassData>(
            "Tonemap",
            FrameGraphPassType::Graphics,
            [&context, hdrInput, bloom](FrameGraphBuilder& builder, TonemapPassData& data)
            {
                data.hdrInput = builder.Read(hdrInput);
                
                if (bloom)
                {
                    data.bloomInput = builder.Read(bloom->bloomBlur);
                }

                RHITextureDesc ldrDesc;
                ldrDesc.width = context.screenWidth;
                ldrDesc.height = context.screenHeight;
                ldrDesc.format = RHIFormat::R8G8B8A8_UNORM;
                ldrDesc.usage = RHITextureUsage::RenderTarget | RHITextureUsage::ShaderResource;
                ldrDesc.name = "LDROutput";
                data.ldrOutput = builder.CreateTexture("LDROutput", ldrDesc);
                data.ldrOutput = builder.Write(data.ldrOutput);
            },
            [&context](RHICommandList& cmdList, const TonemapPassData& data)
            {
                if (context.tonemapPipeline)
                {
                    // cmdList.SetPipelineState(context.tonemapPipeline);
                    cmdList.Draw(3, 1, 0, 0);  // Full-screen triangle
                }
            }
        );

        return pass.ldrOutput;
    }

    inline FrameGraphResourceHandle DeferredFrameGraph::AddFXAAPass(FrameGraph& fg, RenderContext& context,
                                                                     FrameGraphResourceHandle ldrInput)
    {
        struct FXAAPassData
        {
            FrameGraphResourceHandle input;
            FrameGraphResourceHandle output;
        };

        auto& pass = fg.AddPass<FXAAPassData>(
            "FXAA",
            FrameGraphPassType::Compute,
            [&context, ldrInput](FrameGraphBuilder& builder, FXAAPassData& data)
            {
                data.input = builder.Read(ldrInput);

                RHITextureDesc outputDesc;
                outputDesc.width = context.screenWidth;
                outputDesc.height = context.screenHeight;
                outputDesc.format = RHIFormat::R8G8B8A8_UNORM;
                outputDesc.usage = RHITextureUsage::UnorderedAccess | RHITextureUsage::ShaderResource;
                outputDesc.name = "FXAAOutput";
                data.output = builder.CreateTexture("FXAAOutput", outputDesc);
                data.output = builder.Write(data.output);
            },
            [&context](RHICommandList& cmdList, const FXAAPassData& data)
            {
                if (context.fxaaPipeline)
                {
                    // cmdList.Dispatch(...);
                }
            }
        );

        return pass.output;
    }

} // namespace Sea
