#include "Scene/BloomRenderer.h"
#include "Shader/ShaderCompiler.h"
#include "Core/Log.h"

namespace Sea
{
    BloomRenderer::BloomRenderer(Device& device)
        : m_Device(device)
    {
    }

    BloomRenderer::~BloomRenderer()
    {
        Shutdown();
    }

    bool BloomRenderer::Initialize(u32 width, u32 height)
    {
        m_Width = width;
        m_Height = height;

        if (!CreateConstantBuffer())
        {
            SEA_CORE_ERROR("BloomRenderer: Failed to create constant buffer");
            return false;
        }

        if (!CreatePipelines())
        {
            SEA_CORE_ERROR("BloomRenderer: Failed to create pipelines");
            return false;
        }

        if (!CreateResources(width, height))
        {
            SEA_CORE_ERROR("BloomRenderer: Failed to create resources");
            return false;
        }

        SEA_CORE_INFO("BloomRenderer initialized ({}x{})", width, height);
        return true;
    }

    void BloomRenderer::Shutdown()
    {
        ReleaseResources();
        m_ConstantBuffer.reset();
        m_ThresholdPSO.reset();
        m_DownsamplePSO.reset();
        m_UpsamplePSO.reset();
        m_CompositePSO.reset();
        m_RootSignature.reset();
    }

    void BloomRenderer::Resize(u32 width, u32 height)
    {
        if (m_Width == width && m_Height == height)
            return;

        ReleaseResources();
        CreateResources(width, height);
        m_Width = width;
        m_Height = height;
    }

    bool BloomRenderer::CreateConstantBuffer()
    {
        BufferDesc cbDesc;
        cbDesc.size = (sizeof(BloomConstants) + 255) & ~255;
        cbDesc.type = BufferType::Constant;

        m_ConstantBuffer = MakeScope<Buffer>(m_Device, cbDesc);
        if (!m_ConstantBuffer->Initialize(nullptr))
        {
            return false;
        }

        return true;
    }

