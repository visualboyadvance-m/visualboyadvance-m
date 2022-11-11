#!/bin/sh

set -e

export CROSS_OS=windows

[ -n "$BUILD_ENV" ] && eval "$BUILD_ENV"

BUILD_ENV=$BUILD_ENV$(cat <<EOF

export CPPFLAGS="$CPPFLAGS${CPPFLAGS:+ }-DMINGW_HAS_SECURE_API -DFFI_STATIC_BUILD"
export CFLAGS="$CFLAGS${CFLAGS:+ }-static-libgcc -static-libstdc++ -static -lpthread -DMINGW_HAS_SECURE_API -DFFI_STATIC_BUILD -lm"
export CXXFLAGS="$CXXFLAGS${CXXFLAGS:+ }-static-libgcc -static-libstdc++ -static -lpthread -DMINGW_HAS_SECURE_API -DFFI_STATIC_BUILD -lm"
export OBJCXXFLAGS="$OBJCXXFLAGS${OBJCXXFLAGS:+ }-static-libgcc -static-libstdc++ -static -lpthread -DMINGW_HAS_SECURE_API -DFFI_STATIC_BUILD -lm"
export LDFLAGS="$LDFLAGS${LDFLAGS:+ }-static-libgcc -static-libstdc++ -static -lpthread -DMINGW_HAS_SECURE_API -DFFI_STATIC_BUILD -lm"
export LIBS="-lpthread -lm -lole32"

export UUID_LIBS="-luuid_mingw -luuid"

export PKG_CONFIG="\$BUILD_ROOT/root/bin/pkg-config"

EOF
)

export BUILD_ENV

: ${HOST_CC:=ccache gcc}
: ${HOST_CXX:=ccache g++}
: ${HOST_CPPFLAGS:="-I\$BUILD_ROOT/root/include"}
: ${HOST_CFLAGS:="-fPIC -I\$BUILD_ROOT/root/include -L\$BUILD_ROOT/root/lib -pthread -lm"}
: ${HOST_CXXFLAGS:="-fPIC -I\$BUILD_ROOT/root/include -L\$BUILD_ROOT/root/lib -std=gnu++17 -fpermissive -pthread -lm"}
: ${HOST_OBJCXXFLAGS:="-fPIC -I\$BUILD_ROOT/root/include -L\$BUILD_ROOT/root/lib -std=gnu++17 -fpermissive -pthread -lm"}
: ${HOST_LDFLAGS:="-fPIC -L\$BUILD_ROOT/root/lib -pthread -lm"}
: ${HOST_LIBS:=-lm}
: ${HOST_UUID_LIBS:=}
: ${HOST_STRIP:=strip}

. "$(dirname "$0")/../builder/core.sh"

# make separate roots for target and host for cross compiling

if [ ! -L "$BUILD_ROOT/root" ]; then
    mv "$BUILD_ROOT/root" "$BUILD_ROOT/target"
    mkdir -p "$BUILD_ROOT/host"
    ln -sf "$BUILD_ROOT/target" "$BUILD_ROOT/root"
    cp -a "$BUILD_ROOT/target/"* "$BUILD_ROOT/host"

    # share these for both roots
    for d in perl5 share etc man doc; do
        rmdir "$BUILD_ROOT/host/$d"
        ln -s "$BUILD_ROOT/target/$d" "$BUILD_ROOT/host/$d"
    done

    mkdir -p "$BUILD_ROOT/host/bin" "$BUILD_ROOT/target/bin"
fi

ln -sf "$BUILD_ROOT/target" "$BUILD_ROOT/root"

perl_dists="$perl_dists XML-NamespaceSupport XML-SAX-Base XML-SAX"
perl_dists=$(list_remove_duplicates $perl_dists)

# to codesign windows binary
table_insert_after DISTS pkgconfig '
    osslsigncode    https://github.com/mtrojnar/osslsigncode/archive/18810b7e0bb1d8e0d25b6c2565a065cf66bce5d7.tar.gz    bin/osslsigncode
