cmake_minimum_required(VERSION 3.20)
project(SeaEngine VERSION 1.0.0 LANGUAGES CXX)

# C++20 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 编译选项
if(MSVC)
    add_compile_options(/W4 /WX- /MP /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# 选项
option(SEA_ENABLE_RENDERDOC "Enable RenderDoc integration" ON)
option(SEA_BUILD_SAMPLES "Build sample applications" ON)

# 查找DirectX
find_package(directx-headers CONFIG QUIET)
if(NOT directx-headers_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        directx-headers
        GIT_REPOSITORY https://github.com/microsoft/DirectX-Headers.git
        GIT_TAG v1.613.1
    )
    FetchContent_MakeAvailable(directx-headers)
endif()

# 获取第三方依赖
include(FetchContent)

# ImGui (使用docking分支)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.6-docking
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

# ImNodes - 节点编辑器 (只获取源码，不使用其CMakeLists)
FetchContent_Declare(
    imnodes
    GIT_REPOSITORY https://github.com/Nelarius/imnodes.git
    GIT_TAG master
)
FetchContent_GetProperties(imnodes)
if(NOT imnodes_POPULATED)
    FetchContent_Populate(imnodes)
endif()

# spdlog - 日志
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.13.0
)
FetchContent_MakeAvailable(spdlog)

# nlohmann_json - JSON解析
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# ImGui库
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
)
target_include_directories(imgui_lib PUBLIC 
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_compile_definitions(imgui_lib PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(imgui_lib PUBLIC d3d12 dxgi)

# ImNodes库
add_library(imnodes_lib STATIC
    ${imnodes_SOURCE_DIR}/imnodes.cpp
)
target_include_directories(imnodes_lib PUBLIC ${imnodes_SOURCE_DIR})
target_compile_definitions(imnodes_lib PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(imnodes_lib PUBLIC imgui_lib)

# 子目录
add_subdirectory(Source/Core)
add_subdirectory(Source/Graphics)
add_subdirectory(Source/RenderGraph)
add_subdirectory(Source/Shader)
add_subdirectory(Source/Editor)

if(SEA_BUILD_SAMPLES)
    add_subdirectory(Samples)
endif()

# 主引擎库
add_library(SeaEngine INTERFACE)
target_link_libraries(SeaEngine INTERFACE
    SeaCore
    SeaGraphics
    SeaRenderGraph
    SeaShader
    SeaEditor
)
