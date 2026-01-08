add_library(SeaRenderGraph STATIC
    RenderGraph.cpp
    RenderPass.cpp
    ResourceNode.cpp
    PassNode.cpp
    GraphCompiler.cpp
    ResourcePool.cpp
    FrameResource.cpp
)

target_include_directories(SeaRenderGraph PUBLIC 
    ${CMAKE_SOURCE_DIR}/Source
)

target_link_libraries(SeaRenderGraph PUBLIC 
    SeaCore
    SeaGraphics
)