'

table_line_append DIST_CONFIGURE_OVERRIDES osslsigncode 'sh autogen.sh && ./configure --prefix=/usr'

table_line_append DIST_EXTRA_LIBS osslsigncode '-lz -lssl -lcrypto -ldl'

host_dists="$host_dists autoconf autoconf-archive automake m4 gsed bison \
                        flex-2.6.3 flex c2man docbook2x ccache ninja curl osslsigncode"
host_dists=$(list_remove_duplicates $host_dists)

both_dists="$both_dists openssl zlib bzip2 libiconv"

if [ "$os" != windows ]; then
    both_dists="$both_dists libuuid"
fi

both_dists=$(list_remove_duplicates $both_dists)

host_env_base() {
    rm -f "$BUILD_ROOT/root"
    ln -sf "$BUILD_ROOT/host" "$BUILD_ROOT/root"
    host_env_hook 2>/dev/null || :
}

host_env() {
    if [ -z "$OCC" ]; then
        cat <<EOF
OCC="\$CC"
OCXX="\$CXX"
OCPPFLAGS="\$CPPFLAGS"
OCFLAGS="\$CFLAGS"
OCXXFLAGS="\$CXXFLAGS"
OOBJCXXFLAGS="\$OBJCXXFLAGS"
OLDFLAGS="\$LDFLAGS"
OLIBS="\$LIBS"
OUUID_LIBS="\$UUID_LIBS"
OSTRIP="\$STRIP"
OPATH="\$PATH"

OCONFIGURE_REQUIRED_ARGS="\$CONFIGURE_REQUIRED_ARGS"
OCMAKE_REQUIRED_ARGS="\$CMAKE_REQUIRED_ARGS"

$BUILD_ENV

export CC="$HOST_CC"
export CXX="$HOST_CXX"
export CPPFLAGS="$HOST_CPPFLAGS"
export CFLAGS="$HOST_CFLAGS"
export CXXFLAGS="$HOST_CXXFLAGS"
export OBJCXXFLAGS="$HOST_OBJCXXFLAGS"
export LDFLAGS="$HOST_LDFLAGS"
export LIBS="$HOST_LIBS"
export UUID_LIBS="$HOST_UUID_LIBS"
export STRIP="$HOST_STRIP"
export PATH="$BUILD_ROOT/host/bin:\$PATH"

CONFIGURE_REQUIRED_ARGS="\$(puts "\$CONFIGURE_REQUIRED_ARGS" | sed 's/--host[^ ]*//g')"
CMAKE_REQUIRED_ARGS="\$(puts "\$CMAKE_REQUIRED_ARGS" | sed 's/-DCMAKE_TOOLCHAIN_FILE=[^ ]*//g')"

unset TARGET_ENV
export HOST_ENV=1
EOF
    fi

    host_env_base 2>/dev/null || :
}

