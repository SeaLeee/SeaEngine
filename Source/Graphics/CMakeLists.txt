add_library(SeaGraphics STATIC
    Device.cpp
    SwapChain.cpp
    CommandQueue.cpp
    CommandList.cpp
    DescriptorHeap.cpp
    Buffer.cpp
    Texture.cpp
    RenderTarget.cpp
    PipelineState.cpp
    RootSignature.cpp
    Fence.cpp
    GPUResource.cpp
    RenderDocCapture.cpp
    Material.cpp
)

target_include_directories(SeaGraphics PUBLIC 
    ${CMAKE_SOURCE_DIR}/Source
)

target_link_libraries(SeaGraphics PUBLIC 
    SeaCore
    d3d12
    dxgi
    dxguid
    d3dcompiler
    Microsoft::DirectX-Headers
)

if(SEA_ENABLE_RENDERDOC)
    target_compile_definitions(SeaGraphics PRIVATE SEA_ENABLE_RENDERDOC)
endif()
