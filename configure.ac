AC_PREREQ(2.63)

m4_define([as_major_version], [0])
m4_define([as_minor_version], [1])
m4_define([as_micro_version], [2])
m4_define([as_version],
          [as_major_version.as_minor_version.as_micro_version])

AC_INIT([appstream-glib],[as_version])
AC_CONFIG_SRCDIR(libappstream-glib)
AM_INIT_AUTOMAKE([1.9 no-dist-gzip dist-xz tar-ustar foreign])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_SYS_LARGEFILE

GOBJECT_INTROSPECTION_CHECK([0.9.8])

AS_MAJOR_VERSION=as_major_version
AS_MINOR_VERSION=as_minor_version
AS_MICRO_VERSION=as_micro_version
AS_VERSION=as_version
AC_SUBST(AS_MAJOR_VERSION)
AC_SUBST(AS_MINOR_VERSION)
AC_SUBST(AS_MICRO_VERSION)
AC_SUBST(AS_VERSION)

# enable nice build output on automake1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# libtool versioning - this applies to libpackagekit
#
# See http://sources.redhat.com/autobook/autobook/autobook_91.html#SEC91 for details
#
# - If interfaces have been changed or added, but binary compatibility
#   has been preserved, change:
#      CURRENT += 1
#      REVISION = 0
#      AGE += 1
# - If binary compatibility has been broken (eg removed or changed
#   interfaces), change:
#      CURRENT += 1
#      REVISION = 0
#      AGE = 0
# - If the interface is the same as the previous version, but bugs are
#   fixed, change:
#      REVISION += 1
LT_CURRENT=3
LT_REVISION=0
LT_AGE=2
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_HEADER_STDC
LT_INIT
AM_PROG_CC_C_O

# set up gtk-doc
GTK_DOC_CHECK(1.9)

if test "$GCC" = "yes"; then
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wall"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wcast-align -Wno-uninitialized"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-declarations"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wredundant-decls"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wpointer-arith"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wcast-align"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wwrite-strings"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Winit-self"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wreturn-type"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wformat-nonliteral"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wformat-security"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-include-dirs"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-format-attribute"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wsign-compare"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wtype-limits"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wuninitialized"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Waggregate-return"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wdeclaration-after-statement"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wshadow"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wno-strict-aliasing"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Winline"
else
	WARNINGFLAGS_C=""
fi
AC_SUBST(WARNINGFLAGS_C)

# use gperf for faster string -> enum matching
AC_PATH_PROG(GPERF, [gperf], [no])
if test x$GPERF != xno ; then
	AC_DEFINE(HAVE_GPERF,[1], [Use gperf])
fi
AM_CONDITIONAL(HAVE_GPERF, [test x$GPERF != xno])

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.16.1 gio-2.0 gobject-2.0 gthread-2.0)

AC_CONFIG_FILES([
Makefile
libappstream-glib/Makefile
libappstream-glib/appstream-glib.pc
libappstream-glib/as-version.h
data/Makefile
data/tests/Makefile
docs/Makefile
docs/api/Makefile
docs/api/version.xml
])
AC_OUTPUT
