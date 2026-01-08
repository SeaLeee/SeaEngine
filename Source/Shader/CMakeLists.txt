add_library(SeaShader STATIC
    ShaderCompiler.cpp
    ShaderLibrary.cpp
    ShaderReflection.cpp
    HotReload.cpp
)

target_include_directories(SeaShader PUBLIC 
    ${CMAKE_SOURCE_DIR}/Source
)

target_link_libraries(SeaShader PUBLIC 
    SeaCore
    d3dcompiler
    dxcompiler
)
