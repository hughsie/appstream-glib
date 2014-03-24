if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)
endif

AM_CPPFLAGS =							\
	$(GLIB_CFLAGS)						\
	-I$(top_srcdir)/libappstream-glib			\
	-I$(top_builddir)/libappstream-glib			\
	-I.							\
	-DAS_COMPILATION					\
	-DTESTDATADIR=\""$(top_srcdir)/data/tests"\"		\
	-DG_LOG_DOMAIN=\"As\"

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = 						\
	appstream-glib.pc

as-tag-private.h: as-tag.gperf
	$(AM_V_GEN) gperf < $< > $@

lib_LTLIBRARIES =						\
	libappstream-glib.la

libappstream_glib_includedir = $(includedir)/libappstream-glib
libappstream_glib_include_HEADERS =				\
	appstream-glib.h					\
	as-app.h						\
	as-enums.h						\
	as-image.h						\
	as-node.h						\
	as-release.h						\
	as-screenshot.h						\
	as-store.h						\
	as-tag.h						\
	as-utils.h						\
	as-version.h

libappstream_glib_la_SOURCES =					\
	as-app.c						\
	as-app-private.h					\
	as-enums.c						\
	as-image.c						\
	as-image-private.h					\
	as-node.c						\
	as-node-private.h					\
	as-release.c						\
	as-release-private.h					\
	as-screenshot.c						\
	as-screenshot-private.h					\
	as-store.c						\
	as-tag.c						\
	as-utils.c						\
	as-utils-private.h					\
	as-version.h

if HAVE_GPERF
libappstream_glib_la_SOURCES += as-tag-private.h
endif

BUILT_SOURCES = 						\
	as-tag-private.h

CLEANFILES = $(BUILT_SOURCES)

libappstream_glib_la_LIBADD =					\
	$(GLIB_LIBS)

libappstream_glib_la_LDFLAGS =					\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-export-dynamic						\
	-no-undefined						\
	-export-symbols-regex '^as_.*'

libappstream_glib_la_CFLAGS =					\
	$(WARNINGFLAGS_C)

check_PROGRAMS =						\
	as-self-test
as_self_test_SOURCES =						\
	as-self-test.c
as_self_test_LDADD =						\
	$(GLIB_LIBS)						\
	$(lib_LTLIBRARIES)
as_self_test_CFLAGS = $(WARNINGFLAGS_C)

TESTS = as-self-test

if HAVE_INTROSPECTION
introspection_sources =						\
	$(libappstream_glib_include_HEADERS)			\
	$(libappstream_glib_la_SOURCES)

AppStreamGlib-1.0.gir: libappstream-glib.la
AppStreamGlib_1_0_gir_INCLUDES = GObject-2.0 Gio-2.0
AppStreamGlib_1_0_gir_CFLAGS = $(AM_CPPFLAGS)
AppStreamGlib_1_0_gir_SCANNERFLAGS = --identifier-prefix=As \
				--symbol-prefix=as_ \
				--warn-all \
				--add-include-path=$(srcdir)
AppStreamGlib_1_0_gir_EXPORT_PACKAGES = appstream-glib
AppStreamGlib_1_0_gir_LIBS = libappstream-glib.la
AppStreamGlib_1_0_gir_FILES = $(introspection_sources)
INTROSPECTION_GIRS += AppStreamGlib-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

DISTCLEANFILES =						\
	*.log							\
	*.trs

EXTRA_DIST =							\
	as-tag.gperf						\
	as-version.h.in						\
	appstream-glib.pc.in

clean-local:
	rm -f *~
	rm -f $(CLEANFILES)

-include $(top_srcdir)/git.mk
