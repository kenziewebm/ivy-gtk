# Packaging is a separate build target; ordinary development builds stay fast.
pkg_get_variable(IVY_GLIB_PREFIX glib-2.0 prefix)
pkg_get_variable(IVY_GLIB_LIBDIR glib-2.0 libdir)
pkg_get_variable(IVY_GLIB_DATADIR glib-2.0 datadir)
set(IVY_GTK_DATADIR "${GTK4_PREFIX}/share;${ADWAITA_PREFIX}/share")
pkg_get_variable(IVY_PIXBUF_MODULEDIR gdk-pixbuf-2.0 gdk_pixbuf_moduledir)
get_filename_component(IVY_TOOL_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
set(IVY_SEARCH_DIRS ${ADWAITA_LIBRARY_DIRS} ${JSON_C_LIBRARY_DIRS}
    ${LIBCURL_LIBRARY_DIRS} ${LIBARCHIVE_LIBRARY_DIRS}
    "${IVY_GLIB_PREFIX}/bin" "${IVY_GLIB_LIBDIR}" "${IVY_TOOL_DIR}")
list(REMOVE_DUPLICATES IVY_SEARCH_DIRS)
configure_file("${CMAKE_CURRENT_LIST_DIR}/Bundle.cmake.in"
    "${CMAKE_BINARY_DIR}/Bundle.cmake" @ONLY)
add_custom_target(bundle
    COMMAND "${CMAKE_COMMAND}" "-DIVY_EXECUTABLE=$<TARGET_FILE:ivy-gtk>"
        -P "${CMAKE_BINARY_DIR}/Bundle.cmake"
    DEPENDS ivy-gtk
    USES_TERMINAL
    COMMENT "Collecting runtime dependencies and creating a portable ZIP")
