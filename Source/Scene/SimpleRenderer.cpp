#include "Scene/SimpleRenderer.h"
#include "Shader/ShaderCompiler.h"
#include "Core/Log.h"
#include "Core/FileSystem.h"

namespace Sea
{
    SimpleRenderer::SimpleRenderer(Device& device)
        : m_Device(device)
    {
    }

    SimpleRenderer::~SimpleRenderer()
    {
        Shutdown();
    }

    bool SimpleRenderer::Initialize()
    {
        if (!CreateRootSignature())
            return false;

        if (!CreatePipelineStates())
            return false;

        if (!CreateConstantBuffers())
            return false;

        SEA_CORE_INFO("SimpleRenderer initialized");
        return true;
    }

    void SimpleRenderer::Shutdown()
    {
        m_ObjectConstantBuffer.reset();
        m_FrameConstantBuffer.reset();
        m_GridPSO.reset();
        m_PBRPSO.reset();
        m_BasicPSO.reset();
        m_RootSignature.reset();
    }

    bool SimpleRenderer::CreateRootSignature()
    {
        RootSignatureDesc rsDesc;
        
        // Root parameter 0: PerFrame CBV at b0
        RootParameterDesc frameParam;
        frameParam.type = RootParameterDesc::CBV;
        frameParam.shaderRegister = 0;
        frameParam.registerSpace = 0;
        frameParam.visibility = D3D12_SHADER_VISIBILITY_ALL;
        rsDesc.parameters.push_back(frameParam);

        // Root parameter 1: PerObject CBV at b1
        RootParameterDesc objectParam;
        objectParam.type = RootParameterDesc::CBV;
        objectParam.shaderRegister = 1;
        objectParam.registerSpace = 0;
        objectParam.visibility = D3D12_SHADER_VISIBILITY_ALL;
        rsDesc.parameters.push_back(objectParam);

        // Static sampler
        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rsDesc.staticSamplers.push_back(sampler);

        m_RootSignature = MakeScope<RootSignature>(m_Device, rsDesc);
        if (!m_RootSignature->Initialize())
        {
            SEA_CORE_ERROR("Failed to create root signature");
            return false;
        }

        return true;
    }