    bool BloomRenderer::CreatePipelines()
    {
        // 创建根签名
        // Root Parameter 0: CBV (b0)
        // Root Parameter 1: Descriptor Table for SRV (t0)
        RootSignatureDesc rsDesc;
        rsDesc.flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        // CBV at b0
        RootParameterDesc cbvParam;
        cbvParam.type = RootParameterDesc::CBV;
        cbvParam.shaderRegister = 0;
        cbvParam.registerSpace = 0;
        cbvParam.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rsDesc.parameters.push_back(cbvParam);

        // SRV Table at t0-t1 (Upsample shader needs 2 textures)
        RootParameterDesc srvParam;
        srvParam.type = RootParameterDesc::DescriptorTable;
        srvParam.shaderRegister = 0;
        srvParam.registerSpace = 0;
        srvParam.numDescriptors = 2;  // t0 和 t1 用于 Upsample 
        srvParam.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvParam.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rsDesc.parameters.push_back(srvParam);

        // Static sampler (linear clamp)
        D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.MipLODBias = 0;
        samplerDesc.MaxAnisotropy = 1;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
        samplerDesc.MinLOD = 0.0f;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        samplerDesc.ShaderRegister = 0;
        samplerDesc.RegisterSpace = 0;
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rsDesc.staticSamplers.push_back(samplerDesc);

        m_RootSignature = MakeScope<RootSignature>(m_Device, rsDesc);
        if (!m_RootSignature->Initialize())
        {
            SEA_CORE_ERROR("BloomRenderer: Failed to create root signature");
            return false;
        }

        // 编译全屏三角形顶点着色器
        ShaderCompileDesc vsDesc;
        vsDesc.filePath = "Shaders/Fullscreen_VS.hlsl";
        vsDesc.entryPoint = "VSMain";
        vsDesc.stage = ShaderStage::Vertex;
        vsDesc.model = ShaderModel::SM_6_0;
        auto vsResult = ShaderCompiler::Compile(vsDesc);

        if (!vsResult.success)
        {
            SEA_CORE_ERROR("BloomRenderer: Failed to compile Fullscreen_VS: {}", vsResult.errors);
            return false;
        }

        // 通用 PSO 描述
        GraphicsPipelineDesc psoDesc;
        psoDesc.rootSignature = m_RootSignature.get();
        psoDesc.vertexShader = vsResult.bytecode;
        psoDesc.inputLayout = {};  // 全屏三角形无需输入布局
        psoDesc.rtvFormats = { Format::R16G16B16A16_FLOAT };
        psoDesc.dsvFormat = Format::Unknown;
        psoDesc.depthEnable = false;
        psoDesc.depthWrite = false;
        psoDesc.cullMode = CullMode::None;

        // Threshold PSO
        ShaderCompileDesc thresholdPsDesc;
        thresholdPsDesc.filePath = "Shaders/PostProcess/Bloom_Threshold_PS.hlsl";
        thresholdPsDesc.entryPoint = "main";
        thresholdPsDesc.stage = ShaderStage::Pixel;
        thresholdPsDesc.model = ShaderModel::SM_6_0;
        auto thresholdResult = ShaderCompiler::Compile(thresholdPsDesc);

        if (!thresholdResult.success)
        {
            SEA_CORE_ERROR("BloomRenderer: Failed to compile Bloom_Threshold_PS: {}", thresholdResult.errors);
            return false;
        }

        psoDesc.pixelShader = thresholdResult.bytecode;
        m_ThresholdPSO = PipelineState::CreateGraphics(m_Device, psoDesc);
        if (!m_ThresholdPSO)
        {
            SEA_CORE_ERROR("BloomRenderer: Failed to create Threshold PSO");
            return false;
        }

        // Downsample PSO
        ShaderCompileDesc downsamplePsDesc;
        downsamplePsDesc.filePath = "Shaders/PostProcess/Bloom_Downsample_PS.hlsl";
        downsamplePsDesc.entryPoint = "main";
        downsamplePsDesc.stage = ShaderStage::Pixel;
        downsamplePsDesc.model = ShaderModel::SM_6_0;
        auto downsampleResult = ShaderCompiler::Compile(downsamplePsDesc);

        if (!downsampleResult.success)
        {
            SEA_CORE_ERROR("BloomRenderer: Failed to compile Bloom_Downsample_PS: {}", downsampleResult.errors);
            return false;
        }

        psoDesc.pixelShader = downsampleResult.bytecode;
        m_DownsamplePSO = PipelineState::CreateGraphics(m_Device, psoDesc);
        if (!m_DownsamplePSO)
        {
            SEA_CORE_ERROR("BloomRenderer: Failed to create Downsample PSO");
            return false;
        }

        // Upsample PSO
        ShaderCompileDesc upsamplePsDesc;
        upsamplePsDesc.filePath = "Shaders/PostProcess/Bloom_Upsample_PS.hlsl";
        upsamplePsDesc.entryPoint = "main";
        upsamplePsDesc.stage = ShaderStage::Pixel;
        upsamplePsDesc.model = ShaderModel::SM_6_0;
        auto upsampleResult = ShaderCompiler::Compile(upsamplePsDesc);

        if (!upsampleResult.success)
        {
            SEA_CORE_ERROR("BloomRenderer: Failed to compile Bloom_Upsample_PS: {}", upsampleResult.errors);
            return false;
        }

        psoDesc.pixelShader = upsampleResult.bytecode;
        m_UpsamplePSO = PipelineState::CreateGraphics(m_Device, psoDesc);
        if (!m_UpsamplePSO)
        {
            SEA_CORE_ERROR("BloomRenderer: Failed to create Upsample PSO");
            return false;
        }

        // Composite PSO (输出到 backbuffer 格式)
        ShaderCompileDesc compositePsDesc;
        compositePsDesc.filePath = "Shaders/PostProcess/Bloom_Composite_PS.hlsl";
        compositePsDesc.entryPoint = "main";
        compositePsDesc.stage = ShaderStage::Pixel;
        compositePsDesc.model = ShaderModel::SM_6_0;
        auto compositeResult = ShaderCompiler::Compile(compositePsDesc);

        if (!compositeResult.success)
        {
            SEA_CORE_ERROR("BloomRenderer: Failed to compile Bloom_Composite_PS: {}", compositeResult.errors);
            return false;
        }

        psoDesc.pixelShader = compositeResult.bytecode;
        psoDesc.rtvFormats = { Format::R8G8B8A8_UNORM };  // Output to backbuffer
        m_CompositePSO = PipelineState::CreateGraphics(m_Device, psoDesc);
        if (!m_CompositePSO)
        {
            SEA_CORE_ERROR("BloomRenderer: Failed to create Composite PSO");
            return false;
        }

        SEA_CORE_INFO("BloomRenderer: Pipelines created");
        return true;
    }

