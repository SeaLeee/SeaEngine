#include "Scene/DeferredRenderer.h"
#include "Scene/SimpleRenderer.h"  // For SceneObject
#include "Shader/ShaderCompiler.h"
#include "Core/Log.h"

namespace Sea
{
    DeferredRenderer::DeferredRenderer(Device& device)
        : m_Device(device)
    {
    }

    DeferredRenderer::~DeferredRenderer()
    {
        Shutdown();
    }

    bool DeferredRenderer::Initialize(u32 width, u32 height)
    {
        m_Width = width;
        m_Height = height;

        if (!CreateConstantBuffers())
        {
            SEA_CORE_ERROR("DeferredRenderer: Failed to create constant buffers");
            return false;
        }

        if (!CreatePipelines())
        {
            SEA_CORE_ERROR("DeferredRenderer: Failed to create pipelines");
            return false;
        }

        if (!CreateGBufferResources(width, height))
        {
            SEA_CORE_ERROR("DeferredRenderer: Failed to create G-Buffer resources");
            return false;
        }

        SEA_CORE_INFO("DeferredRenderer initialized ({}x{})", width, height);
        return true;
    }

    void DeferredRenderer::Shutdown()
    {
        ReleaseGBufferResources();
        m_GBufferConstantBuffer.reset();
        m_GBufferObjectConstantBuffer.reset();
        m_LightingConstantBuffer.reset();
        m_GBufferPSO.reset();
        m_GBufferWireframePSO.reset();
        m_LightingPSO.reset();
        m_GBufferRootSignature.reset();
        m_LightingRootSignature.reset();
    }

    bool DeferredRenderer::RecompileShaders()
    {
        SEA_CORE_INFO("Recompiling DeferredRenderer shaders...");
        
        // 释放旧的 PSO（保留 Root Signature）
        m_GBufferPSO.reset();
        m_GBufferWireframePSO.reset();
        m_LightingPSO.reset();
        m_GBufferRootSignature.reset();
        m_LightingRootSignature.reset();
        
        // 重新创建 Pipelines
        if (!CreatePipelines())
        {
            SEA_CORE_ERROR("Failed to recompile DeferredRenderer shaders");
            return false;
        }
        
        SEA_CORE_INFO("DeferredRenderer shaders recompiled successfully");
        return true;
    }

    void DeferredRenderer::Resize(u32 width, u32 height)
    {
        if (m_Width == width && m_Height == height)
            return;

        ReleaseGBufferResources();
        CreateGBufferResources(width, height);
        m_Width = width;
        m_Height = height;
    }

    bool DeferredRenderer::CreateConstantBuffers()
    {
        // G-Buffer frame constants
        BufferDesc cbDesc;
        cbDesc.size = (sizeof(GBufferConstants) + 255) & ~255;
        cbDesc.type = BufferType::Constant;
        
        m_GBufferConstantBuffer = MakeScope<Buffer>(m_Device, cbDesc);
        if (!m_GBufferConstantBuffer->Initialize(nullptr))
            return false;

        // G-Buffer object constants (for all objects)
        cbDesc.size = OBJECT_CB_ALIGNMENT * MAX_OBJECTS_PER_FRAME;
        m_GBufferObjectConstantBuffer = MakeScope<Buffer>(m_Device, cbDesc);
        if (!m_GBufferObjectConstantBuffer->Initialize(nullptr))
            return false;

        // Lighting pass constants
        cbDesc.size = (sizeof(LightingConstants) + 255) & ~255;
        m_LightingConstantBuffer = MakeScope<Buffer>(m_Device, cbDesc);
        if (!m_LightingConstantBuffer->Initialize(nullptr))
            return false;

        return true;
    }

