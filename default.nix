with import <nixpkgs> {};
stdenv.mkDerivation {
    name = "visualboyadvance-m";
    buildInputs = [ ninja cmake gcc nasm gettext pkg-config ccache zip sfml zlib ffmpeg wxGTK31-gtk3 mesa glfw SDL2 gtk3-x11 ];
}