    bool BloomRenderer::CreateResources(u32 width, u32 height)
    {
        // 创建 RTV 和 SRV 描述符堆
        DescriptorHeapDesc rtvHeapDesc;
        rtvHeapDesc.type = DescriptorHeapType::RTV;
        rtvHeapDesc.numDescriptors = MIP_COUNT * 2;  // Downsample + Upsample
        rtvHeapDesc.shaderVisible = false;

        m_RTVHeap = MakeScope<DescriptorHeap>(m_Device, rtvHeapDesc);
        if (!m_RTVHeap->Initialize())
        {
            SEA_CORE_ERROR("BloomRenderer: Failed to create RTV heap");
            return false;
        }

        DescriptorHeapDesc srvHeapDesc;
        srvHeapDesc.type = DescriptorHeapType::CBV_SRV_UAV;
        srvHeapDesc.numDescriptors = MIP_COUNT * 2;
        srvHeapDesc.shaderVisible = true;

        m_SRVHeap = MakeScope<DescriptorHeap>(m_Device, srvHeapDesc);
        if (!m_SRVHeap->Initialize())
        {
            SEA_CORE_ERROR("BloomRenderer: Failed to create SRV heap");
            return false;
        }

        // 创建 mip chain 纹理
        auto* device = m_Device.GetDevice();

        u32 mipWidth = width / 2;
        u32 mipHeight = height / 2;

        for (u32 i = 0; i < MIP_COUNT; ++i)
        {
            mipWidth = std::max(1u, mipWidth);
            mipHeight = std::max(1u, mipHeight);

            // Downsample chain
            {
                auto& mip = m_DownsampleChain[i];
                mip.Width = mipWidth;
                mip.Height = mipHeight;

                D3D12_RESOURCE_DESC desc = {};
                desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                desc.Width = mipWidth;
                desc.Height = mipHeight;
                desc.DepthOrArraySize = 1;
                desc.MipLevels = 1;
                desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
                desc.SampleDesc.Count = 1;
                desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
                desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

                D3D12_HEAP_PROPERTIES heapProps = {};
                heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

                D3D12_CLEAR_VALUE clearValue = {};
                clearValue.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

                HRESULT hr = device->CreateCommittedResource(
                    &heapProps,
                    D3D12_HEAP_FLAG_NONE,
                    &desc,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    &clearValue,
                    IID_PPV_ARGS(&mip.Resource)
                );

                if (FAILED(hr))
                {
                    SEA_CORE_ERROR("BloomRenderer: Failed to create downsample mip {}", i);
                    return false;
                }

                // 创建 RTV
                D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
                rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                rtvDesc.Texture2D.MipSlice = 0;

                mip.RTV = m_RTVHeap->GetCPUHandle(i);
                device->CreateRenderTargetView(mip.Resource.Get(), &rtvDesc, mip.RTV);

                // 创建 SRV
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.Texture2D.MipLevels = 1;

                auto cpuHandle = m_SRVHeap->GetCPUHandle(i);
                device->CreateShaderResourceView(mip.Resource.Get(), &srvDesc, cpuHandle);
                mip.SRV = m_SRVHeap->GetGPUHandle(i);
            }

            // Upsample chain
            {
                auto& mip = m_UpsampleChain[i];
                mip.Width = mipWidth;
                mip.Height = mipHeight;

                D3D12_RESOURCE_DESC desc = {};
                desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                desc.Width = mipWidth;
                desc.Height = mipHeight;
                desc.DepthOrArraySize = 1;
                desc.MipLevels = 1;
                desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
                desc.SampleDesc.Count = 1;
                desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
                desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

                D3D12_HEAP_PROPERTIES heapProps = {};
                heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

                D3D12_CLEAR_VALUE clearValue = {};
                clearValue.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

                HRESULT hr = device->CreateCommittedResource(
                    &heapProps,
                    D3D12_HEAP_FLAG_NONE,
                    &desc,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    &clearValue,
                    IID_PPV_ARGS(&mip.Resource)
                );

                if (FAILED(hr))
                {
                    SEA_CORE_ERROR("BloomRenderer: Failed to create upsample mip {}", i);
                    return false;
                }

                // 创建 RTV
                D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
                rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                rtvDesc.Texture2D.MipSlice = 0;

                mip.RTV = m_RTVHeap->GetCPUHandle(MIP_COUNT + i);
                device->CreateRenderTargetView(mip.Resource.Get(), &rtvDesc, mip.RTV);

                // 创建 SRV
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.Texture2D.MipLevels = 1;

                auto cpuHandle = m_SRVHeap->GetCPUHandle(MIP_COUNT + i);
                device->CreateShaderResourceView(mip.Resource.Get(), &srvDesc, cpuHandle);
                mip.SRV = m_SRVHeap->GetGPUHandle(MIP_COUNT + i);
            }

            mipWidth /= 2;
            mipHeight /= 2;
        }

        SEA_CORE_INFO("BloomRenderer: Resources created (6 mip levels)");
        return true;
    }

