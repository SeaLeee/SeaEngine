#include "Scene/SkyRenderer.h"
#include "Shader/ShaderCompiler.h"
#include "Core/Log.h"
#include <cmath>

namespace Sea
{
    SkyRenderer::SkyRenderer(Device& device)
        : m_Device(device)
    {
    }

    SkyRenderer::~SkyRenderer()
    {
        Shutdown();
    }

    bool SkyRenderer::Initialize()
    {
        if (!CreateConstantBuffer())
        {
            SEA_CORE_ERROR("SkyRenderer: Failed to create constant buffer");
            return false;
        }

        if (!CreatePipelines())
        {
            SEA_CORE_ERROR("SkyRenderer: Failed to create pipelines");
            return false;
        }

        UpdateSunDirection();
        SEA_CORE_INFO("SkyRenderer initialized");
        return true;
    }

    void SkyRenderer::Shutdown()
    {
        m_ConstantBuffer.reset();
        m_CloudsPSO.reset();
        m_SkyPSO.reset();
        m_RootSignature.reset();
    }

    bool SkyRenderer::CreatePipelines()
    {
        // 创建根签名
        RootSignatureDesc rsDesc;
        
        // Root parameter 0: Sky CBV at b0
        RootParameterDesc cbvParam;
        cbvParam.type = RootParameterDesc::CBV;
        cbvParam.shaderRegister = 0;
        cbvParam.registerSpace = 0;
        cbvParam.visibility = D3D12_SHADER_VISIBILITY_ALL;
        rsDesc.parameters.push_back(cbvParam);

        m_RootSignature = MakeScope<RootSignature>(m_Device, rsDesc);
        if (!m_RootSignature->Initialize())
        {
            SEA_CORE_ERROR("SkyRenderer: Failed to create root signature");
            return false;
        }

        // 编译天空顶点着色器
        ShaderCompileDesc vsDesc;
        vsDesc.filePath = "Shaders/Sky/Sky_VS.hlsl";
        vsDesc.entryPoint = "VSMain";
        vsDesc.stage = ShaderStage::Vertex;
        vsDesc.model = ShaderModel::SM_6_0;
        auto vsResult = ShaderCompiler::Compile(vsDesc);

        if (!vsResult.success)
        {
            SEA_CORE_ERROR("SkyRenderer: Failed to compile Sky_VS: {}", vsResult.errors);
            return false;
        }

        // 编译天空像素着色器 (不带云)
        ShaderCompileDesc skyPsDesc;
        skyPsDesc.filePath = "Shaders/Sky/Sky_PS.hlsl";
        skyPsDesc.entryPoint = "PSMain";
        skyPsDesc.stage = ShaderStage::Pixel;
        skyPsDesc.model = ShaderModel::SM_6_0;
        auto skyPsResult = ShaderCompiler::Compile(skyPsDesc);

        if (!skyPsResult.success)
        {
            SEA_CORE_ERROR("SkyRenderer: Failed to compile Sky_PS: {}", skyPsResult.errors);
            return false;
        }

        // 编译云层像素着色器
        ShaderCompileDesc cloudsPsDesc;
        cloudsPsDesc.filePath = "Shaders/Sky/Clouds_PS.hlsl";
        cloudsPsDesc.entryPoint = "PSMain";
        cloudsPsDesc.stage = ShaderStage::Pixel;
        cloudsPsDesc.model = ShaderModel::SM_6_0;
        auto cloudsPsResult = ShaderCompiler::Compile(cloudsPsDesc);

        if (!cloudsPsResult.success)
        {
            SEA_CORE_WARN("SkyRenderer: Failed to compile Clouds_PS: {} - Clouds disabled", cloudsPsResult.errors);
            // 继续，只是没有云
        }

        // 空输入布局 (全屏三角形在顶点着色器中生成)
        std::vector<D3D12_INPUT_ELEMENT_DESC> emptyLayout;

        // 创建天空 PSO
        GraphicsPipelineDesc skyPsoDesc;
        skyPsoDesc.rootSignature = m_RootSignature.get();
        skyPsoDesc.vertexShader = vsResult.bytecode;
        skyPsoDesc.pixelShader = skyPsResult.bytecode;
        skyPsoDesc.inputLayout = emptyLayout;
        skyPsoDesc.rtvFormats = { Format::R8G8B8A8_UNORM };
        skyPsoDesc.dsvFormat = Format::D32_FLOAT;
        skyPsoDesc.depthEnable = true;
        skyPsoDesc.depthWrite = false;
        skyPsoDesc.depthFunc = CompareFunc::LessEqual;
        skyPsoDesc.cullMode = CullMode::None;

        m_SkyPSO = PipelineState::CreateGraphics(m_Device, skyPsoDesc);
        if (!m_SkyPSO)
        {
            SEA_CORE_ERROR("SkyRenderer: Failed to create Sky PSO");
            return false;
        }

        // 创建云层 PSO
        if (cloudsPsResult.success)
        {
            GraphicsPipelineDesc cloudsPsoDesc = skyPsoDesc;
            cloudsPsoDesc.pixelShader = cloudsPsResult.bytecode;
            
            m_CloudsPSO = PipelineState::CreateGraphics(m_Device, cloudsPsoDesc);
            if (!m_CloudsPSO)
            {
                SEA_CORE_WARN("SkyRenderer: Failed to create Clouds PSO - Clouds disabled");
            }
        }

        SEA_CORE_INFO("SkyRenderer: Pipelines created");
        return true;
    }

