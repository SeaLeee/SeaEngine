add_library(SeaScene STATIC
    Mesh.cpp
    Mesh.h
    Camera.cpp
    Camera.h
    SimpleRenderer.cpp
    SimpleRenderer.h
    Scene.h
)

target_include_directories(SeaScene PUBLIC
    ${CMAKE_SOURCE_DIR}/Source
    ${tinyobjloader_SOURCE_DIR}
    ${stb_SOURCE_DIR}
    ${directxmath_SOURCE_DIR}/Inc
)

target_link_libraries(SeaScene PUBLIC
    SeaCore
    SeaGraphics
    SeaShader
    tinyobjloader
    DirectXMath
)
