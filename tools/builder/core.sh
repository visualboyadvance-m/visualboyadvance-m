#!/bin/sh

set -e

[ -n "$BASH_VERSION" ] && set -o posix

BUILD_ROOT=${BUILD_ROOT:-$HOME/vbam-build}
TAR=${TAR:-tar --force-local}
CURL=${CURL:-curl --insecure}
PERL_MAKE=${PERL_MAKE:-make}

[ -n "$BUILD_ENV" ] && eval "$BUILD_ENV"

BUILD_ENV=$BUILD_ENV$(cat <<EOF

export BUILD_ROOT="$BUILD_ROOT"

export CC="\${CC:-gcc}"
export CXX="\${CXX:-g++}"

case "\$CC" in
    ccache*)
        :
        ;;
    *)
        if command -v ccache >/dev/null; then
            case "\$REQUIRED_CMAKE_ARGS" in
                *ccache*)
                    :
                    ;;
                *)
                    REQUIRED_CMAKE_ARGS="\$REQUIRED_CMAKE_ARGS -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER=\$CC -DCMAKE_CXX_COMPILER=\$CXX"
                    ;;
            esac
            export CC_ORIG=\$CC
            export CXX_ORIG=\$CXX
            export CC="ccache \$CC"
            export CXX="ccache \$CXX"
        fi
        ;;
esac

export CC_ORIG="\${CC_ORIG:-\$CC}"
export CXX_ORIG="\${CXX_ORIG:-\$CXX}"

export CPPFLAGS="$CPPFLAGS -I$BUILD_ROOT/root/include"
export CFLAGS="$CFLAGS -fPIC -I$BUILD_ROOT/root/include -L$BUILD_ROOT/root/lib -pthread -lm"
export CXXFLAGS="$CXXFLAGS -fPIC -I$BUILD_ROOT/root/include -L$BUILD_ROOT/root/lib -std=gnu++11 -fpermissive -pthread -lm"
export OBJCXXFLAGS="$OBJCXXFLAGS -fPIC -I$BUILD_ROOT/root/include -L$BUILD_ROOT/root/lib -std=gnu++11 -fpermissive -pthread -lm"
export LDFLAGS="$LDFLAGS -fPIC -L$BUILD_ROOT/root/lib -pthread -lm"
export STRIP="${STRIP:-strip}"

if [ -z "\$OPENMP" ] && echo "\$CC" | grep -Eq gcc; then
    export CFLAGS="\$CFLAGS -fopenmp"
    export CXXFLAGS="\$CXXFLAGS -fopenmp"
    export OBJCXXFLAGS="\$OBJCXXFLAGS -fopenmp"
    export LDFLAGS="\$LDFLAGS -fopenmp"
    export OPENMP=1
fi

export CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH:-$BUILD_ROOT/root}"
export PKG_CONFIG_PATH="$BUILD_ROOT/root/lib/pkgconfig:$BUILD_ROOT/root/share/pkgconfig"

export LD_LIBRARY_PATH="$BUILD_ROOT/root/lib"

export PERL_MM_USE_DEFAULT=1
export PERL_EXTUTILS_AUTOINSTALL="--defaultdeps"

export OPENSSL_ROOT="$BUILD_ROOT/root"

export PERL_MB_OPT='--install_base $BUILD_ROOT/root/perl5'
export PERL_MM_OPT='INSTALL_BASE=$BUILD_ROOT/root/perl5'
export PERL5LIB="$BUILD_ROOT/root/perl5/lib/perl5"
export PERL_LOCAL_LIB_ROOT="$BUILD_ROOT/root/perl5"

case "\$PATH" in
    *"$BUILD_ROOT"*)
        ;;
    *)
        export PATH="$BUILD_ROOT/root/bin:$BUILD_ROOT/root/perl5/bin:\$PATH"
        ;;
esac

export PERL_MM_OPT="\$PERL_MM_OPT CCFLAGS='\$CFLAGS' LDDFLAGS='\$LDFLAGS'"

export MANPATH="$BUILD_ROOT/root/man:$BUILD_ROOT/root/share/man:/usr/share/man:/usr/local/share/man"

export XML_CATALOG_FILES="$BUILD_ROOT/root/etc/xml/catalog.xml"

export FORMAT_DIR="$BUILD_ROOT/root/share/xmlto/format"

export XDG_DATA_DIRS="$BUILD_ROOT/root/share"

export FONTCONFIG_PATH="$BUILD_ROOT/root/etc/fonts"

export BISON_PKGDATADIR="$BUILD_ROOT/root/share/bison"

export SWIG_LIB="\$(echo \$BUILD_ROOT/root/share/swig/*)"

if command -v cygpath >/dev/null; then
    export XML_CATALOG_FILES=\$(cygpath -m "\$XML_CATALOG_FILES")
fi

EOF
)

ORIG_PATH=$PATH

eval "$BUILD_ENV"

PRE_BUILD_DISTS="$PRE_BUILD_DISTS bzip2 xz unzip"

DISTS=$DISTS'
    bzip2           https://github.com/nemequ/bzip2/releases/download/v1.0.6/bzip2-1.0.6.tar.gz                 lib/libbz2.a
    xz              https://tukaani.org/xz/xz-5.2.3.tar.gz                                                      lib/liblzma.a
    unzip           https://downloads.sourceforge.net/project/infozip/UnZip%206.x%20%28latest%29/UnZip%206.0/unzip60.tar.gz     bin/unzip
    zlib            https://zlib.net/zlib-1.2.11.tar.gz                                                         lib/libz.a
    ccache          https://www.samba.org/ftp/ccache/ccache-3.4.3.tar.xz                                        bin/ccache
    zip             https://downloads.sourceforge.net/project/infozip/Zip%203.x%20%28latest%29/3.0/zip30.tar.gz                 bin/zip
    openssl         https://www.openssl.org/source/openssl-1.0.2p.tar.gz                                        lib/libssl.a
    cmake           https://cmake.org/files/v3.13/cmake-3.13.0-rc1.tar.gz                                       bin/cmake
    m4              http://ftp.gnu.org/gnu/m4/m4-1.4.18.tar.xz                                                  bin/m4
    autoconf        https://ftp.gnu.org/gnu/autoconf/autoconf-2.69.tar.xz                                       bin/autoconf
    autoconf-archive http://mirror.team-cymru.org/gnu/autoconf-archive/autoconf-archive-2017.09.28.tar.xz       share/aclocal/ax_check_gl.m4
    automake        https://ftp.gnu.org/gnu/automake/automake-1.15.1.tar.xz                                     bin/automake
    libtool         https://ftp.gnu.org/gnu/libtool/libtool-2.4.6.tar.xz                                        bin/libtool
    xorg-macros     https://www.x.org/archive//individual/util/util-macros-1.19.1.tar.bz2                       share/pkgconfig/xorg-macros.pc
    help2man        https://ftp.gnu.org/gnu/help2man/help2man-1.47.5.tar.xz                                     bin/help2man
    libiconv        https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.15.tar.gz                                   lib/libiconv.a
    gettext         http://ftp.gnu.org/pub/gnu/gettext/gettext-0.19.8.1.tar.xz                                  lib/libintl.a
    getopt          http://frodo.looijaard.name/system/files/software/getopt/getopt-1.1.6.tar.gz                bin/getopt
    gsed            http://ftp.gnu.org/gnu/sed/sed-4.4.tar.xz                                                   bin/sed
    bison           https://ftp.gnu.org/gnu/bison/bison-3.0.5.tar.xz                                            bin/bison
    texinfo         http://ftp.gnu.org/gnu/texinfo/texinfo-6.5.tar.xz                                           bin/makeinfo
    flex-2.6.3      https://github.com/westes/flex/releases/download/v2.6.3/flex-2.6.3.tar.gz                   bin/flex
    flex            https://github.com/westes/flex/archive/e7d45afc6aeb49745f17d21ddba4848e0c0118fc.tar.gz      bin/flex
    xmlto           https://releases.pagure.org/xmlto/xmlto-0.0.28.tar.bz2                                      bin/xmlto
    gperf           http://ftp.gnu.org/pub/gnu/gperf/gperf-3.1.tar.gz                                           bin/gperf
    libicu          https://github.com/unicode-org/icu/releases/download/release-63-rc/icu4c-63rc-src.tgz       lib/libicud*t*.a
    pkgconfig       https://pkgconfig.freedesktop.org/releases/pkg-config-0.29.2.tar.gz                         bin/pkg-config
    nasm            http://repo.or.cz/nasm.git/snapshot/53371ddd17b685f8880c22b8b698e494e0f1059b.tar.gz         bin/nasm
    yasm            http://www.tortall.net/projects/yasm/releases/yasm-1.3.0.tar.gz                             bin/yasm
    pcre            https://ftp.pcre.org/pub/pcre/pcre-8.41.tar.bz2                                             lib/libpcre.a
    libffi          ftp://sourceware.org/pub/libffi/libffi-3.2.1.tar.gz                                         lib/libffi.a
    c2man           http://www.ciselant.de/c2man/c2man-2.0@42.tar.gz                                            bin/c2man
    libxml2         ftp://xmlsoft.org/libxml2/libxml2-2.9.8.tar.gz                                              lib/libxml2.a
    libxslt         https://github.com/GNOME/libxslt/archive/v1.1.33-rc1.tar.gz                                 lib/libxslt.a
    XML-NamespaceSupport https://cpan.metacpan.org/authors/id/P/PE/PERIGRIN/XML-NamespaceSupport-1.12.tar.gz    perl5/lib/perl5/XML/NamespaceSupport.pm
    XML-SAX-Base    https://cpan.metacpan.org/authors/id/G/GR/GRANTM/XML-SAX-Base-1.09.tar.gz                   perl5/lib/perl5/XML/SAX/Base.pm
    XML-SAX         https://cpan.metacpan.org/authors/id/G/GR/GRANTM/XML-SAX-0.99.tar.gz                        perl5/lib/perl5/XML/SAX.pm
    docbook2x       https://downloads.sourceforge.net/project/docbook2x/docbook2x/0.8.8/docbook2X-0.8.8.tar.gz  bin/docbook2man
    expat           https://github.com/libexpat/libexpat/archive/R_2_2_4.tar.gz                                 lib/libexpat.a
    libpng          https://download.sourceforge.net/libpng/libpng-1.6.32.tar.xz                                lib/libpng.a
    libjpeg-turbo   https://github.com/libjpeg-turbo/libjpeg-turbo/archive/1.5.2.tar.gz                         lib/libjpeg.a
    libtiff         http://download.osgeo.org/libtiff/tiff-4.0.9.tar.gz                                         lib/libtiff.a