    bool SimpleRenderer::CreatePipelineStates()
    {
        // 编译 Basic shader
        std::string basicShaderPath = "Shaders/Basic.hlsl";
        
        ShaderCompileDesc vsDesc;
        vsDesc.filePath = basicShaderPath;
        vsDesc.entryPoint = "VSMain";
        vsDesc.stage = ShaderStage::Vertex;
        vsDesc.model = ShaderModel::SM_6_0;
        auto vsResult = ShaderCompiler::Compile(vsDesc);

        ShaderCompileDesc psDesc;
        psDesc.filePath = basicShaderPath;
        psDesc.entryPoint = "PSMain";
        psDesc.stage = ShaderStage::Pixel;
        psDesc.model = ShaderModel::SM_6_0;
        auto psResult = ShaderCompiler::Compile(psDesc);

        if (!vsResult.success || !psResult.success)
        {
            SEA_CORE_ERROR("Failed to compile Basic shaders: {} {}", vsResult.errors, psResult.errors);
            return false;
        }

        // 顶点输入布局
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        // Basic PSO
        GraphicsPipelineDesc basicDesc;
        basicDesc.rootSignature = m_RootSignature.get();
        basicDesc.vertexShader = vsResult.bytecode;
        basicDesc.pixelShader = psResult.bytecode;
        basicDesc.inputLayout = inputLayout;
        basicDesc.rtvFormats = { Format::R8G8B8A8_UNORM };
        basicDesc.dsvFormat = Format::D32_FLOAT;
        basicDesc.depthEnable = true;
        basicDesc.depthWrite = true;
        basicDesc.cullMode = CullMode::Back;

        m_BasicPSO = PipelineState::CreateGraphics(m_Device, basicDesc);
        if (!m_BasicPSO)
        {
            SEA_CORE_ERROR("Failed to create Basic PSO");
            return false;
        }

        // Grid shader
        std::string gridShaderPath = "Shaders/Grid.hlsl";
        
        ShaderCompileDesc gridVsDesc;
        gridVsDesc.filePath = gridShaderPath;
        gridVsDesc.entryPoint = "VSMain";
        gridVsDesc.stage = ShaderStage::Vertex;
        gridVsDesc.model = ShaderModel::SM_6_0;
        auto gridVsResult = ShaderCompiler::Compile(gridVsDesc);

        ShaderCompileDesc gridPsDesc;
        gridPsDesc.filePath = gridShaderPath;
        gridPsDesc.entryPoint = "PSMain";
        gridPsDesc.stage = ShaderStage::Pixel;
        gridPsDesc.model = ShaderModel::SM_6_0;
        auto gridPsResult = ShaderCompiler::Compile(gridPsDesc);

        if (!gridVsResult.success || !gridPsResult.success)
        {
            SEA_CORE_ERROR("Failed to compile Grid shaders: {} {}", gridVsResult.errors, gridPsResult.errors);
            return false;
        }

        GraphicsPipelineDesc gridDesc;
        gridDesc.rootSignature = m_RootSignature.get();
        gridDesc.vertexShader = gridVsResult.bytecode;
        gridDesc.pixelShader = gridPsResult.bytecode;
        gridDesc.inputLayout = inputLayout;
        gridDesc.rtvFormats = { Format::R8G8B8A8_UNORM };
        gridDesc.dsvFormat = Format::D32_FLOAT;
        gridDesc.depthEnable = true;
        gridDesc.depthWrite = false;  // Grid 不写深度
        gridDesc.cullMode = CullMode::None;

        m_GridPSO = PipelineState::CreateGraphics(m_Device, gridDesc);
        if (!m_GridPSO)
        {
            SEA_CORE_ERROR("Failed to create Grid PSO");
            return false;
        }

        // PBR shader
        std::string pbrShaderPath = "Shaders/PBR.hlsl";
        
        ShaderCompileDesc pbrVsDesc;
        pbrVsDesc.filePath = pbrShaderPath;
        pbrVsDesc.entryPoint = "VSMainSimple";  // 使用简化版 (自动生成切线)
        pbrVsDesc.stage = ShaderStage::Vertex;
        pbrVsDesc.model = ShaderModel::SM_6_0;
        auto pbrVsResult = ShaderCompiler::Compile(pbrVsDesc);

        ShaderCompileDesc pbrPsDesc;
        pbrPsDesc.filePath = pbrShaderPath;
        pbrPsDesc.entryPoint = "PSMain";
        pbrPsDesc.stage = ShaderStage::Pixel;
        pbrPsDesc.model = ShaderModel::SM_6_0;
        auto pbrPsResult = ShaderCompiler::Compile(pbrPsDesc);

        if (!pbrVsResult.success || !pbrPsResult.success)
        {
            SEA_CORE_WARN("Failed to compile PBR shaders: {} {} - falling back to Basic", 
                         pbrVsResult.errors, pbrPsResult.errors);
            // 如果 PBR 编译失败，使用 Basic 作为后备
            m_PBRPSO = m_BasicPSO;
        }
        else
        {
            GraphicsPipelineDesc pbrDesc;
            pbrDesc.rootSignature = m_RootSignature.get();
            pbrDesc.vertexShader = pbrVsResult.bytecode;
            pbrDesc.pixelShader = pbrPsResult.bytecode;
            pbrDesc.inputLayout = inputLayout;
            pbrDesc.rtvFormats = { Format::R8G8B8A8_UNORM };
            pbrDesc.dsvFormat = Format::D32_FLOAT;
            pbrDesc.depthEnable = true;
            pbrDesc.depthWrite = true;
            pbrDesc.cullMode = CullMode::Back;

            m_PBRPSO = PipelineState::CreateGraphics(m_Device, pbrDesc);
            if (!m_PBRPSO)
            {
                SEA_CORE_WARN("Failed to create PBR PSO, falling back to Basic");
                m_PBRPSO = m_BasicPSO;
            }
            else
            {
                SEA_CORE_INFO("PBR pipeline created successfully");
            }
        }

        return true;
    }

