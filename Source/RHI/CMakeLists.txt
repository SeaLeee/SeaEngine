# RHI Abstract Interface Library
# Platform-agnostic rendering hardware interface

set(RHI_HEADERS
    RHI.h
    RHITypes.h
    RHIResource.h
    RHICommandList.h
    RHIDevice.h
    RHIAdapter.h
    RHIResourceWrappers.h
)

set(RHI_SOURCES
    RHITypes.cpp
)

add_library(SeaRHI STATIC ${RHI_SOURCES} ${RHI_HEADERS})

target_include_directories(SeaRHI PUBLIC 
    ${CMAKE_SOURCE_DIR}/Source
)

# SeaRHI needs SeaGraphics for wrapper types
target_link_libraries(SeaRHI PUBLIC
    SeaCore
)

# Set source groups for IDE
source_group("Header Files" FILES ${RHI_HEADERS})
source_group("Source Files" FILES ${RHI_SOURCES})