#    libcroco        http://ftp.gnome.org/pub/gnome/sources/libcroco/0.6/libcroco-0.6.12.tar.xz                  lib/libcroco-0.6.a
    libuuid         https://downloads.sourceforge.net/project/libuuid/libuuid-1.0.3.tar.gz                      lib/libuuid.a
    freetype        http://download.savannah.gnu.org/releases/freetype/freetype-2.9.1.tar.bz2                   lib/libfreetype.a
    fontconfig      https://freedesktop.org/software/fontconfig/release/fontconfig-2.13.0.tar.bz2               lib/libfontconfig.a
    libgd           https://github.com/libgd/libgd/releases/download/gd-2.2.5/libgd-2.2.5.tar.xz                lib/libgd.a
    dejavu          https://downloads.sourceforge.net/project/dejavu/dejavu/2.37/dejavu-fonts-ttf-2.37.tar.bz2  share/fonts/dejavu/DejaVuSansMono.ttf
    liberation      https://releases.pagure.org/liberation-fonts/liberation-fonts-ttf-2.00.1.tar.gz             share/fonts/liberation/LiberationMono-Regular.ttf
    urw             http://git.ghostscript.com/?p=urw-core35-fonts.git;a=snapshot;h=91edd6ece36e84a1c6d63a1cf63a1a6d84bd443a;sf=tgz share/fonts/urw/URWBookman-Light.ttf
    graphviz        https://gitlab.com/graphviz/graphviz/repository/archive.tar.bz2?ref=bd97cff688f7a7b85b6f1262e14eb1cac0862fcd    bin/dot_static
    docbook4.2      http://www.docbook.org/xml/4.2/docbook-xml-4.2.zip                                                    share/xml/docbook/schema/dtd/4.2/catalog.xml
    docbook4.1.2    http://www.docbook.org/xml/4.1.2/docbkx412.zip                                                        share/xml/docbook/schema/dtd/4.1.2/catalog.xml
    docbook4.3      http://www.docbook.org/xml/4.3/docbook-xml-4.3.zip                                                    share/xml/docbook/schema/dtd/4.3/catalog.xml
    docbook4.4      http://www.docbook.org/xml/4.4/docbook-xml-4.4.zip                                                    share/xml/docbook/schema/dtd/4.4/catalog.xml
    docbook4.5      http://www.docbook.org/xml/4.5/docbook-xml-4.5.zip                                                    share/xml/docbook/schema/dtd/4.5/catalog.xml
    docbook5.0      http://www.docbook.org/xml/5.0/docbook-5.0.zip                                                        share/xml/docbook/schema/dtd/5.0/catalog.xml
    docbook-xsl     https://downloads.sourceforge.net/project/docbook/docbook-xsl/1.79.1/docbook-xsl-1.79.1.tar.bz2       share/xml/docbook/stylesheet/docbook-xsl/catalog.xml
    docbook-xsl-ns  https://downloads.sourceforge.net/project/docbook/docbook-xsl-ns/1.79.1/docbook-xsl-ns-1.79.1.tar.bz2 share/xml/docbook/stylesheet/docbook-xsl-ns/catalog.xml
    python2         https://www.python.org/ftp/python/2.7.15/Python-2.7.15.tar.xz                               bin/python
    python3         https://www.python.org/ftp/python/3.6.5/Python-3.6.5.tar.xz                                 bin/python3
    swig            https://downloads.sourceforge.net/project/swig/swig/swig-3.0.12/swig-3.0.12.tar.gz          bin/swig
    libxml2-python  ftp://xmlsoft.org/libxml2/libxml2-sources-2.9.7.tar.gz                                      lib/python2.7/site-packages/libxml2.py
    doxygen         https://downloads.sourceforge.net/project/doxygen/rel-1.8.14/doxygen-1.8.14.src.tar.gz      bin/doxygen
    bakefile        https://github.com/vslavik/bakefile/archive/33c270a565ba0954cca9611b2443593b2e9e698a.tar.gz bin/bakefile_gen
    XML-Parser      https://cpan.metacpan.org/authors/id/T/TO/TODDR/XML-Parser-2.44.tar.gz                      perl5/man/man3/XML*Parser.3*
    intltool        https://launchpad.net/intltool/trunk/0.51.0/+download/intltool-0.51.0.tar.gz                bin/intltoolize
    ninja           https://github.com/ninja-build/ninja/archive/v1.8.2.tar.gz                                  bin/ninja
    meson           https://github.com/mesonbuild/meson/releases/download/0.44.0/meson-0.44.0.tar.gz            bin/meson
    glib            https://github.com/GNOME/glib/archive/2.58.1.tar.gz                                         lib/libglib-2.0.a
    libgpg-error    https://www.gnupg.org/ftp/gcrypt/libgpg-error/libgpg-error-1.27.tar.bz2                     lib/libgpg-error.a
    libgcrypt       https://www.gnupg.org/ftp/gcrypt/libgcrypt/libgcrypt-1.8.2.tar.bz2                          lib/libgcrypt.a
    libsecret       http://ftp.gnome.org/pub/gnome/sources/libsecret/0.18/libsecret-0.18.5.tar.xz               lib/libsecret-1.a
    sdl2            https://www.libsdl.org/release/SDL2-2.0.8.tar.gz                                            lib/libSDL2.a
    flac            https://github.com/xiph/flac/archive/f7cd466c24fb5d1966943f3ea36a1f4a37858597.tar.gz        lib/libFLAC.a
    libogg          http://downloads.xiph.org/releases/ogg/libogg-1.3.2.tar.xz                                  lib/libogg.a
    libvorbis       https://github.com/xiph/vorbis/archive/v1.3.5.tar.gz                                        lib/libvorbis.a
    harfbuzz        https://www.freedesktop.org/software/harfbuzz/release/harfbuzz-1.7.5.tar.bz2                lib/libharfbuzz.a
    sfml            https://github.com/SFML/SFML/archive/013d053277c980946bc7761a2a088f1cbb788f8c.tar.gz        lib/libsfml-system-s.a
    shared-mime-info http://freedesktop.org/~hadess/shared-mime-info-1.9.tar.xz                                 bin/update-mime-database
    wxwidgets       https://github.com/wxWidgets/wxWidgets/archive/e09d11c7acf768a40adb156c5a32fffb53231432.tar.gz lib/libwx_baseu-3.1*.a
    graphite2       https://github.com/silnrsi/graphite/releases/download/1.3.10/graphite2-1.3.10.tgz           lib/libgraphite2.a
    xvidcore        http://downloads.xvid.org/downloads/xvidcore-1.3.4.tar.bz2                                  lib/libxvidcore.a
    fribidi         https://github.com/fribidi/fribidi/releases/download/v1.0.1/fribidi-1.0.1.tar.bz2           lib/libfribidi.a
    libgsm          http://www.quut.com/gsm/gsm-1.0.17.tar.gz                                                   lib/libgsm.a
    libmodplug      https://github.com/Konstanty/libmodplug/archive/5a39f5913d07ba3e61d8d5afdba00b70165da81d.tar.gz lib/libmodplug.a
    libopencore-amrnb https://downloads.sourceforge.net/project/opencore-amr/opencore-amr/opencore-amr-0.1.5.tar.gz lib/libopencore-amrnb.a
    opus            https://archive.mozilla.org/pub/opus/opus-1.2.1.tar.gz                                      lib/libopus.a
    snappy          https://github.com/google/snappy/archive/1.1.7.tar.gz                                       lib/libsnappy.a
    libsoxr         https://downloads.sourceforge.net/project/soxr/soxr-0.1.2-Source.tar.xz                     lib/libsoxr.a
    speex           http://downloads.us.xiph.org/releases/speex/speex-1.2.0.tar.gz                              lib/libspeex.a
    libtheora       https://github.com/Distrotech/libtheora/archive/17b02c8c564475bb812e540b551219fc42b1f75f.tar.gz lib/libtheora.a
    vidstab         https://github.com/georgmartius/vid.stab/archive/v1.1.0.tar.gz                              lib/libvidstab.a
    libvo-amrwbenc  https://github.com/mstorsjo/vo-amrwbenc/archive/v0.1.3.tar.gz                               lib/libvo-amrwbenc.a
    mp3lame         https://downloads.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz                 lib/libmp3lame.a
    libass          https://github.com/libass/libass/releases/download/0.13.7/libass-0.13.7.tar.xz              lib/libass.a
    libbluray       ftp://ftp.videolan.org/pub/videolan/libbluray/1.0.2/libbluray-1.0.2.tar.bz2                 lib/libbluray.a
    libvpx          http://storage.googleapis.com/downloads.webmproject.org/releases/webm/libvpx-1.6.1.tar.bz2  lib/libvpx.a
    libwavpack      http://www.wavpack.com/wavpack-5.1.0.tar.bz2                                                lib/libwavpack.a
    libx264         ftp://ftp.videolan.org/pub/videolan/x264/snapshots/x264-snapshot-20171228-2245.tar.bz2      lib/libx264.a
    libx265         https://bitbucket.org/multicoreware/x265/downloads/x265_2.5.tar.gz                          lib/libx265.a
    libxavs         https://github.com/Distrotech/xavs/archive/distrotech-xavs-git.tar.gz                       lib/libxavs.a
    libzmq          https://github.com/zeromq/libzmq/releases/download/v4.2.2/zeromq-4.2.2.tar.gz               lib/libzmq.a
    libzvbi         https://downloads.sourceforge.net/project/zapping/zvbi/0.2.35/zvbi-0.2.35.tar.bz2           lib/libzvbi.a
    ffmpeg          https://github.com/ffmpeg/ffmpeg/archive/0c78b6a416c661afed745d44495b5194f714fb96.tar.gz    lib/libavformat.a
'

# for now we don't build ffmpeg because game recording is broken
BUILD_FFMPEG=
PROJECT_ARGS="$PROJECT_ARGS -DENABLE_FFMPEG=NO"

FFMPEG_DISTS='
    graphite2 xvidcore fribidi libgsm libmodplug libopencore-amrnb opus snappy
    libsoxr speex libtheora vidstab libvo-amrwbenc mp3lame libass libbluray
    libvpx libwavpack libx264 libx265 libxavs libzmq libzvbi ffmpeg
'

: ${PATH_SEP:=':'}

# these two can be set to always be included regardless of overrides
REQUIRED_CONFIGURE_ARGS="$REQUIRED_CONFIGURE_ARGS"
REQUIRED_CMAKE_ARGS="$REQUIRED_CMAKE_ARGS"

CONFIGURE_ARGS="$CONFIGURE_ARGS --disable-shared --enable-static --prefix=/usr"

CMAKE_BASE_ARGS="$CMAKE_BASE_ARGS -DBUILD_SHARED_LIBS=NO -DENABLE_SHARED=NO -DCMAKE_PREFIX_PATH:FILEPATH=\"\$CMAKE_PREFIX_PATH\" -DCMAKE_BUILD_TYPE=Release"

CMAKE_ARGS="$CMAKE_BASE_ARGS $CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=/usr"

MESON_ARGS="--prefix /usr --buildtype release --default-library static -Dintrospection=false"

DIST_PATCHES=$DIST_PATCHES'
    docbook2x       https://gist.githubusercontent.com/rkitover/0b5dcc95a0703a9b0e0e7eb6d325a98e/raw/e256d2fad8d19633ac8abe02a0d1e119063d1fd9/docbook2x.patch
    ccache          https://gist.githubusercontent.com/rkitover/4dd30c2841a2131be8715038f538d622/raw/40a8f0f50aa6ca5f9658a0c37aa17393cc80c109/ccache-3.4.2-msys.patch
    graphite2       https://gist.githubusercontent.com/rkitover/418600634d7cf19e2bf1c3708b50c042/raw/839b72d9cda545f2e3b640d743c1bd44b89861b9/graphite2-1.3.10-static.patch
    fontconfig      https://gist.githubusercontent.com/rkitover/542405b4509bd23daa76c98b15a94627/raw/b1ffcfe67c8da22452858216eac4b86afeb93eae/fontconfig-gperf.patch
    python2         https://gist.githubusercontent.com/rkitover/2d9e5baff1f1cc4f2618dee53083bd35/raw/7f33fcf5470a9f1013ac6ae7bb168368a98fe5a0/python-2.7.14-custom-static-openssl.patch https://gist.githubusercontent.com/rkitover/afab7ed3ac7ce1860c43a258571c8ae1/raw/6f5fc90a7acf5f5c3ffda2edf402b28f469a4b3b/python-2.7.14-static-libintl.patch
    python3         https://gist.githubusercontent.com/rkitover/93d89a679705875c59275fb0a8f22b45/raw/6149e7fa3920d6c674c79448c5a4c9313620e06c/python-3.6.3-custom-static-openssl.patch https://gist.githubusercontent.com/rkitover/b18f19eafda3775a9652cc9cdf3ec914/raw/ed14c34bf9f205ccc3a4684dbdb83f8620162b98/python-3.6.3-static-libintl.patch
    intltool        https://gist.githubusercontent.com/rkitover/d638882f52e5d5f8e392cbf6842cd6d0/raw/dcfbe358bbb8b89f88b40a9c3402494552fd33f8/intltool-0.51.0.patch
'

DIST_TAR_ARGS="$DIST_TAR_ARGS
"

DIST_CONFIGURE_TYPES="$DIST_CONFIGURE_TYPES
    unzip           make
    zip             make
    libgd           autoreconf
    harfbuzz        autoconf
    python2         autoreconf
    python3         autoreconf
    libxml2-python  python
    glib            autoreconf
    graphviz        autoreconf
    docbook2x       autoreconf
"

DIST_RELOCATION_TYPES="$DIST_RELOCATION_TYPES
    texinfo         aggressive
"

DIST_PRE_BUILD="$DIST_PRE_BUILD
#    xz              mkdir -p build-aux; touch build-aux/config.rpath; mkdir -p po; touch po/Makefile.in.in; sed -i.bak 's/ po / /g' Makefile.am;
    getopt          sed -i.bak 's/\\\$(LDFLAGS)\\(.*\\)\$/\\1 \$(LDFLAGS)/' Makefile;
    libicu          cd source;
#    flex-2.6.3      sed -i.bak '/^'\"\$TAB\"'tests \\\\\$/d' Makefile.am;
    flex            mkdir -p build-aux; touch build-aux/config.rpath; mkdir -p po; touch po/Makefile.in.in; sed -i.bak '/po \\\\$/d' Makefile.am;
    python3         sed -i.bak '/-Wl,-stack_size,/d' configure.ac;
    libxml2-python  cd python; \
                    sed -i.bak \"s,^ROOT = .*,ROOT = os.getenv('BUILD_ROOT') + '/root',; s|^\\( *platformLibs = \\[ *[^]].*\\)\\] *\$|\\1, 'lzma', 'iconv', 'intl']|\" setup.py;
    bakefile        sed -i.bak '/SUBDIRS = doc/d' Makefile.am;
    unzip           rm -f unix/Contents; ln -sf \$(find unix -mindepth 1 -maxdepth 1) .;
    zip             rm -f unix/Contents; ln -sf \$(find unix -mindepth 1 -maxdepth 1) .;
    gettext         sed -i.bak 's/-Wl,--disable-auto-import//' m4/woe32-dll.m4;
    expat           cd expat; \
                    sed -i.bak '/doc\\/Makefile/d' configure.ac; \
                    sed -i.bak '/SUBDIRS/{; s/ doc//; }' Makefile.am; \
                    sed -i.bak 's/cp \\\$</mv \$</' doc/doc.mk;
    graphviz        sed -i.bak 's/ -export-symbols/ -Wl,-export-symbols/g' \$(find . -name Makefile.am); \
                    putsln '#define __declspec(x)' > declspec.h;
    harfbuzz        touch test/CMakeLists.txt;
    xvidcore        cd build/generic; \
                    sed -i.bak '/^all:/{ s/ *\\\$(SHARED_LIB)//; }; \
                                /^install:/{ s, *\\\$(BUILD_DIR)/\\\$(SHARED_LIB),,; }; \
                                s/\\\$(INSTALL).*\\\$(SHARED_LIB).*/:/; \
                                s/\\\$(LN_S).*\\\$(SHARED_LIB).*/:/; \
                                s/@echo.*\\\$(SHARED_LIB).*/@:/; \
                    ' Makefile;
    libx265         cd source;
    libsoxr         rm -rf tests; mkdir tests; touch tests/CMakeLists.txt;
    XML-SAX         sed -i.bak 's/-MXML::SAX/-Mblib -MXML::SAX/' Makefile.PL;
    docbook2x       sed -i.bak 's/^\\( *SUBDIRS *= *.*\\)doc\\(.*\\)\$/\1\2/'           Makefile.am; \
                    sed -i.bak 's/^\\( *SUBDIRS *= *.*\\)documentation\\(.*\\)\$/\1\2/' xslt/Makefile.am;
