cmake_minimum_required(VERSION 3.21)
project(LanXiChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
set(LANXI_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../../src/services/acquisition/lanxi")
add_executable(lanxi_core_test core_test.cpp "${LANXI_SOURCE}/LanXiCore.cpp")
target_include_directories(lanxi_core_test PRIVATE "${LANXI_SOURCE}")
add_test(NAME lanxi_core COMMAND lanxi_core_test)
add_executable(lanxi_analysis_test analysis_test.cpp "${LANXI_SOURCE}/LanXiAnalysis.cpp" "${LANXI_SOURCE}/LanXiCore.cpp")
target_include_directories(lanxi_analysis_test PRIVATE "${LANXI_SOURCE}")
add_test(NAME lanxi_analysis COMMAND lanxi_analysis_test)
add_executable(lanxi_advanced_test advanced_test.cpp "${LANXI_SOURCE}/LanXiAdvanced.cpp" "${LANXI_SOURCE}/LanXiAnalysis.cpp" "${LANXI_SOURCE}/LanXiCore.cpp")
target_include_directories(lanxi_advanced_test PRIVATE "${LANXI_SOURCE}")
add_test(NAME lanxi_advanced COMMAND lanxi_advanced_test)
option(LANXI_QT_TESTS "Build the real Qt network/backend integration test" OFF)
add_executable(lanxi_interchange_test interchange_test.cpp "${LANXI_SOURCE}/LanXiUff.cpp" "${LANXI_SOURCE}/LanXiStreaming.cpp" "${LANXI_SOURCE}/LanXiAnalysis.cpp" "${LANXI_SOURCE}/LanXiCore.cpp")
target_include_directories(lanxi_interchange_test PRIVATE "${LANXI_SOURCE}")
add_test(NAME lanxi_interchange COMMAND lanxi_interchange_test)
add_executable(lanxi_modal_test modal_test.cpp "${LANXI_SOURCE}/LanXiModal.cpp" "${LANXI_SOURCE}/LanXiAdvanced.cpp" "${LANXI_SOURCE}/LanXiAnalysis.cpp" "${LANXI_SOURCE}/LanXiCore.cpp")
target_include_directories(lanxi_modal_test PRIVATE "${LANXI_SOURCE}")
add_test(NAME lanxi_modal COMMAND lanxi_modal_test)
add_executable(lanxi_sound_power_test sound_power_test.cpp "${LANXI_SOURCE}/LanXiSoundPower.cpp")
target_include_directories(lanxi_sound_power_test PRIVATE "${LANXI_SOURCE}")
add_test(NAME lanxi_sound_power COMMAND lanxi_sound_power_test)
if(LANXI_QT_TESTS)
    find_package(Qt6 6.10 REQUIRED COMPONENTS Core Network Qml)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(CMAKE_AUTOMOC ON)
    add_executable(lanxi_backend_test backend_test.cpp
        "${LANXI_SOURCE}/LanXiSoundPower.cpp"
        "${LANXI_SOURCE}/LanXiAcousticMethods.cpp"
        "${LANXI_SOURCE}/LanXiStructural.cpp"
        "${LANXI_SOURCE}/LanXiModal.cpp"
        "${LANXI_SOURCE}/LanXiUff.cpp"
        "${LANXI_SOURCE}/LanXiStreaming.cpp"
        "${LANXI_SOURCE}/LanXiRecording.cpp"
        "${LANXI_SOURCE}/LanXiInterchange.cpp"
        "${LANXI_SOURCE}/LanXiAudio.h"
        "${LANXI_SOURCE}/LanXiAudio.cpp"
        "${LANXI_SOURCE}/LanXiAdvanced.cpp"
        "${LANXI_SOURCE}/LanXiWorkbench.cpp"
        "${LANXI_SOURCE}/LanXiAnalysis.cpp"
        "${LANXI_SOURCE}/LanXiResults.cpp"
        "${LANXI_SOURCE}/LanXiSetup.cpp"
        "${LANXI_SOURCE}/LanXiCore.cpp" "${LANXI_SOURCE}/LanXiBackend.h"
        "${LANXI_SOURCE}/LanXiBackend.cpp" "${LANXI_SOURCE}/LanXiWorker.h"
        "${LANXI_SOURCE}/LanXiWorker.cpp" "${LANXI_SOURCE}/LanXiSignal.cpp"
        "${LANXI_SOURCE}/LanXiFiles.cpp")
    target_include_directories(lanxi_backend_test PRIVATE "${LANXI_SOURCE}")
    target_link_libraries(lanxi_backend_test PRIVATE Qt6::Core Qt6::Network Qt6::Qml)
    if(MSVC)
        target_compile_options(lanxi_backend_test PRIVATE /utf-8)
    endif()
    add_test(NAME lanxi_backend COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/run_integration.py" "$<TARGET_FILE:lanxi_backend_test>")
    set_tests_properties(lanxi_backend PROPERTIES TIMEOUT 420)
endif()