    bool DeferredRenderer::CreatePipelines()
    {
        // ========== G-Buffer Root Signature ==========
        RootSignatureDesc gbufferRsDesc;
        
        // Root 0: Frame CBV (b0)
        RootParameterDesc frameCbv;
        frameCbv.type = RootParameterDesc::CBV;
        frameCbv.shaderRegister = 0;
        frameCbv.registerSpace = 0;
        frameCbv.visibility = D3D12_SHADER_VISIBILITY_ALL;
        gbufferRsDesc.parameters.push_back(frameCbv);

        // Root 1: Object CBV (b1)
        RootParameterDesc objectCbv;
        objectCbv.type = RootParameterDesc::CBV;
        objectCbv.shaderRegister = 1;
        objectCbv.registerSpace = 0;
        objectCbv.visibility = D3D12_SHADER_VISIBILITY_ALL;
        gbufferRsDesc.parameters.push_back(objectCbv);

        gbufferRsDesc.flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        m_GBufferRootSignature = MakeScope<RootSignature>(m_Device, gbufferRsDesc);
        if (!m_GBufferRootSignature->Initialize())
        {
            SEA_CORE_ERROR("DeferredRenderer: Failed to create G-Buffer root signature");
            return false;
        }

        // ========== Lighting Root Signature ==========
        RootSignatureDesc lightingRsDesc;
        lightingRsDesc.flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        // Root 0: Lighting CBV (b0)
        RootParameterDesc lightingCbv;
        lightingCbv.type = RootParameterDesc::CBV;
        lightingCbv.shaderRegister = 0;
        lightingCbv.registerSpace = 0;
        lightingCbv.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
        lightingRsDesc.parameters.push_back(lightingCbv);

        // Root 1: G-Buffer SRVs (t0-t3) as descriptor table
        RootParameterDesc gbufferSrvs;
        gbufferSrvs.type = RootParameterDesc::DescriptorTable;
        gbufferSrvs.shaderRegister = 0;
        gbufferSrvs.registerSpace = 0;
        gbufferSrvs.numDescriptors = GBufferLayout::COUNT;  // 4 个 G-Buffer 纹理
        gbufferSrvs.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        gbufferSrvs.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
        lightingRsDesc.parameters.push_back(gbufferSrvs);

        // Static sampler
        D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.ShaderRegister = 0;
        samplerDesc.RegisterSpace = 0;
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        lightingRsDesc.staticSamplers.push_back(samplerDesc);

        m_LightingRootSignature = MakeScope<RootSignature>(m_Device, lightingRsDesc);
        if (!m_LightingRootSignature->Initialize())
        {
            SEA_CORE_ERROR("DeferredRenderer: Failed to create Lighting root signature");
            return false;
        }

        // ========== Compile G-Buffer Shaders ==========
        ShaderCompileDesc vsDesc;
        vsDesc.filePath = "Shaders/Deferred/GBuffer_VS.hlsl";
        vsDesc.entryPoint = "VSMain";
        vsDesc.stage = ShaderStage::Vertex;
        vsDesc.model = ShaderModel::SM_6_0;
        auto vsResult = ShaderCompiler::Compile(vsDesc);

        if (!vsResult.success)
        {
            SEA_CORE_ERROR("DeferredRenderer: Failed to compile GBuffer_VS: {}", vsResult.errors);
            return false;
        }

        ShaderCompileDesc psDesc;
        psDesc.filePath = "Shaders/Deferred/GBuffer_PS.hlsl";
        psDesc.entryPoint = "PSMain";
        psDesc.stage = ShaderStage::Pixel;
        psDesc.model = ShaderModel::SM_6_0;
        auto psResult = ShaderCompiler::Compile(psDesc);

        if (!psResult.success)
        {
            SEA_CORE_ERROR("DeferredRenderer: Failed to compile GBuffer_PS: {}", psResult.errors);
            return false;
        }

        // G-Buffer PSO
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        GraphicsPipelineDesc gbufferPsoDesc;
        gbufferPsoDesc.rootSignature = m_GBufferRootSignature.get();
        gbufferPsoDesc.vertexShader = vsResult.bytecode;
        gbufferPsoDesc.pixelShader = psResult.bytecode;
        gbufferPsoDesc.inputLayout = inputLayout;
        gbufferPsoDesc.rtvFormats = { 
            Format::R8G8B8A8_UNORM,      // Albedo + Metallic
            Format::R16G16B16A16_FLOAT,  // Normal + Roughness
            Format::R32G32B32A32_FLOAT,  // Position + AO
            Format::R16G16B16A16_FLOAT   // Emissive
        };
        gbufferPsoDesc.dsvFormat = Format::D32_FLOAT;
        gbufferPsoDesc.depthEnable = true;
        gbufferPsoDesc.depthWrite = true;
        gbufferPsoDesc.depthFunc = CompareFunc::Less;
        gbufferPsoDesc.cullMode = CullMode::Back;

        m_GBufferPSO = PipelineState::CreateGraphics(m_Device, gbufferPsoDesc);
        if (!m_GBufferPSO)
        {
            SEA_CORE_ERROR("DeferredRenderer: Failed to create G-Buffer PSO");
            return false;
        }

        // ========== G-Buffer Wireframe PSO ==========
        GraphicsPipelineDesc wireframePsoDesc = gbufferPsoDesc;
        wireframePsoDesc.fillMode = FillMode::Wireframe;
        m_GBufferWireframePSO = PipelineState::CreateGraphics(m_Device, wireframePsoDesc);
        if (!m_GBufferWireframePSO)
        {
            SEA_CORE_WARN("DeferredRenderer: Failed to create G-Buffer Wireframe PSO");
        }

        // ========== Compile Lighting Shaders ==========
        ShaderCompileDesc lightingVsDesc;
        lightingVsDesc.filePath = "Shaders/Fullscreen_VS.hlsl";
        lightingVsDesc.entryPoint = "VSMain";
        lightingVsDesc.stage = ShaderStage::Vertex;
        lightingVsDesc.model = ShaderModel::SM_6_0;
        auto lightingVsResult = ShaderCompiler::Compile(lightingVsDesc);

        if (!lightingVsResult.success)
        {
            SEA_CORE_ERROR("DeferredRenderer: Failed to compile Fullscreen_VS: {}", lightingVsResult.errors);
            return false;
        }

        ShaderCompileDesc lightingPsDesc;
        lightingPsDesc.filePath = "Shaders/Deferred/DeferredLighting_PS.hlsl";
        lightingPsDesc.entryPoint = "PSMain";
        lightingPsDesc.stage = ShaderStage::Pixel;
        lightingPsDesc.model = ShaderModel::SM_6_0;
        auto lightingPsResult = ShaderCompiler::Compile(lightingPsDesc);

        if (!lightingPsResult.success)
        {
            SEA_CORE_ERROR("DeferredRenderer: Failed to compile DeferredLighting_PS: {}", lightingPsResult.errors);
            return false;
        }

        // Lighting PSO
        GraphicsPipelineDesc lightingPsoDesc;
        lightingPsoDesc.rootSignature = m_LightingRootSignature.get();
        lightingPsoDesc.vertexShader = lightingVsResult.bytecode;
        lightingPsoDesc.pixelShader = lightingPsResult.bytecode;
        lightingPsoDesc.inputLayout = {};  // Fullscreen triangle
        lightingPsoDesc.rtvFormats = { Format::R16G16B16A16_FLOAT };  // HDR output for post-processing
        lightingPsoDesc.dsvFormat = Format::Unknown;
        lightingPsoDesc.depthEnable = false;
        lightingPsoDesc.depthWrite = false;
        lightingPsoDesc.cullMode = CullMode::None;

        m_LightingPSO = PipelineState::CreateGraphics(m_Device, lightingPsoDesc);
        if (!m_LightingPSO)
        {
            SEA_CORE_ERROR("DeferredRenderer: Failed to create Lighting PSO");
            return false;
        }

        SEA_CORE_INFO("DeferredRenderer: Pipelines created");
        return true;
    }

