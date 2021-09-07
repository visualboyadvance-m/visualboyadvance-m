with import <nixpkgs> {};
stdenv.mkDerivation {
    name = "visualboyadvance-m";
    buildInputs = [ ninja cmake gcc nasm gettext pkg-config zip sfml zlib ffmpeg wxGTK31-gtk3 mesa glfw SDL2 gtk3-x11 pcre util-linuxMinimal libselinux libsepol libthai libdatrie xorg.libXdmcp xorg.libXtst libxkbcommon epoxy dbus_libs at-spi2-core ];
}
