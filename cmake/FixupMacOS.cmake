# The recursive scanner has already copied all non-system dependencies. Use
# short @rpath names: prebuilt modules (notably MacPorts' SVG loader) may lack
# the header padding needed for longer @executable_path/../lib references.
find_program(install_name_tool NAMES install_name_tool REQUIRED)
file(REAL_PATH "${root}" root)
file(GLOB binaries "${root}/bin/*" "${root}/lib/*.dylib" "${root}/lib/*.so")
list(APPEND binaries ${staged_modules})
set(real_binaries)
foreach(binary IN LISTS binaries)
    file(REAL_PATH "${binary}" real_binary)
    cmake_path(IS_PREFIX root "${real_binary}" NORMALIZE inside_bundle)
    if(NOT inside_bundle)
        message(FATAL_ERROR "Refusing to modify a binary outside the bundle: ${binary}")
    endif()
    list(APPEND real_binaries "${real_binary}")
endforeach()
list(REMOVE_DUPLICATES real_binaries)

foreach(binary IN LISTS real_binaries)
    get_filename_component(base "${binary}" NAME)
    get_filename_component(dir "${binary}" DIRECTORY)
    file(RELATIVE_PATH to_lib "${dir}" "${root}/lib")
    set(rpath "@loader_path")
    if(NOT to_lib STREQUAL "")
        string(APPEND rpath "/${to_lib}")
    endif()
    execute_process(COMMAND "${otool}" -L "${binary}"
        OUTPUT_VARIABLE dependencies COMMAND_ERROR_IS_FATAL ANY)
    set(changes -id "@rpath/${base}")
    string(REPLACE "\n" ";" dependency_lines "${dependencies}")
    foreach(line IN LISTS dependency_lines)
        string(STRIP "${line}" line)
        if(NOT line MATCHES "^(.+) \\(compatibility version [^)]*\\)$")
            continue()
        endif()
        set(dependency "${CMAKE_MATCH_1}")
        if(dependency MATCHES "^/System/Library/|^/usr/lib/")
            continue()
        endif()
        get_filename_component(dependency_name "${dependency}" NAME)
        # otool -L also lists a dylib's own install ID, including plugin dylibs
        # that live in module directories rather than directly under lib/.
        if(dependency_name STREQUAL base)
            continue()
        endif()
        if(NOT EXISTS "${root}/lib/${dependency_name}")
            message(FATAL_ERROR "Missing bundled dependency of ${binary}: ${dependency}")
        endif()
        list(APPEND changes -change "${dependency}" "@rpath/${dependency_name}")
    endforeach()

    # Remove development rpaths so they cannot mask a missing bundled library.
    execute_process(COMMAND "${otool}" -l "${binary}"
        OUTPUT_VARIABLE load_commands COMMAND_ERROR_IS_FATAL ANY)
    string(REGEX MATCHALL "cmd LC_RPATH\n[^\n]*\n[ \t]*path [^\n]+" old_rpaths "${load_commands}")
    list(REMOVE_DUPLICATES old_rpaths)
    foreach(command IN LISTS old_rpaths)
        string(REGEX REPLACE "^.*\n[ \t]*path (.+) \\(offset [0-9]+\\)$" "\\1" old_rpath "${command}")
        list(APPEND changes -delete_rpath "${old_rpath}")
    endforeach()
    # Apply shrinking references and the new rpath together to fit tight headers.
    list(APPEND changes -add_rpath "${rpath}")
    file(CHMOD "${binary}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    execute_process(COMMAND "${install_name_tool}" ${changes} "${binary}"
        COMMAND_ERROR_IS_FATAL ANY)
endforeach()