    bool DeferredRenderer::CreateGBufferResources(u32 width, u32 height)
    {
        m_Width = width;
        m_Height = height;

        auto* device = m_Device.GetDevice();

        // 创建描述符堆
        DescriptorHeapDesc rtvHeapDesc;
        rtvHeapDesc.type = DescriptorHeapType::RTV;
        rtvHeapDesc.numDescriptors = GBufferLayout::COUNT;
        rtvHeapDesc.shaderVisible = false;

        m_RTVHeap = MakeScope<DescriptorHeap>(m_Device, rtvHeapDesc);
        if (!m_RTVHeap->Initialize())
            return false;

        DescriptorHeapDesc dsvHeapDesc;
        dsvHeapDesc.type = DescriptorHeapType::DSV;
        dsvHeapDesc.numDescriptors = 1;
        dsvHeapDesc.shaderVisible = false;

        m_DSVHeap = MakeScope<DescriptorHeap>(m_Device, dsvHeapDesc);
        if (!m_DSVHeap->Initialize())
            return false;

        DescriptorHeapDesc srvHeapDesc;
        srvHeapDesc.type = DescriptorHeapType::CBV_SRV_UAV;
        srvHeapDesc.numDescriptors = GBufferLayout::COUNT + 1;  // +1 for depth
        srvHeapDesc.shaderVisible = true;

        m_SRVHeap = MakeScope<DescriptorHeap>(m_Device, srvHeapDesc);
        if (!m_SRVHeap->Initialize())
            return false;

        // G-Buffer 格式
        DXGI_FORMAT gbufferFormats[GBufferLayout::COUNT] = {
            DXGI_FORMAT_R8G8B8A8_UNORM,      // Albedo + Metallic
            DXGI_FORMAT_R16G16B16A16_FLOAT,  // Normal + Roughness
            DXGI_FORMAT_R32G32B32A32_FLOAT,  // Position + AO
            DXGI_FORMAT_R16G16B16A16_FLOAT   // Emissive
        };

        // 创建 G-Buffer 纹理
        for (u32 i = 0; i < GBufferLayout::COUNT; ++i)
        {
            auto& gb = m_GBuffer[i];

            D3D12_RESOURCE_DESC desc = {};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Width = width;
            desc.Height = height;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = gbufferFormats[i];
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

            D3D12_CLEAR_VALUE clearValue = {};
            clearValue.Format = gbufferFormats[i];

            HRESULT hr = device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                &clearValue,
                IID_PPV_ARGS(&gb.Resource)
            );

            if (FAILED(hr))
            {
                SEA_CORE_ERROR("DeferredRenderer: Failed to create G-Buffer {}", i);
                return false;
            }

            // RTV
            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.Format = gbufferFormats[i];
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            gb.RTV = m_RTVHeap->GetCPUHandle(i);
            device->CreateRenderTargetView(gb.Resource.Get(), &rtvDesc, gb.RTV);

            // SRV
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = gbufferFormats[i];
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2D.MipLevels = 1;

            auto cpuHandle = m_SRVHeap->GetCPUHandle(i);
            device->CreateShaderResourceView(gb.Resource.Get(), &srvDesc, cpuHandle);
            gb.SRV = m_SRVHeap->GetGPUHandle(i);
        }

