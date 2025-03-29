with import <nixpkgs> {};
stdenv.mkDerivation {
    name = "visualboyadvance-m";
    buildInputs = if stdenv.isDarwin then
        [ ninja cmake nasm faudio gettext libintl pkg-config zip zlib openal ffmpeg wxGTK32 SDL3 pcre pcre2 darwin.apple_sdk.frameworks.System darwin.apple_sdk.frameworks.IOKit darwin.apple_sdk.frameworks.Carbon darwin.apple_sdk.frameworks.Cocoa darwin.apple_sdk.frameworks.QuartzCore darwin.apple_sdk.frameworks.AudioToolbox darwin.apple_sdk.frameworks.OpenGL darwin.apple_sdk.frameworks.OpenAL llvmPackages_latest.clang llvmPackages_latest.bintools ]
    else
        [ ninja cmake gcc clang llvm llvmPackages.libcxx nasm faudio gettext libintl pkg-config zip zlib openal ffmpeg wxGTK32 libGL libGLU glfw SDL3 gtk3-x11 pcre pcre2 util-linuxMinimal libselinux libsepol libthai libdatrie xorg.libXdmcp xorg.libXtst libxkbcommon libepoxy dbus at-spi2-core ];
}
