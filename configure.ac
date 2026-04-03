dnl Process this file with autoconf 2.72 or later to produce
dnl a configure script.
AC_PREREQ([2.72])
AC_INIT
AC_CONFIG_SRCDIR([gdtoa])
AC_CONFIG_SRCDIR([gdtoacfg.h.in])
AC_CONFIG_HEADERS([gdtoacfg.h])
AC_CONFIG_AUX_DIR(autoconf/helpers)

PA_CROSS_COMPILE

AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS

dnl Check for library extension
PA_LIBEXT

dnl Options for debugging and profiling
PA_OPTION_DEBUG
PA_OPTION_PROFILING

AC_CHECK_INCLUDES_DEFAULT
AC_CHECK_HEADERS(float.h)
AC_CHECK_HEADERS(math.h)

AC_CHECK_HEADERS([stdbit.h endian.h sys/endian.h machine/endian.h])
AC_C_BIGENDIAN

AC_TYPE_LONG_DOUBLE
AC_TYPE_LONG_DOUBLE_WIDER

dnl Check for host compiler tools
AC_CHECK_TOOL(AR, ar)
AC_CHECK_TOOL(RANLIB, ranlib, :)

dnl
dnl support function sections (if available)
dnl
PA_OPTION_GC

dnl
dnl If we have gcc, add appropriate code cleanliness options.  Do this
dnl here at the end, because configure relies on being able to use
dnl some very, very old C constructs.
dnl
PA_ADD_CFLAGS([-Wall])
PA_ARG_DISABLED([pedantic],
[disable some extra paranoid compiler warnings],
[],
[PA_ADD_CFLAGS([-W])
PA_ADD_CFLAGS([-pedantic])
])
PA_ADD_CFLAGS([-Wwrite-strings])

dnl Want to turn this on at some point...
PA_ARG_ENABLED([werror],
 [compile with -Werror to error out on any warning],
 [PA_ADD_CFLAGS([-Werror])],
 [PA_ADD_CFLAGS([-Werror=implicit])
  PA_ADD_CFLAGS([-Werror=missing-braces])
  PA_ADD_CFLAGS([-Werror=return-type])
  PA_ADD_CFLAGS([-Werror=trigraphs])
  PA_ADD_CFLAGS([-Werror=pointer-arith])
  PA_ADD_CFLAGS([-Werror=comment])
  PA_ADD_CFLAGS([-Werror=vla])]
)

dnl These warnings are apparently not included in -Wall -W -pedantic
dnl for some bizarre reason.
PA_ADD_CFLAGS([-Werror=strict-prototypes])
PA_ADD_CFLAGS([-Werror=missing-prototypes])
PA_ADD_CFLAGS([-Werror=missing-declarations])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