    bool SimpleRenderer::CreateConstantBuffers()
    {
        // Frame constant buffer
        BufferDesc frameDesc;
        frameDesc.size = (sizeof(FrameConstants) + 255) & ~255;  // 256-byte aligned
        frameDesc.type = BufferType::Constant;
        m_FrameConstantBuffer = MakeScope<Buffer>(m_Device, frameDesc);
        if (!m_FrameConstantBuffer->Initialize(nullptr))
        {
            SEA_CORE_ERROR("Failed to create frame constant buffer");
            return false;
        }

        // Object constant buffer - 足够大以容纳所有对象
        // 每个对象的常量数据需要 256 字节对齐
        BufferDesc objDesc;
        objDesc.size = OBJECT_CB_ALIGNMENT * MAX_OBJECTS_PER_FRAME;
        objDesc.type = BufferType::Constant;
        m_ObjectConstantBuffer = MakeScope<Buffer>(m_Device, objDesc);
        if (!m_ObjectConstantBuffer->Initialize(nullptr))
        {
            SEA_CORE_ERROR("Failed to create object constant buffer");
            return false;
        }

        return true;
    }

    void SimpleRenderer::BeginFrame(Camera& camera, f32 time)
    {
        camera.Update();
        
        // 重置对象索引
        m_CurrentObjectIndex = 0;

        m_FrameConstants.View = camera.GetViewMatrix();
        m_FrameConstants.Projection = camera.GetProjectionMatrix();
        m_FrameConstants.ViewProjection = camera.GetViewProjectionMatrix();
        m_FrameConstants.CameraPosition = camera.GetPosition();
        m_FrameConstants.Time = time;
        m_FrameConstants.LightDirection = m_LightDirection;
        m_FrameConstants.LightColor = m_LightColor;
        m_FrameConstants.LightIntensity = m_LightIntensity;
        m_FrameConstants.AmbientColor = m_AmbientColor;

        m_FrameConstantBuffer->Update(&m_FrameConstants, sizeof(FrameConstants));
    }

