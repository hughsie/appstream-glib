AC_PREREQ(2.63)

m4_define([as_major_version], [0])
m4_define([as_minor_version], [4])
m4_define([as_micro_version], [0])
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

# this refers to the plugin API version
# this is not in any way related to a package or soname version
AS_PLUGIN_VERSION=2
AC_SUBST(AS_PLUGIN_VERSION)
AC_DEFINE_UNQUOTED([AS_PLUGIN_VERSION], "$AS_PLUGIN_VERSION", [plugin API version])

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
LT_CURRENT=7
LT_REVISION=5
LT_AGE=0
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_HEADER_STDC
LT_INIT
AM_PROG_CC_C_O

# internationalization
GETTEXT_PACKAGE=appstream-glib
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Package name for gettext])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.17])
IT_PROG_INTLTOOL([0.40.0])

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

# check for PIE (position independent executable) support
AX_CHECK_COMPILE_FLAG([-fPIE],
		      [AX_CHECK_LINK_FLAG([-fPIE -pie],
					  [PIE_CFLAGS="-fPIE" PIE_LDFLAGS="-pie"])])
AC_SUBST(PIE_CFLAGS)
AC_SUBST(PIE_LDFLAGS)

# bash-completion
AC_ARG_WITH([bashcompletiondir],
            AS_HELP_STRING([--with-bashcompletiondir=DIR], [Bash completions directory]),
            [],
            [AS_IF([$($PKG_CONFIG --exists bash-completion)], [
                with_bashcompletiondir=$($PKG_CONFIG --variable=completionsdir bash-completion)
            ] , [
                with_bashcompletiondir=${datadir}/bash-completion/completions
            ])])
AC_SUBST([bashcompletiondir], [$with_bashcompletiondir])


# use gperf for faster string -> enum matching
AC_PATH_PROG(GPERF, [gperf], [no])
if test x$GPERF != xno ; then
	AC_DEFINE(HAVE_GPERF,[1], [Use gperf])
fi
AM_CONDITIONAL(HAVE_GPERF, [test x$GPERF != xno])

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.16.1 gio-2.0 gobject-2.0 gthread-2.0 gio-unix-2.0 gmodule-2.0)
PKG_CHECK_MODULES(LIBARCHIVE, libarchive)
PKG_CHECK_MODULES(SOUP, libsoup-2.4 >= 2.24)
PKG_CHECK_MODULES(GDKPIXBUF, gdk-pixbuf-2.0 >= 2.14)

# builder (default enabled)
AC_ARG_ENABLE(builder, AS_HELP_STRING([--disable-builder],[Disable AppStream builder support]), enable_builder=$enableval)
if test x$enable_builder != xno; then
	PKG_CHECK_MODULES(GTK, gtk+-3.0)
	PKG_CHECK_MODULES(SQLITE, sqlite3)
	PKG_CHECK_MODULES(FREETYPE, pango fontconfig freetype2 >= 9.10.0)
	AC_PATH_PROG(GCAB, [gcab], [no])
	if test x$GCAB = "xno" ; then
		AC_MSG_ERROR([gcab program not found])
	fi

	# ostree (default enabled)
	AC_ARG_ENABLE(ostree, AS_HELP_STRING([--disable-ostree],[Disable ostree support]), enable_ostree=$enableval)
	if test x$enable_ostree != xno; then
		PKG_CHECK_MODULES(OSTREE, ostree-1 >= 2015.1, HAVE_OSTREE="yes", HAVE_OSTREE="no")
		if test "x$HAVE_OSTREE" = "xyes"; then
			AC_DEFINE(HAVE_OSTREE, 1, [define if OSTREE is installed])
		else
			if test x$enable_ostree = xyes; then
				AC_MSG_ERROR([ostree enabled but not found])
			fi
		fi
	else
		HAVE_OSTREE=no
	fi

	# rpm (default enabled)
	AC_ARG_ENABLE(rpm, AS_HELP_STRING([--disable-rpm],[Disable rpm support]), enable_rpm=$enableval)
	if test x$enable_rpm != xno; then
		PKG_CHECK_MODULES(RPM, rpm, HAVE_RPM="yes", HAVE_RPM="no")
		if test "x$HAVE_RPM" = "xyes"; then
			AC_DEFINE(HAVE_RPM, 1, [define if RPM is installed])
		else
			if test x$enable_rpm = xyes; then
				AC_MSG_ERROR([rpm enabled but not found])
			fi
		fi
	else
		HAVE_RPM=no
	fi

	# alpm (default disabled)
	AC_ARG_ENABLE(alpm, AS_HELP_STRING([--enable-alpm],[Enable alpm support]), enable_alpm=yes, enable_alpm=no)
	if test x$enable_alpm != xno; then
		PKG_CHECK_MODULES(ALPM, libalpm, HAVE_ALPM="yes", HAVE_ALPM="no")
		if test "x$HAVE_ALPM" = "xyes"; then
			AC_DEFINE(HAVE_ALPM, 1, [define if ALPM is installed])
		else
			if test x$enable_alpm = xyes; then
				AC_MSG_ERROR([alpm enabled but not found])
			fi
		fi
	else
		HAVE_ALPM=no
	fi
	enable_builder=yes