"

DIST_POST_BUILD="$DIST_POST_BUILD
    harfbuzz        rebuild_dist freetype --with-harfbuzz=yes;
    flex-2.6.3      build_dist flex || :;
    libtool         ln -sf '$BUILD_ROOT/root/bin/libtoolize' '$BUILD_ROOT/root/bin/glibtoolize';
    glib            rebuild_dist gettext --without-included-glib --without-included-libxml;
    graphviz        (cd '$BUILD_ROOT/root/bin'; path_exists dot_static && ! path_exists dot && ln -sf '$BUILD_ROOT/root/bin/dot_static' ./dot || :);
    libxml2         mkdir -p '$BUILD_ROOT/root/etc/xml'; \
                    xmlcatalog --noout --create \"\$(cygpath -m \"$BUILD_ROOT/root/etc/xml/catalog.xml\")\" || :;
    python2         pip2 install six;
    python3         pip3 install six;
    fontconfig      mkdir -p '$BUILD_ROOT/root/etc/fonts'; \
                    touch '$BUILD_ROOT/root/etc/fonts/fonts.conf'; \
                    sed -i.bak 's|/usr/share/fonts|$BUILD_ROOT/root/share/fonts|g' '$BUILD_ROOT/root/etc/fonts/fonts.conf';
"

DIST_POST_CONFIGURE="$DIST_POST_CONFIGURE
    openssl         sed -E -i.bak ' \
        s/([^\\t]+\\\$\\((BUILD_ONE_CMD|RECURSIVE_BUILD_CMD|RECURSIVE_MAKE)\\))/+ \1/ \
    ' \$(find . -name Makefile);
"

DIST_CONFIGURE_OVERRIDES="$DIST_CONFIGURE_OVERRIDES
    openssl     ./config no-shared --prefix=/usr --openssldir=/etc/ssl
    cmake       ./configure --prefix=/usr --no-qt-gui --parallel=\$NUM_CPUS --enable-ccache
    zlib        ./configure --static --prefix=/usr
    XML-SAX     echo no | PERL_MM_USE_DEFAULT=0 perl Makefile.PL
    wxwidgets   ./configure $REQUIRED_CONFIGURE_ARGS --disable-shared --prefix=/usr --enable-stl --disable-precomp-headers --enable-cxx11 --enable-permissive --with-opengl --with-libpng
"

DIST_BUILD_OVERRIDES="$DIST_BUILD_OVERRIDES
    c2man          ./Configure -de -Dprefix=/usr -Dmansrc=/usr/share/man/man1; \
                   sed -i.bak 's|/[^ ][^ ]*/libfl\\.[^ ]*|-L$BUILD_ROOT/root/lib -lfl|' Makefile; \
                   make -j\$NUM_CPUS; \
                   make install bin='$BUILD_ROOT/root/bin' mansrc='$BUILD_ROOT/root/share/man/man1' privlib='$BUILD_ROOT/root/lib/c2man'
    libxml2-python python setup.py build install && python3 setup.py build install
    setuptools     python bootstrap.py; python easy_install.py .
    pip            easy_install .
    ninja          python configure.py --bootstrap && cp -af ./ninja '$BUILD_ROOT/root/bin'
    docbook4.2     install_docbook_dist schema
    docbook4.1.2   cp '$BUILD_ROOT/dists/docbook4.2/catalog.xml' . ; \
                   sed -i.bak 's/V4.2/V4.1.2/g; s/4.2/4.1.2/g;' catalog.xml; \
                   install_docbook_dist schema
    docbook4.3     install_docbook_dist schema
    docbook4.4     install_docbook_dist schema
    docbook4.5     install_docbook_dist schema
    docbook5.0     install_docbook_dist schema
    docbook-xsl    install_docbook_dist stylesheet
    docbook-xsl-ns install_docbook_dist stylesheet
    dejavu         install_fonts
    liberation     install_fonts
    urw            install_fonts
"

DIST_ARGS="$DIST_ARGS
    gettext     --with-included-gettext --with-included-glib --with-included-libcroco --with-included-libunistring --with-included-libxml --disable-curses CPPFLAGS=\"\$CPPFLAGS -DLIBXML_STATIC\"
    pkgconfig   --with-internal-glib --with-libiconv=gnu
    pcre        --enable-utf8 --enable-pcre8 --enable-pcre16 --enable-pcre32 --enable-unicode-properties --enable-pcregrep-libz --enable-pcregrep-libbz2 --enable-jit
    libxslt     --without-python --without-crypto
    libgd       --without-xpm
    fontconfig  --with-baseconfigdir=/etc/fonts
    graphviz    --disable-ltdl --without-x CFLAGS=\"-include \$PWD/declspec.h $CFLAGS\"
    python2     --with-ensurepip --with-system-expat
    python3     --with-ensurepip --with-system-expat
    glib        --with-libiconv=gnu
    bakefile    --enable-shared
    XML-Parser  EXPATINCPATH='$BUILD_ROOT/root/include' EXPATLIBPATH='$BUILD_ROOT/root/lib'
    doxygen     -DICONV_ACCEPTS_NONCONST_INPUT:BOOL=FALSE -DICONV_ACCEPTS_CONST_INPUT:BOOL=TRUE
    sfml        -DSFML_USE_SYSTEM_DEPS=TRUE
    libcroco    --disable-Bsymbolic
    freetype    --with-harfbuzz=no
    harfbuzz    --with-cairo=no --with-icu=no
    flac        --disable-ogg
    libvpx      --disable-unit-tests --disable-tools --disable-docs --disable-examples
    libxavs     --disable-asm
    libzvbi     --without-x
    libxml2     --without-python
    libbluray   --disable-bdjava-jar --disable-examples
    libopencore-amrnb   --disable-compile-c
    vidstab     -DUSE_OMP=NO
    libx264     --enable-pic
    libx265     -DHIGH_BIT_DEPTH=ON -DENABLE_ASSEMBLY=OFF -DENABLE_CLI=OFF

    ffmpeg      --pkg-config-flags=--static --enable-nonfree --extra-version=tessus --enable-avisynth --enable-fontconfig --enable-gpl --enable-version3 --enable-libass --enable-libbluray --enable-libfreetype --enable-libgsm --enable-libmodplug --enable-libmp3lame --enable-libopencore-amrnb --enable-libopencore-amrwb --enable-libopus --enable-libsnappy --enable-libsoxr --enable-libspeex --enable-libtheora --enable-libvidstab --enable-libvo-amrwbenc --enable-libvorbis --enable-libvpx --enable-libwavpack --enable-libx264 --enable-libx265 --enable-libxavs --enable-libxvid --enable-libzmq --enable-libzvbi --enable-openssl --enable-lzma --extra-cflags='-DMODPLUG_STATIC -DZMQ_STATIC' --extra-cxxflags='-DMODPLUG_STATIC -DZMQ_STATIC' --extra-objcflags='-DMODPLUG_STATIC -DZMQ_STATIC'

#
# TODO: add these if possible (from brew) --enable-indev=qtkit --enable-securetransport --enable-chromaprint --enable-ffplay --enable-frei0r --enable-libbs2b --enable-libcaca --enable-libfdk-aac --enable-libgme --enable-libgsm --enable-librtmp --enable-librubberband --enable-libssh --enable-libtesseract --enable-libtwolame --enable-webp --enable-libzimg
#
# these require > 10.7:
#   --enable-opencl       # requires 10.8
#   --enable-videotoolbox # requires 10.8
"


DIST_BARE_MAKE_ARGS='CC="$CC"'

ALL_MAKE_ARGS='V=1 VERBOSE=1'

# have to disable ccache for openssl
DIST_MAKE_ARGS="$DIST_MAKE_ARGS
    openssl     CC=\"\$CC_ORIG -fPIC\" CXX=\"\$CXX_ORIG -fPIC\"
    getopt      LDFLAGS=\"\$LDFLAGS -lintl -liconv\" CFLAGS=\"\$CFLAGS\"
    bzip2       libbz2.a bzip2 bzip2recover CFLAGS=\"\$CFLAGS\" LDFLAGS=\"\$LDFLAGS\"
    unzip       generic2
    zip         generic
    expat       DOCBOOK_TO_MAN=docbook2man
    shared-mime-info    -j1
    fontconfig  LIBS=\"-lintl -liconv\"
"

DIST_MAKE_INSTALL_ARGS="$DIST_MAKE_INSTALL_ARGS
    openssl     MANDIR=/share/man
    libgsm      CC=\"\$CC\"
"

DIST_EXTRA_LDFLAGS="$DIST_EXTRA_LDFLAGS
    graphviz    -lpcreposix
    doxygen     -lintl -liconv
    ffmpeg      -lm -llzma -lpthread
"

DIST_EXTRA_LIBS="$DIST_EXTRA_LIBS
    gettext             -liconv
    shared-mime-info    \$LD_START_GROUP -lxml2 -lgio-2.0 -lgmodule-2.0 -lgobject-2.0 -lglib-2.0 -lpcre -llzma -lz -lm -lffi -lpthread -liconv -lresolv -ldl \$LD_END_GROUP
    python3             -lintl
    harfbuzz            -lz
    wxwidgets           -ljpeg -ltiff
"

OIFS=$IFS
NL='
'
TAB='	'


builder() {
    setup
    read_command_line "$@"
    install_core_deps
    setup_perl
    delete_outdated_dists
    pre_build_all
    build_prerequisites

    DOWNLOADED_DISTS= UNPACKED_DISTS=

    download_needed_dists

    unpack_needed_dists $DOWNLOADED_DISTS
    build_needed_dists  $UNPACKED_DISTS

    build_project
}

read_command_line() {
    case "$1" in
        --env)
            puts "$BUILD_ENV"
            exit 0
            ;;
        --clean)
            rm -rf "$BUILD_ROOT/dists/*"
            unpack_dists
            exit 0
            ;;
    esac
}

# overridable hook for other scripts
pre_build_all() {
    return 0
}

# NOTE: this is called on source at the end of the file

setup() {
    detect_os

    mkdir -p "$BUILD_ROOT/root/include"
    [ -L "$BUILD_ROOT/root/inc" ] || ln -s "$BUILD_ROOT/root/include" "$BUILD_ROOT/root/inc"

    mkdir -p "$BUILD_ROOT/root/lib"

    for libarch in lib64 lib32; do
        [ -L "$BUILD_ROOT/root/$libarch" ] || ln -s "$BUILD_ROOT/root/lib" "$BUILD_ROOT/root/$libarch"
    done

    [ -n "$target_platform" ] && { [ -L "$BUILD_ROOT/root/lib/$target_platform" ] || ln -s "$BUILD_ROOT/root/lib" "$BUILD_ROOT/root/lib/$target_platform"; }

    OPWD=$PWD
    cd "$BUILD_ROOT/root"
    for d in perl5 share etc man doc; do
        [ -d "$d" ] || mkdir "$d"
    done
    cd "$OPWD"

    if [ -z "$BUILD_FFMPEG" ]; then
        for dist in $FFMPEG_DISTS; do
            table_line_remove DISTS $dist
        done
    fi

    DIST_NAMES=$(  table_column DISTS 0 3)
    DIST_URLS=$(   table_column DISTS 1 3)
    DIST_TARGETS=$(table_column DISTS 2 3)

    DISTS_NUM=$(table_rows DISTS)

    NUM_CPUS=$(num_cpus)

    BUILD_ENV="$BUILD_ENV
export MAKEFLAGS=-j$NUM_CPUS
"
    eval "$BUILD_ENV"

    CHECKOUT=$(find_checkout)

    TMP_DIR=${TMP_DIR:-/tmp/builder-$$}

    setup_tmp_dir

    UNPACK_DIR="$TMP_DIR/unpack"
    mkdir "$UNPACK_DIR"

    DISTS_DIR="$BUILD_ROOT/dists"
    mkdir -p "$DISTS_DIR"
}

num_cpus() {
    if command -v nproc >/dev/null; then
        nproc
        return $?
    fi

    if path_exists /proc/cpuinfo; then
        set -- $(grep '^processor		*:' /proc/cpuinfo | wc -l)
        puts $1
        return 0
    fi

    if command -v sysctl >/dev/null; then
        sysctl -n hw.ncpu
        return 0
    fi

    warn 'cannot determine number of CPU threads, using a default of [1;31m2[0m'

    puts 2
}

setup_perl() {
    if command -v perl >/dev/null; then
        if ! command -v cpanm >/dev/null; then
            perl -MApp::Cpan -e 'App::Cpan->run' -- -f -i App::cpanminus
        fi
    fi
}