        // 创建深度缓冲
        {
            D3D12_RESOURCE_DESC depthDesc = {};
            depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            depthDesc.Width = width;
            depthDesc.Height = height;
            depthDesc.DepthOrArraySize = 1;
            depthDesc.MipLevels = 1;
            depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
            depthDesc.SampleDesc.Count = 1;
            depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

            D3D12_CLEAR_VALUE clearValue = {};
            clearValue.Format = DXGI_FORMAT_D32_FLOAT;
            clearValue.DepthStencil.Depth = 1.0f;

            HRESULT hr = device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &depthDesc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &clearValue,
                IID_PPV_ARGS(&m_DepthBuffer)
            );

            if (FAILED(hr))
            {
                SEA_CORE_ERROR("DeferredRenderer: Failed to create depth buffer");
                return false;
            }

            // DSV
            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            m_DSV = m_DSVHeap->GetCPUHandle(0);
            device->CreateDepthStencilView(m_DepthBuffer.Get(), &dsvDesc, m_DSV);
        }

        SEA_CORE_INFO("DeferredRenderer: G-Buffer created ({}x{})", width, height);
        return true;
    }

    void DeferredRenderer::ReleaseGBufferResources()
    {
        for (auto& gb : m_GBuffer)
        {
            gb.Resource.Reset();
            gb.RTV = {};
            gb.SRV = {};
        }
        m_DepthBuffer.Reset();
        m_DSV = {};
        m_DepthSRV = {};
        m_RTVHeap.reset();
        m_DSVHeap.reset();
        m_SRVHeap.reset();
    }

    void DeferredRenderer::BeginGBufferPass(CommandList& cmdList, Camera& camera, float time)
    {
        auto* d3dCmdList = cmdList.GetCommandList();
        m_CurrentObjectIndex = 0;

        // 转换 G-Buffer 到 RenderTarget 状态
        std::vector<D3D12_RESOURCE_BARRIER> barriers(GBufferLayout::COUNT);
        for (u32 i = 0; i < GBufferLayout::COUNT; ++i)
        {
            barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[i].Transition.pResource = m_GBuffer[i].Resource.Get();
            barriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        }
        d3dCmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

        // 清除 G-Buffer
        float clearColor[4] = { 0, 0, 0, 0 };
        for (u32 i = 0; i < GBufferLayout::COUNT; ++i)
        {
            d3dCmdList->ClearRenderTargetView(m_GBuffer[i].RTV, clearColor, 0, nullptr);
        }
        d3dCmdList->ClearDepthStencilView(m_DSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        // 设置渲染目标
        D3D12_CPU_DESCRIPTOR_HANDLE rtvs[GBufferLayout::COUNT];
        for (u32 i = 0; i < GBufferLayout::COUNT; ++i)
            rtvs[i] = m_GBuffer[i].RTV;
        d3dCmdList->OMSetRenderTargets(GBufferLayout::COUNT, rtvs, FALSE, &m_DSV);

        // 设置视口和裁剪
        D3D12_VIEWPORT viewport = { 0, 0, static_cast<float>(m_Width), static_cast<float>(m_Height), 0, 1 };
        D3D12_RECT scissor = { 0, 0, static_cast<LONG>(m_Width), static_cast<LONG>(m_Height) };
        d3dCmdList->RSSetViewports(1, &viewport);
        d3dCmdList->RSSetScissorRects(1, &scissor);

        // 根据视图模式选择 PSO
        if (m_ViewMode == 1 && m_GBufferWireframePSO)
        {
            d3dCmdList->SetPipelineState(m_GBufferWireframePSO->GetPipelineState());
        }
        else
        {
            d3dCmdList->SetPipelineState(m_GBufferPSO->GetPipelineState());
        }
        d3dCmdList->SetGraphicsRootSignature(m_GBufferRootSignature->GetRootSignature());

        // 更新帧常量
        XMMATRIX view = XMLoadFloat4x4(&camera.GetViewMatrix());
        XMMATRIX proj = XMLoadFloat4x4(&camera.GetProjectionMatrix());
        XMMATRIX viewProj = view * proj;

        XMStoreFloat4x4(&m_FrameConstants.ViewProjection, XMMatrixTranspose(viewProj));
        XMStoreFloat4x4(&m_FrameConstants.View, XMMatrixTranspose(view));
        XMStoreFloat4x4(&m_FrameConstants.Projection, XMMatrixTranspose(proj));
        m_FrameConstants.CameraPosition = camera.GetPosition();
        m_FrameConstants.Time = time;

        m_GBufferConstantBuffer->Update(&m_FrameConstants, sizeof(GBufferConstants));
        d3dCmdList->SetGraphicsRootConstantBufferView(0, m_GBufferConstantBuffer->GetGPUAddress());

        d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    void DeferredRenderer::RenderObjectToGBuffer(CommandList& cmdList, const SceneObject& obj)
    {
        if (!obj.mesh || m_CurrentObjectIndex >= MAX_OBJECTS_PER_FRAME)
            return;

        auto* d3dCmdList = cmdList.GetCommandList();

        // 更新物体常量
        GBufferObjectConstants objConstants = {};
        XMMATRIX world = XMLoadFloat4x4(&obj.transform);
        XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
        
        XMMATRIX worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, world));
        XMStoreFloat4x4(&objConstants.WorldInvTranspose, XMMatrixTranspose(worldInvTranspose));
        
        objConstants.BaseColor = obj.color;
        objConstants.Metallic = obj.metallic;
        objConstants.Roughness = obj.roughness;
        objConstants.AO = obj.ao;
        objConstants.EmissiveIntensity = obj.emissiveIntensity;
        objConstants.EmissiveColor = obj.emissiveColor;

        // 写入到动态常量缓冲区
        u64 offset = m_CurrentObjectIndex * OBJECT_CB_ALIGNMENT;
        m_GBufferObjectConstantBuffer->Update(&objConstants, sizeof(GBufferObjectConstants), offset);

        D3D12_GPU_VIRTUAL_ADDRESS cbAddress = m_GBufferObjectConstantBuffer->GetGPUAddress() + offset;
        d3dCmdList->SetGraphicsRootConstantBufferView(1, cbAddress);

        // 绘制
        auto vbv = obj.mesh->GetVertexBuffer()->GetVertexBufferView();
        auto ibv = obj.mesh->GetIndexBuffer()->GetIndexBufferView();
        d3dCmdList->IASetVertexBuffers(0, 1, &vbv);
        d3dCmdList->IASetIndexBuffer(&ibv);
        d3dCmdList->DrawIndexedInstanced(obj.mesh->GetIndexCount(), 1, 0, 0, 0);

        m_CurrentObjectIndex++;
    }

    void DeferredRenderer::EndGBufferPass(CommandList& cmdList)
    {
        auto* d3dCmdList = cmdList.GetCommandList();

        // 转换 G-Buffer 到 ShaderResource 状态
        std::vector<D3D12_RESOURCE_BARRIER> barriers(GBufferLayout::COUNT);
        for (u32 i = 0; i < GBufferLayout::COUNT; ++i)
        {
            barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[i].Transition.pResource = m_GBuffer[i].Resource.Get();
            barriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        }
        d3dCmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    }

    void DeferredRenderer::LightingPass(CommandList& cmdList,
                                         D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
                                         ID3D12Resource* outputResource,
                                         u32 outputWidth, u32 outputHeight)
    {
        auto* d3dCmdList = cmdList.GetCommandList();

        // 设置渲染目标
        d3dCmdList->OMSetRenderTargets(1, &outputRTV, FALSE, nullptr);

        // 设置视口
        D3D12_VIEWPORT viewport = { 0, 0, static_cast<float>(outputWidth), static_cast<float>(outputHeight), 0, 1 };
        D3D12_RECT scissor = { 0, 0, static_cast<LONG>(outputWidth), static_cast<LONG>(outputHeight) };
        d3dCmdList->RSSetViewports(1, &viewport);
        d3dCmdList->RSSetScissorRects(1, &scissor);

        // 设置 PSO 和根签名
        d3dCmdList->SetPipelineState(m_LightingPSO->GetPipelineState());
        d3dCmdList->SetGraphicsRootSignature(m_LightingRootSignature->GetRootSignature());

        // 设置描述符堆
        ID3D12DescriptorHeap* heaps[] = { m_SRVHeap->GetHeap() };
        d3dCmdList->SetDescriptorHeaps(1, heaps);

        // 更新光照常量
        LightingConstants lightConstants = {};
        XMMATRIX viewProj = XMLoadFloat4x4(&m_FrameConstants.ViewProjection);
        XMMATRIX invViewProj = XMMatrixInverse(nullptr, XMMatrixTranspose(viewProj));
        XMStoreFloat4x4(&lightConstants.InvViewProjection, XMMatrixTranspose(invViewProj));
        lightConstants.CameraPosition = m_FrameConstants.CameraPosition;
        lightConstants.Time = m_FrameConstants.Time;
        lightConstants.LightDirection = m_LightDirection;
        lightConstants.LightIntensity = m_LightIntensity;
        lightConstants.LightColor = m_LightColor;
        lightConstants.AmbientIntensity = m_Settings.AmbientIntensity;
        lightConstants.AmbientColor = m_AmbientColor;

        m_LightingConstantBuffer->Update(&lightConstants, sizeof(LightingConstants));
        d3dCmdList->SetGraphicsRootConstantBufferView(0, m_LightingConstantBuffer->GetGPUAddress());

        // G-Buffer SRVs
        d3dCmdList->SetGraphicsRootDescriptorTable(1, m_GBuffer[0].SRV);

        // 绘制全屏三角形
        d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3dCmdList->DrawInstanced(3, 1, 0, 0);
    }

    ID3D12Resource* DeferredRenderer::GetGBufferResource(u32 index) const
    {
        if (index < GBufferLayout::COUNT)
            return m_GBuffer[index].Resource.Get();
        return nullptr;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DeferredRenderer::GetGBufferSRV(u32 index) const
    {
        if (index < GBufferLayout::COUNT)
            return m_GBuffer[index].SRV;
        return {};
    }
}
