#include "Ocean.h"
#include "Core/Log.h"
#include "Graphics/CommandList.h"
#include "Shader/ShaderCompiler.h"

#include <cmath>

using namespace DirectX;

namespace Sea
{
    // 常量缓冲区结构
    struct OceanCBData
    {
        XMFLOAT4X4 viewProj;
        XMFLOAT4X4 world;
        XMFLOAT4 cameraPos;
        XMFLOAT4 oceanParams;   // patch, grid, time, unused
        XMFLOAT4 sunDirection;
        XMFLOAT4 oceanColor;
        XMFLOAT4 skyColor;
    };

    struct OceanComputeCBData
    {
        XMFLOAT4 windDirection;  // xy = dir, z = speed, w = amplitude
        XMFLOAT4 timeParams;     // x = time, y = choppiness, z = gridSize, w = patchSize
        XMUINT4 resolution;      // x = N, y = log2(N), zw = unused
    };

    Ocean::Ocean(Device& device)
        : m_Device(device)
    {
    }

    bool Ocean::Initialize(const OceanParams& params)
    {
        m_Params = params;
        SEA_CORE_INFO("Initializing Ocean simulation ({}x{})", params.resolution, params.resolution);

        if (!CreateTextures())
        {
            SEA_CORE_ERROR("Failed to create ocean textures");
            return false;
        }

        if (!CreateOceanMesh())
        {
            SEA_CORE_ERROR("Failed to create ocean mesh");
            return false;
        }

        if (!CreateRenderPipeline())
        {
            SEA_CORE_ERROR("Failed to create ocean render pipeline");
            return false;
        }

        m_Initialized = true;
        SEA_CORE_INFO("Ocean simulation initialized successfully");
        return true;
    }

    bool Ocean::CreateTextures()
    {
        u32 N = m_Params.resolution;

        // 位移贴图 (RGBA32F - xyz displacement, w unused)
        TextureDesc dispDesc;
        dispDesc.width = N;
        dispDesc.height = N;
        dispDesc.format = Format::R32G32B32A32_FLOAT;
        dispDesc.usage = TextureUsage::ShaderResource;
        dispDesc.name = "OceanDisplacement";

        m_DisplacementTexture = MakeScope<Texture>(m_Device, dispDesc);
        if (!m_DisplacementTexture->Initialize())
            return false;

        // 法线贴图 (RGBA8 - xyz normal, w foam)
        TextureDesc normDesc;
        normDesc.width = N;
        normDesc.height = N;
        normDesc.format = Format::R8G8B8A8_UNORM;
        normDesc.usage = TextureUsage::ShaderResource;
        normDesc.name = "OceanNormal";

        m_NormalTexture = MakeScope<Texture>(m_Device, normDesc);
        if (!m_NormalTexture->Initialize())
            return false;

        // 创建常量缓冲
        BufferDesc cbDesc;
        cbDesc.size = sizeof(OceanCBData);
        cbDesc.type = BufferType::Constant;
        cbDesc.name = "OceanCB";

        m_OceanCB = MakeScope<Buffer>(m_Device, cbDesc);
        if (!m_OceanCB->Initialize())
            return false;

        return true;
    }