clear_build_env() {
    for var in CC CXX CFLAGS CPPFLAGS CXXFLAGS OBJCXXFLAGS LDFLAGS CMAKE_PREFIX_PATH PKG_CONFIG_PATH PERL_MM_USE_DEFAULT PERL_EXTUTILS_AUTOINSTALL OPENSSL_ROOT PERL_MB_OPT PERL_MM_OPT PERL5LIB PERL_LOCAL_LIB_ROOT; do
        unset $var
    done
    export PATH="$ORIG_PATH"
}

set_build_env() {
    eval "$BUILD_ENV"
}

install_core_deps() {
    ${os}_install_core_deps

    # get platform now that we have gcc
    detect_os

    # things like ccache may have been installed, re-eval build env
    eval "$BUILD_ENV"
}

installing_core_deps() {
    puts "${NL}[32mInstalling core dependencies for your OS...[0m${NL}${NL}"
}

done_msg() {
    puts "${NL}[32mDone!!![0m${NL}${NL}"
}

unknown_install_core_deps() {
    :
}

linux_install_core_deps() {
    # detect host architecture
    case "$(uname -a)" in
        *x86_64*)
            amd64=1
            ;;
        *i686*)
            i686=1
            ;;
    esac

    if [ -f /etc/debian_version ]; then
        debian_install_core_deps
    elif [ -f /etc/fedora-release ]; then
        fedora_install_core_deps
    elif [ -f /etc/arch-release ]; then
        archlinux_install_core_deps
    elif [ -f /etc/solus-release ]; then
        solus_install_core_deps
    elif path_exists /etc/os-release && [ "$(. /etc/os-release; puts "$ID_LIKE")" = suse ]; then
        suse_install_core_deps
    fi
}

debian_install_core_deps() {
    installing_core_deps

    sudo apt-get -qq update || :
    sudo apt-get -qy install build-essential g++ curl ccache perl

    done_msg
}

fedora_install_core_deps() {
    installing_core_deps

    sudo dnf install -y --nogpgcheck --best --allowerasing gcc gcc-c++ make redhat-rpm-config curl perl ccache file patch findutils
}

suse_install_core_deps() {
    installing_core_deps

    sudo zypper in -y gcc gcc-c++ binutils glibc-devel-static make curl perl ccache file patch
}

archlinux_install_core_deps() {
    installing_core_deps

    # check for gcc-multilib
    gcc_pkg=gcc
    if sudo pacman -Q gcc-multilib >/dev/null 2>&1; then
        gcc_pkg=gcc-multilib
    fi

    # update catalogs
    sudo pacman -Sy

    # not using the base-devel group because it can break gcc-multilib
    sudo pacman --noconfirm --needed -S $gcc_pkg binutils file grep gawk gzip make patch sed util-linux curl ccache perl

    done_msg
}

solus_install_core_deps() {
    installing_core_deps

    sudo eopkg -y update-repo
    sudo eopkg -y install -c system.devel curl perl

    done_msg
}

windows_install_core_deps() {
    if [ -n "$msys2" ]; then
        msys2_install_core_deps
    elif [ -n "$cygwin" ]; then
        cygwin_install_core_deps
    fi
}

msys2_install_core_deps() {
    case "$MSYSTEM" in
        MINGW32)
            target='mingw-w64-i686'
            ;;
        *)
            target='mingw-w64-x86_64'
            ;;
    esac

    installing_core_deps

    # update catalogs
    pacman -Sy

    set --
    for p in binutils curl crt-git gcc gcc-libs headers-git tools-git windows-default-manifest libmangle-git; do
        set -- "$@" "${target}-${p}"
    done

    # install
    pacman --noconfirm --needed -S make tar patch diffutils ccache perl msys2-w32api-headers msys2-runtime-devel gcc gcc-libs mpfr windows-default-manifest python2 "$@"

    # make sure msys perl takes precedence over mingw perl if the latter is installed
    mkdir -p "$BUILD_ROOT/root/bin"
    ln -sf /usr/bin/perl.exe "$BUILD_ROOT/root/bin/perl.exe"

    # alias python2 to python
    ln -sf /usr/bin/python2.exe "$BUILD_ROOT/root/bin/python.exe"

    # activate ccache
    eval "$BUILD_ENV"

    done_msg
}

cygwin_install_core_deps() {
    installing_core_deps

    target="mingw64-${target_cpu}"

    curl -L rawgit.com/transcode-open/apt-cyg/master/apt-cyg > "$BUILD_ROOT/root/bin/apt-cyg"
    chmod +x "$BUILD_ROOT/root/bin/apt-cyg"

    hash -r

    apt-cyg update

    set --
    for p in binutils gcc-core gcc-g++ headers windows-default-manifest; do
        set -- "$@" "${target}-${p}"
    done

    apt-cyg install make tar patch diffutils ccache perl m4 cygwin32-w32api-headers gcc-core gcc-g++ mpfr windows-default-manifest python2 libncurses-devel "$@"

    # alias python2 to python
    ln -sf /usr/bin/python2.exe "$BUILD_ROOT/root/bin/python.exe"

    # activate ccache
    eval "$BUILD_ENV"

    done_msg
}

mac_install_core_deps() {
    if ! xcode-select -p >/dev/null 2>&1 && \
       ! pkgutil --pkg-info=com.apple.pkg.CLTools_Executables >/dev/null 2>&1 && \
       ! pkgutil --pkg-info=com.apple.pkg.DeveloperToolsCLI >/dev/null 2>&1; then

        error 'Please install XCode and the XCode Command Line Tools, then run this script again. On newer systems this can be done with: [35m;xcode-select --install[0m'
    fi
}

setup_tmp_dir() {
    # mkdir -m doesn't work on some versions of msys and similar
    rm -rf "$TMP_DIR"
    if ! ( mkdir -m 700 "$TMP_DIR" 2>/dev/null || mkdir "$TMP_DIR" 2>/dev/null || [ -d "$TMP_DIR" ] ); then
        die "Failed to create temporary directory: '[1;35m$TMP_DIR[0m"
    fi

    chmod 700 "$TMP_DIR" 2>/dev/null || :

    trap 'quit $?' EXIT PIPE HUP INT QUIT ILL TRAP KILL BUS TERM
}

quit() {
    cd "$HOME" || :
    rm -rf "$TMP_DIR" || :
    exit "${1:-0}"
}

detect_os() {
    case "$(uname -s)" in
        Linux)
            os=linux
            ;;
        Darwin)
            os=mac
            ;;
        MINGW*|MSYS*)
            os=windows
            msys2=1
            ;;
        CYGWIN*)
            os=windows
            cygwin=1
            ;;
        *)
            os=unknown
            ;;
    esac

    case "$(uname -a)" in
        *x86_64*)
            bits=64
            ;;
        *i686*)
            bits=32
            ;;
    esac

    target_platform=$($CC -dumpmachine 2>/dev/null) || :

    LD_START_GROUP= LD_END_GROUP=
    if ld -v 2>/dev/null | grep -Eq GNU; then
        LD_START_GROUP='-Wl,--start-group'
        LD_END_GROUP='-Wl,--end-group'
    fi
}

delete_outdated_dists() {
    [ ! -d "$BUILD_ROOT/downloads" ] && return 0

    files=
    for current_dist in $DIST_NAMES; do
        files="$files $(dist_file "$current_dist")"
    done

    IFS=$NL
    find "$BUILD_ROOT/downloads" -maxdepth 1 -type f -not -name '.*' | \
    while read -r file; do
        IFS=$OIFS
        if ! list_contains "$file" $files; then
            puts "${NL}[32mDeleting outdated dist archive: [1;34m$file[0m${NL}${NL}"
            rm -f "$file"

            dist_dir="$BUILD_ROOT/dists/${file##*/}"
            while [ ! -d "$dist_dir" ]; do
                case "$file_dist" in
                    *-*)
                        dist_dir=${dist_dir%-*}
                        ;;
                    *)
                        dist_dir=
                        break
                        ;;
                esac
            done

            if [ -n "$dist_dir" ] && [ -d "$dist_dir" ]; then
                puts "${NL}[32mDeleting outdated dist unpack dir: [1;34m$dist_dir[0m${NL}${NL}"
                rm -rf "$dist_dir"
            fi
        fi
    done
    IFS=$OIFS

    (
        cd "$BUILD_ROOT/dists"

        IFS=$NL
        find . -maxdepth 1 -type d -not -name '.*' | \
        while read -r dir; do
            IFS=$OIFS
            dir=${dir#./}
            if ! list_contains "$dir" $DIST_NAMES; then
                puts "${NL}[32mDeleting outdated dist unpack dir: [1;34m$dir[0m${NL}${NL}"
                rm -rf "$dir"
            fi
        done
        IFS=$OIFS
    )
}

build_prerequisites() {
    dists_are_installed $PRE_BUILD_DISTS && return 0

    puts "${NL}[32mFetching and building prerequisites...[0m${NL}${NL}"

    for current_dist in $PRE_BUILD_DISTS; do
        get_dist $current_dist
        build_dist_if_needed $current_dist
    done

    puts "${NL}[32mDone with prerequisites.[0m${NL}${NL}"
}

dists_are_installed() {
    for current_dist; do
        if ! path_exists "$(install_artifact $current_dist)"; then
            return 1
        fi
    done
}

download_needed_dists() {
    running_jobs=
    max_jobs=25
    setup_jobs
    job_failed_hook download_failed
    for current_dist in $DIST_NAMES; do
        if ! path_exists "$(dist_file "$current_dist")"; then
            (
                start_job
                write_job_info dist_name $current_dist
                {
                    download_dist $current_dist
                    write_job_exit_status
                } 2>&1 | write_job_output
            ) &
            running_jobs="$running_jobs $!"

            wait_jobs running_jobs $max_jobs

            # FIXME: this should only run on a job_success hook, hooks need to be fixed
            DOWNLOADED_DISTS="$DOWNLOADED_DISTS $current_dist"
        fi
    done

    wait_all_jobs running_jobs
    cleanup_jobs
}

unpack_needed_dists() {
    running_jobs=
    max_jobs=$NUM_CPUS
    setup_jobs
    job_failed_hook unpack_failed
    for current_dist in $DIST_NAMES; do
        if list_contains "$current_dist" "$@" || ! path_exists "$(dist_dir "$current_dist")"; then
            (
                start_job
                write_job_info dist_name $current_dist
                {
                    unpack_dist $current_dist
                    write_job_exit_status
                } 2>&1 | write_job_output
            ) &
            running_jobs="$running_jobs $!"

            wait_jobs running_jobs $max_jobs

            # FIXME: this should only run on a job_success hook, hooks need to be fixed
            UNPACKED_DISTS="$UNPACKED_DISTS $current_dist"
        fi
    done

    wait_all_jobs running_jobs
    cleanup_jobs
}

dist_url() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_url: dist name required'

    dist_idx=$(list_index $current_dist $DIST_NAMES)

    putsln "$(list_get $dist_idx $DIST_URLS)"
}

dist_file() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_file: dist name required'

    dist_url=$(dist_url "$current_dist")

    dist_file=${dist_url##*/}

    # remove query string stuff
    dist_file=${dist_file%\?*}

    # set full path
    dist_file="$BUILD_ROOT/downloads/$current_dist-$dist_file"

    puts "$(resolve_link "$dist_file")"
}

dist_dir() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_dir: dist name required'

    putsln "$BUILD_ROOT/dists/$current_dist"
}

get_dist() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'get_dist: dist name required'

    download_dist "$current_dist"
    unpack_dist "$current_dist"
}

download_dist() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'download_dist: dist name required'

    dist_url=$(dist_url "$current_dist")
    dist_file=$(dist_file "$current_dist")
    dist_dir="$DISTS_DIR/$current_dist"

    mkdir -p "$BUILD_ROOT/downloads"
    cd "$BUILD_ROOT/downloads"

    if [ ! -f "$dist_file" ]; then
        puts "${NL}[32mFetching [1;35m$current_dist[0m: [1;34m$dist_url[0m${NL}${NL}"
        $CURL -SsL "$dist_url" -o "$dist_file"

        case "$dist_file" in
            *.*)
                ;;
            *)
                # no extension, try to figure out if zip or .tar.gz

                new_dist_file=$(echo "$dist_file" | sed 's/-* *$//') # remove trailing dash and spaces

                case "$(file "$dist_file")" in
                    *gzip*)
                        mv "$dist_file" "$new_dist_file.tar.gz"
                        ln -sf "$new_dist_file.tar.gz" "$dist_file"
                        ;;
                    *Zip\ archive*)
                        mv "$dist_file" "$new_dist_file.zip"
                        ln -sf "$new_dist_file.zip" "$dist_file"
                        ;;
                esac

                ;;
        esac
    fi
}

rtrim() {
    str=$1

    while [ "$str" != "${str% }" ]; do
        str="${str% }"
    done

    puts "$str"
}

download_failed() {
    job_pid=$1

    error "Fetching $current_dist failed, check the URL:${NL}${NL}$(cat "$TMP_DIR/job_output/$job_pid")"

    rm -f "$(dist_file "$current_dist")"

    exit 1
}

