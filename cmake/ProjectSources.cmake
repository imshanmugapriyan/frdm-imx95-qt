include_guard(GLOBAL)
get_filename_component(FRDM_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# Paths in each manifest are relative to the repository root, which also keeps
# the embedded QML directory layout identical to the source directory layout.
set(FRDM_APP_SOURCES)
set(FRDM_QML_FILES)
set(FRDM_NATIVE_SOURCES)
set(FRDM_RESOURCE_FILES)
include("${FRDM_SOURCE_ROOT}/src/app/Sources.cmake")
include("${FRDM_SOURCE_ROOT}/src/shell/Sources.cmake")
include("${FRDM_SOURCE_ROOT}/src/shared/Sources.cmake")
include("${FRDM_SOURCE_ROOT}/src/services/Sources.cmake")
include("${FRDM_SOURCE_ROOT}/src/tools/Sources.cmake")

# Qt's generated type registrar includes native QML headers by filename.
set(FRDM_NATIVE_INCLUDE_DIRS)
foreach(source_file IN LISTS FRDM_NATIVE_SOURCES)
    get_filename_component(source_dir "${FRDM_SOURCE_ROOT}/${source_file}" DIRECTORY)
    list(APPEND FRDM_NATIVE_INCLUDE_DIRS "${source_dir}")
endforeach()
list(REMOVE_DUPLICATES FRDM_NATIVE_INCLUDE_DIRS)
