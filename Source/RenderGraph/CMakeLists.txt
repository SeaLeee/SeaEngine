add_library(SeaRenderGraph STATIC
    # New FrameGraph system (RHI-based)
    FrameGraph.cpp
    FrameGraph.h
    DeferredFrameGraph.h
    
    # Legacy RenderGraph (to be deprecated)
    RenderGraph.cpp
    RenderPassContext.cpp
    ResourceNode.cpp
    PassNode.cpp
    GraphCompiler.cpp
    ResourcePool.cpp
    FrameResource.cpp
    PassTemplate.cpp
    PassTemplate.h
)

target_include_directories(SeaRenderGraph PUBLIC 
    ${CMAKE_SOURCE_DIR}/Source
)

target_link_libraries(SeaRenderGraph PUBLIC 
    SeaCore
    SeaRHI
    SeaGraphics
    nlohmann_json::nlohmann_json
)