    void BloomRenderer::ReleaseResources()
    {
        for (auto& mip : m_DownsampleChain)
        {
            mip.Resource.Reset();
            mip.RTV = {};
            mip.SRV = {};
        }
        for (auto& mip : m_UpsampleChain)
        {
            mip.Resource.Reset();
            mip.RTV = {};
            mip.SRV = {};
        }
        m_RTVHeap.reset();
        m_SRVHeap.reset();
    }

    void BloomRenderer::Render(CommandList& cmdList, 
                                D3D12_GPU_DESCRIPTOR_HANDLE inputSRV,
                                D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
                                ID3D12Resource* outputResource,
                                u32 outputWidth, u32 outputHeight)
    {
        if (!m_Settings.Enabled)
            return;

        auto* d3dCmdList = cmdList.GetCommandList();

        // 更新常量缓冲区
        BloomConstants constants = {};
        constants.TexelSizeX = 1.0f / static_cast<float>(m_Width);
        constants.TexelSizeY = 1.0f / static_cast<float>(m_Height);
        constants.BloomThreshold = m_Settings.Threshold;
        constants.BloomIntensity = m_Settings.Intensity;
        constants.BloomRadius = m_Settings.Radius;
        constants.BloomTintR = m_Settings.TintR;
        constants.BloomTintG = m_Settings.TintG;
        constants.BloomTintB = m_Settings.TintB;
        constants.Bloom1Weight = m_Settings.Mip1Weight;
        constants.Bloom2Weight = m_Settings.Mip2Weight;
        constants.Bloom3Weight = m_Settings.Mip3Weight;
        constants.Bloom4Weight = m_Settings.Mip4Weight;
        constants.Bloom5Weight = m_Settings.Mip5Weight;
        constants.Bloom6Weight = m_Settings.Mip6Weight;

        m_ConstantBuffer->Update(&constants, sizeof(BloomConstants));

        // 设置根签名和描述符堆
        d3dCmdList->SetGraphicsRootSignature(m_RootSignature->GetRootSignature());
        ID3D12DescriptorHeap* heaps[] = { m_SRVHeap->GetHeap() };
        d3dCmdList->SetDescriptorHeaps(1, heaps);
        d3dCmdList->SetGraphicsRootConstantBufferView(0, m_ConstantBuffer->GetGPUAddress());

        // Pass 1: Threshold - 从场景提取高亮
        ThresholdPass(cmdList, inputSRV);

        // Pass 2-6: Downsample chain
        for (u32 i = 1; i < MIP_COUNT; ++i)
        {
            DownsamplePass(cmdList, i);
        }

        // Pass 7-11: Upsample chain (从最小 mip 向上)
        for (i32 i = MIP_COUNT - 1; i >= 0; --i)
        {
            UpsamplePass(cmdList, static_cast<u32>(i));
        }

        // Pass 12: Composite - 将 bloom 与场景合并
        CompositePass(cmdList, inputSRV, outputRTV, outputResource, outputWidth, outputHeight);
    }

