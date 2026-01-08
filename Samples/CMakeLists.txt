add_executable(SampleApp WIN32
    Main.cpp
    SampleApp.cpp
    SampleApp.h
)

target_link_libraries(SampleApp PRIVATE SeaEngine)

# 复制着色器到输出目录
add_custom_command(TARGET SampleApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Shaders
    $<TARGET_FILE_DIR:SampleApp>/Shaders
)