target_env_base() {
    rm -f "$BUILD_ROOT/root"
    ln -sf "$BUILD_ROOT/target" "$BUILD_ROOT/root"

    # make links to executables in the target as well
    IFS=$NL
    for exe in $(find "$BUILD_ROOT/host/bin" -maxdepth 1 -type f); do
        IFS=$OIFS
        basename=${exe##*/}
        if ! path_exists "$BUILD_ROOT/target/bin/$basename"; then
            ln -s "$exe" "$BUILD_ROOT/target/bin/$basename";
        fi
    done
    IFS=$OIFS

    target_env_hook 2>/dev/null || :
}

target_env() {
    if [ -n "$OCC" ]; then
        cat <<EOF
export CC="\$OCC"
export CXX="\$OCXX"
export CPPFLAGS="\$OCPPFLAGS"
export CFLAGS="\$OCFLAGS"
export CXXFLAGS="\$OCXXFLAGS"
export OBJCXXFLAGS="\$OOBJCXXFLAGS"
export LDFLAGS="\$OLDFLAGS"
export LIBS="\$OLIBS"
export UUID_LIBS="\$OUUID_LIBS"
export STRIP="\$OSTRIP"
export PATH="\$OPATH"
OCC= OCXX= OCPPFLAGS= OCFLAGS= OCXXFLAGS= OOBJCXXFLAGS= OLDFLAGS= OLIBS= OUUID_LIBS= OSTRIP= OPATH=

CONFIGURE_REQUIRED_ARGS="\$OCONFIGURE_REQUIRED_ARGS"
CMAKE_REQUIRED_ARGS="\$OCMAKE_REQUIRED_ARGS"
OCONFIGURE_REQUIRED_ARGS= OCMAKE_REQUIRED_ARGS=

unset HOST_ENV
export TARGET_ENV=1
$BUILD_ENV
EOF
    fi

    target_env_base 2>/dev/null || :
}

# replace install artifact paths with absolute paths into host and target trees
# so that the check for which dists are already built works correctly
pre_build_all() {
    new_dists=
    IFS=$NL
    for dist in $DISTS; do
        IFS=$OIFS

        set -- $dist

        case "$(table_line DIST_PRE_BUILD "$1")" in
            *host_env*)
                path="$BUILD_ROOT/host/$3"
                ;;
            *)
                path="$BUILD_ROOT/target/$3"
                ;;
        esac

        new_dists="$new_dists $1 $2 $path $NL"
    done
    IFS=$OIFS

    DISTS=$new_dists
}

for dist in $host_dists $perl_dists; do
    table_line_append  DIST_PRE_BUILD  $dist ':; eval "$(host_env)";'
    table_line_replace DIST_POST_BUILD $dist "eval \"\$(target_env)\"; $(table_line DIST_POST_BUILD $dist)"
done

for dist in $both_dists; do
    duplicate_dist $dist "${dist}-target"

    table_line_append  DIST_PRE_BUILD  $dist ':; eval "$(host_env)";'
    table_line_replace DIST_POST_BUILD $dist "eval \"\$(target_env)\"; $(table_line DIST_POST_BUILD $dist)"
done

remove_dists='graphviz python2 python3 meson swig libxml2-python doxygen bakefile setuptools pip XML-Parser intltool libsecret shared-mime-info'

for dist in $remove_dists; do
    if ! list_contains $dist $do_not_remove_dists; then
        table_line_remove DISTS "$dist"
    fi
done

# build bzip2 for target with autotools patch
bzip2_target=$(table_line DISTS bzip2-target)

table_line_remove DISTS bzip2-target

table_insert_after DISTS libtool "bzip2-target $bzip2_target"

table_line_append DIST_PATCHES bzip2-target 'https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-bzip2/bzip2-buildsystem.all.patch'

table_line_remove DIST_MAKE_ARGS bzip2-target
# done with bzip2-target

# use mingw version of libuuid

if [ "$os" != windows ]; then
    table_line_remove  DISTS libuuid-target
    table_insert_after DISTS libuuid "libuuid-target https://github.com/h0tw1r3/libuuid-mingw/archive/1.0.1.tar.gz lib/libuuid_mingw.a"
else
    table_line_replace DISTS libuuid                "https://github.com/h0tw1r3/libuuid-mingw/archive/1.0.1.tar.gz lib/libuuid_mingw.a"
fi

# done with libuuid

table_line_append DIST_PRE_BUILD zlib ":; \
    sed -i.bak ' \
        s/defined(_WIN32)  *||  *defined(__CYGWIN__)/defined(_WIN32)/ \
    ' gzguts.h; \
"

table_line_append DIST_POST_BUILD zlib-target ":; \
    rm -f \$BUILD_ROOT/root/lib/libz.dll.a \$BUILD_ROOT/root/bin/libz.dll; \
"

table_line_replace DIST_CONFIGURE_TYPES     zlib-target cmake
table_line_append  DIST_ARGS                zlib-target -DUNIX=1
table_line_remove  DIST_CONFIGURE_OVERRIDES zlib-target

