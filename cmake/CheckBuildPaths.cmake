include_guard(GLOBAL)

# Qt 6 cachegen embeds the relative QML/JS source path in the generated C++
# basename. CMake can hash its directory, but that basename remains long.
# Estimate the shortened MinGW dependency path, including the .obj.d suffix.
# Kept Qt-free so the reported Windows failure can be regression-tested in -P mode.
function(frdm_qml_dependency_path_length build_dir target out_length out_source)
    string(REPLACE "\\" "/" build_root "${build_dir}")
    string(REGEX REPLACE "/+$" "" build_root "${build_root}")
    set(longest_length 0)
    set(longest_source "")
    foreach(qml_file IN LISTS ARGN)
        string(REGEX REPLACE "\\.(js|mjs|qml)$" "_\\1" cache_name "${qml_file}")
        string(REGEX REPLACE "[$#?/ \\]+" "_" cache_name "${cache_name}")
        set(dependency_path "${build_root}/CMakeFiles/${target}.dir/00000000000000000000000000000000/${target}_${cache_name}.cpp.obj.d")
        string(LENGTH "${dependency_path}" dependency_length)
        if(dependency_length GREATER longest_length)
            set(longest_length "${dependency_length}")
            set(longest_source "${qml_file}")
        endif()
    endforeach()
    set(${out_length} "${longest_length}" PARENT_SCOPE)
    set(${out_source} "${longest_source}" PARENT_SCOPE)
endfunction()

function(frdm_check_mingw_build_paths build_dir target)
    frdm_qml_dependency_path_length("${build_dir}" "${target}" longest_length longest_source ${ARGN})
    # MAX_PATH includes the terminating null character, so 260 visible characters
    # already exceed the traditional Windows limit used by affected MinGW tools.
    if(longest_length GREATER_EQUAL 260)
        message(FATAL_ERROR
            "The MinGW build directory is too long for this project's generated QML files.\n"
            "  Build directory: ${build_dir}\n"
            "  QML source: ${longest_source}\n"
            "  Estimated dependency path: ${longest_length} characters even with a hashed object directory.\n"
            "In Qt Creator > Projects > Build Settings, set Build directory to a NEW short path, "
            "for example E:/b/suno-debug, then Run CMake and Build.\n"
            "Keep the source project where it is. Do not copy CMakeCache.txt into the new directory.\n"
            "See docs/BUILDING.md. Cleaning the existing long directory does not shorten its paths.")
    endif()
endfunction()
