cmake_minimum_required(VERSION 3.25)

project(ivy-gtk C)

set(CMAKE_C_STANDARD 11)

if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(PKG_CONFIG_EXECUTABLE "/usr/local/bin/pkgconf")
endif()

find_package(PkgConfig REQUIRED)

pkg_check_modules(GTK4 REQUIRED gtk4)
pkg_check_modules(ADWAITA REQUIRED libadwaita-1)

add_executable(ivy-gtk
    src/main.c
)

# we only include libadwaita's thingies because adwaita depends on gtk4
# and 'ld' would warn about duplicates

target_include_directories(ivy-gtk PRIVATE
    ${ADWAITA_INCLUDE_DIRS}
)

target_compile_options(ivy-gtk PRIVATE
    ${ADWAITA_CFLAGS_OTHER}
)

target_link_directories(ivy-gtk PRIVATE
    ${ADWAITA_LIBRARY_DIRS}
)

target_link_libraries(ivy-gtk PRIVATE
    ${ADWAITA_LIBRARIES}
)
