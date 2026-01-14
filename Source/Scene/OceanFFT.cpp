// OceanFFT.cpp
// FFT-based Ocean Wave Simulation Implementation
// Based on Jerry Tessendorf's "Simulating Ocean Water"
//         and GodotOceanWaves by 2Retr0

#include "OceanFFT.h"
#include "Core/Log.h"
#include "Graphics/CommandList.h"
#include "Shader/ShaderCompiler.h"

#include <cmath>
#include <random>

using namespace DirectX;

namespace Sea
{
    // ========================================================================
    // Constants
    // ========================================================================
    static constexpr u32 FFT_THREAD_GROUP_SIZE = 256;
    static constexpr u32 SPECTRUM_THREAD_GROUP_SIZE = 8;
    static constexpr u32 TRANSPOSE_TILE_SIZE = 16;

    // ========================================================================
    // Constant Buffer Structures
    // ========================================================================
    struct SpectrumComputeCB
    {
        i32 seedX, seedY;
        f32 tileLengthX, tileLengthY;
        f32 alpha;
        f32 peakFrequency;
        f32 windSpeed;
        f32 windAngle;
        f32 depth;
        f32 swell;
        f32 detail;
        f32 spread;
        u32 cascadeIndex;
        f32 _padding[3];
    };

    struct ModulateCB
    {
        f32 tileLengthX, tileLengthY;
        f32 depth;
        f32 time;
        u32 cascadeIndex;
        f32 _padding[3];
    };

    struct FFTCB
    {
        u32 stage;
        u32 direction;
        u32 spectrumIndex;
        u32 cascadeIndex;
        u32 mapSize;
        u32 logN;
        u32 pingPong;
        f32 _padding;
    };

    struct TransposeCB
    {
        u32 mapSize;
        u32 spectrumIndex;
        u32 cascadeIndex;
        f32 _padding;
    };

    struct UnpackCB
    {
        u32 mapSize;
        u32 cascadeIndex;
        f32 whitecap;
        f32 foamGrowRate;
        f32 foamDecayRate;
        f32 _padding[3];
    };

    // ========================================================================
    // Static Helper Functions
    // ========================================================================
    
    static u32 Log2(u32 n)
    {
        u32 result = 0;
        while ((1u << result) < n) result++;
        return result;
    }

    f32 OceanFFT::JONSWAPAlpha(f32 windSpeed, f32 fetchLength)
    {
        // fetchLength in km, convert to meters
        f32 F = fetchLength * 1000.0f;
        f32 U = windSpeed;
        constexpr f32 g = 9.81f;
        
        // α = 0.076 * (U²/(F*g))^0.22
        return 0.076f * std::pow((U * U) / (F * g), 0.22f);
    }

    f32 OceanFFT::JONSWAPPeakFrequency(f32 windSpeed, f32 fetchLength)
    {
        // fetchLength in km, convert to meters
        f32 F = fetchLength * 1000.0f;
        f32 U = windSpeed;
        constexpr f32 g = 9.81f;
        
        // ω_p = 22 * (g²/(U*F))^(1/3)
        return 22.0f * std::pow((g * g) / (U * F), 1.0f / 3.0f);
    }

    // ========================================================================
    // Constructor / Destructor
    // ========================================================================
    
    OceanFFT::OceanFFT(Device& device)
        : m_Device(device)
    {
        // Initialize default cascade parameters
        // Cascade 0: Large waves (swell)
        m_Params.cascades[0].tileLength = 250.0f;
        m_Params.cascades[0].windSpeed = 20.0f;
        m_Params.cascades[0].swell = 0.8f;
        m_Params.cascades[0].spread = 0.1f;
        m_Params.cascades[0].displacementScale = 1.0f;
        m_Params.cascades[0].normalScale = 0.5f;
        
        // Cascade 1: Medium waves
        m_Params.cascades[1].tileLength = 50.0f;
        m_Params.cascades[1].windSpeed = 15.0f;
        m_Params.cascades[1].swell = 0.5f;
        m_Params.cascades[1].spread = 0.3f;
        m_Params.cascades[1].displacementScale = 0.5f;
        m_Params.cascades[1].normalScale = 1.0f;
        
        // Cascade 2: Small waves (chop)
        m_Params.cascades[2].tileLength = 10.0f;
        m_Params.cascades[2].windSpeed = 10.0f;
        m_Params.cascades[2].swell = 0.2f;
        m_Params.cascades[2].spread = 0.5f;
        m_Params.cascades[2].displacementScale = 0.2f;
        m_Params.cascades[2].normalScale = 1.5f;
        
        // Cascade 3: Detail ripples
        m_Params.cascades[3].tileLength = 2.0f;
        m_Params.cascades[3].windSpeed = 5.0f;
        m_Params.cascades[3].swell = 0.1f;
        m_Params.cascades[3].spread = 0.7f;
        m_Params.cascades[3].displacementScale = 0.05f;
        m_Params.cascades[3].normalScale = 2.0f;
        
        // Random seeds for reproducibility
        std::random_device rd;
        for (u32 i = 0; i < OceanFFTParams::MAX_CASCADES; ++i)
        {
            m_Params.cascades[i].spectrumSeedX = rd() % 10000;
            m_Params.cascades[i].spectrumSeedY = rd() % 10000;
        }
    }

    OceanFFT::~OceanFFT()
    {
        Shutdown();
    }

    // ========================================================================
    // Initialize / Shutdown
    // ========================================================================
    
    bool OceanFFT::Initialize(const OceanFFTParams& params)
    {
        m_Params = params;
        
        // Validate map size is power of 2
        if ((m_Params.mapSize & (m_Params.mapSize - 1)) != 0)
        {
            SEA_CORE_ERROR("OceanFFT: Map size must be power of 2, got {}", m_Params.mapSize);
            return false;
        }
        
        SEA_CORE_INFO("Initializing FFT Ocean simulation ({}x{}, {} cascades)", 
            m_Params.mapSize, m_Params.mapSize, m_Params.numCascades);
        
        if (!CreateTextures())
        {
            SEA_CORE_ERROR("OceanFFT: Failed to create textures");
            return false;
        }
        
        if (!CreateBuffers())
        {
            SEA_CORE_ERROR("OceanFFT: Failed to create buffers");
            return false;
        }
        
        if (!CreateDescriptorHeaps())
        {
            SEA_CORE_ERROR("OceanFFT: Failed to create descriptor heaps");
            return false;
        }
        
        if (!CreateComputePipelines())
        {
            SEA_CORE_ERROR("OceanFFT: Failed to create compute pipelines");
            return false;
        }
        
        if (!CreateRenderPipeline())
        {
            SEA_CORE_ERROR("OceanFFT: Failed to create render pipeline");
            return false;
        }
        
        if (!CreateMesh())
        {
            SEA_CORE_ERROR("OceanFFT: Failed to create ocean mesh");
            return false;
        }
        
        m_Initialized = true;
        SEA_CORE_INFO("FFT Ocean simulation initialized successfully");
        return true;
    }

