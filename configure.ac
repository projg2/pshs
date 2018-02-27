AC_PREREQ([2.60])
AC_INIT([pshs], [0.3.3])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([1.9 foreign dist-bzip2 subdir-objects -Wall])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])

AC_LANG([C++])
AC_PROG_CXX
AC_USE_SYSTEM_EXTENSIONS

AC_CHECK_FUNCS([nl_langinfo])
AC_TYPE_INTMAX_T

PKG_CHECK_MODULES([LIBEVENT], [libevent])

AC_ARG_ENABLE([libmagic],
	[AS_HELP_STRING([--disable-libmagic],
		[Disable use of libmagic])])
AS_IF([test x"$enable_libmagic" != x"no"], [
	AC_CHECK_LIB([magic], [magic_descriptor])
])

AC_ARG_ENABLE([netlink],
	[AS_HELP_STRING([--disable-netlink],
		[Disable getting IP via netlink])])
AS_IF([test x"$enable_netlink" != x"no"], [
	AC_CHECK_LIB([netlink], [rtnl_wilddump_request])

	AC_C_INLINE
	AS_IF([test $ac_cv_lib_netlink_rtnl_wilddump_request = yes], [
		AC_CACHE_CHECK([for 3-argument rtnl_dump_filter()],
				[my_cv_have_rtnl_dump_filter_3arg],
		[
			AC_COMPILE_IFELSE(
			[
				AC_LANG_PROGRAM([
#include <stdio.h>
#include <sys/socket.h>
#include <libnetlink.h>
					],
					[rtnl_dump_filter(0, 0, 0)])
			],
			[
				my_cv_have_rtnl_dump_filter_3arg=yes
			],
			[
				my_cv_have_rtnl_dump_filter_3arg=no
			])
		])

		AS_IF([test $my_cv_have_rtnl_dump_filter_3arg = yes],
		[
			AC_DEFINE([HAVE_RTNL_DUMP_FILTER_3ARG], [1],
				[Define if your rtnl_dump_filter() takes three arguments])
		])

		# newer versions of libnetlink need linking to libmnl
		PKG_CHECK_MODULES([LIBMNL], [libmnl], [], [])
	])
])

AC_ARG_ENABLE([qrencode],
	[AS_HELP_STRING([--disable-qrencode],
		[Disable use of qrencode to print URL qrcodes])])
AS_IF([test x"$enable_qrencode" != x"no"], [
	PKG_CHECK_MODULES([QRENCODE], [libqrencode], [
		AC_DEFINE([HAVE_LIBQRENCODE], [1], [define if you have libqrencode])
	], [
		AC_MSG_WARN([libqrencode not found, qrcode printing disabled])
	])
])

AC_ARG_ENABLE([ssl],
	[AS_HELP_STRING([--disable-ssl],
		[Disable SSL/TLS support])])
AS_IF([test x"$enable_ssl" != x"no"], [
	# libevent 2.1 required because of evhttp_set_bevcb()
	PKG_CHECK_MODULES([LIBSSL], [libssl libcrypto libevent >= 2.1 libevent_openssl], [
		AC_DEFINE([HAVE_LIBSSL], [1], [define if you have libssl])
	], [
		AC_MSG_WARN([openssl or libevent-2.1 not found, SSL support disabled])
	])
])

AC_ARG_ENABLE([upnp],
	[AS_HELP_STRING([--disable-upnp],
		[Disable use of UPnP (via libminiupnpc)])])
AS_IF([test x"$enable_libupnp" != x"no"], [
	AC_CHECK_LIB([miniupnpc], [UPNP_AddPortMapping])
])

AC_ARG_ENABLE([debug],
	[AS_HELP_STRING([--disable-debug],
		[Disable debugging asserts])])
AS_IF([test x"$enable_debug" = x"no"], [
	AC_DEFINE([NDEBUG], [1], [Set this to disable debugging asserts])
])

AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
