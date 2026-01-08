add_library(SeaEditor STATIC
    Editor.cpp
    NodeEditor.cpp
    PassNodeWidget.cpp
    ResourceNodeWidget.cpp
    PropertyPanel.cpp
    ShaderEditor.cpp
    GraphSerializer.cpp
    ImGuiRenderer.cpp
)

target_include_directories(SeaEditor PUBLIC 
    ${CMAKE_SOURCE_DIR}/Source
)

target_link_libraries(SeaEditor PUBLIC 
    SeaCore
    SeaGraphics
    SeaRenderGraph
    SeaShader
    imgui_lib
    imnodes_lib
)
