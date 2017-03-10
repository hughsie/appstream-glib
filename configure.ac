AC_PREREQ(2.63)

m4_define([as_major_version], [0])
m4_define([as_minor_version], [6])
m4_define([as_micro_version], [11])
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
AS_PLUGIN_VERSION=5
AC_SUBST(AS_PLUGIN_VERSION)
AC_DEFINE_UNQUOTED([AS_PLUGIN_VERSION], "$AS_PLUGIN_VERSION", [plugin API version])

# enable nice build output on automake1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# libtool versioning - this applies to libappstream-glib
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
LT_CURRENT=8
LT_REVISION=11
LT_AGE=0
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

AS_ALL_LINGUAS
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_HEADER_STDC
LT_INIT
AM_PROG_CC_C_O

# We use C99 features
AC_PROG_CC_C99
AS_IF([test "$ac_cv_prog_cc_c99" = "no"],[AC_MSG_ERROR([C99 support is required])])

WARN_CFLAGS_EXTRA="
	-Waggregate-return
	-Warray-bounds
	-Wcast-align
	-Wclobbered
	-Wconversion
	-Wdeclaration-after-statement
	-Wempty-body
	-Wextra
	-Wformat=2
	-Wformat-nonliteral
	-Wformat-security
	-Wformat-signedness
	-Wignored-qualifiers
	-Wimplicit-function-declaration
	-Winit-self
	-Winline
	-Wmissing-declarations
	-Wmissing-format-attribute
	-Wmissing-include-dirs
	-Wmissing-noreturn
	-Wmissing-parameter-type
	-Wmissing-prototypes
	-Wnested-externs
	-Wno-discarded-qualifiers
	-Wno-missing-field-initializers
	-Wno-strict-aliasing
	-Wno-suggest-attribute=format
	-Wno-unused-parameter
	-Wold-style-definition
	-Woverride-init
	-Wpacked
	-Wpointer-arith
	-Wredundant-decls
	-Wreturn-type
	-Wshadow
	-Wsign-compare
	-Wstrict-aliasing
	-Wstrict-prototypes
	-Wswitch-default
	-Wtype-limits
	-Wundef
	-Wuninitialized
	-Wunused
	-Wunused-but-set-variable
	-Wwrite-strings"
AX_APPEND_COMPILE_FLAGS([$WARN_CFLAGS_EXTRA], [WARN_CFLAGS])
AC_SUBST(WARN_CFLAGS)

# internationalization
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.7])
AC_SUBST([GETTEXT_PACKAGE], [appstream-glib])
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Package name for gettext])

# set up gtk-doc
GTK_DOC_CHECK(1.9)

# check for PIE (position independent executable) support
AX_CHECK_COMPILE_FLAG([-fPIE],
		      [AX_CHECK_LINK_FLAG([-fPIE -pie],
					  [PIE_CFLAGS="-fPIE" PIE_LDFLAGS="-pie"])])
AC_SUBST(PIE_CFLAGS)
AC_SUBST(PIE_LDFLAGS)

# check for full RELRO (relocation read-only) support
AX_CHECK_LINK_FLAG([-Wl,-z,relro,-z,now],
		   [RELRO_LDFLAGS="-Wl,-z,relro,-z,now"])
AC_SUBST([RELRO_LDFLAGS])

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
if test x$GPERF = xno ; then
	AC_MSG_ERROR([gperf not found])
fi

GPERF_TEST="$(echo foo,bar | ${GPERF} -L ANSI-C)"
AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM([
                #include <string.h>
                const char * in_word_set(const char *, size_t);
                $GPERF_TEST]
        )],
        [GPERF_LEN_TYPE=size_t],
        [AC_COMPILE_IFELSE(
                [AC_LANG_PROGRAM([
                        #include <string.h>
                        const char * in_word_set(const char *, unsigned);
                        $GPERF_TEST]
                )],
                [GPERF_LEN_TYPE=unsigned],
                [AC_MSG_ERROR([unable to determine gperf len type])]
        )]
)
AC_DEFINE_UNQUOTED([GPERF_LEN_TYPE], [$GPERF_LEN_TYPE], [gperf len type])

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.45.8 gio-2.0 gobject-2.0 gthread-2.0 gio-unix-2.0 gmodule-2.0)
PKG_CHECK_MODULES(UUID, uuid)
PKG_CHECK_MODULES(LIBARCHIVE, libarchive)
PKG_CHECK_MODULES(SOUP, libsoup-2.4 >= 2.51.92)
PKG_CHECK_MODULES(JSON_GLIB, json-glib-1.0 >= 1.1.1)
PKG_CHECK_MODULES(GDKPIXBUF, gdk-pixbuf-2.0 >= 2.31.5)

# firmware support (default enabled)
AC_ARG_ENABLE(firmware, AS_HELP_STRING([--disable-firmware],[Disable cabinet archive support]), enable_firmware=$enableval)
if test x$enable_firmware != xno; then
	PKG_CHECK_MODULES(GCAB, libgcab-1.0)
	AC_DEFINE(HAVE_GCAB,[1], [Use gcab])
	enable_firmware=yes
	GCAB_DEPENDENCY=libgcab-1.0
else
	enable_firmware=no
	GCAB_DEPENDENCY=
fi
AM_CONDITIONAL(HAVE_GCAB, test x$enable_firmware = xyes)
AC_SUBST([GCAB_DEPENDENCY])

# font support (default enabled)
AC_ARG_ENABLE(fonts, AS_HELP_STRING([--disable-fonts],[Disable font support]), enable_fonts=$enableval)
if test x$enable_fonts != xno; then
	AC_DEFINE(HAVE_FONTS,[1], [Use fonts])
	enable_fonts=yes
else
	enable_fonts=no
fi
AM_CONDITIONAL(HAVE_FONTS, test x$enable_fonts = xyes)

# builder (default enabled)
AC_ARG_ENABLE(builder, AS_HELP_STRING([--disable-builder],[Disable AppStream builder support]), enable_builder=$enableval)
if test x$enable_builder != xno; then

	# only for fonts */
	if test x$enable_fonts != xno; then
		PKG_CHECK_MODULES(GTK, gtk+-3.0)
		PKG_CHECK_MODULES(FREETYPE, pango fontconfig freetype2 >= 9.10.0)
	fi

	# only for firmware */
	if test x$enable_firmware != xno; then
		AC_PATH_PROG(GCAB, [gcab], [no])
		if test x$GCAB = "xno" ; then
			AC_MSG_ERROR([gcab program not found])
		fi
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

dnl ---------------------------------------------------------------------------
dnl - Use libstemmer for search stemming
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(stemmer, AS_HELP_STRING([--enable-stemmer],[enable search stemmer]),
              enable_stemmer=$enableval,enable_stemmer=no)
AM_CONDITIONAL(HAVE_LIBSTEMMER, test x$enable_stemmer = xyes)
if test x$enable_stemmer = xyes; then
  AC_CHECK_HEADER(libstemmer.h, [], [AC_MSG_ERROR([No libstemmer.h])])
  STEMMER_LIBS="-lstemmer"
  AC_SUBST(STEMMER_LIBS)
  AC_DEFINE(HAVE_LIBSTEMMER,1,[define if libstemmer is installed])
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
    Stemming support:        ${enable_stemmer}
    Builder support:         ${enable_builder}
    Firmware support:        ${enable_firmware}
    Fonts support:           ${enable_fonts}
    Bash completion dir:     ${with_bashcompletiondir}
              ])
