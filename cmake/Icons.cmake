# Reuse GTK's image library to generate native icons from the one source PNG.
# No Python, ImageMagick, or platform-specific image tools are needed to build.
pkg_check_modules(PIXBUF REQUIRED IMPORTED_TARGET gdk-pixbuf-2.0)
add_executable(ivy-icon-converter EXCLUDE_FROM_ALL
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/icon-converter.c")
target_link_libraries(ivy-icon-converter PRIVATE PkgConfig::PIXBUF)
set(IVY_ICON_DIR "${CMAKE_CURRENT_BINARY_DIR}/assets")
file(MAKE_DIRECTORY "${IVY_ICON_DIR}")
add_custom_command(
    OUTPUT "${IVY_ICON_DIR}/ivy.ico" "${IVY_ICON_DIR}/ivy.icns"
    COMMAND $<TARGET_FILE:ivy-icon-converter>
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ivy.png"
        "${IVY_ICON_DIR}/ivy.ico" "${IVY_ICON_DIR}/ivy.icns"
    DEPENDS ivy-icon-converter "${CMAKE_CURRENT_SOURCE_DIR}/assets/ivy.png"
    VERBATIM
    COMMENT "Generating application icons")
add_custom_target(ivy-icons DEPENDS "${IVY_ICON_DIR}/ivy.ico" "${IVY_ICON_DIR}/ivy.icns")
add_dependencies(ivy-gtk ivy-icons)

# GtkApplication automatically uses icons under its application resource path,
# with the application ID as the default window icon name. Embedding the PNG
# also makes ordinary development builds independent of the working directory.
pkg_get_variable(IVY_GLIB_BINDIR glib-2.0 bindir)
find_program(GLIB_COMPILE_RESOURCES NAMES glib-compile-resources
    HINTS "${IVY_GLIB_BINDIR}" REQUIRED)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ivy-resources.c"
    COMMAND "${GLIB_COMPILE_RESOURCES}" --generate-source --c-name ivy
        "--target=${CMAKE_CURRENT_BINARY_DIR}/ivy-resources.c"
        "--sourcedir=${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ivy.gresource.xml"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/ivy.gresource.xml"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ivy.png"
    VERBATIM)
target_sources(ivy-gtk PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/ivy-resources.c")

if(WIN32)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ivy-icon.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/ivy-icon.rc" @ONLY)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/ivy-icon.rc"
        PROPERTIES OBJECT_DEPENDS "${IVY_ICON_DIR}/ivy.ico")
    target_sources(ivy-gtk PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/ivy-icon.rc")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include(GNUInstallDirs)
    install(TARGETS ivy-gtk RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/ivy.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps"
        RENAME xyz.kenziewebm.ivy.png)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/xyz.kenziewebm.ivy.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
endif()