    void BloomRenderer::ThresholdPass(CommandList& cmdList, D3D12_GPU_DESCRIPTOR_HANDLE inputSRV)
    {
        auto* d3dCmdList = cmdList.GetCommandList();
        auto& target = m_DownsampleChain[0];

        // 转换目标到 RenderTarget
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = target.Resource.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        d3dCmdList->ResourceBarrier(1, &barrier);

        // 设置渲染目标
        d3dCmdList->OMSetRenderTargets(1, &target.RTV, FALSE, nullptr);

        // 更新 texel size
        BloomConstants constants = {};
        constants.TexelSizeX = 1.0f / static_cast<float>(target.Width);
        constants.TexelSizeY = 1.0f / static_cast<float>(target.Height);
        constants.BloomThreshold = m_Settings.Threshold;
        constants.BloomIntensity = m_Settings.Intensity;
        constants.BloomRadius = m_Settings.Radius;
        constants.BloomTintR = m_Settings.TintR;
        constants.BloomTintG = m_Settings.TintG;
        constants.BloomTintB = m_Settings.TintB;
        constants.Bloom1Weight = m_Settings.Mip1Weight;
        constants.Bloom2Weight = m_Settings.Mip2Weight;
        constants.Bloom3Weight = m_Settings.Mip3Weight;
        constants.Bloom4Weight = m_Settings.Mip4Weight;
        constants.Bloom5Weight = m_Settings.Mip5Weight;
        constants.Bloom6Weight = m_Settings.Mip6Weight;
        m_ConstantBuffer->Update(&constants, sizeof(BloomConstants));

        // 设置视口
        D3D12_VIEWPORT viewport = { 0, 0, static_cast<float>(target.Width), static_cast<float>(target.Height), 0, 1 };
        D3D12_RECT scissor = { 0, 0, static_cast<LONG>(target.Width), static_cast<LONG>(target.Height) };
        d3dCmdList->RSSetViewports(1, &viewport);
        d3dCmdList->RSSetScissorRects(1, &scissor);

        // 绘制
        d3dCmdList->SetPipelineState(m_ThresholdPSO->GetPipelineState());
        d3dCmdList->SetGraphicsRootDescriptorTable(1, inputSRV);
        d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3dCmdList->DrawInstanced(3, 1, 0, 0);

        // 转换回 ShaderResource
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        d3dCmdList->ResourceBarrier(1, &barrier);
    }