unpack_dist() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'unpack_dist: dist name required'

    dist_file=$(dist_file "$current_dist")
    dist_dir="$DISTS_DIR/$current_dist"

    if [ ! -f "$dist_file" ]; then
        error "unpack_dist: missing dist file for dist '$current_dist': '$dist_file'"
    fi

    puts "${NL}[32mUnpacking [1;35m$current_dist[0m${NL}${NL}"
    rm -rf "$dist_dir"
    mkdir "$dist_dir"

    OPWD=$PWD

    unpack_dir="$UNPACK_DIR/$current_dist-$$"
    mkdir "$unpack_dir"
    cd "$unpack_dir"

    eval "set -- $(dist_tar_args "$current_dist")"

    case "$dist_file" in
        *.tar)
            $TAR $@ -xf "$dist_file"
            ;;
        *.tar.gz|*.tgz)
            $TAR $@ -zxf "$dist_file"
            ;;
        *.tar.xz)
            xz -dc "$dist_file" | $TAR $@ -xf -
            ;;
        *.tar.bz2)
            bzip2 -dc "$dist_file" | $TAR $@ -xf -
            ;;
        *.zip)
            unzip -q "$dist_file"
            ;;
    esac

    if [ $(list_length *) -eq 1 ] && [ -d * ]; then
        # one archive dir
        cd *
    fi

    $TAR -cf - . | (cd "$dist_dir"; $TAR -xf -)

    cd "$TMP_DIR"
    rm -rf "$unpack_dir"

    (
        cd "$dist_dir"
        dist_post_unpack "$current_dist"
    )

    cd "$OPWD"
}

unpack_failed() {
    job_pid=$1

    error "Unpacking $current_dist failed:${NL}${NL}$(cat "$TMP_DIR/job_output/$job_pid")"

    rm -rf "$DISTS_DIR/$current_dist"

    exit 1
}

setup_jobs() {
    rm -rf "$TMP_DIR/job_status" "$TMP_DIR/job_output"
    mkdir -p "$TMP_DIR/job_status" "$TMP_DIR/job_output"
    _job_failed_hook=
}

cleanup_jobs() {
    rm -rf "$TMP_DIR/job_status" "$TMP_DIR/job_output"
    _job_failed_hook=
}

start_job() {
    current_job_pid=$(exec sh -c 'printf "%s" $PPID')
}

write_job_exit_status() {
    _exit_status=$?
    putsln "job_exited='$_exit_status'" >> "$TMP_DIR/job_status/$current_job_pid" 2>/dev/null
}

write_job_info() {
    [ -n "$1" ] || die 'write_job_info: key name required'
    putsln "${1}='${2}'" >> "$TMP_DIR/job_status/$current_job_pid" 2>/dev/null
}

write_job_output() {
    tee -a "$TMP_DIR/job_output/$current_job_pid"
}

wait_all_jobs() {
    [ -n "$1" ] || die 'wait_all_jobs: jobs list var name required'

    while [ "$(eval list_length "\$$1")" -gt 0 ]; do
        sleep 0.2
        check_jobs $1
    done
}

wait_jobs() {
    [ -n "$1" ]                 || die 'limit_jobs: jobs list var name required'
    [ -n "$2" ] && [ $2 -ge 0 ] || die 'limit_jobs: max jobs number required'

    while [ "$(eval list_length "\$$1")" -ge $2 ]; do
        sleep 0.05
        check_jobs $1
    done
}

running_jobs() {
    alive_list_var=$1
    [ -n "$alive_list_var" ] || die 'running_jobs: alive list variable name required'
    reaped_list_var=$2
    [ -n "$reaped_list_var" ] || die 'running_jobs: reaped list variable name required'

    jobs_file="$TMP_DIR/jobs_list.txt"

    jobs -l > "$jobs_file"

    eval "$alive_list_var="
    eval "$reaped_list_var="
    IFS=$NL
    # will get pair: <PID> <state>
    for job in $(sed <"$jobs_file" -n 's/^\[[0-9]\{1,\}\] *[-+]\{0,1\}  *\([0-9][0-9]*\)  *\([A-Za-z]\{1,\}\).*/\1 \2/p'); do
        IFS=$OIFS
        set -- $job
        pid=$1 state=$2
        
        case "$state" in
            Stopped)
                kill $pid 2>/dev/null || :
                eval "$reaped_list_var=\"\$$reaped_list_var $pid\""
                ;;
            Running)
                eval "$alive_list_var=\"\$$alive_list_var $pid\""
                ;;
        esac
    done
    IFS=$OIFS

    rm -f "$jobs_file"
}

check_jobs() {
    jobs_list_var=$1
    [ -n "$jobs_list_var" ] || die 'check_jobs: jobs list variable name required'

    running_jobs alive reaped

    new_jobs=
    for job in $(eval puts \$$jobs_list_var); do
        if list_contains $job $alive; then
            new_jobs="$new_jobs $job"
        else
            job_status_file="$TMP_DIR/job_status/$job"
            job_output_file="$TMP_DIR/job_output/$job"

            if [ -f "$job_status_file" ]; then
                job_exited=
                eval "$(cat "$job_status_file")"

                if [ -n "$job_exited" ] && [ "$job_exited" -eq 0 ]; then
                    rm "$job_status_file" "$job_output_file"
                else
                    current_dist=$dist_name

                    error "A job has failed, winding down pending jobs..."

                    while [ "$(list_length $alive)" -ne 0 ]; do
                        for pid in $alive; do
                            if ! list_contains $pid $last_alive; then
                                kill $pid 2>/dev/null || :
                            else
                                kill -9 $pid 2>/dev/null || :
                            fi
                        done

                        last_alive=$alive

                        sleep 0.2

                        running_jobs alive reaped
                    done

                    # don't want signals to interrupt sleep
                    trap - PIPE HUP ALRM

                    if [ "$os" != windows ]; then
                        sleep 30 || :
                    else
                        # this is painfully slow on msys2/cygwin
                        warn 'Please wait, this will take a while...'

                        sleep 330 || :
                    fi

                    call_job_failed_hook $job
                fi
            fi
        fi
    done

    eval "$jobs_list_var=\$new_jobs"
}

job_failed_hook() {
    [ -n "$1" ] || die 'job_failed_hoook: sh function name required'

    _job_failed_hook=$1
}

call_job_failed_hook() {
    [ -n "$1" ] && [ $1 -gt 0 ] || die 'call_job_failed_hook: job pid required'

    if [ -n "$_job_failed_hook" ]; then
        eval $_job_failed_hook '"$@"'
    fi
}

# fall back to 1 second sleeps if fractional sleep is not supported
sleep() {
    if ! command sleep "$@" 2>/dev/null; then
        sleep_secs=${1%%.*}
        [ $# -gt 0 ] && shift
        if [ -z "$sleep_secs" ] || [ "$sleep_secs" -lt 1 ]; then
            sleep_secs=1
        fi
        command sleep $sleep_secs "$@"
    fi
}

build_dists() {
    for current_dist; do
        build_dist "$current_dist"
        BUILT_DISTS="$BUILT_DISTS $current_dist"
    done
}

build_needed_dists() {
    for current_dist in $DIST_NAMES; do
        if list_contains "$current_dist" "$@"; then
            build_dist $current_dist
        else
            build_dist_if_needed $current_dist
        fi
    done
}

build_dist_if_needed() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'build_dist_if_needed: dist name required'
    shift

    if ! path_exists "$(install_artifact $current_dist)"; then
        build_dist $current_dist "$@"
        BUILT_DISTS="$BUILT_DISTS $current_dist"
    fi
}

rebuild_dist() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'rebuild_dist: dist name required'
    shift

    rm -rf "$BUILD_ROOT/dists/$current_dist"

    unpack_dist "$current_dist"

    build_dist "$current_dist" "$@"
}

build_dist() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'build_dist: dist name required'
    shift
    extra_dist_args=$@

    if ! [ -n "$(table_line DISTS $current_dist)" ]; then
        warn "no such dist: $current_dist"
        return
    fi

    cd "$DISTS_DIR/$current_dist"

    puts "${NL}[32mBuilding [1;35m$current_dist[0m${NL}${NL}"

    ORIG_LDFLAGS=$LDFLAGS
    ORIG_LIBS=$LIBS

    # have to make sure C++ flags are passed when linking, but only for C++ and **NOT** C
    # this fails if there are any .c files in the project
    if [ "$(find . -name '*.cpp' -o -name '*.cc' | wc -l)" -ne 0 -a "$(find . -name '*.c' | wc -l)" -eq 0 ]; then
        export LDFLAGS="$CXXFLAGS $LDFLAGS"
    fi

    export LDFLAGS="$LDFLAGS $(eval puts "$(dist_extra_ldflags "$current_dist")")"
    export LIBS="$LIBS $(eval puts "$(dist_extra_libs "$current_dist")")"

    dist_patch "$current_dist"
    dist_pre_build "$current_dist"

    configure_override=$(dist_configure_override "$current_dist")
    install_override=$(dist_install_override "$current_dist")
    build_override=$(dist_build_override "$current_dist")
    config_type=$(dist_configure_type "$current_dist")

    if [ -n "$build_override" ]; then
        eval "set -- $extra_dist_args"
        echo_eval_run "$build_override $@"

        path_exists "$(install_artifact $current_dist)"
    else
        if [ "$config_type" = meson ] || [ -z "$config_type" -a -f meson.build ]; then
            mkdir -p build
            cd build

            if [ -n "$configure_override" ]; then
                eval "set -- $extra_dist_args"
                echo_eval_run "$configure_override $@"
            else
                eval "set -- $(dist_args "$current_dist" meson) $extra_dist_args"
                echo_run meson .. "$@"
            fi
            dist_post_configure "$current_dist"
            eval "set -- $(dist_ninja_args "$current_dist")"
            echo_run ninja -j $NUM_CPUS "$@"

            if [ -z "$install_override" ]; then
                rm -rf destdir
                mkdir destdir

                echo_eval_run 'DESTDIR="$PWD/destdir" ninja '"$(dist_make_install_args "$current_dist")"' install || :'

                install_dist "$current_dist"
            else
                echo_eval_run "$install_override $(dist_make_install_args "$current_dist")"
            fi

            path_exists "$(install_artifact $current_dist)"
        elif [ "$config_type" = autoconf -o "$config_type" = autoreconf ] || [ -z "$config_type" -a \( -f configure -o -f Configure -o -f configure.ac -o -f configure.in -o -f Makefile.am \) ]; then
            # workaround a sometimes autoconf bug
            touch config.rpath

            if [ -n "$configure_override" ]; then
                eval "set -- $extra_dist_args"
                echo_eval_run "$configure_override $@"
            else
                autogen=

                if [ "$config_type" = autoreconf ] || [ ! -f configure ]; then
                    autogen=1

                    if [ -f autogen.sh ]; then
                        chmod +x autogen.sh
                        eval "set -- $REQUIRED_CONFIGURE_ARGS $(dist_args "$current_dist" autoconf) $extra_dist_args"
                        echo_run ./autogen.sh "$@"
                    elif [ -f buildconf.sh ]; then
                        chmod +x buildconf.sh
                        eval "set -- $REQUIRED_CONFIGURE_ARGS $(dist_args "$current_dist" autoconf) $extra_dist_args"
                        echo_run ./buildconf.sh "$@"
                    elif [ -f bootstrap ]; then
                        chmod +x bootstrap
                        eval "set -- $REQUIRED_CONFIGURE_ARGS $(dist_args "$current_dist" autoconf) $extra_dist_args"
                        echo_run ./bootstrap "$@"
                    else
                        if [ -d m4 ]; then
                            echo_run aclocal --force -I m4
                        else
                            echo_run aclocal --force
                        fi

                        if command -v glibtoolize >/dev/null; then
                            echo_run glibtoolize --force
                        elif command -v libtoolize >/dev/null; then
                            echo_run libtoolize --force
                        fi

                        echo_run autoheader || :
                        echo_run autoconf --force

                        if command -v gtkdocize >/dev/null; then
                            echo_run gtkdocize 2>/dev/null || :
                        fi

                        [ -f Makefile.am ] && echo_run automake --foreign --add-missing --copy
                    fi
                fi

                if [ -z "$autogen" ] || ! path_exists config.status; then
                    if path_exists Configure; then
                        chmod +x ./Configure
                        eval "set -- $REQUIRED_CONFIGURE_ARGS $(dist_args "$current_dist" autoconf) $extra_dist_args"
                        echo_run ./Configure "$@"
                    else
                        chmod +x ./configure
                        eval "set -- $REQUIRED_CONFIGURE_ARGS $(dist_args "$current_dist" autoconf) $extra_dist_args"
                        echo_run ./configure "$@"
                    fi
                fi
            fi

            dist_post_configure "$current_dist"
            eval "set -- $(dist_make_args "$current_dist")"
            echo_run make -j$NUM_CPUS "$@"

            if [ -z "$install_override" ]; then
                rm -rf destdir
                mkdir destdir

                eval "set -- $(dist_make_install_args "$current_dist")"

                make_install "$@"
                install_dist "$current_dist"
            else
                echo_eval_run "$install_override $(dist_make_install_args "$current_dist")"
            fi

            path_exists "$(install_artifact $current_dist)"
        elif [ "$config_type" = cmakeninja ]; then
            if ! command -v ninja >/dev/null; then
                error "configure type 'cmakeninja' requested but ninja is not available yet";
            fi

            mkdir -p build
            cd build

            if [ -n "$configure_override" ]; then
                eval "set -- $extra_dist_args"
                echo_eval_run "$configure_override $@"
            else
                eval "set -- $REQUIRED_CMAKE_ARGS $(dist_args "$current_dist" cmake) $extra_dist_args -G Ninja"
                echo_run cmake .. "$@"
            fi
            dist_post_configure "$current_dist"
            eval "set -- $(dist_ninja_args "$current_dist")"
            echo_run ninja -j$NUM_CPUS "$@"

            if [ -z "$install_override" ]; then
                rm -rf destdir
                mkdir destdir

                eval "set -- $(dist_make_install_args "$current_dist")"

                echo_run make "$@" install DESTDIR="$PWD/destdir" || :

                install_dist "$current_dist"
            else
                echo_eval_run "$install_override $(dist_make_install_args "$current_dist")"
            fi

            path_exists "$(install_artifact $current_dist)"
        elif [ "$config_type" = cmake ] || [ -z "$config_type" -a -f CMakeLists.txt ]; then
            mkdir -p build
            cd build

            if [ -n "$configure_override" ]; then
                eval "set -- $extra_dist_args"
                echo_eval_run "$configure_override $@"
            else
                eval "set -- $REQUIRED_CMAKE_ARGS $(dist_args "$current_dist" cmake) $extra_dist_args"
                echo_run cmake .. "$@"
            fi
            dist_post_configure "$current_dist"
            eval "set -- $(dist_make_args "$current_dist")"
            echo_run make -j$NUM_CPUS "$@"

            if [ -z "$install_override" ]; then
                rm -rf destdir
                mkdir destdir

                eval "set -- $(dist_make_install_args "$current_dist")"

                echo_run make "$@" install DESTDIR="$PWD/destdir" || :

                install_dist "$current_dist"
            else
                echo_eval_run "$install_override $(dist_make_install_args "$current_dist")"
            fi

            path_exists "$(install_artifact $current_dist)"
        elif [ "$config_type" = python ] || [ -z "$config_type" -a -f setup.py ]; then
            if [ -z "$install_override" ]; then
                pip=
                if grep -Eq 'Python :: 3' PKG-INFO 2>/dev/null; then
                    pip=pip3
                fi
                if grep -Eq 'Python :: 2' PKG-INFO 2>/dev/null; then
                    pip="$pip pip2"
                fi

                # default to python2 if no package info
                [ -z "$pip" ] && pip=pip

                for pip in $pip; do
                    if [ -n "$configure_override" ]; then
                        eval "set -- $extra_dist_args"
                        echo_eval_run "$configure_override $@"
                    else
                        eval "set -- $(dist_args "$current_dist" python) $extra_dist_args"
                        echo_run $pip install . "$@"
                    fi
                done
            else
                echo_eval_run "$install_override $(dist_make_install_args "$current_dist")"
            fi

            path_exists "$(install_artifact $current_dist)"
        elif [ "$config_type" = perl ] || [ -z "$config_type" -a -f Makefile.PL ]; then
            echo_run cpanm --notest --installdeps .

            if [ -n "$configure_override" ]; then
                eval "set -- $extra_dist_args"
                echo_eval_run "$configure_override $@"
            else
                eval "set -- $(dist_args "$current_dist" perl) $extra_dist_args"
                echo_run perl Makefile.PL "$@"
            fi

            dist_post_configure "$current_dist"
            eval "set -- $(dist_make_args "$current_dist")"
            echo_run $PERL_MAKE "$@" # dmake doesn't understand -j

            if [ -z "$install_override" ]; then
                eval "set -- $(dist_make_install_args "$current_dist")"
                echo_run $PERL_MAKE "$@" install || :
            else
                echo_eval_run "$install_override $(dist_make_install_args "$current_dist")"
            fi

            path_exists "$(install_artifact $current_dist)"
        elif [ "$config_type" = make ] || [ -z "$config_type" -a \( -f Makefile -o -f makefile \) ]; then
            makefile=makefile
            if [ -f Makefile ]; then
                makefile=Makefile
            fi

            eval "set -- $DIST_BARE_MAKE_ARGS $(dist_make_args "$current_dist")"

            echo_run make -j$NUM_CPUS "$@"

            if [ -z "$install_override" ]; then
                eval "set -- $(dist_make_install_args "$current_dist")"

                make_install "$@"

                # some bare makefiles just have no DESTDIR mechanism of any sort
                if [ -d destdir ]; then
                    install_dist "$current_dist"
                fi
            else
                echo_eval_run "$install_override $(dist_make_install_args "$current_dist")"
            fi

            path_exists "$(install_artifact $current_dist)"
        else
            die "don't know how to build [1;35m$current_dist[0m, please define a BUILD_OVERRIDE"
        fi
    fi

    dist_post_build "$current_dist"

    export LDFLAGS="$ORIG_LDFLAGS"
    export LIBS="$ORIG_LIBS"

    done_msg
}

