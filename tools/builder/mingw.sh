#!/bin/sh

set -e

CROSS_OS=windows

[ -n "$BUILD_ENV" ] && eval "$BUILD_ENV"

BUILD_ENV=$BUILD_ENV$(cat <<EOF

export CFLAGS="$CFLAGS -static-libgcc -static-libstdc++ -static -lpthread"
export CXXFLAGS="$CXXFLAGS -static-libgcc -static-libstdc++ -static -lpthread"
export OBJCXXFLAGS="$OBJCXXFLAGS -static-libgcc -static-libstdc++ -static -lpthread"
export LDFLAGS="$LDFLAGS -static-libgcc -static-libstdc++ -static -lpthread"

export UUID_LIBS="-luuid_mingw -luuid"

EOF
)

: ${HOST_CC:=ccache gcc}
: ${HOST_CXX:=ccache g++}
: ${HOST_CC_ORIG:=gcc}
: ${HOST_CXX_ORIG:=g++}
: ${HOST_STRIP:=strip}

. "$(dirname "$0")/../builder/core.sh"

# make separate roots for target and host for cross compiling

if [ ! -L "$BUILD_ROOT/root" ]; then
    mv "$BUILD_ROOT/root" "$BUILD_ROOT/target"
    mkdir "$BUILD_ROOT/host"
    ln -sf "$BUILD_ROOT/target" "$BUILD_ROOT/root"
    cp -a "$BUILD_ROOT/target/"* "$BUILD_ROOT/host"

    # share these for both roots
    for d in perl5 share etc man doc; do
        rmdir "$BUILD_ROOT/host/$d"
        ln -s "$BUILD_ROOT/target/$d" "$BUILD_ROOT/host/$d"
    done

    mkdir "$BUILD_ROOT/host/bin" "$BUILD_ROOT/target/bin"
fi

ln -sf "$BUILD_ROOT/target" "$BUILD_ROOT/root"

perl_dists="$perl_dists XML-NamespaceSupport XML-SAX-Base XML-SAX"
perl_dists=$(list_remove_duplicates $perl_dists)

host_dists="$host_dists autoconf autoconf-archive automake m4 gsed bison \
                        flex-2.6.3 flex c2man docbook2x ccache"
host_dists=$(list_remove_duplicates $host_dists)

both_dists="$both_dists openssl zlib bzip2 libiconv"

if [ "$os" != windows ]; then
    both_dists="$both_dists libuuid"
fi

both_dists=$(list_remove_duplicates $both_dists)

set_host_env() {
    rm -f "$BUILD_ROOT/root"
    ln -sf "$BUILD_ROOT/host" "$BUILD_ROOT/root"
    if [ -z "$OCC" ]; then
        OCC=$CC
        OCXX=$CXX
        OCC_ORIG=$CC_ORIG
        OCXX_ORIG=$CXX_ORIG
        OSTRIP=$STRIP

        export CC="$HOST_CC"
        export CXX="$HOST_CXX"
        export CC_ORIG="$HOST_CC_ORIG"
        export CXX_ORIG="$HOST_CXX_ORIG"
        export STRIP="$HOST_STRIP"

        OCFLAGS=$CFLAGS OCPPFLAGS=$CPPFLAGS OCXXFLAGS=$CXXFLAGS OOBJCXXFLAGS=$OBJCXXFLAGS OLDFLAGS=$LDFLAGS

        CFLAGS=$(     puts "$CFLAGS"      | sed 's/ -static-libgcc -static-libstdc++ -static -lpthread//g')
        CXXFLAGS=$(   puts "$CXXFLAGS"    | sed 's/ -static-libgcc -static-libstdc++ -static -lpthread//g')
        OBJCXXFLAGS=$(puts "$OBJCXXFLAGS" | sed 's/ -static-libgcc -static-libstdc++ -static -lpthread//g')
        LDFLAGS=$(    puts "$LDFLAGS"     | sed 's/ -static-libgcc -static-libstdc++ -static -lpthread//g')

        OREQUIRED_CONFIGURE_ARGS=$REQUIRED_CONFIGURE_ARGS
        OREQUIRED_CMAKE_ARGS=$REQUIRED_CMAKE_ARGS

        REQUIRED_CONFIGURE_ARGS=$(puts "$REQUIRED_CONFIGURE_ARGS" | sed 's/--host[^ ]*//g')
        REQUIRED_CMAKE_ARGS=$(puts "$REQUIRED_CMAKE_ARGS" | sed 's/-DCMAKE_TOOLCHAIN_FILE=[^ ]*//g')
    fi

    set_host_env_hook 2>/dev/null || :
}

