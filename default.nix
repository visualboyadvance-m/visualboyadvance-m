with import <nixpkgs> {};
stdenv.mkDerivation {
    name = "visualboyadvance-m";
    buildInputs = if stdenv.isDarwin then
        [ ninja cmake gcc nasm gettext pkg-config zip sfml zlib ffmpeg wxGTK32 SDL2 pcre ]
    else
        [ ninja cmake gcc nasm gettext pkg-config zip sfml zlib ffmpeg wxGTK32 mesa glfw SDL2 gtk3-x11 pcre util-linuxMinimal libselinux libsepol libthai libdatrie xorg.libXdmcp xorg.libXtst libxkbcommon epoxy dbus at-spi2-core ];
}
