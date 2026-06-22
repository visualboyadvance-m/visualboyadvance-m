with import <nixpkgs> {};
stdenv.mkDerivation {
    name = "visualboyadvance-m";
    buildInputs = if stdenv.isDarwin then
        [ ninja cmake nasm faudio gettext libintl libtiff pkg-config zip zlib openal ffmpeg x264 x265 wxwidgets_3_3 sdl3 pcre pcre2 llvmPackages_latest.clang llvmPackages_latest.bintools moltenvk vulkan-loader vulkan-headers ]
    else
        [ ninja cmake gcc clang llvm llvmPackages.libcxx nasm faudio gettext libintl libtiff pkg-config zip zlib openal ffmpeg x264 x265 wxGTK33 libGL libGLU glfw sdl3 gtk3-x11 pcre pcre2 util-linuxMinimal libselinux libsepol libthai libdatrie xorg.libXdmcp xorg.libXtst libxkbcommon libepoxy dbus at-spi2-core vulkan-loader vulkan-headers ];
}