zlib_dist=$(table_line DISTS zlib-target)

table_line_remove  DISTS zlib-target

table_insert_after DISTS cmake "zlib-target $zlib_dist"

# mingw -ldl equivalent, needed by some things
table_insert_after DISTS cmake "dlfcn https://github.com/dlfcn-win32/dlfcn-win32/archive/v1.1.2.tar.gz lib/libdl.a"

table_line_replace DIST_CONFIGURE_TYPES dlfcn cmake

if [ "$target_bits" -eq 32 ]; then
    # this is necessary for a linkable libffi on i686 for whatever reason
    # see: https://bugzilla.mozilla.org/show_bug.cgi?id=1336569
    table_line_append DIST_EXTRA_CPPFLAGS libffi -DSYMBOL_UNDERSCORE
fi

libicu=libicu

if [ -n "$(table_line DISTS libicu-target || :)" ]; then
    libicu=libicu-target
fi

table_line_append DIST_PATCHES $libicu " \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-icu/0004-move-to-bin.mingw.patch \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-icu/0007-actually-move-to-bin.mingw.patch \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-icu/0008-data-install-dir.mingw.patch \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-icu/0009-fix-bindir-in-config.mingw.patch \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-icu/0011-sbin-dir.mingw.patch \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-icu/0012-libprefix.mingw.patch \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-icu/0014-mingwize-pkgdata.mingw.patch \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-icu/0015-debug.mingw.patch \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-icu/0016-icu-pkgconfig.patch \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-icu/0017-icu-config-versioning.patch \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-icu/0021-mingw-static-libraries-without-s.patch \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-icu/0023-fix-twice-include-platform_make_fragment.patch \
"

table_line_append DIST_PRE_BUILD $libicu ":; sed -E -i.bak 's/@echo -n /@printf \"%s\" /g' config/mh-mingw*;"

table_insert_after DISTS libiconv-target '
    catgets         https://downloads.sourceforge.net/project/mingw/MinGW/Extension/catgets/mingw-catgets-1.0.1/mingw-catgets-1.0.1-src.tar.gz    include/langinfo.h
'

table_line_append DIST_PATCHES catgets "\
    https://gist.githubusercontent.com/rkitover/4fe26d4af9e20234ba7821100356b0a6/raw/1f11522e9feea3c2a431beca57fc3db07ca44a1c/mingw-catgets-mc_realloc-and-langinfo.patch \
"

table_line_append DIST_POST_BUILD catgets ":; \
    rm -f \$BUILD_ROOT/root/lib/libcatgets.dll.a \$BUILD_ROOT/root/bin/libcatgets.dll; \
"

table_line_append DIST_PATCHES fontconfig-target " \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-fontconfig/0001-fix-config-linking.all.patch \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-fontconfig/0002-fix-mkdir.mingw.patch \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-fontconfig/0004-fix-mkdtemp.mingw.patch \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-fontconfig/0005-fix-setenv.mingw.patch \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-fontconfig/0007-pkgconfig.mingw.patch \
"

table_line_append DIST_PATCHES libgd 'https://gist.githubusercontent.com/rkitover/c64ea5b83ddea94ace58c40c7de42879/raw/fbaf4885fbefb302116b56626c0e191df514e8c6/libgd-2.2.4-mingw-static.patch'

table_insert_before DISTS sfml '
    openal          https://github.com/kcat/openal-soft/archive/openal-soft-1.19.1.tar.gz                      lib/libOpenAL32.a
'

table_line_append DIST_ARGS openal '-DLIBTYPE=STATIC -DALSOFT_UTILS=OFF -DALSOFT_EXAMPLES=OFF -DALSOFT_TESTS=OFF'

table_line_append DIST_PATCHES mp3lame "https://raw.githubusercontent.com/msys2/MINGW-packages/master/mingw-w64-lame/0007-revert-posix-code.patch"

table_line_append DIST_POST_BUILD libgsm ":; \
    rm -f \$BUILD_ROOT/root/lib/libgsm.dll.a \$BUILD_ROOT/root/bin/libgsm.dll; \
"

