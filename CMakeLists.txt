cmake_minimum_required(VERSION 3.20)

project(Gocator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MACOSX_RPATH ON)

set(GOPXL_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/GoPxL-SDK" CACHE PATH "GoPxL SDK root")

add_library(gocator_core
    src/gocator/GocatorDiscovery.cpp
    src/gocator/GocatorSdkRuntime.cpp
    src/gocator/GocatorSettingsManager.cpp
)

target_include_directories(gocator_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${GOPXL_SDK_DIR}/include/kApi"
        "${GOPXL_SDK_DIR}/include/GoApi"
        "${GOPXL_SDK_DIR}/include/GoPxLSdk"
)

target_link_directories(gocator_core
    PUBLIC
        "${GOPXL_SDK_DIR}/lib"
)

target_link_libraries(gocator_core
    PUBLIC
        kApi
        GoApi
        GoPxLSdk
)

add_executable(gocator_settings
    src/main.cpp
)

target_link_libraries(gocator_settings
    PRIVATE
        gocator_core
)

foreach(target gocator_core gocator_settings)
    set_target_properties(${target} PROPERTIES
        BUILD_RPATH "${GOPXL_SDK_DIR}/lib"
        INSTALL_RPATH "@executable_path/../GoPxL-SDK/lib"
    )
endforeach()
