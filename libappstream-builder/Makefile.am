SUBDIRS = plugins

if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS =					\
	--add-include-path=$(srcdir)				\
	--add-include-path=$(top_builddir)/libappstream-glib
INTROSPECTION_COMPILER_ARGS =					\
	--includedir=$(srcdir)					\
	--includedir=$(top_builddir)/libappstream-glib

endif

AM_CPPFLAGS =							\
	$(GLIB_CFLAGS)						\
	$(GDKPIXBUF_CFLAGS)					\
	$(GTK_CFLAGS)						\
	$(OSTREE_CFLAGS)					\
	$(SOUP_CFLAGS)						\
	-I$(top_srcdir)/libappstream-glib			\
	-I$(top_builddir)/libappstream-glib			\
	-I.							\
	-DAS_COMPILATION					\
	-DTESTDIRSRC=\""$(top_srcdir)/data/tests"\"		\
	-DTESTDIRBUILD=\""$(top_builddir)/data/tests"\"		\
	-DASB_PLUGIN_DIR=\"$(libdir)/asb-plugins\"		\
	-DG_LOG_DOMAIN=\"Asb\"

AS_GLIB_LIBS =							\
	$(top_builddir)/libappstream-glib/libappstream-glib.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = 						\
	appstream-builder.pc

lib_LTLIBRARIES =						\
	libappstream-builder.la

libappstream_builder_includedir = $(includedir)/libappstream-builder
libappstream_builder_include_HEADERS =				\
	appstream-builder.h					\
	asb-app.h

libappstream_builder_la_SOURCES =				\
	asb-app.c						\
	asb-app.h						\
	asb-context.c						\
	asb-context.h						\
	asb-context-private.h					\
	asb-package.c						\
	asb-package-cab.c					\
	asb-package-cab.h					\
	asb-package-deb.c					\
	asb-package-deb.h					\
	asb-package.h						\
	asb-panel.c						\
	asb-panel.h						\
	asb-task.c						\
	asb-task.h						\
	asb-utils.c						\
	asb-utils.h						\
	asb-plugin.c						\
	asb-plugin.h						\
	asb-plugin-loader.c					\
	asb-plugin-loader.h

if HAVE_RPM
libappstream_builder_la_SOURCES +=				\
	asb-package-rpm.c					\
	asb-package-rpm.h
endif

if HAVE_ALPM
libappstream_builder_la_SOURCES +=				\
	asb-package-alpm.c					\
	asb-package-alpm.h
endif

if HAVE_OSTREE
libappstream_builder_la_SOURCES +=				\
	asb-package-ostree.c					\
	asb-package-ostree.h
endif

libappstream_builder_la_LIBADD =				\
	$(AS_GLIB_LIBS)						\
	$(RPM_LIBS)						\
	$(ALPM_LIBS)						\
	$(OSTREE_LIBS)						\
	$(GLIB_LIBS)						\
	$(GTK_LIBS)						\
	$(LIBARCHIVE_LIBS)

libappstream_builder_la_LDFLAGS =				\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-export-dynamic						\
	-no-undefined						\
	-export-symbols-regex '^asb_.*'

libappstream_builder_la_CFLAGS =				\
	$(WARNINGFLAGS_C)

check_PROGRAMS =						\
	asb-self-test
asb_self_test_SOURCES =						\
	asb-self-test.c
asb_self_test_LDADD =						\
	$(AS_GLIB_LIBS)						\
	$(GLIB_LIBS)						\
	$(GDKPIXBUF_LIBS)					\
	$(lib_LTLIBRARIES)
asb_self_test_CFLAGS = $(WARNINGFLAGS_C)

TESTS = asb-self-test

if HAVE_INTROSPECTION
introspection_sources =						\
	asb-app.c						\
	asb-app.h						\
	asb-context.c						\
	asb-context.h						\
	asb-context-private.h					\
	asb-package.c						\
	asb-package.h						\
	asb-task.c						\
	asb-task.h

AppStreamBuilder-1.0.gir: libappstream-builder.la
AppStreamBuilder_1_0_gir_INCLUDES =				\
	AppStreamGlib-1.0					\
	GdkPixbuf-2.0						\
	Gio-2.0							\
	GObject-2.0
AppStreamBuilder_1_0_gir_CFLAGS = $(AM_CPPFLAGS)
AppStreamBuilder_1_0_gir_SCANNERFLAGS = --identifier-prefix=Asb	\
				--symbol-prefix=asb_		\
				--warn-all			\
				--add-include-path=$(srcdir)
AppStreamBuilder_1_0_gir_EXPORT_PACKAGES = appstream-builder
AppStreamBuilder_1_0_gir_LIBS =					\
	$(AS_GLIB_LIBS)						\
	libappstream-builder.la					\
	archive
if HAVE_RPM
AppStreamBuilder_1_0_gir_LIBS +=				\
	rpmio 							\
	rpm
endif
AppStreamBuilder_1_0_gir_FILES = $(introspection_sources)
INTROSPECTION_GIRS += AppStreamBuilder-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES = $(gir_DATA) $(typelib_DATA)
endif

DISTCLEANFILES =						\
	*.log							\
	*.trs

EXTRA_DIST =							\
	appstream-builder.pc.in

clean-local:
	rm -f *~
	rm -f $(CLEANFILES)

-include $(top_srcdir)/git.mk
