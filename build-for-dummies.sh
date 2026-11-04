#!/usr/bin/env bash

set -e

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
bundle_requested=false
case "${1:-}" in
	--package) bundle_requested=true ;;
	"") ;;
	*) echo "usage: bash build-for-dummies.sh [--package]"; exit 1 ;;
esac
if [ "$#" -gt 1 ]; then
	echo "usage: bash build-for-dummies.sh [--package]"
	exit 1
fi

build_project() {
	echo "starting build"
	cmake -B build -S .
	cmake --build build
	cp "build/ivy-gtk${1:-}" .
	if "$bundle_requested"; then
		cmake --build build --target bundle
	fi
	echo "done"
	tput bel 2>/dev/null || true
}

build_osx() {
	export PKG_CONFIG_PATH="/opt/local/share/pkgconfig"
	local pkgconf="/opt/local/bin/pkg-config"

	if [ ! -x "$pkgconf" ]; then
		echo "error: MacPorts pkg-config not found at $pkgconf"
		echo "try running 'sudo port install pkgconfig'"
		exit 1
	fi

	if ! command -v cmake >/dev/null; then
		echo "error: cmake not found in PATH"
		echo "try running 'sudo port install cmake'"
		exit 1
	fi

	for dep in gtk4 libadwaita-1 json-c libcurl libarchive; do
		local package="$dep"
		case "$dep" in
			libadwaita-1) package="libadwaita" ;;
			libcurl) package="curl" ;;
		esac
		if ! "$pkgconf" --exists "$dep"; then
			echo "error: $dep not found by pkg-config"
			echo "try running 'sudo port install $package'"
			exit 1
		fi
		echo "found $dep"
	done

	if [ -d build ]; then
		echo "deleting build directory"
		rm -r build
	fi
	build_project
}

build_linux() {
	for tool in cmake pkg-config cc make; do
		if ! command -v "$tool" >/dev/null; then
			echo "error: $tool not found in PATH"
			echo "install $tool with your package manager"
			exit 1
		fi
	done

	for dep in gtk4 libadwaita-1 json-c libcurl libarchive; do
		if ! pkg-config --exists "$dep"; then
			echo "error: $dep not found by pkg-config"
			echo "install ${dep%-1} with your package manager"
			exit 1
		fi
		echo "found $dep"
	done
	build_project
}

build_msys() {
	for tool in pkg-config cmake; do
		if [ "$(command -v "$tool")" != "/ucrt64/bin/$tool" ]; then
			echo "error: $tool is not ucrt64"
			echo "try running 'pacman -S mingw-w64-ucrt-x86_64-$tool'"
			exit 1
		fi
		echo "found ucrt64 $tool"
	done

	for dep in gtk4 libadwaita-1 json-c libcurl libarchive; do
		local package="$dep"
		case "$dep" in
			libadwaita-1) package="libadwaita mingw-w64-ucrt-x86_64-adwaita-icon-theme" ;;
			libcurl) package="curl" ;;
		esac
		if ! pkg-config --exists "$dep"; then
			echo "error: $dep not found by pkg-config"
			echo "try running 'pacman -S mingw-w64-ucrt-x86_64-$package'"
			exit 1
		fi
		echo "found $dep"
	done

	if [ ! -e "/ucrt64/include/pthread.h" ]; then
		echo "error: winpthreads not found"
		echo "try running 'pacman -S mingw-w64-ucrt-x86_64-winpthreads'"
		exit 1
	fi
	echo "found winpthreads"

	if [ -d build ]; then
		echo "deleting build directory"
		rm -r build
	fi
	build_project .exe
}

case "$(uname -s)" in
	Darwin) build_osx ;;
	Linux) build_linux ;;
	MSYS*|MINGW*) build_msys ;;
	*) echo "Unsupported OS!"; exit 1 ;;
esac
