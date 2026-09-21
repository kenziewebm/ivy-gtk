#!/usr/bin/env bash

build_osx() {
	if [ -d build ]; then
		echo "deleting build directory"
		rm -r build
	fi

	if [ "$(which pkgconf)" != "/usr/local/bin/pkgconf" ]; then
		echo "error: pkgconf not installed in /usr/local/bin/pkgconf"
		echo "try running 'brew install pkg-config'"
		exit
	fi
	echo "found pkgconf"

	if ! which cmake >/dev/null; then
		echo "error: cmake not found in PATH"
		echo "try running 'brew install cmake'"
		exit
	fi
	echo "found cmake"

	if ! pkgconf gtk4; then
		echo "error: gtk4 not found by pkgconf"
		echo "try running 'brew install gtk4'"
		exit
	fi
	echo "found gtk4"

	if ! pkgconf libadwaita-1; then
		echo "error: libadwaita not found by pkgconf"
		echo "try running 'brew install libadwaita adwaita-icon-theme'"
		exit
	fi
	echo "found libadwaita"
	echo "starting build (will send notification when done)"
	cmake -B build -S .
	cmake --build build
	cp build/ivy-gtk .
	echo "done"
	tput bel
}

case "$(uname)" in
	"Darwin") build_osx  ;;
	"Linux")  build_linux;;
	"Msys")   build_msys ;;
	*) echo "Unsupported OS!"; exit;;
esac