    void OceanFFT::Shutdown()
    {
        if (!m_Initialized) return;
        
        m_SpectrumTexture.reset();
        m_FFTBuffer.reset();
        m_ButterflyFactors.reset();
        m_DisplacementMaps.reset();
        m_NormalMaps.reset();
        
        m_SpectrumComputeRS.reset();
        m_SpectrumModulateRS.reset();
        m_FFTButterflyRS.reset();
        m_FFTComputeRS.reset();
        m_TransposeRS.reset();
        m_UnpackRS.reset();
        
        m_SpectrumComputePSO.reset();
        m_SpectrumModulatePSO.reset();
        m_FFTButterflyPSO.reset();
        m_FFTComputePSO.reset();
        m_TransposePSO.reset();
        m_UnpackPSO.reset();
        
        m_ComputeUAVHeap.reset();
        m_ComputeSRVHeap.reset();
        
        m_OceanMesh.reset();
        m_RenderCB.reset();
        m_RenderRS.reset();
        m_RenderPSO.reset();
        m_WireframePSO.reset();
        m_RenderSRVHeap.reset();
        
        m_Initialized = false;
    }

    // ========================================================================
    // Resource Creation
    // ========================================================================
    
    bool OceanFFT::CreateTextures()
    {
        u32 N = m_Params.mapSize;
        u32 cascades = m_Params.numCascades;
        
        // Initial spectrum texture (2D Array)
        // Format: RG16F (complex number: real, imaginary)
        TextureDesc specDesc;
        specDesc.width = N;
        specDesc.height = N;
        specDesc.arraySize = cascades;
        specDesc.format = Format::R16G16B16A16_FLOAT;  // Use RGBA16F for compatibility
        specDesc.usage = TextureUsage::ShaderResource | TextureUsage::UnorderedAccess;
        specDesc.name = "OceanSpectrum";
        
        m_SpectrumTexture = MakeScope<Texture>(m_Device, specDesc);
        if (!m_SpectrumTexture->Initialize())
        {
            SEA_CORE_ERROR("Failed to create spectrum texture");
            return false;
        }
        
        // Displacement maps (2D Array)
        // Format: RGBA16F (XYZ displacement, W unused)
        TextureDesc dispDesc;
        dispDesc.width = N;
        dispDesc.height = N;
        dispDesc.arraySize = cascades;
        dispDesc.format = Format::R16G16B16A16_FLOAT;
        dispDesc.usage = TextureUsage::ShaderResource | TextureUsage::UnorderedAccess;
        dispDesc.name = "OceanDisplacement";
        
        m_DisplacementMaps = MakeScope<Texture>(m_Device, dispDesc);
        if (!m_DisplacementMaps->Initialize())
        {
            SEA_CORE_ERROR("Failed to create displacement maps");
            return false;
        }
        
        // Normal/Foam maps (2D Array)
        // Format: RGBA16F (gradient.xy, dhx_dx, foam) - matches GodotOceanWaves
        TextureDesc normDesc;
        normDesc.width = N;
        normDesc.height = N;
        normDesc.arraySize = cascades;
        normDesc.format = Format::R16G16B16A16_FLOAT;
        normDesc.usage = TextureUsage::ShaderResource | TextureUsage::UnorderedAccess;
        normDesc.name = "OceanNormalFoam";
        
        m_NormalMaps = MakeScope<Texture>(m_Device, normDesc);
        if (!m_NormalMaps->Initialize())
        {
            SEA_CORE_ERROR("Failed to create normal/foam maps");
            return false;
        }
        
        return true;
    }
    
    bool OceanFFT::CreateBuffers()
    {
        u32 N = m_Params.mapSize;
        u32 cascades = m_Params.numCascades;
        u32 logN = Log2(N);
        
        // FFT buffer: 4 spectra * N*N * 2 (ping-pong) * cascades * sizeof(float2)
        // But we can simplify by processing one cascade at a time
        // Layout: [cascade][spectrum][row][col] = [cascade][4][N][N]
        u64 fftBufferSize = cascades * 4 * N * N * sizeof(f32) * 2;
        
        BufferDesc fftDesc;
        fftDesc.size = fftBufferSize;
        fftDesc.type = BufferType::Structured;
        fftDesc.stride = sizeof(f32) * 2;  // float2
        fftDesc.name = "OceanFFTBuffer";
        
        m_FFTBuffer = MakeScope<Buffer>(m_Device, fftDesc);
        if (!m_FFTBuffer->Initialize())
        {
            SEA_CORE_ERROR("Failed to create FFT buffer");
            return false;
        }
        
        // Butterfly factors: logN * N * sizeof(float4)
        u64 butterflySize = logN * N * sizeof(f32) * 4;
        
        BufferDesc butterflyDesc;
        butterflyDesc.size = butterflySize;
        butterflyDesc.type = BufferType::Structured;
        butterflyDesc.stride = sizeof(f32) * 4;
        butterflyDesc.name = "OceanButterfly";
        
        m_ButterflyFactors = MakeScope<Buffer>(m_Device, butterflyDesc);
        if (!m_ButterflyFactors->Initialize())
        {
            SEA_CORE_ERROR("Failed to create butterfly buffer");
            return false;
        }
        
        // Render constant buffer
        BufferDesc cbDesc;
        cbDesc.size = sizeof(OceanRenderCB);
        cbDesc.type = BufferType::Constant;
        cbDesc.name = "OceanRenderCB";
        
        m_RenderCB = MakeScope<Buffer>(m_Device, cbDesc);
        if (!m_RenderCB->Initialize())
        {
            SEA_CORE_ERROR("Failed to create render constant buffer");
            return false;
        }
        
        return true;
    }
    
    bool OceanFFT::CreateDescriptorHeaps()
    {
        u32 N = m_Params.mapSize;
        u32 cascades = m_Params.numCascades;
        auto* d3dDevice = m_Device.GetDevice();
        
        // Compute heap - holds all UAVs and SRVs for compute shaders
        DescriptorHeapDesc computeHeapDesc;
        computeHeapDesc.type = DescriptorHeapType::CBV_SRV_UAV;
        computeHeapDesc.numDescriptors = 64;
        computeHeapDesc.shaderVisible = true;
        
        m_ComputeUAVHeap = MakeScope<DescriptorHeap>(m_Device, computeHeapDesc);
        if (!m_ComputeUAVHeap->Initialize())
        {
            SEA_CORE_ERROR("Failed to create compute UAV heap");
            return false;
        }
        
        m_ComputeSRVHeap = MakeScope<DescriptorHeap>(m_Device, computeHeapDesc);
        if (!m_ComputeSRVHeap->Initialize())
        {
            SEA_CORE_ERROR("Failed to create compute SRV heap");
            return false;
        }
        
        // Render SRV heap - for pixel shader texture access
        DescriptorHeapDesc renderHeapDesc;
        renderHeapDesc.type = DescriptorHeapType::CBV_SRV_UAV;
        renderHeapDesc.numDescriptors = 32;
        renderHeapDesc.shaderVisible = true;
        
        m_RenderSRVHeap = MakeScope<DescriptorHeap>(m_Device, renderHeapDesc);
        if (!m_RenderSRVHeap->Initialize())
        {
            SEA_CORE_ERROR("Failed to create render SRV heap");
            return false;
        }
        
        // ========== Create UAV descriptors in compute heap ==========
        u32 uavIndex = 0;
        
        // Spectrum texture UAV (array)
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Texture2DArray.MipSlice = 0;
            uavDesc.Texture2DArray.FirstArraySlice = 0;
            uavDesc.Texture2DArray.ArraySize = cascades;
            
            d3dDevice->CreateUnorderedAccessView(
                m_SpectrumTexture->GetResource(),
                nullptr,
                &uavDesc,
                m_ComputeUAVHeap->GetCPUHandle(uavIndex++));
            m_SpectrumUAVIndex = uavIndex - 1;
        }
        
