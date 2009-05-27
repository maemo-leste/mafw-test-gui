#
# configure.ac for MAFW Test GUI Application
# 
# Author: Visa Smolander <visa.smolander@nokia.com>
#
# Copyright (C) 2007, 2008, 2009 Nokia. All rights reserved.

AC_PREREQ(2.53)
AC_INIT(mafw-test-gui, 0.1.2009.22-4, http://www.maemo.org/)
AM_INIT_AUTOMAKE()
AC_CONFIG_SRCDIR(src/main.c)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_ISC_POSIX
AC_PROG_CC
AC_STDC_HEADERS
AC_PROG_LIBTOOL
AC_FUNC_MMAP

GTK_REQUIRED=2.10
MAFW_REQUIRED=0.1

PKG_CHECK_MODULES(GTHREAD, gthread-2.0)
PKG_CHECK_MODULES(HILDON, hildon-1 >= 1.99)
PKG_CHECK_MODULES(GCONF, gconf-2.0 >= 2.0)
PKG_CHECK_MODULES(MAFW, mafw >= $MAFW_REQUIRED
		  mafw-shared >= $MAFW_REQUIRED)

# glib-genmarshal
GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_GENMARSHAL)

# Debugging
AC_ARG_ENABLE([debug],
        AS_HELP_STRING([--enable-debug],
		       [Enable debug messages (default = no)]),
        [AC_DEFINE([MAFW_DEBUG], [1], [Enable extra debug messages])
	 CFLAGS="$CFLAGS -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Werror -O0 -ggdb3"],
        [AC_DEFINE([G_DEBUG_DISABLE], [1], [Disable g_debug calls])
	 CFLAGS="$CFLAGS -O2"])

# Run uninstalled
AC_ARG_ENABLE(uninstalled,
	[  --enable-uninstalled          run uninstalled],,
        enable_uninstalled=no)
if test "x$enable_uninstalled" = "xyes"; then
        AC_MSG_WARN("You want to run the tools uninstalled only? Fine!")
fi

AM_CONDITIONAL([UNINSTALLED], [test "x$enable_uninstalled" = "xyes"])

# Gettext
GETTEXT_PACKAGE=mtg
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, ["$GETTEXT_PACKAGE"], 
  [Define the gettext package to be used])

ALL_LINGUAS=""
AM_GLIB_GNU_GETTEXT
AM_GLIB_DEFINE_LOCALEDIR([LOCALEDIR])

# Makefiles
AC_OUTPUT([
	Makefile
	src/Makefile
	data/Makefile
])