else
	enable_builder=no
fi
AM_CONDITIONAL(HAVE_BUILDER, test x$enable_builder = xyes)
AM_CONDITIONAL(HAVE_RPM, test x$HAVE_RPM = xyes)
AM_CONDITIONAL(HAVE_OSTREE, test x$HAVE_OSTREE = xyes)
AM_CONDITIONAL(HAVE_ALPM, test x$HAVE_ALPM = xyes)

# man files
AC_ARG_ENABLE(man,
              [AS_HELP_STRING([--disable-man],
                              [Disable building man page])],,
              enable_man=maybe)
AS_IF([test "x$enable_man" != "xno"],
  [AC_CHECK_PROG([have_xsltproc], [xsltproc], [yes], [no])
   JH_CHECK_XML_CATALOG([-//OASIS//DTD DocBook XML V4.3//EN],
     [DocBook XML DTD V4.3], [have_docbookdtd=yes], [have_docbookdtd=no])
   JH_CHECK_XML_CATALOG(
     [http://docbook.sourceforge.net/release/xsl/current/html/refentry.xsl],
     [DocBook XSL Stylesheets], [have_docbookxsl=yes], [have_docbookxsl=no])
   AS_IF([test "x$have_xsltproc" = "xyes" -a "x$have_docbookdtd" = "xyes" \
     -a "x$have_docbookxsl" = "xyes"],
     [have_manutils=yes],
     [AS_IF([test "x$enable_man" = "xyes"],
       [AC_MSG_ERROR(
         [manpage generation requested but required utilities were not found])])
      have_manutils=no])],
  [have_manutils=no])
AM_CONDITIONAL([ENABLE_MAN], [test "x$have_manutils" = "xyes"])

dnl ---------------------------------------------------------------------------
dnl - Use libyaml for DEP-11 support
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(dep11, AS_HELP_STRING([--enable-dep11],[enable DEP-11]),
              enable_dep11=$enableval,enable_dep11=yes)
AM_CONDITIONAL(HAVE_DEP11, test x$enable_dep11 = xyes)
if test x$enable_dep11 = xyes; then
  AC_CHECK_HEADER(yaml.h, [], [AC_MSG_ERROR([No yaml.h])])
  YAML_LIBS="-lyaml"
  AC_SUBST(YAML_LIBS)
  AC_DEFINE(AS_BUILD_DEP11,1,[Build DEP-11 code])
fi

AC_CONFIG_FILES([
Makefile
client/Makefile
libappstream-builder/Makefile
libappstream-builder/appstream-builder.pc
libappstream-builder/plugins/Makefile
libappstream-glib/Makefile
libappstream-glib/appstream-glib.pc
libappstream-glib/as-version.h
data/Makefile
data/installed-tests/Makefile
data/tests/Makefile
docs/Makefile
docs/api/Makefile
docs/api/version.xml
po/Makefile.in
])
AC_OUTPUT
AC_MSG_RESULT([
    $PACKAGE_NAME $VERSION

    prefix:                  ${prefix}
    sysconf dir:             ${sysconfdir}
    datarootdir:             ${datarootdir}
    includedir:              ${includedir}
    lib dir:                 ${libdir}
    DEP-11 support:          ${enable_dep11}
    Builder support:         ${enable_builder}
    Bash completion dir:     ${with_bashcompletiondir}
              ])