        // Displacement maps UAV (array)
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Texture2DArray.MipSlice = 0;
            uavDesc.Texture2DArray.FirstArraySlice = 0;
            uavDesc.Texture2DArray.ArraySize = cascades;
            
            d3dDevice->CreateUnorderedAccessView(
                m_DisplacementMaps->GetResource(),
                nullptr,
                &uavDesc,
                m_ComputeUAVHeap->GetCPUHandle(uavIndex++));
            m_DisplacementUAVIndex = uavIndex - 1;
        }
        
        // Normal/Foam maps UAV (array) - gradient format for FFT output
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Texture2DArray.MipSlice = 0;
            uavDesc.Texture2DArray.FirstArraySlice = 0;
            uavDesc.Texture2DArray.ArraySize = cascades;
            
            d3dDevice->CreateUnorderedAccessView(
                m_NormalMaps->GetResource(),
                nullptr,
                &uavDesc,
                m_ComputeUAVHeap->GetCPUHandle(uavIndex++));
            m_NormalUAVIndex = uavIndex - 1;
        }
        
        // FFT buffer UAV (structured buffer)
        {
            u32 numElements = cascades * 4 * N * N;
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = DXGI_FORMAT_UNKNOWN;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.FirstElement = 0;
            uavDesc.Buffer.NumElements = numElements;
            uavDesc.Buffer.StructureByteStride = sizeof(f32) * 2;
            
            d3dDevice->CreateUnorderedAccessView(
                m_FFTBuffer->GetResource(),
                nullptr,
                &uavDesc,
                m_ComputeUAVHeap->GetCPUHandle(uavIndex++));
            m_FFTBufferUAVIndex = uavIndex - 1;
        }
        
        // Butterfly factors UAV
        {
            u32 logN = Log2(N);
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = DXGI_FORMAT_UNKNOWN;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.FirstElement = 0;
            uavDesc.Buffer.NumElements = logN * N;
            uavDesc.Buffer.StructureByteStride = sizeof(f32) * 4;
            
            d3dDevice->CreateUnorderedAccessView(
                m_ButterflyFactors->GetResource(),
                nullptr,
                &uavDesc,
                m_ComputeUAVHeap->GetCPUHandle(uavIndex++));
            m_ButterflyUAVIndex = uavIndex - 1;
        }
        
        // ========== Create consecutive UAVs for Unpack shader (u0=FFTBuffer, u1=Displacement, u2=Normal) ==========
        m_UnpackUAVStartIndex = uavIndex;
        
        // FFT buffer UAV for unpack (same as m_FFTBufferUAVIndex but in consecutive group)
        {
            u32 numElements = cascades * 4 * N * N;
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = DXGI_FORMAT_UNKNOWN;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.FirstElement = 0;
            uavDesc.Buffer.NumElements = numElements;
            uavDesc.Buffer.StructureByteStride = sizeof(f32) * 2;
            
            d3dDevice->CreateUnorderedAccessView(
                m_FFTBuffer->GetResource(),
                nullptr,
                &uavDesc,
                m_ComputeUAVHeap->GetCPUHandle(uavIndex++));
        }
        
        // Displacement UAV for unpack
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Texture2DArray.MipSlice = 0;
            uavDesc.Texture2DArray.FirstArraySlice = 0;
            uavDesc.Texture2DArray.ArraySize = cascades;
            
            d3dDevice->CreateUnorderedAccessView(
                m_DisplacementMaps->GetResource(),
                nullptr,
                &uavDesc,
                m_ComputeUAVHeap->GetCPUHandle(uavIndex++));
        }
        
        // Normal UAV for unpack
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Texture2DArray.MipSlice = 0;
            uavDesc.Texture2DArray.FirstArraySlice = 0;
            uavDesc.Texture2DArray.ArraySize = cascades;
            
            d3dDevice->CreateUnorderedAccessView(
                m_NormalMaps->GetResource(),
                nullptr,
                &uavDesc,
                m_ComputeUAVHeap->GetCPUHandle(uavIndex++));
        }
        
        // ========== Create SRV descriptors in compute SRV heap ==========
        u32 srvIndex = 0;
        
        // Spectrum texture SRV
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2DArray.MipLevels = 1;
            srvDesc.Texture2DArray.ArraySize = cascades;
            
            d3dDevice->CreateShaderResourceView(
                m_SpectrumTexture->GetResource(),
                &srvDesc,
                m_ComputeSRVHeap->GetCPUHandle(srvIndex++));
            m_SpectrumSRVIndex = srvIndex - 1;
        }
        
        // FFT buffer SRV
        {
            u32 numElements = cascades * 4 * N * N;
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.FirstElement = 0;
            srvDesc.Buffer.NumElements = numElements;
            srvDesc.Buffer.StructureByteStride = sizeof(f32) * 2;
            
            d3dDevice->CreateShaderResourceView(
                m_FFTBuffer->GetResource(),
                &srvDesc,
                m_ComputeSRVHeap->GetCPUHandle(srvIndex++));
            m_FFTBufferSRVIndex = srvIndex - 1;
        }
        
        // Butterfly factors SRV
        {
            u32 logN = Log2(N);
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.FirstElement = 0;
            srvDesc.Buffer.NumElements = logN * N;
            srvDesc.Buffer.StructureByteStride = sizeof(f32) * 4;
            
            d3dDevice->CreateShaderResourceView(
                m_ButterflyFactors->GetResource(),
                &srvDesc,
                m_ComputeSRVHeap->GetCPUHandle(srvIndex++));
            m_ButterflySRVIndex = srvIndex - 1;
        }
        
        // ========== Create SRVs in render heap for pixel shader ==========
        u32 renderSrvIndex = 0;
        
        // Displacement maps SRV
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2DArray.MostDetailedMip = 0;
            srvDesc.Texture2DArray.MipLevels = 1;
            srvDesc.Texture2DArray.FirstArraySlice = 0;
            srvDesc.Texture2DArray.ArraySize = cascades;
            srvDesc.Texture2DArray.PlaneSlice = 0;
            srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
            
            d3dDevice->CreateShaderResourceView(
                m_DisplacementMaps->GetResource(),
                &srvDesc,
                m_RenderSRVHeap->GetCPUHandle(renderSrvIndex++));
        }
        
        // Normal/Foam maps SRV (gradient format for lighting)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2DArray.MostDetailedMip = 0;
            srvDesc.Texture2DArray.MipLevels = 1;
            srvDesc.Texture2DArray.FirstArraySlice = 0;
            srvDesc.Texture2DArray.ArraySize = cascades;
            srvDesc.Texture2DArray.PlaneSlice = 0;
            srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
            
            d3dDevice->CreateShaderResourceView(
                m_NormalMaps->GetResource(),
                &srvDesc,
                m_RenderSRVHeap->GetCPUHandle(renderSrvIndex++));
        }
        
        // Spectrum texture SRV (for debug visualization)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2DArray.MipLevels = 1;
            srvDesc.Texture2DArray.ArraySize = cascades;
            
            d3dDevice->CreateShaderResourceView(
                m_SpectrumTexture->GetResource(),
                &srvDesc,
                m_RenderSRVHeap->GetCPUHandle(renderSrvIndex++));
        }
        
        SEA_CORE_INFO("FFT Ocean descriptor heaps created with {} UAVs, {} compute SRVs, {} render SRVs",
            uavIndex, srvIndex, renderSrvIndex);
        
        return true;
    }
    
    bool OceanFFT::CreateComputePipelines()
    {
        SEA_CORE_INFO("Creating FFT Ocean compute pipelines...");
        
        // ========== Root Signatures ==========
        
        // Spectrum compute RS: Constants(b0), UAV(u0)
        {
            RootSignatureDesc desc;
            desc.flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
            
            RootParameterDesc constParam;
            constParam.type = RootParameterDesc::Constants;
            constParam.shaderRegister = 0;
            constParam.registerSpace = 0;
            constParam.num32BitValues = sizeof(SpectrumComputeCB) / 4;
            desc.parameters.push_back(constParam);
            
            RootParameterDesc uavParam;
            uavParam.type = RootParameterDesc::DescriptorTable;
            uavParam.shaderRegister = 0;
            uavParam.registerSpace = 0;
            uavParam.numDescriptors = 1;
            uavParam.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            desc.parameters.push_back(uavParam);
            
            m_SpectrumComputeRS = MakeScope<RootSignature>(m_Device, desc);
            if (!m_SpectrumComputeRS->Initialize())
            {
                SEA_CORE_ERROR("Failed to create spectrum compute root signature");
                return false;
            }
        }
        
        // Modulate RS: Constants(b0), SRV(t0), UAV(u0)
        {
            RootSignatureDesc desc;
            desc.flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
            
            RootParameterDesc constParam;
            constParam.type = RootParameterDesc::Constants;
            constParam.shaderRegister = 0;
            constParam.registerSpace = 0;
            constParam.num32BitValues = sizeof(ModulateCB) / 4;
            desc.parameters.push_back(constParam);
            
            RootParameterDesc srvParam;
            srvParam.type = RootParameterDesc::DescriptorTable;
            srvParam.shaderRegister = 0;
            srvParam.registerSpace = 0;
            srvParam.numDescriptors = 1;
            srvParam.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            desc.parameters.push_back(srvParam);
            
            RootParameterDesc uavParam;
            uavParam.type = RootParameterDesc::DescriptorTable;
            uavParam.shaderRegister = 0;
            uavParam.registerSpace = 0;
            uavParam.numDescriptors = 1;
            uavParam.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            desc.parameters.push_back(uavParam);
            
            m_SpectrumModulateRS = MakeScope<RootSignature>(m_Device, desc);
            if (!m_SpectrumModulateRS->Initialize())
            {
                SEA_CORE_ERROR("Failed to create modulate root signature");
                return false;
            }
        }
        
        // Butterfly RS: Constants(b0), UAV(u0)
        {
            RootSignatureDesc desc;
            desc.flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
            
            RootParameterDesc constParam;
            constParam.type = RootParameterDesc::Constants;
            constParam.shaderRegister = 0;
            constParam.registerSpace = 0;
            constParam.num32BitValues = 4;  // mapSize, logN, padding
            desc.parameters.push_back(constParam);
            
            RootParameterDesc uavParam;
            uavParam.type = RootParameterDesc::DescriptorTable;
            uavParam.shaderRegister = 0;
            uavParam.registerSpace = 0;
            uavParam.numDescriptors = 1;
            uavParam.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            desc.parameters.push_back(uavParam);
            
            m_FFTButterflyRS = MakeScope<RootSignature>(m_Device, desc);
            if (!m_FFTButterflyRS->Initialize())
            {
                SEA_CORE_ERROR("Failed to create butterfly root signature");
                return false;
            }
        }
        
        // FFT compute RS: Constants(b0), SRV(t0), UAV(u0-u1)
        {
            RootSignatureDesc desc;
            desc.flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
            
            RootParameterDesc constParam;
            constParam.type = RootParameterDesc::Constants;
            constParam.shaderRegister = 0;
            constParam.registerSpace = 0;
            constParam.num32BitValues = sizeof(FFTCB) / 4;
            desc.parameters.push_back(constParam);
            
            RootParameterDesc srvParam;
            srvParam.type = RootParameterDesc::DescriptorTable;
            srvParam.shaderRegister = 0;
            srvParam.registerSpace = 0;
            srvParam.numDescriptors = 1;
            srvParam.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            desc.parameters.push_back(srvParam);
            
            RootParameterDesc uavParam;
            uavParam.type = RootParameterDesc::DescriptorTable;
            uavParam.shaderRegister = 0;
            uavParam.registerSpace = 0;
            uavParam.numDescriptors = 2;
            uavParam.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            desc.parameters.push_back(uavParam);
            
            m_FFTComputeRS = MakeScope<RootSignature>(m_Device, desc);
            if (!m_FFTComputeRS->Initialize())
            {
                SEA_CORE_ERROR("Failed to create FFT compute root signature");
                return false;
            }
        }
        
        // Transpose RS: Constants(b0), UAV(u0-u1)
        {
            RootSignatureDesc desc;
            desc.flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
            
            RootParameterDesc constParam;
            constParam.type = RootParameterDesc::Constants;
            constParam.shaderRegister = 0;
            constParam.registerSpace = 0;
            constParam.num32BitValues = sizeof(TransposeCB) / 4;
            desc.parameters.push_back(constParam);
            
            RootParameterDesc uavParam;
            uavParam.type = RootParameterDesc::DescriptorTable;
            uavParam.shaderRegister = 0;
            uavParam.registerSpace = 0;
            uavParam.numDescriptors = 2;
            uavParam.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            desc.parameters.push_back(uavParam);
            
            m_TransposeRS = MakeScope<RootSignature>(m_Device, desc);
            if (!m_TransposeRS->Initialize())
            {
                SEA_CORE_ERROR("Failed to create transpose root signature");
                return false;
            }
        }
        
        // Unpack RS: Constants(b0), UAV(u0-u2) - FFT buffer, displacement, normal maps
        {
            RootSignatureDesc desc;
            desc.flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
            
            RootParameterDesc constParam;
            constParam.type = RootParameterDesc::Constants;
            constParam.shaderRegister = 0;
            constParam.registerSpace = 0;
            constParam.num32BitValues = sizeof(UnpackCB) / 4;
            desc.parameters.push_back(constParam);
            
            RootParameterDesc uavParam;
            uavParam.type = RootParameterDesc::DescriptorTable;
            uavParam.shaderRegister = 0;
            uavParam.registerSpace = 0;
            uavParam.numDescriptors = 3;  // FFT buffer, displacement, normal maps
            uavParam.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            desc.parameters.push_back(uavParam);
            
            m_UnpackRS = MakeScope<RootSignature>(m_Device, desc);
            if (!m_UnpackRS->Initialize())
            {
                SEA_CORE_ERROR("Failed to create unpack root signature");
                return false;
            }
        }
        
        // ========== Compile Shaders and Create PSOs ==========
        
        auto compileCS = [](const std::string& path) -> std::vector<u8> {
            ShaderCompileDesc desc;
            desc.filePath = path;
            desc.entryPoint = "main";
            desc.stage = ShaderStage::Compute;
            desc.model = ShaderModel::SM_6_0;
            
            auto result = ShaderCompiler::Compile(desc);
            if (!result.success)
            {
                SEA_CORE_ERROR("Failed to compile shader {}: {}", path, result.errors);
            }
            return result.bytecode;
        };
        
        // Spectrum compute PSO
        {
            auto cs = compileCS("Shaders/Ocean/FFT/OceanSpectrum_CS.hlsl");
            if (cs.empty()) return false;
            
            ComputePipelineDesc desc;
            desc.rootSignature = m_SpectrumComputeRS.get();
            desc.computeShader = std::move(cs);
            
            m_SpectrumComputePSO = PipelineState::CreateCompute(m_Device, desc);
            if (!m_SpectrumComputePSO)
            {
                SEA_CORE_ERROR("Failed to create spectrum compute PSO");
                return false;
            }
        }
        
        // Modulate PSO
        {
            auto cs = compileCS("Shaders/Ocean/FFT/OceanModulate_CS.hlsl");
            if (cs.empty()) return false;
            
            ComputePipelineDesc desc;
            desc.rootSignature = m_SpectrumModulateRS.get();
            desc.computeShader = std::move(cs);
            
            m_SpectrumModulatePSO = PipelineState::CreateCompute(m_Device, desc);
            if (!m_SpectrumModulatePSO)
            {
                SEA_CORE_ERROR("Failed to create modulate PSO");
                return false;
            }
        }
        
        // Butterfly PSO
        {
            auto cs = compileCS("Shaders/Ocean/FFT/FFTButterfly_CS.hlsl");
            if (cs.empty()) return false;
            
            ComputePipelineDesc desc;
            desc.rootSignature = m_FFTButterflyRS.get();
            desc.computeShader = std::move(cs);
            
            m_FFTButterflyPSO = PipelineState::CreateCompute(m_Device, desc);
            if (!m_FFTButterflyPSO)
            {
                SEA_CORE_ERROR("Failed to create butterfly PSO");
                return false;
            }
        }
        
        // FFT compute PSO
        {
            auto cs = compileCS("Shaders/Ocean/FFT/FFTCompute_CS.hlsl");
            if (cs.empty()) return false;
            
            ComputePipelineDesc desc;
            desc.rootSignature = m_FFTComputeRS.get();
            desc.computeShader = std::move(cs);
            
            m_FFTComputePSO = PipelineState::CreateCompute(m_Device, desc);
            if (!m_FFTComputePSO)
            {
                SEA_CORE_ERROR("Failed to create FFT compute PSO");
                return false;
            }
        }
        
        // Transpose PSO
        {
            auto cs = compileCS("Shaders/Ocean/FFT/FFTTranspose_CS.hlsl");
            if (cs.empty()) return false;
            
            ComputePipelineDesc desc;
            desc.rootSignature = m_TransposeRS.get();
            desc.computeShader = std::move(cs);
            
            m_TransposePSO = PipelineState::CreateCompute(m_Device, desc);
            if (!m_TransposePSO)
            {
                SEA_CORE_ERROR("Failed to create transpose PSO");
                return false;
            }
        }
        
        // Unpack PSO
        {
            auto cs = compileCS("Shaders/Ocean/FFT/FFTUnpack_CS.hlsl");
            if (cs.empty()) return false;
            
            ComputePipelineDesc desc;
            desc.rootSignature = m_UnpackRS.get();
            desc.computeShader = std::move(cs);
            
            m_UnpackPSO = PipelineState::CreateCompute(m_Device, desc);
            if (!m_UnpackPSO)
            {
                SEA_CORE_ERROR("Failed to create unpack PSO");
                return false;
            }
        }
        
        SEA_CORE_INFO("FFT Ocean compute pipelines created successfully");
        return true;
    }
    
    bool OceanFFT::CreateRenderPipeline()
    {
        SEA_CORE_INFO("Creating FFT Ocean render pipeline...");
        
        // Root signature
        RootSignatureDesc rsDesc;
        rsDesc.flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        
        // CBV at b0
        RootParameterDesc cbvParam;
        cbvParam.type = RootParameterDesc::CBV;
        cbvParam.shaderRegister = 0;
        cbvParam.registerSpace = 0;
        rsDesc.parameters.push_back(cbvParam);
        
        // Descriptor table for SRVs (displacement, normal maps)
        RootParameterDesc srvParam;
        srvParam.type = RootParameterDesc::DescriptorTable;
        srvParam.shaderRegister = 0;
        srvParam.registerSpace = 0;
        srvParam.numDescriptors = 3;
        srvParam.rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rsDesc.parameters.push_back(srvParam);
        
        // Static samplers
        D3D12_STATIC_SAMPLER_DESC linearWrap = {};
        linearWrap.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        linearWrap.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        linearWrap.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        linearWrap.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        linearWrap.ShaderRegister = 0;
        linearWrap.RegisterSpace = 0;
        linearWrap.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rsDesc.staticSamplers.push_back(linearWrap);
        
        D3D12_STATIC_SAMPLER_DESC linearClamp = linearWrap;
        linearClamp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        linearClamp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        linearClamp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        linearClamp.ShaderRegister = 1;
        rsDesc.staticSamplers.push_back(linearClamp);
        
        m_RenderRS = MakeScope<RootSignature>(m_Device, rsDesc);
        if (!m_RenderRS->Initialize())
        {
            SEA_CORE_ERROR("Failed to create ocean render root signature");
            return false;
        }
        
        // Compile shaders
        ShaderCompileDesc vsDesc;
        vsDesc.filePath = "Shaders/Ocean/OceanFFT_VS.hlsl";
        vsDesc.entryPoint = "main";
        vsDesc.stage = ShaderStage::Vertex;
        vsDesc.model = ShaderModel::SM_6_0;
        auto vsResult = ShaderCompiler::Compile(vsDesc);
        
        ShaderCompileDesc psDesc;
        psDesc.filePath = "Shaders/Ocean/OceanFFT_PS.hlsl";
        psDesc.entryPoint = "main";
        psDesc.stage = ShaderStage::Pixel;
        psDesc.model = ShaderModel::SM_6_0;
        auto psResult = ShaderCompiler::Compile(psDesc);
        
        if (!vsResult.success || !psResult.success)
        {
            SEA_CORE_ERROR("Failed to compile ocean render shaders: VS={}, PS={}", 
                          vsResult.errors, psResult.errors);
            return false;
        }
        
        // Input layout - must match Vertex struct (position, normal, texcoord, color)
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };
        
        // Graphics PSO (solid)
        GraphicsPipelineDesc psoDesc;
        psoDesc.rootSignature = m_RenderRS.get();
        psoDesc.vertexShader = vsResult.bytecode;
        psoDesc.pixelShader = psResult.bytecode;
        psoDesc.inputLayout = inputLayout;
        psoDesc.topology = PrimitiveTopology::TriangleList;
        psoDesc.fillMode = FillMode::Solid;
        psoDesc.cullMode = CullMode::None;  // Ocean is viewed from both sides
        psoDesc.depthEnable = true;
        psoDesc.depthWrite = true;
        psoDesc.depthFunc = CompareFunc::Less;
        psoDesc.rtvFormats = {Format::R16G16B16A16_FLOAT};  // HDR render target
        psoDesc.dsvFormat = Format::D32_FLOAT;
        
        m_RenderPSO = PipelineState::CreateGraphics(m_Device, psoDesc);
        if (!m_RenderPSO)
        {
            SEA_CORE_ERROR("Failed to create ocean render PSO");
            return false;
        }
        
        // Wireframe PSO
        psoDesc.fillMode = FillMode::Wireframe;
        m_WireframePSO = PipelineState::CreateGraphics(m_Device, psoDesc);
        if (!m_WireframePSO)
        {
            SEA_CORE_WARN("Failed to create ocean wireframe PSO");
            // Not critical, continue without it
        }
        
        SEA_CORE_INFO("FFT Ocean render pipeline created successfully");
        return true;
    }
    
    bool OceanFFT::CreateMesh()
    {
        // Create a large plane mesh for the ocean surface
        const u32 gridRes = 256;  // Resolution
        const f32 size = 1000.0f; // World size in meters
        const f32 halfSize = size * 0.5f;
        const f32 cellSize = size / static_cast<f32>(gridRes);
        
        std::vector<Vertex> vertices;
        std::vector<u32> indices;
        
        vertices.reserve((gridRes + 1) * (gridRes + 1));
        indices.reserve(gridRes * gridRes * 6);
        
        // Generate vertices
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
                v.normal = {0.0f, 1.0f, 0.0f};
                v.texCoord = {
                    static_cast<f32>(x) / static_cast<f32>(gridRes),
                    static_cast<f32>(z) / static_cast<f32>(gridRes)
                };
                vertices.push_back(v);
            }
        }
        
        // Generate indices
        for (u32 z = 0; z < gridRes; ++z)
        {
            for (u32 x = 0; x < gridRes; ++x)
            {
                u32 topLeft = z * (gridRes + 1) + x;
                u32 topRight = topLeft + 1;
                u32 bottomLeft = topLeft + (gridRes + 1);
                u32 bottomRight = bottomLeft + 1;
                
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
        
        SEA_CORE_INFO("Created ocean mesh with {} vertices, {} triangles", 
            vertices.size(), indices.size() / 3);
        
        return true;
    }
    
    // ========================================================================
    // Update
    // ========================================================================
    
    void OceanFFT::Update(f32 deltaTime, CommandList& cmdList)
    {
        if (!m_Initialized) return;
        
        m_Time += deltaTime;
        
        // Rate limiting for performance
        bool shouldUpdate = (m_Time >= m_NextUpdateTime);
        
        if (shouldUpdate)
        {
            m_NextUpdateTime = m_Time + (1.0f / m_Params.updatesPerSecond);
            
            // If textures were in SRV state (for rendering), transition back to UAV
            if (m_TexturesReadyForRender)
            {
                auto* d3dCmdList = cmdList.GetCommandList();
                D3D12_RESOURCE_BARRIER barriers[2] = {};
                
                barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barriers[0].Transition.pResource = m_DisplacementMaps->GetResource();
                barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
                barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                
                barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barriers[1].Transition.pResource = m_NormalMaps->GetResource();
                barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
                barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                
                d3dCmdList->ResourceBarrier(2, barriers);
                m_TexturesReadyForRender = false;
            }
            
            // Generate butterfly factors once
            if (!m_ButterflyGenerated)
            {
                GenerateButterflyFactors(cmdList);
                m_ButterflyGenerated = true;
            }
            
            // Update each cascade
            for (u32 i = 0; i < m_Params.numCascades; ++i)
            {
                UpdateCascade(cmdList, i, deltaTime);
            }
            
            // Transition displacement and normal maps from UAV to SRV for rendering
            auto* d3dCmdList = cmdList.GetCommandList();
            D3D12_RESOURCE_BARRIER barriers[2] = {};
            
            barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Transition.pResource = m_DisplacementMaps->GetResource();
            barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            
            barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[1].Transition.pResource = m_NormalMaps->GetResource();
            barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            
            d3dCmdList->ResourceBarrier(2, barriers);
            m_TexturesReadyForRender = true;
        }
    }
    
    void OceanFFT::GenerateButterflyFactors(CommandList& cmdList)
    {
        // This only needs to run once per map size change
        SEA_CORE_INFO("Generating FFT butterfly factors...");
        
        auto* d3dCmdList = cmdList.GetCommandList();
        u32 N = m_Params.mapSize;
        u32 logN = Log2(N);
        
        // Set compute pipeline
        d3dCmdList->SetPipelineState(m_FFTButterflyPSO->GetPipelineState());
        d3dCmdList->SetComputeRootSignature(m_FFTButterflyRS->GetRootSignature());
        
        // Set descriptor heap
        ID3D12DescriptorHeap* heaps[] = { m_ComputeUAVHeap->GetHeap() };
        d3dCmdList->SetDescriptorHeaps(1, heaps);
        
        // Root constants: mapSize, logN
        struct ButterflyConstants {
            u32 mapSize;
            u32 logN;
            u32 _padding[2];
        } constants;
        constants.mapSize = N;
        constants.logN = logN;
        
        d3dCmdList->SetComputeRoot32BitConstants(0, 4, &constants, 0);
        
        // Set UAV (butterfly factors buffer)
        d3dCmdList->SetComputeRootDescriptorTable(1, m_ComputeUAVHeap->GetGPUHandle(m_ButterflyUAVIndex));
        
        // Dispatch: one thread per (stage, element) pair
        // Total elements: logN * N
        u32 numThreadGroups = (logN * N + FFT_THREAD_GROUP_SIZE - 1) / FFT_THREAD_GROUP_SIZE;
        d3dCmdList->Dispatch(numThreadGroups, 1, 1);
        
        // Barrier for UAV access
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = m_ButterflyFactors->GetResource();
        d3dCmdList->ResourceBarrier(1, &barrier);
        
        SEA_CORE_INFO("Butterfly factors generated for {}x{} FFT ({} stages)", N, N, logN);
    }
    
    void OceanFFT::UpdateCascade(CommandList& cmdList, u32 cascadeIndex, f32 delta)
    {
        auto& cascade = m_Params.cascades[cascadeIndex];
        cascade.time = m_Time;
        
        // Regenerate spectrum if parameters changed
        if (cascade.needsSpectrumRebuild)
        {
            GenerateSpectrum(cmdList, cascadeIndex);
            cascade.needsSpectrumRebuild = false;
        }
        
        // Modulate spectrum with time
        ModulateSpectrum(cmdList, cascadeIndex);
        
        // Perform 2D FFT
        PerformFFT(cmdList, cascadeIndex);
        
        // Unpack to displacement/normal maps
        UnpackFFTResults(cmdList, cascadeIndex);
    }
    
    void OceanFFT::GenerateSpectrum(CommandList& cmdList, u32 cascadeIndex)
    {
        auto* d3dCmdList = cmdList.GetCommandList();
        auto& cascade = m_Params.cascades[cascadeIndex];
        u32 N = m_Params.mapSize;
        
        // Set compute pipeline
        d3dCmdList->SetPipelineState(m_SpectrumComputePSO->GetPipelineState());
        d3dCmdList->SetComputeRootSignature(m_SpectrumComputeRS->GetRootSignature());
        
        // Set descriptor heap
        ID3D12DescriptorHeap* heaps[] = { m_ComputeUAVHeap->GetHeap() };
        d3dCmdList->SetDescriptorHeaps(1, heaps);
        
        // Calculate JONSWAP parameters
        f32 alpha = JONSWAPAlpha(cascade.windSpeed, cascade.fetchLength);
        f32 peakFreq = JONSWAPPeakFrequency(cascade.windSpeed, cascade.fetchLength);
        f32 windAngle = cascade.windDirection * XM_PI / 180.0f;
        
        // Fill constant buffer
        SpectrumComputeCB cb;
        cb.seedX = cascade.spectrumSeedX;
        cb.seedY = cascade.spectrumSeedY;
        cb.tileLengthX = cascade.tileLength;
        cb.tileLengthY = cascade.tileLength;
        cb.alpha = alpha;
        cb.peakFrequency = peakFreq;
        cb.windSpeed = cascade.windSpeed;
        cb.windAngle = windAngle;
        cb.depth = m_Params.depth;
        cb.swell = cascade.swell;
        cb.detail = cascade.detail;
        cb.spread = cascade.spread;
        cb.cascadeIndex = cascadeIndex;
        
        d3dCmdList->SetComputeRoot32BitConstants(0, sizeof(cb) / 4, &cb, 0);
        
        // Set UAV (spectrum texture)
        d3dCmdList->SetComputeRootDescriptorTable(1, m_ComputeUAVHeap->GetGPUHandle(m_SpectrumUAVIndex));
        
        // Dispatch: 8x8 thread groups
        u32 groupsX = (N + SPECTRUM_THREAD_GROUP_SIZE - 1) / SPECTRUM_THREAD_GROUP_SIZE;
        u32 groupsY = (N + SPECTRUM_THREAD_GROUP_SIZE - 1) / SPECTRUM_THREAD_GROUP_SIZE;
        d3dCmdList->Dispatch(groupsX, groupsY, 1);
        
        // UAV barrier
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = m_SpectrumTexture->GetResource();
        d3dCmdList->ResourceBarrier(1, &barrier);
    }
    
    void OceanFFT::ModulateSpectrum(CommandList& cmdList, u32 cascadeIndex)
    {
        auto* d3dCmdList = cmdList.GetCommandList();
        auto& cascade = m_Params.cascades[cascadeIndex];
        u32 N = m_Params.mapSize;
        
        // Set compute pipeline
        d3dCmdList->SetPipelineState(m_SpectrumModulatePSO->GetPipelineState());
        d3dCmdList->SetComputeRootSignature(m_SpectrumModulateRS->GetRootSignature());
        
        // Set descriptor heaps - need both SRV and UAV
        ID3D12DescriptorHeap* heaps[] = { m_ComputeUAVHeap->GetHeap() };
        d3dCmdList->SetDescriptorHeaps(1, heaps);
        
        // Fill constant buffer
        ModulateCB cb;
        cb.tileLengthX = cascade.tileLength;
        cb.tileLengthY = cascade.tileLength;
        cb.depth = m_Params.depth;
        cb.time = m_Time;
        cb.cascadeIndex = cascadeIndex;
        
        d3dCmdList->SetComputeRoot32BitConstants(0, sizeof(cb) / 4, &cb, 0);
        
        // Set SRV (spectrum texture input)
        d3dCmdList->SetComputeRootDescriptorTable(1, m_ComputeUAVHeap->GetGPUHandle(m_SpectrumUAVIndex));
        
        // Set UAV (FFT buffer output)
        d3dCmdList->SetComputeRootDescriptorTable(2, m_ComputeUAVHeap->GetGPUHandle(m_FFTBufferUAVIndex));
        
        // Dispatch
        u32 groupsX = (N + SPECTRUM_THREAD_GROUP_SIZE - 1) / SPECTRUM_THREAD_GROUP_SIZE;
        u32 groupsY = (N + SPECTRUM_THREAD_GROUP_SIZE - 1) / SPECTRUM_THREAD_GROUP_SIZE;
        d3dCmdList->Dispatch(groupsX, groupsY, 1);
        
        // UAV barrier
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = m_FFTBuffer->GetResource();
        d3dCmdList->ResourceBarrier(1, &barrier);
    }
    
    void OceanFFT::PerformFFT(CommandList& cmdList, u32 cascadeIndex)
    {
        auto* d3dCmdList = cmdList.GetCommandList();
        u32 N = m_Params.mapSize;
        u32 logN = Log2(N);
        
        // Set compute pipeline
        d3dCmdList->SetPipelineState(m_FFTComputePSO->GetPipelineState());
        d3dCmdList->SetComputeRootSignature(m_FFTComputeRS->GetRootSignature());
        
        // Set descriptor heaps
        ID3D12DescriptorHeap* heaps[] = { m_ComputeUAVHeap->GetHeap() };
        d3dCmdList->SetDescriptorHeaps(1, heaps);
        
        // Set butterfly factors SRV (use compute SRV heap)
        // For simplicity, we'll use the UAV heap which also has the data
        // Root 1: Butterfly factors (SRV-like access via UAV)
        d3dCmdList->SetComputeRootDescriptorTable(1, m_ComputeUAVHeap->GetGPUHandle(m_ButterflyUAVIndex));
        
        // Root 2: FFT buffer UAV (ping-pong)
        d3dCmdList->SetComputeRootDescriptorTable(2, m_ComputeUAVHeap->GetGPUHandle(m_FFTBufferUAVIndex));
        
        // We need to perform FFT on 4 spectra: Dx, Dy, Dz, and slope
        // Each spectrum is in a separate "channel" of our FFT buffer
        
        // For each spectrum (0-3)
        for (u32 spectrumIdx = 0; spectrumIdx < 4; ++spectrumIdx)
        {
            // ============ Horizontal FFT (rows) ============
            for (u32 stage = 0; stage < logN; ++stage)
            {
                FFTCB cb;
                cb.stage = stage;
                cb.direction = 0;  // Horizontal
                cb.spectrumIndex = spectrumIdx;
                cb.cascadeIndex = cascadeIndex;
                cb.mapSize = N;
                cb.logN = logN;
                cb.pingPong = stage % 2;
                
                d3dCmdList->SetComputeRoot32BitConstants(0, sizeof(cb) / 4, &cb, 0);
                
                // Dispatch: one thread per element in a row
                d3dCmdList->Dispatch(N / FFT_THREAD_GROUP_SIZE, N, 1);
                
                // UAV barrier between stages
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                barrier.UAV.pResource = m_FFTBuffer->GetResource();
                d3dCmdList->ResourceBarrier(1, &barrier);
            }
            
            // ============ Transpose ============
            TransposeFFTBuffer(cmdList, cascadeIndex);
            
            // ============ Vertical FFT (now horizontal on transposed data) ============
            for (u32 stage = 0; stage < logN; ++stage)
            {
                FFTCB cb;
                cb.stage = stage;
                cb.direction = 1;  // Vertical (treated as horizontal on transposed)
                cb.spectrumIndex = spectrumIdx;
                cb.cascadeIndex = cascadeIndex;
                cb.mapSize = N;
                cb.logN = logN;
                cb.pingPong = (logN + stage) % 2;
                
                d3dCmdList->SetComputeRoot32BitConstants(0, sizeof(cb) / 4, &cb, 0);
                
                d3dCmdList->Dispatch(N / FFT_THREAD_GROUP_SIZE, N, 1);
                
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                barrier.UAV.pResource = m_FFTBuffer->GetResource();
                d3dCmdList->ResourceBarrier(1, &barrier);
            }
            
            // ============ Transpose back ============
            TransposeFFTBuffer(cmdList, cascadeIndex);
        }
    }
    
    void OceanFFT::TransposeFFTBuffer(CommandList& cmdList, u32 cascadeIndex)
    {
        auto* d3dCmdList = cmdList.GetCommandList();
        u32 N = m_Params.mapSize;
        
        // Set transpose pipeline
        d3dCmdList->SetPipelineState(m_TransposePSO->GetPipelineState());
        d3dCmdList->SetComputeRootSignature(m_TransposeRS->GetRootSignature());
        
        // Set descriptor heap
        ID3D12DescriptorHeap* heaps[] = { m_ComputeUAVHeap->GetHeap() };
        d3dCmdList->SetDescriptorHeaps(1, heaps);
        
        TransposeCB cb;
        cb.mapSize = N;
        cb.spectrumIndex = 0;  // Transpose all spectra at once
        cb.cascadeIndex = cascadeIndex;
        
        d3dCmdList->SetComputeRoot32BitConstants(0, sizeof(cb) / 4, &cb, 0);
        
        // Set UAV (FFT buffer - in-place transpose)
        d3dCmdList->SetComputeRootDescriptorTable(1, m_ComputeUAVHeap->GetGPUHandle(m_FFTBufferUAVIndex));
        
        // Dispatch: 16x16 tiles
        u32 groupsX = (N + TRANSPOSE_TILE_SIZE - 1) / TRANSPOSE_TILE_SIZE;
        u32 groupsY = (N + TRANSPOSE_TILE_SIZE - 1) / TRANSPOSE_TILE_SIZE;
        d3dCmdList->Dispatch(groupsX, groupsY, 1);
        
        // UAV barrier
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = m_FFTBuffer->GetResource();
        d3dCmdList->ResourceBarrier(1, &barrier);
    }
    
    void OceanFFT::UnpackFFTResults(CommandList& cmdList, u32 cascadeIndex)
    {
        auto* d3dCmdList = cmdList.GetCommandList();
        auto& cascade = m_Params.cascades[cascadeIndex];
        u32 N = m_Params.mapSize;
        
        // Set unpack pipeline
        d3dCmdList->SetPipelineState(m_UnpackPSO->GetPipelineState());
        d3dCmdList->SetComputeRootSignature(m_UnpackRS->GetRootSignature());
        
        // Set descriptor heap (all descriptors are in UAV heap)
        ID3D12DescriptorHeap* heaps[] = { m_ComputeUAVHeap->GetHeap() };
        d3dCmdList->SetDescriptorHeaps(1, heaps);
        
        UnpackCB cb;
        cb.mapSize = N;
        cb.cascadeIndex = cascadeIndex;
        cb.whitecap = cascade.whitecap;
        cb.foamGrowRate = cascade.foamGrowRate;
        cb.foamDecayRate = cascade.foamDecayRate;
        
        d3dCmdList->SetComputeRoot32BitConstants(0, sizeof(cb) / 4, &cb, 0);
        
        // Root 1: UAVs - FFT buffer, displacement, normal maps (consecutive in heap)
        d3dCmdList->SetComputeRootDescriptorTable(1, m_ComputeUAVHeap->GetGPUHandle(m_UnpackUAVStartIndex));
        
        // Dispatch
        u32 groupsX = (N + SPECTRUM_THREAD_GROUP_SIZE - 1) / SPECTRUM_THREAD_GROUP_SIZE;
        u32 groupsY = (N + SPECTRUM_THREAD_GROUP_SIZE - 1) / SPECTRUM_THREAD_GROUP_SIZE;
        d3dCmdList->Dispatch(groupsX, groupsY, 1);
        
        // UAV barriers for output textures
        D3D12_RESOURCE_BARRIER barriers[2] = {};
        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barriers[0].UAV.pResource = m_DisplacementMaps->GetResource();
        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barriers[1].UAV.pResource = m_NormalMaps->GetResource();
        d3dCmdList->ResourceBarrier(2, barriers);
    }
    
    // ========================================================================
    // Render
    // ========================================================================
    
    void OceanFFT::Render(CommandList& cmdList, const Camera& camera)
    {
        if (!m_Initialized || !m_OceanMesh) return;
        
        // Update constant buffer
        OceanRenderCB cb;
        
        XMMATRIX view = XMLoadFloat4x4(&camera.GetViewMatrix());
        XMMATRIX proj = XMLoadFloat4x4(&camera.GetProjectionMatrix());
        XMMATRIX viewProj = XMMatrixMultiply(view, proj);
        XMStoreFloat4x4(&cb.viewProj, XMMatrixTranspose(viewProj));
        
        // World matrix (centered on camera for infinite ocean)
        XMFLOAT3 camPos = camera.GetPosition();
        XMMATRIX world = XMMatrixTranslation(camPos.x, 0.0f, camPos.z);
        XMStoreFloat4x4(&cb.world, XMMatrixTranspose(world));
        
        cb.cameraPos = camPos;
        cb.time = m_Time;
        
        XMVECTOR sunDirNorm = XMVector3Normalize(XMLoadFloat3(&m_SunDirection));
        XMStoreFloat3(&cb.sunDirection, sunDirNorm);
        cb.sunIntensity = m_SunIntensity;
        
        cb.waterColor = m_Params.waterColor;
        cb.foamColor = m_Params.foamColor;
        cb.roughness = m_Params.roughness;
        cb.normalStrength = m_Params.normalStrength;
        cb.numCascades = m_Params.numCascades;
        
        // Map scales for each cascade
        for (u32 i = 0; i < OceanFFTParams::MAX_CASCADES; ++i)
        {
            if (i < m_Params.numCascades)
            {
                f32 tileLen = m_Params.cascades[i].tileLength;
                cb.mapScales[i] = XMFLOAT4(
                    1.0f / tileLen,                          // UV scale X
                    1.0f / tileLen,                          // UV scale Y
                    m_Params.cascades[i].displacementScale,  // Displacement scale
                    m_Params.cascades[i].normalScale         // Normal scale
                );
            }
            else
            {
                cb.mapScales[i] = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
            }
        }
        
        m_RenderCB->Update(&cb, sizeof(cb));
        
        // Set pipeline state
        auto* d3dCmdList = cmdList.GetCommandList();
        auto* pso = (m_ViewMode == 1 && m_WireframePSO) ? m_WireframePSO.get() : m_RenderPSO.get();
        
        d3dCmdList->SetPipelineState(pso->GetPipelineState());
        d3dCmdList->SetGraphicsRootSignature(m_RenderRS->GetRootSignature());
        
        // Bind constant buffer
        d3dCmdList->SetGraphicsRootConstantBufferView(0, m_RenderCB->GetGPUAddress());
        
        // Bind textures
        if (m_RenderSRVHeap)
        {
            ID3D12DescriptorHeap* heaps[] = { m_RenderSRVHeap->GetHeap() };
            d3dCmdList->SetDescriptorHeaps(1, heaps);
            d3dCmdList->SetGraphicsRootDescriptorTable(1, m_RenderSRVHeap->GetGPUHandle(0));
        }
        
        // Draw mesh
        d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        
        D3D12_VERTEX_BUFFER_VIEW vbv = m_OceanMesh->GetVertexBuffer()->GetVertexBufferView();
        D3D12_INDEX_BUFFER_VIEW ibv = m_OceanMesh->GetIndexBuffer()->GetIndexBufferView();
        d3dCmdList->IASetVertexBuffers(0, 1, &vbv);
        d3dCmdList->IASetIndexBuffer(&ibv);
        
        d3dCmdList->DrawIndexedInstanced(m_OceanMesh->GetIndexCount(), 1, 0, 0, 0);
    }
    
    // ========================================================================
    // Parameter Access
    // ========================================================================
    
    void OceanFFT::SetCascadeParams(u32 index, const WaveCascadeParams& params)
    {
        if (index >= OceanFFTParams::MAX_CASCADES) return;
        m_Params.cascades[index] = params;
        m_Params.cascades[index].needsSpectrumRebuild = true;
    }
    
    WaveCascadeParams& OceanFFT::GetCascadeParams(u32 index)
    {
        static WaveCascadeParams dummy;
        if (index >= OceanFFTParams::MAX_CASCADES) return dummy;
        return m_Params.cascades[index];
    }
    
    void OceanFFT::RebuildAllSpectra()
    {
        for (u32 i = 0; i < m_Params.numCascades; ++i)
        {
            m_Params.cascades[i].needsSpectrumRebuild = true;
        }
    }
}
