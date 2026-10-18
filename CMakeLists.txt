cmake_minimum_required(VERSION 3.21)

project(FRDM_IMX95_QT_DSW VERSION 0.1 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Layer and tool manifests are also checked by the Qt-free CMake script mode.
include(cmake/VerifySources.cmake)

if(WIN32 AND MINGW)
    # Leave room for compiler dependency-file suffixes. Directory hashing cannot
    # rescue a long build root plus a long Qt-generated C++ basename.
    if(NOT DEFINED CMAKE_OBJECT_PATH_MAX)
        set(CMAKE_OBJECT_PATH_MAX 240)
    endif()
    include(cmake/CheckBuildPaths.cmake)
    frdm_check_mingw_build_paths("${CMAKE_BINARY_DIR}" appFRDM_IMX95_QT_DSW ${FRDM_QML_FILES})
endif()

find_package(Qt6 6.10 REQUIRED COMPONENTS Quick Network)
option(SUNO_LANXI_AUDIO "Enable audio audition using the approved Qt Multimedia package" OFF)
if(SUNO_LANXI_AUDIO)
    find_package(Qt6 6.10 REQUIRED COMPONENTS Multimedia)
endif()

qt_standard_project_setup(REQUIRES 6.10)

# Content of downloaded design from Figma to Qt or similar
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

qt_add_executable(appFRDM_IMX95_QT_DSW
    ${FRDM_APP_SOURCES}
)

qt_add_qml_module(appFRDM_IMX95_QT_DSW
    URI FRDM_IMX95_QT_DSW
    QML_FILES ${FRDM_QML_FILES}
    SOURCES ${FRDM_NATIVE_SOURCES}
    RESOURCES ${FRDM_RESOURCE_FILES}
)

set_target_properties(appFRDM_IMX95_QT_DSW PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appFRDM_IMX95_QT_DSW
    PRIVATE Qt6::Quick Qt6::Network
)
target_include_directories(appFRDM_IMX95_QT_DSW PRIVATE ${FRDM_NATIVE_INCLUDE_DIRS})
if(SUNO_LANXI_AUDIO)
    target_compile_definitions(appFRDM_IMX95_QT_DSW PRIVATE SUNO_LANXI_AUDIO)
    target_link_libraries(appFRDM_IMX95_QT_DSW PRIVATE Qt6::Multimedia)
endif()
if(MSVC)
    target_compile_options(appFRDM_IMX95_QT_DSW PRIVATE /utf-8)
endif()

install(TARGETS appFRDM_IMX95_QT_DSW
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Windows packages need the Qt DLLs, platform plugin, and imported QML modules.
# The Yocto image/recipe supplies runtime dependencies for the Linux target.
if(WIN32 AND NOT CMAKE_CROSSCOMPILING)
    qt_generate_deploy_qml_app_script(
        TARGET appFRDM_IMX95_QT_DSW
        OUTPUT_SCRIPT deploy_script
    )
    install(SCRIPT ${deploy_script})
endif()