make_install() {
    rm -rf destdir
    mkdir -p destdir
    cd destdir

    prefix=$(dist_prefix $current_dist)

    # sometimes make install doesn't try to pre-create the dest dirs
    mkdir -p ./${prefix}/man/man1 ./${prefix}/man/man3 ./${prefix}/share/man/man1 ./${prefix}/share/man/man3 ./${prefix}/inc ./${prefix}/include ./${prefix}/bin ./${prefix}/lib

    # some dists understand DESTDIR but not combined with prefix
    for p in man share inc include bin lib; do
        ln -s ./${prefix}/$p $p
    done

    cd ..

    if grep -Eq 'DESTDIR|cmake_install\.cmake' $(find . -name Makefile -o -name makefile -o -name '*.mk' -o -name '*.mak') 2>/dev/null; then
        echo_run make install prefix="${prefix}" PREFIX="${prefix}" DESTDIR="$PWD/destdir" "$@" || :
    else
        echo_run make install prefix="$PWD/destdir${prefix}" PREFIX="$PWD/destdir${prefix}" INSTALL_PREFIX="$PWD/destdir${prefix}" INSTALL_ROOT="$PWD/destdir${prefix}" INSTALLTOP="/..${prefix}/" "$@" || :
    fi
}

# assumes make install has run into ./destdir
install_dist() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'install_dist: dist name required'
    [ -d destdir         ] || die 'install_dist: ./destdir does not exist'

    prefix=$(dist_prefix $current_dist)
    rel_prefix=${prefix#/}

    # if there is an extra prefix, like e.g. 'msys64' on msys2 before 'usr/',
    # remove it
    if ([ "$(list_length destdir/*)" -eq 1 ] && [ ! -d destdir${prefix} ]) || \
       ([ "$(list_length destdir/*)" -eq 2 ] && [ "$(find destdir${prefix} -type f 2>/dev/null | wc -l)" -eq 0 ]); then
        mv destdir/${prefix}/* tmp-usr
        rm -rf destdir/*
        mkdir -p destdir${prefix%/*}
        mv tmp-usr destdir${prefix}
    fi

    # move libs out of platforms dirs like lib/x86_64-linux-gnu/ and lib64/
    # and adjust pkgconfig files

    dest_lib_dir="destdir${prefix}/lib"

    [ -n "$target_platform" ] && dest_platform_lib_dir="$dest_lib_dir/$target_platform"
    [ -n "$bits"            ] && dest_bits_lib_dir="destdir${prefix}/lib$bits"

    for platf_dir in "$dest_platform_lib_dir" "$dest_bits_lib_dir"; do
        if [ -n "$platf_dir" ] && [ -d "$platf_dir" ]; then
            if [ -d "$platf_dir/pkgconfig" ]; then
                sed -i.bak "s,lib/$target_platform,lib,g" "$platf_dir/pkgconfig"/*.pc
                rm -f "$platf_dir/pkgconfig"/*.pc.bak
            fi

            mkdir -p "$dest_lib_dir"

            (cd "$platf_dir"; $TAR -cf - .) | (cd "$dest_lib_dir"; $TAR -xf -)

            rm -rf "$platf_dir"
        fi
    done

    # copy platform includes to the regular include dirs
    IFS=$NL
    for platform_inc_dir in $(find "destdir${prefix}/lib/" -mindepth 2 -maxdepth 2 -type d -name include 2>/dev/null || :); do
        IFS=$OIFS
        (
            inc_dir=${platform_inc_dir%/*}
            inc_dir="destdir${prefix}/include/${inc_dir##*/}"

            mkdir -p "$inc_dir"

            (cd "$platform_inc_dir"; $TAR -cf - .) | (cd "$inc_dir"; $TAR -xf -)
        )
    done
    IFS=$OIFS

    # check that key file was built
    path_exists "destdir${prefix}/$(install_artifact_relative "$current_dist")"

    # when cross compiling, resolve build root to host or target
    inst_root=$(resolve_link "$BUILD_ROOT/root")
    inst_root="$inst_root/${rel_prefix#usr}"

    # build file list and sed script to replace file paths in text files and
    # scripts

    file_list="$TMP_DIR/file_list_$$.txt"
    rm -f "$file_list"

    (cd "destdir"; IFS=$NL;
    find "./$prefix" etc 2>/dev/null | while read -r f; do
        IFS=$OIFS

        f=$(normalize_relative_path "$f")
        [ -n "$f" ] || continue

        putsln "$f" >> "$file_list"
    done)
    IFS=$OIFS
    [ -f "$file_list" ]

    sed_scr="$TMP_DIR/sed_scr_$$.sed"
    sed_scr_usr="$TMP_DIR/sed_scr_usr_$$.sed"
    sed_scr_etc="$TMP_DIR/sed_scr_etc_$$.sed"
    rm -f "$sed_scr" "$sed_scr_usr "$sed_scr_etc""

    # build sed script using shortest possible common paths,
    # ignoring docs, man and info pages and top level dirs
    IFS=$NL
    escaped_prefix=$(puts "$rel_prefix" | sed 's,/,\\/,g')

    sed "
        /^[^/]*\$/d
        /^${escaped_prefix}\\/man\$/d
        /^${escaped_prefix}\\/man\\//d
        /^${escaped_prefix}\\/share\\/doc\$/d
        /^${escaped_prefix}\\/share\\/doc\\//d
        /^${escaped_prefix}\\/share\\/man\$/d
        /^${escaped_prefix}\\/share\\/man\\//d
        /^${escaped_prefix}\\/share\\/info\$/d
        /^${escaped_prefix}\\/share\\/info\\//d
        /^${escaped_prefix}\\/[^/][^/]*\$/d
        s|^\\(${escaped_prefix}/[^/][^/]*/[^/][^/]*\\)/[^/].*|\\1|
        s|^\\(etc/[^/][^/]*\\)/[^/].*|\\1|
    " "$file_list" | sort -u | \
    while read -r f; do
        IFS=$OIFS
        variants=$f
        [ "$f" != "${f%.exe}" ] && variants="$variants ${f%.exe}"
        for f in $variants; do
            case "$f" in
                ${rel_prefix}/*)
                    f=${f#${rel_prefix}/}
                    cat >>"$sed_scr_usr" <<EOF
                        s|^/usr/\\($f/*\\)\$|$inst_root/\\1|
                        s|^/usr/\\($f[^a-zA-Z0-9]\\)|$inst_root/\\1|
                        s|\\([^a-zA-Z0-9]\\)/usr/\\($f/*\\)\$|\\1$inst_root/\\2|
                        s|\\([^a-zA-Z0-9]\\)/usr/\\($f[^a-zA-Z0-9]\\)|\\1$inst_root/\\2|g
EOF
                    ;;
                *)
                    cat >>"$sed_scr_usr" <<EOF
                        s|^/\\($f/*\\)\$|$inst_root/\\1|
                        s|^/\\($f[^a-zA-Z0-9]\\)|$inst_root/\\1|
                        s|\\([^a-zA-Z0-9]\\)\\($f/*\\)\$|\\1$inst_root/\\2|
                        s|\\([^a-zA-Z0-9]\\)/\\($f[^a-zA-Z0-9]\\)|\\1$inst_root/\\2|g
EOF
                    ;;
            esac
        done
    done
    IFS=$OIFS

    # group sed script under a /usr/ and /etc/ pattern addresses to speed it up
    #
    cat >"${sed_scr}.work" <<'EOF'
        /\/usr/{
EOF
    # also add special rules to rewrite 'prefix' variables in scripts
    cat >>"${sed_scr}.work" <<EOF
           s|\([Pp][Rr][Ee][Ff][Ii][Xx].*=.*['"]\)/usr\(/*['"]\)|\1$inst_root\2|g
EOF

    # if the relocation mode for the dist is 'aggressive', rewrite all '/usr'
    # prefixes everywhere except for the shebang on the first line and /usr/local
    if [ "$(dist_relocation_type "$current_dist")" = aggressive ]; then
        cat >>"${sed_scr}.work" <<EOF
           2,\${
             s|/usr/local$|/USRLOCAL|
             s|/usr/local/|/USRLOCAL/|g

             s|/usr|$inst_root|g

             s|/USRLOCAL/|/usr/local/|g
             s|/USRLOCAL$|/usr/local|
           }
EOF
    fi

    cat >>"${sed_scr}.work" <<EOF
$(cat "$sed_scr_usr")
        }
EOF

    if [ -f "${sed_scr_etc}" ]; then
      cat >>"${sed_scr}.work" <<EOF
        /\\/etc\\//{
$(cat "$sed_scr_etc")
        }
EOF
    fi

    mv "${sed_scr}.work" "$sed_scr"
    rm -f "$sed_scr_usr "$sed_scr_etc""

    tmp_prefix="$PWD/destdir"

    # the relocation sed script will be spun out as parallel jobs
    running_jobs=
    max_jobs=$NUM_CPUS
    setup_jobs
    job_failed_hook relocation_failed

    defer_cmds=
    OLDPWD=$PWD
    mkdir -p "$inst_root"
    cd "$inst_root"
    IFS=$NL
    for f in $(cat "$file_list"); do
        IFS=$OIFS

        # usr/ is the prefix, but etc/* goes under root/etc/*
        if [ "$f" = "$rel_prefix" ]; then continue; fi

        rel_prefix=${prefix#/}
        dest_f=${f#$rel_prefix}
        dest_f=${dest_f#/}
        rel_prefix=${rel_prefix#usr}
        rel_prefix=${rel_prefix#/}

        if [ -d "$tmp_prefix/$f" ]; then
            echo_run mkdir -p "$dest_f"
            continue
        fi

        # move usr/man files to usr/share/man
        case "$dest_f" in
            man/*)
                dest_f="share/$dest_f"
                ;;
        esac

        if [ ! -d "${dest_f%/*}" ]; then echo_run mkdir -p "${dest_f%/*}"; fi

        # if destination exists as a symlink, remove it first instead of
        # overwriting the destination of the symlink
        rm -f "$dest_f" 2>/dev/null || :

        # rewrite symlinks pointing to /usr/*
        if [ -h "$tmp_prefix/$f" ]; then
            link_dest=$(expr "$(ls -l "$tmp_prefix/$f")" : '.* -> \(.*\)$' | sed 's|^/usr/|'"$inst_root/|")

            # rewrite relative links to absolute ones
            case "$link_dest" in
                /*)
                    ;;
                *)
                    link_dest="$PWD/${dest_f%/*}/$link_dest"
                    ;;
            esac

            link_dest=$(fully_resolve_link "$link_dest")

            if [ -e "$link_dest" ]; then
                echo_run ln -sf "$link_dest" "$dest_f"
            else
                # this is for windows as well, where symlinks can't point to a
                # file that doesn't (yet) exist
                defer_cmds="$defer_cmds
ln -sf \"$link_dest\" \"$dest_f\"
"
            fi

            continue
        fi

        # don''t relocate headers, man and info pages and docs
        case "$dest_f" in
            share/doc/*|share/man/*|share/info/*|include/*)
                echo_run cp -af "$tmp_prefix/$f" "$dest_f" || echo_run cp -rf "$tmp_prefix/$f" "$dest_f"
                continue
                ;;
        esac

        if file "$tmp_prefix/$f" | grep -Eiq ':.*text'; then
            (
                start_job
                write_job_info dist_name "$current_dist"
                write_job_info file_name "$tmp_prefix/$f"

                if [ -x "$tmp_prefix/$f" ]; then
                    putsln "[32mRelocating executable script[0m[35m:[0m $dest_f"
                else
                    putsln "[32mRelocating text file[0m[35m:[0m $dest_f"
                fi

                {
                    LANG=C sed -f "$sed_scr" "$tmp_prefix/$f" >"$dest_f"

                    # rewrite prefix in pkgconfig and libtool files
                    case "$dest_f" in
                        lib/*.l[ao]|lib/pkgconfig/*.pc|share/pkgconfig/*.pc)
                            cp "$dest_f" "${dest_f}.work"
                            LANG=C sed '
                                /\/usr/{
                                    s|/usr/local$|/USRLOCAL|
                                    s|/usr/local/|/USRLOCAL/|g

                                    s|\([^a-zA-Z0-9]\)/usr$|\1'"$inst_root"'|
                                    s|\([^a-zA-Z0-9]\)/usr/|\1'"$inst_root/"'|g
                                    s|\(-[IL]\)/usr/|\1'"$inst_root/"'|g

                                    s|/USRLOCAL/|/usr/local/|g
                                    s|/USRLOCAL$|/usr/local|
                                }
                            ' "${dest_f}.work" > "$dest_f"
                            rm -f "${dest_f}.work"
                            ;;
                    esac
                } 2>&1 | write_job_output
                write_job_exit_status

                if [ -x "$tmp_prefix/$f" ]; then chmod +x "$dest_f" ; fi
            ) &
            running_jobs="$running_jobs $!"

            wait_jobs running_jobs $max_jobs

            continue
        fi

        echo_run cp -af "$tmp_prefix/$f" "$dest_f" || echo_run cp -rf "$tmp_prefix/$f" "$dest_f"

        # when cross-compiling, link arch-suffixed libs to their normal names
        if [ -n "$target_arch" ]; then
            case "$f" in
                */lib/*-${target_arch}.a)
                    echo_run ln -sf "$PWD/$dest_f" "${dest_f%%-${target_arch}.a}.a"
                    ;;
            esac
        fi
    done
    IFS=$OIFS

    wait_all_jobs running_jobs
    cleanup_jobs

    IFS=$NL
    for cmd in $defer_cmds; do
        IFS=$OIFS
        eval echo_run "$cmd"
    done
    IFS=$OIFS

    cd "$OLDPWD"

    rm -f "$file_list" "$sed_scr"

    # things in build env may depend on what was just installed
    eval "$BUILD_ENV"

    # find new things in PATH
    hash -r
}

relocation_failed() {
    error "Relocating file '$file_name' from '$current_dist' failed:${NL}${NL}$(cat "$TMP_DIR/job_output/$1")"

    exit 1
}

normalize_relative_path() {
    p=$1
    [ -n "$p" ] || die 'normalize_relative_path: path required'

    p=${p#.}

    while :; do
        case "$p" in
            /*)
                p=${p#/}
                ;;
            */)
                p=${p%/}
                ;;
            *)
                break
                ;;
        esac
    done

    putsln "$p"
}

