# Register each built-in tool's source manifest explicitly. No source globbing.
list(APPEND FRDM_QML_FILES src/tools/ToolRegistry.qml)
include("${CMAKE_CURRENT_LIST_DIR}/diagnostics/live_electrodogram/Sources.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/diagnostics/acoustic_analytics/Sources.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/diagnostics/lanxi/Sources.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/utilities/notepad/Sources.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/utilities/calculator/Sources.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/utilities/results/Sources.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/utilities/settings/Sources.cmake")
