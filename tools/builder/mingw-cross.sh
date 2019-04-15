#!/bin/sh

set -e

target_bits=64
target_cpu=x86_64
lib_suffix=64

case "$1" in
    -64)
        shift
        ;;
    -32)
        target_bits=32
        target_cpu=i686
        lib_suffix=
        shift
        ;;
esac

target_arch="${target_cpu}-w64-mingw32"

export BUILD_ROOT="$HOME/vbam-build-mingw${target_bits}"

do_not_remove_dists=

host_dists='
    unzip zip cmake autoconf autoconf-archive automake getopt m4 gsed bison
    flex-2.6.3 flex c2man docbook2x libtool help2man texinfo xmlto pkgconfig
    nasm yasm xorg-macros dejavu liberation urw graphviz docbook4.2
    docbook4.1.2 docbook4.3 docbook4.4 docbook4.5 docbook5.0 docbook-xsl
    docbook-xsl-ns python2 python3 swig doxygen bakefile setuptools pip
    intltool ninja meson shared-mime-info gperf
'

both_dists='
    openssl zlib bzip2 libiconv gettext xz libxml2 expat libpng freetype
    fontconfig libicu
'

[ -n "$BUILD_ENV" ] && eval "$BUILD_ENV"

BUILD_ENV=$BUILD_ENV$(cat <<EOF

export CC='${target_arch}-gcc'
export CXX='${target_arch}-g++'
export STRIP='${target_arch}-strip'

export CPPFLAGS="$CPPFLAGS"
export CFLAGS="$CFLAGS${CFLAGS:+ }-L/usr/${target_arch}/usr/lib${lib_suffix}"
export CXXFLAGS="$CXXFLAGS${CXXFLAGS:+ }-L/usr/${target_arch}/usr/lib${lib_suffix}"
export OBJCXXFLAGS="$OBJCXXFLAGS${OBJCXXFLAGS:+ }-L/usr/${target_arch}/usr/lib${lib_suffix}"
export LDFLAGS="-L/usr/${target_arch}/usr/lib${lib_suffix} $LDFLAGS"

EOF
)

export BUILD_ENV

export REQUIRED_CONFIGURE_ARGS="--host=${target_arch}"

export REQUIRED_CMAKE_ARGS="$REQUIRED_CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE='$(perl -MCwd=abs_path -le "print abs_path(q{${0%/*}/../../cmake/Toolchain-cross-MinGW-w64-${target_cpu}.cmake})")'"

. "${0%/*}/../builder/mingw.sh"

installing_cross_deps() {
    puts "${NL}[32mInstalling cross dependencies for your OS...[0m${NL}${NL}"
}

fedora_install_cross_deps() {
    pkg_prefix="mingw${target_bits}"

    set --
    for p in gcc cpp gcc-c++ binutils headers crt filesystem winpthreads-static; do
        set -- "$@" "${pkg_prefix}-${p}"
    done

    sudo dnf install -y --nogpgcheck --best --allowerasing "$@" gettext-devel wxGTK3-devel python
}

suse_install_cross_deps() {
    suse_dist=$(. /etc/os-release; echo $PRETTY_NAME | sed 's/ /_/g')

    sudo zypper ar -f https://download.opensuse.org/repositories/windows:/mingw:/win64/${suse_dist}/windows:mingw:win64.repo || :
    sudo zypper ar -f https://download.opensuse.org/repositories/windows:/mingw:/win32/${suse_dist}/windows:mingw:win32.repo || :

    sudo zypper refresh

    pkg_prefix="mingw${target_bits}"

    set --
    for p in cross-gcc cross-cpp cross-gcc-c++ cross-binutils headers filesystem winpthreads-devel; do
        set -- "$@" "${pkg_prefix}-${p}"
    done

    sudo zypper in -y "$@" gettext-tools wxGTK3-3_2-devel python
}

case "$linux_distribution" in
    fedora)
        installing_cross_deps
        fedora_install_cross_deps
        done_msg
        ;;
    suse)
        installing_cross_deps
        suse_install_cross_deps
        done_msg
        ;;
esac

openssl_host=mingw
[ "$target_bits" -eq 64 ] && openssl_host=mingw64

table_line_replace DIST_CONFIGURE_OVERRIDES openssl-target "./Configure $openssl_host no-shared --prefix=/usr --openssldir=/etc/ssl --cross-compile-prefix=${target_arch}-"

table_line_append DIST_PRE_BUILD bzip2-target ':; sed -i.bak '\''s,include <sys\\stat.h>,include <sys/stat.h>,g'\'' *.c;'

table_line_append DIST_ARGS libicu-target "--with-cross-build=\$BUILD_ROOT/dists/libicu/source"

# the native tools openal uses for building can be problematic when cross-compiling
table_line_append DIST_PATCHES openal '-p0 https://gist.githubusercontent.com/rkitover/d371d199ee0ac67864d0940aa7e7c12c/raw/29f3bc4afaba41b35b3fcbd9d18d1f0a22e3dc13/openal-cross-no-cmake-for-native-tools.patch'

table_line_replace DIST_POST_BUILD harfbuzz "$(table_line DIST_POST_BUILD harfbuzz | sed 's/rebuild_dist freetype /rebuild_dist freetype-target /')"

table_line_replace DIST_POST_BUILD glib     "$(table_line DIST_POST_BUILD glib     | sed 's/rebuild_dist gettext /rebuild_dist gettext-target /')"

table_line_append DIST_ARGS libsoxr '-DHAVE_WORDS_BIGENDIAN_EXITCODE=0'

vpx_target=x86-win32-gcc
[ "$target_bits" -eq 64 ] && vpx_target=x86_64-win64-gcc

table_line_replace DIST_CONFIGURE_OVERRIDES libvpx "./configure --target=$vpx_target \$CONFIGURE_ARGS $(table_line DIST_ARGS libvpx)"

table_line_remove  DIST_ARGS                libvpx

table_line_replace DIST_CONFIGURE_OVERRIDES ffmpeg "\
    ./configure --arch=$target_cpu --target-os=mingw32 --cross-prefix=${target_arch}- \
        --pkg-config='\$BUILD_ROOT/host/bin/pkg-config' \
        \$CONFIGURE_ARGS $(table_line DIST_ARGS ffmpeg) \
"

table_line_remove  DIST_ARGS                ffmpeg
