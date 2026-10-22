#!/usr/bin/env bash

build_osx() {
    export PKG_CONFIG_PATH="/opt/local/share/pkgconfig"
	local pkgconf="/opt/local/bin/pkg-config"
	local port_install="sudo port install pkgconfig cmake gtk4 libadwaita json-c curl"

	if [ -d build ]; then
		echo "deleting build directory"
		rm -r build
	fi

	if [ ! -x "$pkgconf" ]; then
		echo "error: MacPorts pkgconf not found at $pkgconf"
		echo "try running '$port_install'"
		exit 1
	fi
	echo "found pkgconf"

	if ! command -v cmake >/dev/null; then
		echo "error: cmake not found in PATH"
		echo "try running 'sudo port install cmake'"
		exit 1
	fi
	echo "found cmake"

	if ! "$pkgconf" --exists gtk4; then
		echo "error: gtk4 not found by pkgconf"
		echo "try running 'sudo port install gtk4'"
		exit 1
	fi
	echo "found gtk4"

	if ! "$pkgconf" --exists libadwaita-1; then
		echo "error: libadwaita not found by pkgconf"
		echo "try running 'sudo port install libadwaita'"
		exit 1
	fi
	echo "found libadwaita"

	if ! "$pkgconf" --exists json-c; then
		echo "error: json-c not found by pkgconf"
		echo "try running 'sudo port install json-c'"
		exit 1
	fi

	if ! "$pkgconf" --exists libcurl; then
		echo "error: libcurl not found by pkgconf"
		echo "try running 'sudo port install curl'"
		exit 1
	fi

	echo "starting build (will send notification when done)"
	cmake -B build -S .
	cmake --build build
	cp build/ivy-gtk .
	echo "done"
	tput bel
}

build_msys() {
	if [ -d build ]; then
		echo "deleting build directory"
		rm -r build
	fi

	if [ "$(which pkg-config)" != "/ucrt64/bin/pkg-config" ]; then
		echo "error: pkg-config is not ucrt64"
		echo "try running 'pacman -S mingw-w64-ucrt-x86_64-pkg-config"
		exit
	fi
	echo "found ucrt64 pkg-config"

	if [ "$(which cmake)" != "/ucrt64/bin/cmake" ]; then
		echo "error: cmake is not ucrt64"
		echo "try running 'pacman -S mingw-w64-ucrt-x86_64-cmake"
		exit
	fi
	echo "found ucrt64 cmake"

	if ! pkg-config gtk4; then 
		echo "error: gtk4 not found by pkg-config"
		echo "try running 'pacman -S mingw-w64-ucrt-x86_64-gtk4'"
		exit
	fi
	echo "found gtk4"
	
	if ! pkg-config libadwaita-1; then
		echo "error: libadwaita not found by pkg-config"
		echo "try running 'pacman -S mingw-w64-ucrt-x86_64-libadwaita mingw-w64-ucrt-x86_64-adwaita-icon-theme'"
		exit
	fi

	if ! pkg-config json-c; then
		echo "error: json-c not found by pkg-config"
		echo "try running 'pacman -S mingw-w64-ucrt-x86_64-json-c'"
		exit
	fi

	echo "found libadwaita"
	echo "starting build (will notify when done)"
	cmake -B build -S .
	cmake --build build
	cp build/ivy-gtk.exe .
	echo "done"
	tput bel
}

case "$(uname -o)" in
	"Darwin") build_osx  ;;
	"Linux")  build_linux;;
	"Msys")   build_msys ;;
	*) echo "Unsupported OS!"; exit;;
esac