table_line_append DIST_POST_BUILD ffmpeg ":; \
    sed -i.bak 's/-lSDL2main//g; s/-lstdc++//g; s/-lgcc_s//g; s/-lgcc//g; s/-lpthread//g' \$BUILD_ROOT/root/lib/pkgconfig/libav*.pc
"

table_line_append DIST_PRE_BUILD        xvidcore ":; sed -i.bak 's/STATIC_LIB=\"xvidcore\\./STATIC_LIB=\"libxvidcore./; \
                                                          s/SHARED_LIB=\"xvidcore\\./SHARED_LIB=\"libxvidcore./' configure.in; \
"
table_line_replace DIST_CONFIGURE_TYPES xvidcore autoreconf

# this was my attempt to fix the problem with msys paths in the perl configs, but does not help
# so the doc dir is removed from expat
#table_line_append DIST_POST_CONFIGURE docbook2x ":; sed -i.bak 's,q</c/,q<c:/,; /xsltproc-program'\\''/{ s/\([a-z]\)>, *$/\1.exe>,/; }' perl/config.pl;"

table_line_append DIST_ARGS libsoxr '-DWITH_OPENMP=NO'

table_line_append DIST_CONFIGURE_OVERRIDES ffmpeg "--extra-ldflags='-Wl,-allow-multiple-definition' --extra-libs='-lwsock32 -lws2_32 -liphlpapi -lfreetype'"

table_line_append DIST_ARGS gettext "--enable-threads=windows"

table_line_append DIST_ARGS glib -Dforce_posix_threads=true

table_line_append  DIST_PATCHES glib "\
    https://raw.githubusercontent.com/msys2/MINGW-packages/master/mingw-w64-glib2/0001-Update-g_fopen-g_open-and-g_creat-to-open-with-FILE_.patch \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-glib2/0001-win32-Make-the-static-build-work-with-MinGW-when-pos.patch \
    https://raw.githubusercontent.com/msys2/MINGW-packages/master/mingw-w64-glib2/0001-disable-some-tests-when-static.patch \
    https://gist.githubusercontent.com/rkitover/2edaf9583fb3068bb14016571e6f7d01/raw/ece80116d5618f372464f02392a9bcab670ce6c1/glib-mingw-no-strerror_s.patch \
"

table_line_append DIST_PATCHES graphite2 "\
    https://raw.githubusercontent.com/msys2/MINGW-packages/master/mingw-w64-graphite2/001-graphite2-1.3.12-win64.patch \
"

table_line_append DIST_PATCHES libgsm "\
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-gsm/0001-adapt-makefile-to.mingw.patch \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-gsm/0002-adapt-config-h-to.mingw.patch \
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-gsm/0003-fix-ln.mingw.patch \
"

table_line_append DIST_PATCHES libtheora "\
    http://src.fedoraproject.org/cgit/rpms/mingw-libtheora.git/plain/mingw-libtheora-1.1.1-rint.patch?id=a35cf93c3068ed4c8a59691a3cf129766d875444 \
"

table_line_append DIST_PRE_BUILD  wxwidgets ":; \
    if path_exists \$BUILD_ROOT/root/include/langinfo.h; then \
        mv \$BUILD_ROOT/root/include/langinfo.h \$BUILD_ROOT/root/include/langinfo.bak; \
    fi;
"

table_line_append DIST_POST_BUILD  wxwidgets ":; \
    if path_exists \$BUILD_ROOT/root/include/langinfo.bak; then \
        mv \$BUILD_ROOT/root/include/langinfo.bak \$BUILD_ROOT/root/include/langinfo.h; \
    fi;
"

if [ "$target_bits" = 32 ]; then
    table_line_append DIST_EXTRA_CFLAGS libvpx -mstackrealign
else
    table_line_append DIST_EXTRA_CFLAGS libvpx -fno-asynchronous-unwind-tables
fi