    bool Ocean::CreateOceanMesh()
    {
        // 创建海面网格
        const u32 gridRes = 128;  // 网格分辨率
        const f32 size = m_Params.gridSize;
        const f32 halfSize = size * 0.5f;
        const f32 cellSize = size / static_cast<f32>(gridRes);

        std::vector<Vertex> vertices;
        std::vector<u32> indices;

        // 生成顶点
        for (u32 z = 0; z <= gridRes; ++z)
        {
            for (u32 x = 0; x <= gridRes; ++x)
            {
                Vertex v;
                v.position = {
                    -halfSize + x * cellSize,
                    0.0f,
                    -halfSize + z * cellSize
                };
                v.normal = { 0.0f, 1.0f, 0.0f };
                v.texCoord = {
                    static_cast<f32>(x) / gridRes,
                    static_cast<f32>(z) / gridRes
                };
                v.color = { 1.0f, 1.0f, 1.0f, 1.0f };
                vertices.push_back(v);
            }
        }

        // 生成索引
        for (u32 z = 0; z < gridRes; ++z)
        {
            for (u32 x = 0; x < gridRes; ++x)
            {
                u32 topLeft = z * (gridRes + 1) + x;
                u32 topRight = topLeft + 1;
                u32 bottomLeft = (z + 1) * (gridRes + 1) + x;
                u32 bottomRight = bottomLeft + 1;

                // 两个三角形
                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);

                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);
            }
        }

        m_OceanMesh = MakeScope<Mesh>();
        if (!m_OceanMesh->CreateFromVertices(m_Device, vertices, indices))
        {
            SEA_CORE_ERROR("Failed to create ocean mesh");
            return false;
        }
        return true;
    }

    bool Ocean::CreateRenderPipeline()
    {
        // 创建根签名
        RootSignatureDesc rsDesc;
        rsDesc.flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        // Root parameter 0: OceanCB at b0
        RootParameterDesc cbvParam;
        cbvParam.type = RootParameterDesc::CBV;
        cbvParam.shaderRegister = 0;
        cbvParam.registerSpace = 0;
        cbvParam.visibility = D3D12_SHADER_VISIBILITY_ALL;
        rsDesc.parameters.push_back(cbvParam);

        m_RenderRootSig = MakeScope<RootSignature>(m_Device, rsDesc);
        if (!m_RenderRootSig->Initialize())
        {
            SEA_CORE_ERROR("Failed to create Ocean root signature");
            return false;
        }

        // 编译着色器
        std::string vsPath = "Shaders/Ocean/OceanGerstner_VS.hlsl";
        std::string psPath = "Shaders/Ocean/OceanGerstner_PS.hlsl";
        
        ShaderCompileDesc vsDesc;
        vsDesc.filePath = vsPath;
        vsDesc.entryPoint = "VSMain";
        vsDesc.stage = ShaderStage::Vertex;
        vsDesc.model = ShaderModel::SM_6_0;
        auto vsResult = ShaderCompiler::Compile(vsDesc);
        
        ShaderCompileDesc psDesc;
        psDesc.filePath = psPath;
        psDesc.entryPoint = "PSMain";
        psDesc.stage = ShaderStage::Pixel;
        psDesc.model = ShaderModel::SM_6_0;
        auto psResult = ShaderCompiler::Compile(psDesc);
        
        if (!vsResult.success || !psResult.success)
        {
            SEA_CORE_ERROR("Failed to compile Ocean shaders: VS={}, PS={}", 
                          vsResult.errors, psResult.errors);
            return false;
        }

        // 输入布局 - 必须与 Vertex 结构匹配
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // 创建PSO
        GraphicsPipelineDesc psoDesc;
        psoDesc.rootSignature = m_RenderRootSig.get();
        psoDesc.vertexShader = vsResult.bytecode;
        psoDesc.pixelShader = psResult.bytecode;
        psoDesc.inputLayout = inputLayout;
        psoDesc.rtvFormats = { Format::R16G16B16A16_FLOAT };  // HDR output for post-processing
        psoDesc.dsvFormat = Format::D32_FLOAT;
        psoDesc.depthEnable = true;
        psoDesc.depthWrite = true;
        psoDesc.cullMode = CullMode::None;  // 双面渲染海面
        psoDesc.fillMode = FillMode::Solid;

        m_RenderPSO = PipelineState::CreateGraphics(m_Device, psoDesc);
        if (!m_RenderPSO)
        {
            SEA_CORE_ERROR("Failed to create Ocean PSO");
            return false;
        }

        SEA_CORE_INFO("Ocean render pipeline created successfully");
        return true;
    }

    void Ocean::Update(f32 deltaTime, CommandList& cmdList)
    {
        if (!m_Initialized) return;

        m_Time += deltaTime;

        // 简化版本：在CPU上生成基于Gerstner波的位移数据
        // 真正的FFT版本需要在GPU上执行，这里先用简化方案
        
        // 更新位移和法线贴图的内容
        // （完整实现需要Compute Shader执行FFT）
    }

    void Ocean::Render(CommandList& cmdList, const Camera& camera)
    {
        if (!m_Initialized || !m_OceanMesh) return;

        auto* cmdListPtr = cmdList.GetCommandList();

        // 更新常量缓冲
        OceanCBData cbData;
        
        XMMATRIX view = XMLoadFloat4x4(&camera.GetViewMatrix());
        XMMATRIX proj = XMLoadFloat4x4(&camera.GetProjectionMatrix());
        XMMATRIX viewProj = view * proj;
        XMStoreFloat4x4(&cbData.viewProj, viewProj);
        
        // 海面世界变换（放在原点）
        XMStoreFloat4x4(&cbData.world, XMMatrixIdentity());
        
        XMFLOAT3 camPos = camera.GetPosition();
        cbData.cameraPos = { camPos.x, camPos.y, camPos.z, 1.0f };
        
        cbData.oceanParams = { m_Params.patchSize, m_Params.gridSize, m_Time, m_Params.amplitude };
        cbData.sunDirection = { m_SunDirection.x, m_SunDirection.y, m_SunDirection.z, 0.0f };
        cbData.oceanColor = m_OceanColor;
        cbData.skyColor = m_SkyColor;

        m_OceanCB->Update(&cbData, sizeof(cbData));

        // 设置管线状态
        cmdListPtr->SetPipelineState(m_RenderPSO->GetPipelineState());
        cmdListPtr->SetGraphicsRootSignature(m_RenderRootSig->GetRootSignature());

        // 绑定常量缓冲
        cmdListPtr->SetGraphicsRootConstantBufferView(0, m_OceanCB->GetGPUAddress());

        // 设置图元类型
        cmdListPtr->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // 绑定网格并绘制
        D3D12_VERTEX_BUFFER_VIEW vbv = m_OceanMesh->GetVertexBuffer()->GetVertexBufferView();
        D3D12_INDEX_BUFFER_VIEW ibv = m_OceanMesh->GetIndexBuffer()->GetIndexBufferView();
        
        cmdListPtr->IASetVertexBuffers(0, 1, &vbv);
        cmdListPtr->IASetIndexBuffer(&ibv);
        cmdListPtr->DrawIndexedInstanced(m_OceanMesh->GetIndexCount(), 1, 0, 0, 0);
    }

    // 以下是完整FFT实现所需的方法（待后续完善）
    bool Ocean::CreateComputePipelines()
    {
        // TODO: 实现完整的FFT计算管线
        return true;
    }

    void Ocean::RebuildSpectrum(CommandList& cmdList)
    {
        // TODO: 执行频谱初始化Compute Shader
    }

    void Ocean::UpdateSpectrum(CommandList& cmdList)
    {
        // TODO: 每帧更新频谱
    }

    void Ocean::PerformFFT(CommandList& cmdList, ID3D12Resource* input, ID3D12Resource* output)
    {
        // TODO: 执行FFT
    }

    void Ocean::GenerateNormals(CommandList& cmdList)
    {
        // TODO: 生成法线贴图
    }
}
