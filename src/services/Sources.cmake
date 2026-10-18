list(APPEND FRDM_QML_FILES
    src/services/diagnostics/qml/DiagnosticSession.qml
    src/services/diagnostics/qml/Analysis.js
    src/services/diagnostics/qml/SessionModel.js
)
list(APPEND FRDM_NATIVE_SOURCES
    src/services/diagnostics/native/DiagnosticFiles.h
    src/services/diagnostics/native/DiagnosticFiles.cpp
    src/services/desktop/native/DesktopFiles.h
    src/services/desktop/native/DesktopFiles.cpp
)
include("${CMAKE_CURRENT_LIST_DIR}/acquisition/lanxi/Sources.cmake")
