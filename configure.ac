AC_PREREQ(2.61)
AC_INIT([qmmf-recorder], 1.0.0)
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Check for programs
LT_INIT
AC_PROG_LIBTOOL
AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
PKG_PROG_PKG_CONFIG

AS_CASE([$host],
        [arm*], [ARM=yes],
        [ARM=no]
)

ARM=yes

AC_ARG_WITH(sanitized-headers,
      AS_HELP_STRING([--with-sanitized-headers=DIR],
         [Specify the location of the sanitized Linux headers]),
      [CPPFLAGS="$CPPFLAGS -idirafter $withval"])

AC_ARG_WITH([gralloc_library],
      AC_HELP_STRING([--with-gralloc-library=@<:@dir@:>@],
         [Specify the location of the Gralloc library]),
      [gralloc_libdir=$withval],
      with_gralloc_library=no)

if test "x$with_gralloc_library" != "xno"; then
   AC_SUBST([LIBGRALLOC], "${gralloc_libdir}")
fi

AC_ARG_WITH([mm_core],
      AC_HELP_STRING([with-mm-core=@<:@dir@:>@],
         [Specify the location of the mm-core library]),
      [mm_core_libdir=$withval],
      with_mm_core_library=no)

if test "x$with_mm_core_library" != "xno"; then
   AC_SUBST([MMCORE_CFLAGS], "${mm_core_libdir}")
fi

AC_ARG_WITH([camerahal],
      AC_HELP_STRING([with-camerahal=@<:@dir@:>@],
         [Specify the location of CameraHal]),
      [camerahal_libdir=$withval],
      with_camerahal=no)

if test "x$with_camerahal" != "xno"; then
   AC_SUBST([CAMERAHAL_CFLAGS], "${camerahal_libdir}")
fi

AC_SUBST([CFLAGS])
AC_SUBST([CPPFLAGS])
AC_SUBST([LIBS])

AC_CONFIG_FILES([ \
    Makefile \
    common/overlay/Makefile \
    common/cameraadaptor/Makefile \
    common/audio/src/client/Makefile \
    common/audio/src/service/Makefile \
    common/audio/test/samples/Makefile \
    common/codecadaptor/Makefile \
    recorder/src/client/Makefile \
    recorder/src/service/Makefile \
    display/src/client/Makefile \
    display/src/service/Makefile \
    player/src/client/Makefile \
    player/src/service/Makefile \
    qmmf-server/Makefile \
    qmmf-sdk.pc \
    recorder/test/samples/Makefile \
    recorder/test/gtest/Makefile \
    display/test/samples/Makefile \
    display/test/gtest/Makefile \
    player/test/samples/Makefile \
])
AC_OUTPUT
