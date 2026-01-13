#include "Scene/TonemapRenderer.h"
#include "Shader/ShaderCompiler.h"
#include "Core/Log.h"

namespace Sea
{
    TonemapRenderer::TonemapRenderer(Device& device)
        : m_Device(device)
    {
    }

    TonemapRenderer::~TonemapRenderer()
    {
        Shutdown();
    }

    bool TonemapRenderer::Initialize()
    {
        if (!CreateConstantBuffer())
        {
            SEA_CORE_ERROR("TonemapRenderer: Failed to create constant buffer");
            return false;
        }

        if (!CreatePipeline())
        {
            SEA_CORE_ERROR("TonemapRenderer: Failed to create pipeline");
            return false;
        }

        SEA_CORE_INFO("TonemapRenderer initialized");
        return true;
    }

    void TonemapRenderer::Shutdown()
    {
        m_ConstantBuffer.reset();
        m_PSO.reset();
        m_RootSignature.reset();
        m_SRVHeap.reset();
    }

    bool TonemapRenderer::CreateConstantBuffer()
    {
        BufferDesc cbDesc;
        cbDesc.size = (sizeof(TonemapConstants) + 255) & ~255;
        cbDesc.type = BufferType::Constant;

        m_ConstantBuffer = MakeScope<Buffer>(m_Device, cbDesc);
        if (!m_ConstantBuffer->Initialize(nullptr))
        {
            return false;
        }

        // 创建 SRV 描述符堆（用于绑定 HDR 和 Bloom 纹理）
        DescriptorHeapDesc srvHeapDesc;
        srvHeapDesc.type = DescriptorHeapType::CBV_SRV_UAV;
        srvHeapDesc.numDescriptors = 4;  // 预留足够空间
        srvHeapDesc.shaderVisible = true;

        m_SRVHeap = MakeScope<DescriptorHeap>(m_Device, srvHeapDesc);
        if (!m_SRVHeap->Initialize())
        {
            SEA_CORE_ERROR("TonemapRenderer: Failed to create SRV heap");
            return false;
        }

        return true;
    }

    bool TonemapRenderer::CreatePipeline()
    {
        // 创建根签名
        // Root Parameter 0: CBV (b1) - Tonemap constants
        // Root Parameter 1: Descriptor Table for SRV (t0, t1) - HDR + Bloom
        RootSignatureDesc rsDesc;
        rsDesc.flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        // CBV at b1 (与 shader 匹配)
        RootParameterDesc cbvParam;
        cbvParam.type = RootParameterDesc::CBV;
        cbvParam.shaderRegister = 1;  // b1
        cbvParam.registerSpace = 0;
        cbvParam.visibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rsDesc.parameters.push_back(cbvParam);

        // SRV Table at t0-t1 (HDR + Bloom)
        RootParameterDesc srvParam;
        srvParam.type = RootParameterDesc::DescriptorTable;
        srvParam.shaderRegister = 0;
        srvParam.registerSpace = 0;
        srvParam.numDescriptors = 2;  // t0 和 t1
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
            SEA_CORE_ERROR("TonemapRenderer: Failed to create root signature");
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
            SEA_CORE_ERROR("TonemapRenderer: Failed to compile Fullscreen_VS: {}", vsResult.errors);
            return false;
        }

        // 编译 Tonemap 像素着色器
        ShaderCompileDesc psDesc;
        psDesc.filePath = "Shaders/Tonemap_PS.hlsl";
        psDesc.entryPoint = "main";
        psDesc.stage = ShaderStage::Pixel;
        psDesc.model = ShaderModel::SM_6_0;
        auto psResult = ShaderCompiler::Compile(psDesc);

        if (!psResult.success)
        {
            SEA_CORE_ERROR("TonemapRenderer: Failed to compile Tonemap_PS: {}", psResult.errors);
            return false;
        }

        // 创建 PSO
        GraphicsPipelineDesc psoDesc;
        psoDesc.rootSignature = m_RootSignature.get();
        psoDesc.vertexShader = vsResult.bytecode;
        psoDesc.pixelShader = psResult.bytecode;
        psoDesc.inputLayout = {};  // 全屏三角形无需输入布局
        psoDesc.rtvFormats = { Format::R8G8B8A8_UNORM };  // LDR 输出
        psoDesc.dsvFormat = Format::Unknown;
        psoDesc.depthEnable = false;
        psoDesc.depthWrite = false;
        psoDesc.cullMode = CullMode::None;

        m_PSO = PipelineState::CreateGraphics(m_Device, psoDesc);
        if (!m_PSO)
        {
            SEA_CORE_ERROR("TonemapRenderer: Failed to create PSO");
            return false;
        }

        SEA_CORE_INFO("TonemapRenderer: Pipeline created");
        return true;
    }

    void TonemapRenderer::Render(CommandList& cmdList,
                                  D3D12_GPU_DESCRIPTOR_HANDLE inputSRV,
                                  D3D12_GPU_DESCRIPTOR_HANDLE bloomSRV,
                                  D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
                                  ID3D12Resource* outputResource,
                                  u32 outputWidth, u32 outputHeight)
    {
        if (!m_Settings.Enabled)
            return;

        auto* d3dCmdList = cmdList.GetCommandList();

        // 转换输出资源到 RenderTarget 状态
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = outputResource;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        d3dCmdList->ResourceBarrier(1, &barrier);

        // 更新常量缓冲区
        TonemapConstants constants = {};
        constants.Exposure = m_Settings.Exposure;
        constants.Gamma = m_Settings.Gamma;
        constants.TonemapOperator = m_Settings.Operator;
        constants.BloomIntensity = m_Settings.BloomIntensity;
        constants.BloomTintR = m_Settings.BloomTintR;
        constants.BloomTintG = m_Settings.BloomTintG;
        constants.BloomTintB = m_Settings.BloomTintB;
        constants.BloomEnabled = m_Settings.BloomEnabled ? 1.0f : 0.0f;

        m_ConstantBuffer->Update(&constants, sizeof(TonemapConstants));

        // 设置渲染目标
        d3dCmdList->OMSetRenderTargets(1, &outputRTV, FALSE, nullptr);

        // 设置视口
        D3D12_VIEWPORT viewport = { 0, 0, static_cast<float>(outputWidth), 
                                    static_cast<float>(outputHeight), 0, 1 };
        D3D12_RECT scissor = { 0, 0, static_cast<LONG>(outputWidth), 
                               static_cast<LONG>(outputHeight) };
        d3dCmdList->RSSetViewports(1, &viewport);
        d3dCmdList->RSSetScissorRects(1, &scissor);

        // 设置 PSO 和根签名
        d3dCmdList->SetPipelineState(m_PSO->GetPipelineState());
        d3dCmdList->SetGraphicsRootSignature(m_RootSignature->GetRootSignature());

        // 设置常量缓冲区
        d3dCmdList->SetGraphicsRootConstantBufferView(0, m_ConstantBuffer->GetGPUAddress());

        // 设置描述符堆和 SRV
        // 注意：调用者需要确保描述符堆已设置且 SRV 正确绑定
        d3dCmdList->SetGraphicsRootDescriptorTable(1, inputSRV);

        // 绘制全屏三角形
        d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3dCmdList->DrawInstanced(3, 1, 0, 0);

        // 转换输出资源回 Common 状态
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        d3dCmdList->ResourceBarrier(1, &barrier);
    }
}
