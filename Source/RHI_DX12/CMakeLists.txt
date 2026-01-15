# RHI DirectX 12 Implementation
# Platform-specific implementation of the RHI interface for D3D12

set(RHI_DX12_HEADERS
    DX12RHI.h
)

set(RHI_DX12_SOURCES
    DX12RHI.cpp
    DX12CommandList.cpp
    DX12Device.cpp
)

add_library(SeaRHI_DX12 STATIC ${RHI_DX12_SOURCES} ${RHI_DX12_HEADERS})

target_include_directories(SeaRHI_DX12 PUBLIC 
    ${CMAKE_SOURCE_DIR}/Source
)

target_link_libraries(SeaRHI_DX12 PUBLIC
    SeaRHI
    SeaCore
    d3d12
    dxgi
    dxguid
)

# Enable all DX12 headers
target_compile_definitions(SeaRHI_DX12 PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# Set source groups for IDE
source_group("Header Files" FILES ${RHI_DX12_HEADERS})
source_group("Source Files" FILES ${RHI_DX12_SOURCES})