    void SimpleRenderer::RenderObject(CommandList& cmdList, const SceneObject& obj)
    {
        if (!obj.mesh) return;
        
        // 检查对象索引是否超出限制
        if (m_CurrentObjectIndex >= MAX_OBJECTS_PER_FRAME)
        {
            SEA_CORE_WARN("Too many objects to render in one frame!");
            return;
        }

        // 更新 object constants
        ObjectConstants objConst = {};
        objConst.World = obj.transform;
        
        // 计算逆转置矩阵用于法线变换
        XMMATRIX world = XMLoadFloat4x4(&obj.transform);
        XMMATRIX worldInvTrans = XMMatrixTranspose(XMMatrixInverse(nullptr, world));
        XMStoreFloat4x4(&objConst.WorldInvTranspose, worldInvTrans);
        
        // 使用材质或直接参数
        if (obj.material)
        {
            const auto& params = obj.material->GetParams();
            objConst.BaseColor = params.albedo;
            objConst.Metallic = params.metallic;
            objConst.Roughness = params.roughness;
            objConst.AO = params.ao;
            objConst.EmissiveIntensity = params.emissiveIntensity;
            objConst.EmissiveColor = params.emissiveColor;
            objConst.NormalScale = params.normalScale;
        }
        else
        {
            objConst.BaseColor = obj.color;
            objConst.Metallic = obj.metallic;
            objConst.Roughness = obj.roughness;
            objConst.AO = obj.ao;
            objConst.EmissiveIntensity = obj.emissiveIntensity;
            objConst.EmissiveColor = obj.emissiveColor;
            objConst.NormalScale = 1.0f;
        }
        objConst.TextureFlags = 0;  // 暂时不使用贴图

        // 计算当前对象在缓冲区中的偏移量
        u32 objectOffset = m_CurrentObjectIndex * OBJECT_CB_ALIGNMENT;
        m_ObjectConstantBuffer->Update(&objConst, sizeof(ObjectConstants), objectOffset);

        // 设置管线状态
        auto* d3dCmdList = cmdList.GetCommandList();
        d3dCmdList->SetGraphicsRootSignature(m_RootSignature->GetRootSignature());
        
        // 选择 PBR 或 Basic 管线
        if (m_UsePBR && m_PBRPSO)
        {
            d3dCmdList->SetPipelineState(m_PBRPSO->GetPipelineState());
        }
        else
        {
            d3dCmdList->SetPipelineState(m_BasicPSO->GetPipelineState());
        }

        // 设置常量缓冲 - 使用偏移后的地址
        d3dCmdList->SetGraphicsRootConstantBufferView(0, m_FrameConstantBuffer->GetGPUAddress());
        d3dCmdList->SetGraphicsRootConstantBufferView(1, m_ObjectConstantBuffer->GetGPUAddress() + objectOffset);

        // 设置顶点和索引缓冲
        D3D12_VERTEX_BUFFER_VIEW vbv = obj.mesh->GetVertexBuffer()->GetVertexBufferView();
        D3D12_INDEX_BUFFER_VIEW ibv = obj.mesh->GetIndexBuffer()->GetIndexBufferView();
        d3dCmdList->IASetVertexBuffers(0, 1, &vbv);
        d3dCmdList->IASetIndexBuffer(&ibv);
        d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // 绘制
        d3dCmdList->DrawIndexedInstanced(obj.mesh->GetIndexCount(), 1, 0, 0, 0);
        
        // 增加对象索引
        m_CurrentObjectIndex++;
    }

    void SimpleRenderer::RenderGrid(CommandList& cmdList, Mesh& gridMesh)
    {
        if (m_CurrentObjectIndex >= MAX_OBJECTS_PER_FRAME)
        {
            SEA_CORE_WARN("Too many objects to render grid!");
            return;
        }
        
        auto* d3dCmdList = cmdList.GetCommandList();
        d3dCmdList->SetGraphicsRootSignature(m_RootSignature->GetRootSignature());
        d3dCmdList->SetPipelineState(m_GridPSO->GetPipelineState());

        d3dCmdList->SetGraphicsRootConstantBufferView(0, m_FrameConstantBuffer->GetGPUAddress());

        // 创建一个单位世界矩阵
        ObjectConstants objConst = {};
        XMStoreFloat4x4(&objConst.World, XMMatrixIdentity());
        XMStoreFloat4x4(&objConst.WorldInvTranspose, XMMatrixIdentity());
        objConst.BaseColor = { 1, 1, 1, 1 };
        
        // 使用偏移量
        u32 objectOffset = m_CurrentObjectIndex * OBJECT_CB_ALIGNMENT;
        m_ObjectConstantBuffer->Update(&objConst, sizeof(ObjectConstants), objectOffset);
        d3dCmdList->SetGraphicsRootConstantBufferView(1, m_ObjectConstantBuffer->GetGPUAddress() + objectOffset);

        D3D12_VERTEX_BUFFER_VIEW vbv = gridMesh.GetVertexBuffer()->GetVertexBufferView();
        D3D12_INDEX_BUFFER_VIEW ibv = gridMesh.GetIndexBuffer()->GetIndexBufferView();
        d3dCmdList->IASetVertexBuffers(0, 1, &vbv);
        d3dCmdList->IASetIndexBuffer(&ibv);
        d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        d3dCmdList->DrawIndexedInstanced(gridMesh.GetIndexCount(), 1, 0, 0, 0);
        
        m_CurrentObjectIndex++;
    }
}
