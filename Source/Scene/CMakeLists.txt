add_library(SeaScene STATIC
    Mesh.cpp
    Mesh.h
    Camera.cpp
    Camera.h
    SimpleRenderer.cpp
    SimpleRenderer.h
    SceneRenderer.cpp
    SceneRenderer.h
    Scene.h
    SceneFile.cpp
    SceneFile.h
    SceneManager.cpp
    SceneManager.h
    Ocean.cpp
    Ocean.h
    SkyRenderer.cpp
    SkyRenderer.h
)

target_include_directories(SeaScene PUBLIC
    ${CMAKE_SOURCE_DIR}/Source
    ${tinyobjloader_SOURCE_DIR}
    ${stb_SOURCE_DIR}
    ${directxmath_SOURCE_DIR}/Inc
    ${json_SOURCE_DIR}/include
)

target_link_libraries(SeaScene PUBLIC
    SeaCore
    SeaGraphics
    SeaShader
    tinyobjloader
    DirectXMath
    nlohmann_json::nlohmann_json
)
