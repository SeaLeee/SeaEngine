add_library(SeaCore STATIC
    Application.cpp
    Window.cpp
    Input.cpp
    Timer.cpp
    Log.cpp
    FileSystem.cpp
)

target_include_directories(SeaCore PUBLIC 
    ${CMAKE_SOURCE_DIR}/Source
)

target_link_libraries(SeaCore PUBLIC 
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    imgui_lib
)
