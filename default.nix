with import <nixpkgs> {};
stdenv.mkDerivation {
    name = "visualboyadvance-m";
    buildInputs = if stdenv.isDarwin then
        [ ninja cmake gcc nasm gettext pkg-config zip sfml zlib openal ffmpeg wxGTK32 SDL2 pcre pcre2 darwin.apple_sdk.frameworks.IOKit darwin.apple_sdk.frameworks.Carbon darwin.apple_sdk.frameworks.Cocoa darwin.apple_sdk.frameworks.QuartzCore darwin.apple_sdk.frameworks.AudioToolbox darwin.apple_sdk.frameworks.OpenGL darwin.apple_sdk.frameworks.OpenAL ]
    else
        [ ninja cmake gcc nasm gettext pkg-config zip sfml zlib openal ffmpeg wxGTK32 mesa glfw SDL2 gtk3-x11 pcre pcre2 util-linuxMinimal libselinux libsepol libthai libdatrie xorg.libXdmcp xorg.libXtst libxkbcommon epoxy dbus at-spi2-core ];
}