unset_host_env() {
    rm -f "$BUILD_ROOT/root"
    ln -sf "$BUILD_ROOT/target" "$BUILD_ROOT/root"

    if [ -n "$OCC" ]; then
        export CC="$OCC"
        export CXX="$OCXX"
        export CC_ORIG="$OCC_ORIG"
        export CXX_ORIG="$OCXX_ORIG"
        export STRIP="$OSTRIP"
        OCC= OCXX= OCC_ORIG= OCXX_ORIG= OSTRIP=

        export CFLAGS="$OCFLAGS"
        export CPPFLAGS="$OCPPFLAGS"
        export CXXFLAGS="$OCXXFLAGS"
        export OBJCXXFLAGS="$OOBJCXXFLAGS"
        export LDFLAGS="$OLDFLAGS"
        OCFLAGS= OCPPFLAGS= OCXXFLAGS= OOBJCXXFLAGS= OLDFLAGS=

        REQUIRED_CONFIGURE_ARGS=$OREQUIRED_CONFIGURE_ARGS
        REQUIRED_CMAKE_ARGS=$OREQUIRED_CMAKE_ARGS
        OREQUIRED_CONFIGURE_ARGS=
        OREQUIRED_CMAKE_ARGS=
    fi

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

    unset_host_env_hook 2>/dev/null || :
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
            *set_host_env*)
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
    table_line_append  DIST_PRE_BUILD  $dist ':; set_host_env;'
    table_line_replace DIST_POST_BUILD $dist "unset_host_env; $(table_line DIST_POST_BUILD $dist)"
done

for dist in $both_dists; do
    duplicate_dist $dist "${dist}-target"

    table_line_append  DIST_PRE_BUILD  $dist ':; set_host_env;'
    table_line_replace DIST_POST_BUILD $dist "unset_host_env; $(table_line DIST_POST_BUILD $dist)"
done

remove_dists='graphviz python2 python3 swig libxml2-python doxygen bakefile setuptools pip meson XML-Parser intltool ninja libsecret shared-mime-info'

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

table_insert_after DISTS libiconv-target '
    catgets         https://downloads.sourceforge.net/project/mingw/MinGW/Extension/catgets/mingw-catgets-1.0.1/mingw-catgets-1.0.1-src.tar.gz    include/langinfo.h
'

table_line_append DIST_PATCHES catgets "\
    https://gist.githubusercontent.com/rkitover/4fe26d4af9e20234ba7821100356b0a6/raw/715b89f23b0e13a5d1859bfeee600f43edd35c07/mingw-catgets-mc_realloc-and-langinfo.patch \
"

table_line_append DIST_POST_BUILD catgets ":; \
    rm -f \$BUILD_ROOT/root/lib/libcatgets.dll.a \$BUILD_ROOT/root/bin/libcatgets.dll; \
"

table_line_append DIST_PATCHES libgd 'https://gist.githubusercontent.com/rkitover/c64ea5b83ddea94ace58c40c7de42879/raw/fbaf4885fbefb302116b56626c0e191df514e8c6/libgd-2.2.4-mingw-static.patch'

table_insert_before DISTS sfml '
    openal          https://github.com/kcat/openal-soft/archive/openal-soft-1.19.0.tar.gz                      lib/libOpenAL32.a
'

table_line_append DIST_ARGS openal '-DLIBTYPE=STATIC -DALSOFT_UTILS=OFF -DALSOFT_EXAMPLES=OFF -DALSOFT_TESTS=OFF'

table_line_replace DIST_ARGS mp3lame "LDFLAGS='$LDFLAGS $BUILD_ROOT/root/lib/libcatgets.a'"

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

table_line_append DIST_ARGS ffmpeg "--extra-ldflags='-Wl,-allow-multiple-definition' --extra-libs='-lwsock32 -lws2_32 -liphlpapi -lfreetype'"

table_line_append DIST_ARGS gettext "--enable-threads=windows"

table_line_append  DIST_PATCHES glib "\
    https://src.fedoraproject.org/rpms/mingw-glib2/raw/master/f/0001-Use-CreateFile-on-Win32-to-make-sure-g_unlink-always.patch \
    https://src.fedoraproject.org/rpms/mingw-glib2/raw/master/f/glib-formaterror.patch \
    https://src.fedoraproject.org/rpms/mingw-glib2/raw/master/f/glib-include-time-h-for-localtime_r.patch \
    https://src.fedoraproject.org/rpms/mingw-glib2/raw/master/f/glib-ipmreqsource.patch \
    https://src.fedoraproject.org/rpms/mingw-glib2/raw/master/f/glib-prefer-constructors-over-DllMain.patch \
    https://gist.githubusercontent.com/rkitover/2edaf9583fb3068bb14016571e6f7d01/raw/ece80116d5618f372464f02392a9bcab670ce6c1/glib-mingw-no-strerror_s.patch \
"

table_line_append DIST_PATCHES graphite2 "\
    https://raw.githubusercontent.com/Alexpux/MINGW-packages/master/mingw-w64-graphite2/001-graphite2-1.3.8-win64.patch \
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
    if path_exists $BUILD_ROOT/root/include/langinfo.h; then \
        mv $BUILD_ROOT/root/include/langinfo.h $BUILD_ROOT/root/include/langinfo.bak; \
    fi;
"

table_line_append DIST_POST_BUILD  wxwidgets ":; \
    if path_exists $BUILD_ROOT/root/include/langinfo.bak; then \
        mv $BUILD_ROOT/root/include/langinfo.bak $BUILD_ROOT/root/include/langinfo.h; \
    fi;
"
