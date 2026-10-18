# Run with cmake -P tests/build_paths.test.cmake. No Qt or compiler is loaded.
cmake_minimum_required(VERSION 3.21)
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/CheckBuildPaths.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/ProjectSources.cmake")
set(target appFRDM_IMX95_QT_DSW)
set(reported_root "E:/Linux/Linux_Application/FRDM_IMX95_QT_DSW/build/Desktop_Qt_6_12_0_MinGW_64_bit_Debug")
set(reported_source "src/tools/diagnostics/acoustic_analytics/qml/AutomaticMicCalibration.qml")

if(DEFINED FRDM_TEST_BUILD_DIRECTORY)
    frdm_check_mingw_build_paths("${FRDM_TEST_BUILD_DIRECTORY}" "${target}" ${FRDM_QML_FILES})
    return()
endif()

frdm_qml_dependency_path_length("${reported_root}" "${target}" length source "${reported_source}")
if(NOT length EQUAL 260 OR NOT source STREQUAL reported_source)
    message(FATAL_ERROR "Reported dependency-path fixture changed: ${length}, ${source}")
endif()
message(STATUS "PASS reported MinGW dependency path is exactly 260 characters")

frdm_qml_dependency_path_length("E:\\b\\suno-debug\\" "${target}" length source "${reported_source}")
if(NOT length EQUAL 188)
    message(FATAL_ERROR "Short Windows build root should yield 188 characters, got ${length}")
endif()
message(STATUS "PASS short build root reduces the same dependency path to 188 characters")

frdm_qml_dependency_path_length("${reported_root}" "${target}" length source ${FRDM_QML_FILES})
if(NOT length EQUAL 260 OR NOT source STREQUAL reported_source)
    message(FATAL_ERROR "Current manifest maximum differs from the reported failure: ${length}, ${source}")
endif()
message(STATUS "PASS longest generated basename is identified from the actual manifests")

execute_process(COMMAND "${CMAKE_COMMAND}" "-DFRDM_TEST_BUILD_DIRECTORY=${reported_root}"
    -P "${CMAKE_CURRENT_LIST_FILE}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(result EQUAL 0 OR NOT error MATCHES "MinGW build directory is too long" OR NOT error MATCHES "E:/b/suno-debug")
    message(FATAL_ERROR "Long-root configuration did not give actionable feedback: ${output}\n${error}")
endif()
message(STATUS "PASS unsafe build root is rejected with the recovery path")

execute_process(COMMAND "${CMAKE_COMMAND}" "-DFRDM_TEST_BUILD_DIRECTORY=E:/b/suno-debug"
    -P "${CMAKE_CURRENT_LIST_FILE}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Short-root configuration was rejected: ${output}\n${error}")
endif()
message(STATUS "PASS short build root accepts all current QML/JS files")
message(STATUS "5 build-path checks passed; no Qt or compiler used")
