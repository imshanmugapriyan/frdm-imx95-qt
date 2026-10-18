# Can run independently: cmake -P cmake/VerifySources.cmake
# This script loads no Qt package and invokes no compiler.
cmake_minimum_required(VERSION 3.21)
include("${CMAKE_CURRENT_LIST_DIR}/ProjectSources.cmake")
set(FRDM_MISSING_SOURCES)
set(FRDM_ALL_SOURCES ${FRDM_APP_SOURCES} ${FRDM_QML_FILES}
    ${FRDM_NATIVE_SOURCES} ${FRDM_RESOURCE_FILES})
foreach(source_file IN LISTS FRDM_ALL_SOURCES)
    if(NOT EXISTS "${FRDM_SOURCE_ROOT}/${source_file}"
       OR IS_DIRECTORY "${FRDM_SOURCE_ROOT}/${source_file}")
        list(APPEND FRDM_MISSING_SOURCES "${source_file}")
    endif()
endforeach()
if(FRDM_MISSING_SOURCES)
    list(JOIN FRDM_MISSING_SOURCES "\n  " missing_source_lines)
    message(FATAL_ERROR
        "The project source copy is incomplete. Missing files:\n  ${missing_source_lines}\n"
        "Copy the complete source package, including src/ and cmake/. "
        "Cleaning the build cannot restore source files.")
endif()

set(FRDM_QML_TYPE_NAMES)
foreach(source_file IN LISTS FRDM_QML_FILES)
    if(source_file MATCHES "\\.qml$")
        get_filename_component(type_name "${source_file}" NAME_WE)
        if(type_name IN_LIST FRDM_QML_TYPE_NAMES)
            message(FATAL_ERROR "Duplicate QML type ${type_name}; use a unique tool-prefixed filename.")
        endif()
        list(APPEND FRDM_QML_TYPE_NAMES "${type_name}")
    endif()
endforeach()
list(LENGTH FRDM_ALL_SOURCES source_count)
message(STATUS "Verified ${source_count} application source/resource files")
