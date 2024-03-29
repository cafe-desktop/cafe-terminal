m4_define(mt_major_version, 1)
m4_define(mt_minor_version, 25)
m4_define(mt_micro_version, 1)
m4_define(mt_version, mt_major_version.mt_minor_version.mt_micro_version)

m4_define(mt_api_version, 0)

AC_INIT([cafe-terminal],[mt_version],
        [https://github.com/cafe-desktop/cafe-terminal/issues],
        [cafe-terminal], [https://cafe-desktop.org])
AC_CONFIG_SRCDIR([src/terminal.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.9 foreign no-dist-gzip dist-xz check-news])

AC_SUBST(TERMINAL_MAJOR_VERSION, mt_major_version)
AC_SUBST(TERMINAL_MINOR_VERSION, mt_minor_version)
AC_SUBST(TERMINAL_MICRO_VERSION, mt_micro_version)
AC_SUBST(TERMINAL_API_VERSION, mt_api_version)

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

if test -z "$enable_maintainer_mode"; then
  enable_maintainer_mode=yes
fi
AM_MAINTAINER_MODE([enable])

CAFE_COMMON_INIT
CAFE_DEBUG_CHECK([no])
CAFE_COMPILE_WARNINGS
#CAFE_MAINTAINER_MODE_DEFINES

AC_C_BIGENDIAN

PKG_PROG_PKG_CONFIG
AC_PROG_CC
LT_INIT

GLIB_REQUIRED=2.50.0
GIO_REQUIRED=2.50.0
CTK_REQUIRED=3.22.0
BTE_REQUIRED=0.48
DCONF_REQUIRED=0.13.4

PKG_CHECK_MODULES([TERM],
  [bte-2.91 >= $BTE_REQUIRED
   glib-2.0 >= $GLIB_REQUIRED
   gthread-2.0
   gio-2.0 >= $GIO_REQUIRED
   ctk+-3.0 >= $CTK_REQUIRED
   dconf >= $DCONF_REQUIRED
   libnotify
   x11])

# ********
# smclient
# ********

PKG_CHECK_MODULES(SMCLIENT, sm >= 1.0.0)
AC_SUBST([SMCLIENT_CFLAGS])
AC_SUBST([SMCLIENT_LIBS])

# *****
# GSettings
# *****

GLIB_GSETTINGS

GLIB_GENMARSHAL="$($PKG_CONFIG --variable=glib_genmarshal glib-2.0)"
AC_SUBST([GLIB_GENMARSHAL])
GLIB_MKENUMS="$($PKG_CONFIG --variable=glib_mkenums glib-2.0)"
AC_SUBST([GLIB_MKENUMS])

# **********************
# GLIB_COMPILE_RESOURCES
# **********************
AC_ARG_VAR([GLIB_COMPILE_RESOURCES],[glib-compile-resources bin])
AC_PATH_PROG([GLIB_COMPILE_RESOURCES],[glib-compile-resources],[])
if test -z "$GLIB_COMPILE_RESOURCES"; then
  AC_MSG_ERROR([glib-compile-resources not found])
fi

# *******
# XMLLINT
# *******
AC_ARG_VAR([XMLLINT],[xmllint bin])
AC_PATH_PROG([XMLLINT],[xmllint],[])
if test -z "$XMLLINT"; then
  AC_MSG_ERROR([xmllint not found])
fi

# *****
# S/Key
# *****

AC_MSG_CHECKING([whether to enable s/key support])
AC_ARG_ENABLE([skey],
  [AS_HELP_STRING([--disable-skey],[disable s/key support (default: enabled)])],
  [],[enable_skey=yes])
AC_MSG_RESULT([$enable_skey])

if test "$enable_skey" = "yes"; then
  AC_DEFINE([ENABLE_SKEY],[1],[Define if s/key support is enabled])
fi

AM_CONDITIONAL([ENABLE_SKEY],[test "$enable_skey" = "yes"])

# *************
# Documentation
# *************

YELP_HELP_INIT

# ****************************

# ****************************
AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT_REQUIRE_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE=cafe-terminal
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[Gettext package])

AC_CONFIG_FILES([
Makefile
cafe-terminal.desktop.in
src/Makefile
src/org.cafe.terminal.gschema.xml
src/terminal-version.h
help/Makefile
po/Makefile.in
])

AM_COND_IF([ENABLE_SKEY],
           [AC_CONFIG_FILES([src/skey/Makefile])])

AC_OUTPUT

# ****************************

echo "
	cafe-terminal-$VERSION:

	prefix:                 ${prefix}
	source code location:   ${srcdir}
	compiler:               ${CC}
	cflags:                 ${CFLAGS}
	warning flags:          ${WARN_CFLAGS}
	linker flags:           ${LDFLAGS}

	s/key support:          ${enable_skey}
"