    void BloomRenderer::DownsamplePass(CommandList& cmdList, u32 mipLevel)
    {
        auto* d3dCmdList = cmdList.GetCommandList();
        auto& source = m_DownsampleChain[mipLevel - 1];
        auto& target = m_DownsampleChain[mipLevel];

        // 转换目标到 RenderTarget
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = target.Resource.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        d3dCmdList->ResourceBarrier(1, &barrier);

        // 设置渲染目标
        d3dCmdList->OMSetRenderTargets(1, &target.RTV, FALSE, nullptr);

        // 更新 texel size
        BloomConstants constants = {};
        constants.TexelSizeX = 1.0f / static_cast<float>(source.Width);
        constants.TexelSizeY = 1.0f / static_cast<float>(source.Height);
        constants.BloomThreshold = m_Settings.Threshold;
        constants.BloomIntensity = m_Settings.Intensity;
        constants.BloomRadius = m_Settings.Radius;
        constants.BloomTintR = m_Settings.TintR;
        constants.BloomTintG = m_Settings.TintG;
        constants.BloomTintB = m_Settings.TintB;
        constants.Bloom1Weight = m_Settings.Mip1Weight;
        constants.Bloom2Weight = m_Settings.Mip2Weight;
        constants.Bloom3Weight = m_Settings.Mip3Weight;
        constants.Bloom4Weight = m_Settings.Mip4Weight;
        constants.Bloom5Weight = m_Settings.Mip5Weight;
        constants.Bloom6Weight = m_Settings.Mip6Weight;
        m_ConstantBuffer->Update(&constants, sizeof(BloomConstants));

        // 设置视口
        D3D12_VIEWPORT viewport = { 0, 0, static_cast<float>(target.Width), static_cast<float>(target.Height), 0, 1 };
        D3D12_RECT scissor = { 0, 0, static_cast<LONG>(target.Width), static_cast<LONG>(target.Height) };
        d3dCmdList->RSSetViewports(1, &viewport);
        d3dCmdList->RSSetScissorRects(1, &scissor);

        // 绘制
        d3dCmdList->SetPipelineState(m_DownsamplePSO->GetPipelineState());
        d3dCmdList->SetGraphicsRootDescriptorTable(1, source.SRV);
        d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3dCmdList->DrawInstanced(3, 1, 0, 0);

        // 转换回 ShaderResource
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        d3dCmdList->ResourceBarrier(1, &barrier);
    }

    void BloomRenderer::UpsamplePass(CommandList& cmdList, u32 mipLevel)
    {
        auto* d3dCmdList = cmdList.GetCommandList();
        auto& target = m_UpsampleChain[mipLevel];
        
        // 源是下一级的 upsample 结果，或者最小 mip 的 downsample 结果
        D3D12_GPU_DESCRIPTOR_HANDLE sourceSRV;
        u32 sourceWidth, sourceHeight;
        
        if (mipLevel == MIP_COUNT - 1)
        {
            // 最小级别：从 downsample chain 读取
            sourceSRV = m_DownsampleChain[mipLevel].SRV;
            sourceWidth = m_DownsampleChain[mipLevel].Width;
            sourceHeight = m_DownsampleChain[mipLevel].Height;
        }
        else
        {
            // 其他级别：从上一级 upsample 结果读取
            sourceSRV = m_UpsampleChain[mipLevel + 1].SRV;
            sourceWidth = m_UpsampleChain[mipLevel + 1].Width;
            sourceHeight = m_UpsampleChain[mipLevel + 1].Height;
        }

        // 转换目标到 RenderTarget
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = target.Resource.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        d3dCmdList->ResourceBarrier(1, &barrier);

        // 设置渲染目标
        d3dCmdList->OMSetRenderTargets(1, &target.RTV, FALSE, nullptr);

        // 更新 texel size (使用源纹理尺寸)
        BloomConstants constants = {};
        constants.TexelSizeX = 1.0f / static_cast<float>(sourceWidth);
        constants.TexelSizeY = 1.0f / static_cast<float>(sourceHeight);
        constants.BloomThreshold = m_Settings.Threshold;
        constants.BloomIntensity = m_Settings.Intensity;
        constants.BloomRadius = m_Settings.Radius;
        constants.BloomTintR = m_Settings.TintR;
        constants.BloomTintG = m_Settings.TintG;
        constants.BloomTintB = m_Settings.TintB;
        constants.Bloom1Weight = m_Settings.Mip1Weight;
        constants.Bloom2Weight = m_Settings.Mip2Weight;
        constants.Bloom3Weight = m_Settings.Mip3Weight;
        constants.Bloom4Weight = m_Settings.Mip4Weight;
        constants.Bloom5Weight = m_Settings.Mip5Weight;
        constants.Bloom6Weight = m_Settings.Mip6Weight;
        m_ConstantBuffer->Update(&constants, sizeof(BloomConstants));

        // 设置视口
        D3D12_VIEWPORT viewport = { 0, 0, static_cast<float>(target.Width), static_cast<float>(target.Height), 0, 1 };
        D3D12_RECT scissor = { 0, 0, static_cast<LONG>(target.Width), static_cast<LONG>(target.Height) };
        d3dCmdList->RSSetViewports(1, &viewport);
        d3dCmdList->RSSetScissorRects(1, &scissor);

        // 绘制
        d3dCmdList->SetPipelineState(m_UpsamplePSO->GetPipelineState());
        d3dCmdList->SetGraphicsRootDescriptorTable(1, sourceSRV);
        d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3dCmdList->DrawInstanced(3, 1, 0, 0);

        // 转换回 ShaderResource
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        d3dCmdList->ResourceBarrier(1, &barrier);
    }

