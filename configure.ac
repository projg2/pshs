AC_PREREQ([2.60])
AC_INIT([pshs], [0])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([1.6 foreign dist-bzip2 -Wall])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])

AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS

PKG_CHECK_MODULES([LIBEVENT], [libevent])

AC_ARG_ENABLE([debug],
	[AS_HELP_STRING([--disable-debug],
		[Disable debugging asserts])])
AS_IF([test x"$enable_debug" = x"no"], [
	AC_DEFINE([NDEBUG], [1], [Set this to disable debugging asserts])
])

AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
