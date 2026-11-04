# Ivy

deadlock mod manager written in c, built with gtk4

## Features
- mod list with details downloaded from deadlockmods
- install delete mods
- fuzzy search
- auto find game dir and mod dir
- mostly compatible with the other (worse) tauri deadlock mod manager
- custom game dir i think should work

## Planned features
- updates
- dependancy resolution
- maybe combining vpk files?

## Building

our patent-pending "Build for dummies :p" build system automagically handles most edge cases and tells you what
dependancies to install to successfully build the thing. it can also give you a zip file with the `--bundle` flag

### Linux
install the dependencies with your package manager, then run:

```sh
bash build-for-dummies.sh
./ivy-gtk
```

### macOS
uses macports since i have an intel hackintosh. install xcode build tools and depends, then run:

```sh
xcode-select --install
sudo port install pkgconfig cmake gtk4 libadwaita json-c curl libarchive
bash build-for-dummies.sh
./ivy-gtk
```

### Windows
msys ucrt64 since i am not touching msvc

```sh
pacman -S mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-pkg-config mingw-w64-ucrt-x86_64-gtk4 mingw-w64-ucrt-x86_64-libadwaita mingw-w64-ucrt-x86_64-adwaita-icon-theme mingw-w64-ucrt-x86_64-json-c mingw-w64-ucrt-x86_64-curl mingw-w64-ucrt-x86_64-libarchive mingw-w64-ucrt-x86_64-winpthreads
bash build-for-dummies.sh
./ivy-gtk.exe
```

### Others
you can freestyle cmake if you want

```sh
cmake -S . -B build
cmake --build build
./build/ivy-gtk
```