    void BloomRenderer::CompositePass(CommandList& cmdList, 
                                       D3D12_GPU_DESCRIPTOR_HANDLE sceneSRV,
                                       D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
                                       ID3D12Resource* outputResource,
                                       u32 outputWidth, u32 outputHeight)
    {
        auto* d3dCmdList = cmdList.GetCommandList();

        // 转换输出到 RenderTarget
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = outputResource;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        d3dCmdList->ResourceBarrier(1, &barrier);

        // 设置渲染目标
        d3dCmdList->OMSetRenderTargets(1, &outputRTV, FALSE, nullptr);

        // 更新常量
        BloomConstants constants = {};
        constants.TexelSizeX = 1.0f / static_cast<float>(m_UpsampleChain[0].Width);
        constants.TexelSizeY = 1.0f / static_cast<float>(m_UpsampleChain[0].Height);
        constants.BloomThreshold = m_Settings.Threshold;
        constants.BloomIntensity = m_Settings.Intensity;
        constants.BloomRadius = m_Settings.Radius;
        constants.BloomTintR = m_Settings.TintR;
        constants.BloomTintG = m_Settings.TintG;
        constants.BloomTintB = m_Settings.TintB;
        constants.Bloom1Weight = m_Settings.Mip1Weight;
        constants.Bloom2Weight = m_Settings.Mip2Weight;
        constants.Bloom3Weight = m_Settings.Mip3Weight;
        constants.Bloom4Weight = m_Settings.Mip4Weight;
        constants.Bloom5Weight = m_Settings.Mip5Weight;
        constants.Bloom6Weight = m_Settings.Mip6Weight;
        m_ConstantBuffer->Update(&constants, sizeof(BloomConstants));

        // 设置视口
        D3D12_VIEWPORT viewport = { 0, 0, static_cast<float>(outputWidth), static_cast<float>(outputHeight), 0, 1 };
        D3D12_RECT scissor = { 0, 0, static_cast<LONG>(outputWidth), static_cast<LONG>(outputHeight) };
        d3dCmdList->RSSetViewports(1, &viewport);
        d3dCmdList->RSSetScissorRects(1, &scissor);

        // 绘制 (使用最终 upsample 结果作为 bloom)
        d3dCmdList->SetPipelineState(m_CompositePSO->GetPipelineState());
        d3dCmdList->SetGraphicsRootDescriptorTable(1, m_UpsampleChain[0].SRV);
        d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3dCmdList->DrawInstanced(3, 1, 0, 0);

        // 转换回 Present
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        d3dCmdList->ResourceBarrier(1, &barrier);
    }
}