echo_run() {
    putsln "[32mExecuting[0m[35m:[0m $(cmd_with_quoted_args "$@")"
    "$@"
}

echo_eval_run() {
    putsln "[32mExecuting[0m[35m:[0m $@"
    eval "$@"
}

cmd_with_quoted_args() {
    [ -n "$1" ] || error 'cmd_with_quoted_args: command required'
    res="$1 "
    shift
    for arg; do
        res="$res '$arg'"
    done
    puts "$res"
}

remove_drive_prefix() {
    path=$1
    [ -n "$path" ] || die 'remove_drive_prefix: path required'

    if [ -n "$msys2" ]; then
        path=${path#/[a-zA-Z]/}
    elif [ -n "$cygwin" ]; then
        path=${path#/cygdrive/[a-zA-Z]/}
    fi

    # remove windows drive prefixes such as c:
    path=${path#[a-zA-Z]:}

    # remove all but one slash at the beginning (double slashes have special meaning on windows)
    while :; do
        case "$path" in
            /*)
                path=${path#/}
                ;;
            *)
                break
                ;;
        esac
    done

    puts "/$path"
}

list_get() {
    pos=$1
    [ -n "$pos" ] || die 'list_get: position to retrieve required'
    shift

    i=0
    for item; do
        if [ $i -eq $pos ]; then
            puts "$item"
            return 0
        fi

        i=$((i + 1))
    done
}

list_index() {
    item=$1
    [ -n "$item" ] || die 'list_index: item to find required'
    shift

    i=0
    for element; do
        if [ "$element" = "$item" ]; then
            puts $i
            return 0
        fi

        i=$((i + 1))
    done

    return 1
}

dist_args() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_args: dist name required'
    buildsys=$2

    case "$buildsys" in
        autoconf)
            puts "$CONFIGURE_ARGS $(table_line DIST_ARGS $current_dist)" || :
            ;;
        cmake)
            puts "$CMAKE_ARGS $(table_line DIST_ARGS $current_dist)" || :
            ;;
        meson)
            puts "$MESON_ARGS $(table_line DIST_ARGS $current_dist)" || :
            ;;
        perl)
            puts "$(table_line DIST_ARGS $current_dist)" || :
            ;;
        python)
            puts "$(table_line DIST_ARGS $current_dist)" || :
            ;;
        *)
            die "dist_args: buildsystem type required, must be 'autoconf', 'cmake' or 'perl'"
            ;;
    esac
}

dist_prefix() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_prefix: dist name required'

    prefix=$(table_line DIST_PREFIX $current_dist) || :
    [ -n "$prefix" ] || prefix=/usr

    while :; do
        case "$prefix" in
            */)
                prefix=${prefix%/}
                ;;
            *)
                break
                ;;
        esac
    done

    puts "$prefix"
}

dist_tar_args() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_tar_args: dist name required'

    puts "$(table_line DIST_TAR_ARGS $current_dist)" || :
}

dist_configure_override() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_configure_override: dist name required'

    puts "$(table_line DIST_CONFIGURE_OVERRIDES $current_dist)" || :
}

dist_install_override() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_install_override: dist name required'

    puts "$(table_line DIST_INSTALL_OVERRIDES $current_dist)" || :
}

dist_build_override() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_build_override: dist name required'

    puts "$(table_line DIST_BUILD_OVERRIDES $current_dist)" || :
}

dist_configure_type() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_configure_type: dist name required'

    puts "$(table_line DIST_CONFIGURE_TYPES $current_dist)" || :
}

dist_relocation_type() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_relocation_type: dist name required'

    puts "$(table_line DIST_RELOCATION_TYPES $current_dist)" || :
}

dist_make_args() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_make_args: dist name required'

    puts "$ALL_MAKE_ARGS $(table_line DIST_MAKE_ARGS $current_dist)" || :
}

dist_ninja_args() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_ninja_args: dist name required'

    puts "-v $(table_line DIST_MAKE_ARGS $current_dist)" || :
}

dist_make_install_args() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_make_install_args: dist name required'

    puts "$(table_line DIST_MAKE_INSTALL_ARGS $current_dist)" || :
}

dist_extra_ldflags() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_extra_ldflags: dist name required'

    puts "$(table_line DIST_EXTRA_LDFLAGS $current_dist)" || :
}

dist_extra_libs() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_extra_libs: dist name required'

    puts "$(table_line DIST_EXTRA_LIBS $current_dist)" || :
}

dist_patch() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_patch: dist name required'

    _patch_level=-p1

    for _patch_url in $(table_line DIST_PATCHES $current_dist); do
        case "$_patch_url" in
            -p*)
                _patch_level=$_patch_url
                continue
                ;;
        esac

        _patch_file=${_patch_url##*/}
        _patch_file=${_patch_file%%\?*}

        if [ ! -f "$_patch_file" ]; then
            puts "${NL}[32mApplying patch [1;34m$_patch_url[0m to [1;35m$current_dist[0m${NL}${NL}"

            $CURL -SsL "$_patch_url" -o "$_patch_file"
            patch -l $_patch_level < "$_patch_file"

            # reset patch level to 1 which is default
            _patch_level=-p1

            done_msg
        else
            puts "${NL}[32mPatch [1;34m$_patch_url[0m to [1;35m$current_dist[0m is already appplied...${NL}${NL}"
        fi
    done
}

dist_pre_build() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_pre_build: dist name required'

    if _cmd=$(table_line DIST_PRE_BUILD $current_dist); then
        puts "${NL}[32mRunning pre-build for: [1;35m$current_dist[0m:${NL}$_cmd${NL}${NL}"

        eval "$_cmd"
    fi
}

dist_post_unpack() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_post_unpack: dist name required'

    if _cmd=$(table_line DIST_POST_UNPACK $current_dist); then
        puts "${NL}[32mRunning post-unpack for: [1;35m$current_dist[0m:${NL}$_cmd${NL}${NL}"

        eval "$_cmd"
    fi
}

dist_post_configure() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_post_configure: dist name required'

    if _cmd=$(table_line DIST_POST_CONFIGURE $current_dist); then
        puts "${NL}[32mRunning post-configure for: [1;35m$current_dist[0m:${NL}$_cmd${NL}${NL}"

        eval "$_cmd"
    fi

    # sometimes PREFIX/include gets added to header search
    # definitely don't want this
    # also definitely don't want any kind of -Werror
    find . -name Makefile | while IFS=$NL read -r make_file; do
        sed -i.bak '
            s,-I/usr/include , ,g
            s,-I/usr/include$,,g
            s,-Werror[^ ]* , ,g
            s,-Werror[^ ]*$,,g
        ' "$make_file"
    done
}

dist_post_build() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'dist_post_build: dist name required'

    if _cmd=$(table_line DIST_POST_BUILD $current_dist); then
        puts "${NL}[32mRunning post-build for: [1;35m$current_dist[0m:${NL}$_cmd${NL}${NL}"

        eval "$_cmd"
    fi
}

install_docbook_dist() {
    _type=$1

    _dist_ver=$(echo "$PWD" | sed 's/.*[^0-9.]\([0-9.]*\)$/\1/')

    case "$_type" in
        stylesheet)
            _dir="stylesheet/${PWD##*/}"
            ;;
        schema)
            _dir="schema/dtd/$_dist_ver"
            ;;
        *)
            die "install_docbook_dist: type of dist required, must be 'stylesheet' or 'schema'"
            ;;
    esac

    _dir="$BUILD_ROOT/root/share/xml/docbook/$_dir"

    echo_run mkdir -p "$_dir"
    echo_run cp -af * "$_dir" || echo_run cp -rf * "$_dir"

    # on cygwin/msys write native POSIX paths to catalog
    _dir=$(cygpath -m "$_dir")

    if [ -f "$_dir/catalog.xml" ]; then
        echo_run xmlcatalog --noout --del "file://$_dir/catalog.xml" "$(cygpath -m "$BUILD_ROOT/root/etc/xml/catalog.xml")" || :
        echo_run xmlcatalog --noout --add nextCatalog '' "file://$_dir/catalog.xml" "$(cygpath -m "$BUILD_ROOT/root/etc/xml/catalog.xml")"
    fi
}

