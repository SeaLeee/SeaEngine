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

# 查找并复制 DXC DLL 到输出目录
find_program(DXC_EXECUTABLE dxc
    PATHS
    "$ENV{WindowsSdkDir}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
    "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64"
    "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64"
    "C:/Program Files (x86)/Windows Kits/10/bin/10.0.19041.0/x64"
)

if(DXC_EXECUTABLE)
    get_filename_component(DXC_DIR ${DXC_EXECUTABLE} DIRECTORY)
    add_custom_command(TARGET SampleApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DXC_DIR}/dxcompiler.dll"
        $<TARGET_FILE_DIR:SampleApp>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DXC_DIR}/dxil.dll"
        $<TARGET_FILE_DIR:SampleApp>
        COMMENT "Copying DXC DLLs..."
    )
endif()
