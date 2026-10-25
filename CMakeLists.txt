cmake_minimum_required(VERSION 3.25)

project(ivy-gtk C)

set(CMAKE_C_STANDARD 11)


if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
#    if(EXISTS "/opt/local/bin/pkgconf")
        set(ENV{PKG_CONFIG_PATH} "/opt/local/share/pkgconfig")
#        set(PKG_CONFIG_EXECUTABLE "/opt/local/bin/pkgconf" CACHE FILEPATH "pkgconf executable" FORCE)
#    endif()
endif()

find_package(PkgConfig REQUIRED)

pkg_check_modules(GTK4 REQUIRED gtk4)
pkg_check_modules(ADWAITA REQUIRED libadwaita-1)
pkg_check_modules(JSON_C REQUIRED json-c)
pkg_check_modules(LIBCURL REQUIRED libcurl)

add_executable(ivy-gtk
    src/main.c
    src/modlist.c
    src/net.c
    src/sanitizer.c
    $<$<PLATFORM_ID:Windows>:src/steam-win.c>
    $<$<PLATFORM_ID:Darwin>:src/steam-mac.c>
    $<$<PLATFORM_ID:Linux>:src/steam-unix.c>
    src/installer.c
    src/ui.c
)

# we only include libadwaita's thingies because adwaita depends on gtk4
# and 'ld' would warn about duplicates

target_include_directories(ivy-gtk PRIVATE
    ${ADWAITA_INCLUDE_DIRS}
    ${JSON_C_INCLUDE_DIRS}
    ${LIBCURL_INCLUDE_DIRS}
)

target_compile_options(ivy-gtk PRIVATE
    ${ADWAITA_CFLAGS_OTHER}
    ${JSON_C_CFLAGS_OTHER}
    ${LIBCURL_CFLAGS_OTHER}
)

target_link_directories(ivy-gtk PRIVATE
    ${ADWAITA_LIBRARY_DIRS}
    ${JSON_C_LIBRARY_DIRS}
    ${LIBCURL_LIBRARY_DIRS}
)

target_link_libraries(ivy-gtk PRIVATE
    ${ADWAITA_LIBRARIES}
    ${JSON_C_LIBRARIES}
    ${LIBCURL_LIBRARIES}
)

IF(MINGW)
	target_link_libraries(ivy-gtk PRIVATE winpthread)
endif()
