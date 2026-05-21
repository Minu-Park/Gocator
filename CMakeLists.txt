cmake_minimum_required(VERSION 3.20)

project(Gocator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MACOSX_RPATH ON)

set(GOPXL_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/GoPxL-SDK" CACHE PATH "GoPxL SDK root")
set(GOCATOR_DEFAULT_QT_PREFIX "/opt/Qt/6.7.2/gcc_64" CACHE PATH "Default Qt prefix")
option(GOCATOR_BUILD_QT_UI "Build Qt Widgets debug UI when Qt is available" ON)

if(GOCATOR_BUILD_QT_UI AND NOT CMAKE_PREFIX_PATH AND EXISTS "${GOCATOR_DEFAULT_QT_PREFIX}")
    list(APPEND CMAKE_PREFIX_PATH "${GOCATOR_DEFAULT_QT_PREFIX}")
endif()

add_library(gocator_core
    C++/Gocator.cpp
    C++/Internal/GocatorAcquisition.cpp
    C++/Internal/GocatorConnection.cpp
    C++/Internal/GocatorDiscovery.cpp
    C++/Internal/GocatorParameterSet.cpp
    C++/Internal/GocatorResourceClient.cpp
    C++/Internal/GocatorSdkRuntime.cpp
    C++/Internal/GocatorSettingsManager.cpp
)

target_include_directories(gocator_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/C++"
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

add_executable(gocator
    src/main.cpp
    src/cli_main.cpp
)

if(GOCATOR_BUILD_QT_UI)
    find_package(Qt6 QUIET COMPONENTS Widgets Concurrent)
    if(Qt6_FOUND)
        set_target_properties(gocator_core PROPERTIES AUTOMOC ON)
        target_sources(gocator_core PRIVATE
            C++/Utility/Qt/QGocatorWidget.cpp
            C++/Utility/Qt/QGocatorWidget.h
        )
        target_link_libraries(gocator_core PUBLIC Qt6::Widgets Qt6::Concurrent)
        target_compile_definitions(gocator_core PUBLIC GOCATOR_HAS_UI)

        target_sources(gocator PRIVATE src/ui_main.cpp)
        target_link_libraries(gocator PRIVATE Qt6::Widgets)
        target_compile_definitions(gocator PRIVATE GOCATOR_HAS_UI)
    endif()
endif()

target_link_libraries(gocator
    PRIVATE
        gocator_core
)

set(gocator_runtime_targets
    gocator_core
    gocator
)

foreach(target ${gocator_runtime_targets})
    set_target_properties(${target} PROPERTIES
        BUILD_RPATH "${GOPXL_SDK_DIR}/lib"
        INSTALL_RPATH "@executable_path/../GoPxL-SDK/lib"
    )
endforeach()