install_fonts() {
    if [ -d fontconfig ]; then
        install -v -m644 fontconfig/*.conf "$BUILD_ROOT/root/etc/fonts/conf.d"
    fi

    font_found=
    IFS=$NL
    for ttf in $(find . -name '*.ttf' -o -name '*.TTF' -o -name '*.pfm' -o -name '*.PFM' -o -name '*.pfb' -o -name '*.PFB' -o -name '*.afb' -o -name '*.AFB'); do
        IFS=$OIFS
        font_found=1
        if [ ! -d "$BUILD_ROOT/root/share/fonts/${PWD##*/}" ]; then
            install -v -d -m755 "$BUILD_ROOT/root/share/fonts/${PWD##*/}"
        fi
        install -v -m644 "$ttf" "$BUILD_ROOT/root/share/fonts/${PWD##*/}"
    done
    IFS=$OIFS

    [ -n "$font_found" ] && echo_run fc-cache -fv "$BUILD_ROOT/root/share/fonts/${PWD##*/}"
}

table_line() {
    table=$1
    [ -n "$table" ] || die 'table_line: table name required'
    name=$2
    [ -n "$name" ]  || die 'table_line: item name required'

    table=$(table_contents $table)

    IFS=$NL
    for line in $table; do
        IFS=$OIFS
        set -- $line
        if [ "$1" = "$name" ]; then
            shift
            puts "$@"
            return 0
        fi
    done
    IFS=$OIFS

    return 1
}

table_line_append() {
    table=$1
    [ -n "$table" ]      || die 'table_line_append: table name required'
    name=$2
    [ -n "$name" ]       || die 'table_line_append: item name required'
    append_str=$3
    [ -n "$append_str" ] || die 'table_line_append: string to append required'

    table_name=$table
    table=$(table_contents $table)

    new_table=
    line_appended=
    IFS=$NL
    for line in $table; do
        IFS=$OIFS
        set -- $line
        if [ "$1" = "$name" ]; then
            new_table="${new_table}$@ ${append_str}${NL}"
            line_appended=1
        else
            new_table="${new_table}${@}${NL}"
        fi
    done
    IFS=$OIFS

    if [ -z "$line_appended" ]; then
        # make new entry
        new_table="${new_table}${name} ${append_str}${NL}"
    fi

    eval "$table_name=\$new_table"
}

table_line_replace() {
    table=$1
    [ -n "$table" ]   || die 'table_line_replace: table name required'
    name=$2
    [ -n "$name" ]    || die 'table_line_replace: item name required'
    set_str=$3
    [ -n "$set_str" ] || die 'table_line_replace: string to set required'

    table_name=$table
    table=$(table_contents $table)

    new_table=
    line_found=
    IFS=$NL
    for line in $table; do
        IFS=$OIFS
        set -- $line
        if [ "$1" = "$name" ]; then
            new_table="${new_table}$1 ${set_str}${NL}"
            line_found=1
        else
            new_table="${new_table}${@}${NL}"
        fi
    done
    IFS=$OIFS

    if [ -z "$line_found" ]; then
        # make new entry
        new_table="${new_table}${name} ${set_str}${NL}"
    fi

    eval "$table_name=\$new_table"
}

table_insert_after() {
    table=$1
    [ -n "$table" ]   || die 'table_insert_after: table name required'
    name=$2
    [ -n "$name" ]    || die 'table_insert_after: item name to insert after required'
    new_line=$3
    [ -n "$new_line" ] || die 'table_insert_after: new line required'

    table_name=$table
    table=$(table_contents $table)

    new_table=
    line_found=
    IFS=$NL
    for line in $table; do
        IFS=$OIFS
        set -- $line
        new_table="${new_table}${@}${NL}"

        if [ "$1" = "$name" ]; then
            new_table="${new_table}${new_line}${NL}"
            line_found=1
        fi
    done
    IFS=$OIFS

    [ -n "$line_found" ] || error 'table_insert_after: item to insert after not found'

    eval "$table_name=\$new_table"
}

table_insert_before() {
    table=$1
    [ -n "$table" ]   || die 'table_insert_before: table name required'
    name=$2
    [ -n "$name" ]    || die 'table_insert_before: item name to insert before required'
    new_line=$3
    [ -n "$new_line" ] || die 'table_insert_before: new line required'

    table_name=$table
    table=$(table_contents $table)

    new_table=
    line_found=
    IFS=$NL
    for line in $table; do
        IFS=$OIFS
        set -- $line

        if [ "$1" = "$name" ]; then
            new_table="${new_table}${new_line}${NL}"
            line_found=1
        fi

        new_table="${new_table}${@}${NL}"
    done
    IFS=$OIFS

    [ -n "$line_found" ] || die 'table_insert_before: item to insert before not found'

    eval "$table_name=\$new_table"
}

table_line_remove() {
    table=$1
    [ -n "$table" ]      || die 'table_line_remove: table name required'
    name=$2
    [ -n "$name" ]       || die 'table_line_remove: item name required'

    table_name=$table
    table=$(table_contents $table)

    new_table=
    IFS=$NL
    for line in $table; do
        IFS=$OIFS
        set -- $line
        if [ "$1" != "$name" ]; then
            new_table="${new_table}${@}${NL}"
        fi
    done
    IFS=$OIFS

    eval "$table_name=\$new_table"
}

find_checkout() {
    (
        cd "$(dirname "$0")"
        while [ "$PWD" != / ]; do
            if [ -f src/version.h.in ]; then
                puts "$PWD"
                exit 0
            fi

            cd ..
        done
        exit 1
    ) || die 'cannot find project checkout'
}

error() {
    puts >&2 "${NL}[31mERROR[0m: $@${NL}${NL}"
}

warn() {
    puts >&2 "${NL}[35mWARNING[0m: $@${NL}${NL}"
}

die() {
    error "$@"
    exit 1
}

build_project() {
    puts "${NL}[32mBuilding project: [1;34m$CHECKOUT[0m${NL}${NL}"

    target_os=${CROSS_OS:-$os}

    dist_pre_build project

    mkdir -p "$BUILD_ROOT/project"
    cd "$BUILD_ROOT/project"

    # FIXME: turn LTO back on when everything works
    echo_eval_run cmake "'$CHECKOUT'" $REQUIRED_CMAKE_ARGS -DVBAM_STATIC=ON -DENABLE_LTO=OFF $PROJECT_ARGS $CMAKE_BASE_ARGS $@
    echo_run make -j$NUM_CPUS

    if [ "$target_os" = mac ]; then
        $STRIP visualboyadvance-m.app/Contents/MacOS/visualboyadvance-m

        codesign -s "Developer ID Application" --deep ./visualboyadvance-m.app || :

        rm -f ./visualboyadvance-m-Mac.zip
        zip -9r ./visualboyadvance-m-Mac.zip ./visualboyadvance-m.app
    elif [ "$target_os" != windows ] && path_exists visualboyadvance-m; then
        $STRIP visualboyadvance-m
    elif [ "$target_os" = windows ] && path_exists visualboyadvance-m.exe; then
        $STRIP visualboyadvance-m.exe
    fi

    dist_post_build project

    puts "${NL}[32mBuild Successful!!![0m${NL}${NL}Build results can be found in: [1;34m$BUILD_ROOT/project[0m${NL}${NL}"
}

duplicate_dist() {
    src_dist=$1
    [ -n "$src_dist" ]   || die 'duplicate_dist: source dist required'
    dest_dist=$2
    [ -n "$dest_dist" ]  || die 'duplicate_dist: destination dist required'

    IFS=$NL
    for table in DISTS $(set | sed -n 's/^\(DIST_[A-Z_]*\)=.*/\1/p'); do
        IFS=$OIFS
        if line=$(table_line $table $src_dist); then
            table_insert_after $table $src_dist "$dest_dist $line"
        fi
    done
    IFS=$OIFS
}

table_column() {
    table=$1
    [ -n "$table" ]    || die 'table_column: table name required'
    col=$2
    [ -n "$col" ]      || die 'table_column: column required'
    row_size=$3
    [ -n "$row_size" ] || die 'table_column: row_size required'

    table=$(table_contents $table)

    i=0
    res=
    for item in $table; do
        if [ $((i % row_size)) -eq "$col" ]; then
            res="$res $item"
        fi
        i=$((i + 1))
    done

    puts "$res"
}

table_rows() {
    table=$1
    [ -n "$table" ] || die 'table_rows: table name required'

    table=$(table_contents $table)

    i=0
    IFS=$NL
    for line in $table; do
        i=$((i + 1))
    done
    IFS=$OIFS

    puts $i
}

table_contents() {
    table=$1
    [ -n "$table" ] || die 'table_contents: table name required'

    # filter comments and blank lines
    eval puts "\"\$$table\"" | grep -Ev '^ *(#|$)' || :
}

list_contains() {
    _item=$1
    [ -n "$_item" ] || die 'list_contains: item required'
    shift

    for _pos; do
        [ "$_item" = "$_pos" ] && return 0
    done

    return 1
}

list_length() {
    puts $#
}

list_remove_duplicates() {
    _seen=
    for _item; do
        if ! list_contains $_item $_seen; then
            _seen="$_seen $_item"
        fi
    done
    echo $_seen
}

install_artifact() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'install_artifact: dist name required'

    prefix=$(dist_prefix $current_dist)
    prefix=${prefix#/usr}

    set -- $(table_line DISTS $current_dist)

    eval "path=\"\$$#\""

    # for cross builds, the files are absolute paths into the host and target
    # trees
    case "$path" in
        /*)
            puts "$path"
            return 0
            ;;
    esac

    puts "$BUILD_ROOT/root${prefix}/${path}"
}

install_artifact_relative() {
    current_dist=$1
    [ -n "$current_dist" ] || die 'install_artifact_relative: dist name required'

    set -- $(table_line DISTS $current_dist)

    eval "path=\"\$$#\""

    # for cross builds, the files are absolute paths into the host and target
    # trees
    puts "$path" | sed 's, *'"$BUILD_ROOT"'/[^/]*/,,'
}

echo() {
    if [ -n "$BASH_VERSION" -a "$os" != mac ]; then
        builtin echo -e "$@"
    else
        command echo "$@"
    fi
}

puts() {
    [ $# -gt 0 ] || return 0

    printf '%s' "$1"
    shift

    for _str; do
        printf ' %s' "$_str"
    done
}

putsln() {
    puts "$@"
    printf '\n'
}

path_exists() {
    [ -z "$1" ] && return 1

    if [ -e "$1" ] || [ -L "$1" ] || [ -d "$1" ]; then
        return 0
    fi

    # check unquoted versions in case of globs
    # but must escape spaces first
    escaped=$(gsub ' ' '\ ' "$1")
    [ -e $escaped ] || [ -L $escaped ] || [ -d $escaped ]
}

gsub() {
    match=$1
    repl=$2
    shift; shift;

    res=
    str="$@"

    while [ -n "$str" ]; do
        case "$str" in
            *$match*)
                res="$res${str%%$match*}$repl"
                str="${str#*$match}"
                ;;
            *)
                res="$res$str"
                break
                ;;
        esac
    done

    printf '%s' "$res"
}

# on msys2 `ln -sf` to an existing link silently fails
# so delete it first
ln() {
    if [ $# -eq 3 ] && [ "$1" = "-sf" ] && [ -h "$3" ]; then
        rm -f "$3"
    fi
    command ln "$@"
}

cygpath() {
    if sh -c 'command -v cygpath' >/dev/null; then
        command cygpath "$@"
    else
        case "$1" in
            -*)
                shift
                ;;
        esac

        echo "$@"
    fi
}

fully_resolve_link() {
    file=$1
    # get initial part for non-absolute path, or blank for absolute
    path=${file%%/*}
    # and set $file to the rest
    file=${file#*/}

    OLDIFS=$IFS
    IFS='/'
    for part in $file; do
        [ ! -z "$part" ] && path=$(resolve_link "$path/$part")
    done
    IFS=$OLDIFS

    # remove 'foo/..' path parts
    while :; do
        case "$path" in
            */../*|*/..)
                path=$(echo "$path" | sed 's,//*[^/][^/]*//*\.\./*,/,g')
                ;;
            *)
                break
                ;;
        esac
    done

    # remove trailing /s
    while [ "$path" != "${path%/}" ]; do
        path=${path%/}
    done

    echo "$path"
}

resolve_link() {
    file=$1

    while [ -h "$file" ]; do
        ls0=$(ls -ld "$file")
        new_link=$(expr "$ls0" : '.* -> \(.*\)$')
        if expr "$new_link" : '/.*' > /dev/null; then
            file="$new_link"
        else
            file="${file%/*}"/"$new_link"
        fi
    done

    echo "$file"
}

#cpanm() {
#    if command -v cpanm >/dev/null; then
#        cpanm "$@"
#    else
#        # why the fuck does this segfault?
#        perl -MApp::cpanminus::fatscript -le 'my $c = App::cpanminus::script->new; $c->parse_options(@ARGV); $c->doit;' -- "$@"
#    fi
#}

# this needs to run on source, not just after entry
setup