    bool SkyRenderer::CreateConstantBuffer()
    {
        BufferDesc cbDesc;
        cbDesc.size = (sizeof(SkyConstants) + 255) & ~255;
        cbDesc.type = BufferType::Constant;

        m_ConstantBuffer = MakeScope<Buffer>(m_Device, cbDesc);
        if (!m_ConstantBuffer->Initialize(nullptr))
        {
            return false;
        }

        return true;
    }

    void SkyRenderer::Update(f32 deltaTime)
    {
        m_TotalTime += deltaTime;
        
        // 自动时间推进
        if (m_AutoTimeProgress)
        {
            m_TimeOfDay += deltaTime * 0.1f; // 每秒 0.1 小时
            if (m_TimeOfDay >= 24.0f)
                m_TimeOfDay -= 24.0f;
            
            UpdateSunDirection();
        }
    }

    void SkyRenderer::SetTimeOfDay(float hours)
    {
        m_TimeOfDay = fmodf(hours, 24.0f);
        if (m_TimeOfDay < 0) m_TimeOfDay += 24.0f;
        UpdateSunDirection();
    }

    void SkyRenderer::SetSunAzimuth(float degrees)
    {
        m_SunAzimuth = fmodf(degrees, 360.0f);
        if (m_SunAzimuth < 0) m_SunAzimuth += 360.0f;
        UpdateSunDirection();
    }

    void SkyRenderer::SetSunElevation(float degrees)
    {
        m_SunElevation = std::max(-90.0f, std::min(90.0f, degrees));
        UpdateSunDirection();
    }

    void SkyRenderer::UpdateSunDirection()
    {
        // 从时间计算太阳位置
        // 12:00 = 太阳在最高点
        // 6:00 和 18:00 = 日出/日落
        float hourAngle = (m_TimeOfDay - 12.0f) * 15.0f; // 每小时 15 度
        
        // 使用太阳仰角和方位角
        float elevation = m_SunElevation;
        float azimuth = m_SunAzimuth;
        
        // 如果使用时间自动模式，从时间计算
        if (m_AutoTimeProgress)
        {
            // 简化的太阳轨迹
            elevation = 90.0f - std::abs(hourAngle);
            elevation = std::max(-20.0f, elevation); // 夜间太阳在地平线下
            azimuth = (m_TimeOfDay < 12.0f) ? 90.0f : 270.0f; // 东升西落
        }
        
        // 转换为弧度
        float elevRad = elevation * 3.14159265f / 180.0f;
        float azimRad = azimuth * 3.14159265f / 180.0f;
        
        // 计算太阳方向向量
        float cosElev = cosf(elevRad);
        m_Settings.SunDirection.x = cosElev * sinf(azimRad);
        m_Settings.SunDirection.y = sinf(elevRad);
        m_Settings.SunDirection.z = cosElev * cosf(azimRad);
        
        // 根据太阳高度调整颜色
        if (elevation < 0)
        {
            // 夜间 - 月光色调
            m_Settings.SunColor = { 0.3f, 0.35f, 0.5f };
            m_Settings.SunIntensity = 0.5f;
        }
        else if (elevation < 10)
        {
            // 日出/日落
            float t = elevation / 10.0f;
            m_Settings.SunColor = { 1.0f, 0.5f + t * 0.3f, 0.3f + t * 0.4f };
            m_Settings.SunIntensity = 5.0f + t * 5.0f;
        }
        else
        {
            // 白天
            m_Settings.SunColor = { 1.0f, 0.95f, 0.85f };
            m_Settings.SunIntensity = 10.0f;
        }
    }

    void SkyRenderer::Render(CommandList& cmdList, Camera& camera)
    {
        if (!m_Settings.EnableSky)
            return;

        if (!m_SkyPSO)
        {
            SEA_CORE_WARN("SkyRenderer::Render - No PSO available!");
            return;
        }

        // 更新常量缓冲区
        SkyConstants constants = {};
        
        // 计算逆视图投影矩阵
        XMMATRIX view = XMLoadFloat4x4(&camera.GetViewMatrix());
        XMMATRIX proj = XMLoadFloat4x4(&camera.GetProjectionMatrix());
        XMMATRIX viewProj = view * proj;
        XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);
        XMStoreFloat4x4(&constants.InvViewProj, XMMatrixTranspose(invViewProj));
        
        constants.CameraPosition = camera.GetPosition();
        constants.Time = m_TotalTime;
        constants.SunDirection = m_Settings.SunDirection;
        constants.SunIntensity = m_Settings.SunIntensity;
        constants.SunColor = m_Settings.SunColor;
        constants.AtmosphereScale = m_Settings.AtmosphereScale;
        constants.GroundColor = m_Settings.GroundColor;
        constants.CloudCoverage = m_Settings.EnableClouds ? m_Settings.CloudCoverage : 0.0f;
        constants.CloudDensity = m_Settings.CloudDensity;
        constants.CloudHeight = m_Settings.CloudHeight;

        m_ConstantBuffer->Update(&constants, sizeof(SkyConstants));

        // 渲染
        auto* d3dCmdList = cmdList.GetCommandList();
        d3dCmdList->SetGraphicsRootSignature(m_RootSignature->GetRootSignature());
        
        // 选择带云或不带云的 PSO
        if (m_Settings.EnableClouds && m_CloudsPSO)
        {
            d3dCmdList->SetPipelineState(m_CloudsPSO->GetPipelineState());
        }
        else
        {
            d3dCmdList->SetPipelineState(m_SkyPSO->GetPipelineState());
        }
        
        d3dCmdList->SetGraphicsRootConstantBufferView(0, m_ConstantBuffer->GetGPUAddress());
        d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        
        // 绘制全屏三角形 (3 个顶点)
        d3dCmdList->DrawInstanced(3, 1, 0, 0);
    }
}
