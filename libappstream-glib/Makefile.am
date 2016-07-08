if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)
endif

AM_CPPFLAGS =							\
	$(GLIB_CFLAGS)						\
	$(GCAB_CFLAGS)						\
	$(GDKPIXBUF_CFLAGS)					\
	$(LIBARCHIVE_CFLAGS)					\
	$(SOUP_CFLAGS)						\
	$(UUID_CFLAGS)						\
	$(YAML_CFLAGS)						\
	-I$(top_srcdir)/libappstream-glib			\
	-I$(top_builddir)/libappstream-glib			\
	-I.							\
	-DAS_COMPILATION					\
	-DTESTDIRSRC=\""$(top_srcdir)/data/tests"\"		\
	-DTESTDIRBUILD=\""$(top_builddir)/data/tests"\"		\
	-DLOCALSTATEDIR=\""$(localstatedir)"\"			\
	-DG_LOG_DOMAIN=\"As\"

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = 						\
	appstream-glib.pc

if HAVE_GPERF
as-tag-private.h: as-tag.gperf
	$(AM_V_GEN) gperf < $< > $@
endif

as-resources.c: appstream-glib.gresource.xml			\
		as-stock-icons.txt				\
		as-license-ids.txt				\
		as-category-ids.txt				\
		as-environment-ids.txt
	$(AM_V_GEN)						\
	glib-compile-resources					\
		--sourcedir=$(srcdir)				\
		--sourcedir=$(top_builddir)/data		\
		--target=$@					\
		--generate-source				\
		--c-name as					\
		$(srcdir)/appstream-glib.gresource.xml
as-resources.h: appstream-glib.gresource.xml			\
		as-stock-icons.txt				\
		as-license-ids.txt				\
		as-category-ids.txt				\
		as-environment-ids.txt
	$(AM_V_GEN)						\
	glib-compile-resources					\
		--sourcedir=$(srcdir)				\
		--sourcedir=$(top_builddir)/data		\
		--target=$@					\
		--generate-header				\
		--c-name as					\
		$(srcdir)/appstream-glib.gresource.xml

BUILT_SOURCES =							\
	as-resources.c						\
	as-resources.h

lib_LTLIBRARIES =						\
	libappstream-glib.la

libappstream_glib_includedir = $(includedir)/libappstream-glib
libappstream_glib_include_HEADERS =				\
	appstream-glib.h					\
	as-app.h						\
	as-app-builder.h					\
	as-bundle.h						\
	as-checksum.h						\
	as-content-rating.h					\
	as-enums.h						\
	as-icon.h						\
	as-image.h						\
	as-inf.h						\
	as-markup.h						\
	as-node.h						\
	as-problem.h						\
	as-profile.h						\
	as-provide.h						\
	as-release.h						\
	as-review.h						\
	as-screenshot.h						\
	as-store.h						\
	as-suggest.h						\
	as-tag.h						\
	as-translation.h					\
	as-utils.h						\
	as-version.h

nodist_libappstream_glib_la_SOURCES =
libappstream_glib_la_SOURCES =					\
	as-app.c						\
	as-app-desktop.c					\
	as-app-builder.c					\
	as-app-inf.c						\
	as-app-private.h					\
	as-app-validate.c					\
	as-bundle.c						\
	as-bundle-private.h					\
	as-checksum.c						\
	as-checksum-private.h					\
	as-content-rating.c					\
	as-content-rating-private.h				\
	as-enums.c						\
	as-icon.c						\
	as-icon-private.h					\
	as-image.c						\
	as-image-private.h					\
	as-inf.c						\
	as-inf.h						\
	as-markup.c						\
	as-monitor.c						\
	as-monitor.h						\
	as-node.c						\
	as-node-private.h					\
	as-problem.c						\
	as-problem.h						\
	as-profile.c						\
	as-profile.h						\
	as-provide.c						\
	as-provide-private.h					\
	as-release.c						\
	as-release-private.h					\
	as-review.c						\
	as-review-private.h					\
	as-resources.c						\
	as-resources.h						\
	as-screenshot.c						\
	as-screenshot-private.h					\
	as-stemmer.c						\
	as-stemmer.h						\
	as-store.c						\
	as-suggest.c						\
	as-suggest-private.h					\
	as-tag.c						\
	as-translation.c					\
	as-translation-private.h				\
	as-utils.c						\
	as-utils-private.h					\
	as-version.h						\
	as-yaml.c						\
	as-yaml.h

if HAVE_GCAB
libappstream_glib_la_SOURCES +=					\
	as-store-cab.c 						\
	as-store-cab.h
endif

if HAVE_GPERF
nodist_libappstream_glib_la_SOURCES += as-tag-private.h
BUILT_SOURCES += as-tag-private.h
endif

CLEANFILES = $(BUILT_SOURCES)

libappstream_glib_la_LIBADD =					\
	$(GLIB_LIBS)						\
	$(GCAB_LIBS)						\
	$(GDKPIXBUF_LIBS)					\
	$(LIBARCHIVE_LIBS)					\
	$(SOUP_LIBS)						\
	$(STEMMER_LIBS)						\
	$(UUID_LIBS)						\
	$(YAML_LIBS)

libappstream_glib_la_LDFLAGS =					\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-export-dynamic						\
	-no-undefined						\
	-export-symbols-regex '^as_.*'

libappstream_glib_la_CFLAGS =					\
	$(WARN_CFLAGS)

check_PROGRAMS =						\
	as-self-test
as_self_test_SOURCES =						\
	as-self-test.c
as_self_test_LDADD =						\
	$(GLIB_LIBS)						\
	$(GDKPIXBUF_LIBS)					\
	$(LIBARCHIVE_LIBS)					\
	$(SOUP_LIBS)						\
	$(STEMMER_LIBS)						\
	$(UUID_LIBS)						\
	$(YAML_LIBS)						\
	$(lib_LTLIBRARIES)
as_self_test_CFLAGS = $(WARN_CFLAGS)

TESTS = as-self-test

if HAVE_INTROSPECTION
introspection_sources =						\
	as-app.c						\
	as-app-validate.c					\
	as-app.h						\
	as-bundle.c						\
	as-bundle.h						\
	as-checksum.c						\
	as-checksum.h						\
	as-content-rating.c					\
	as-content-rating.h					\
	as-enums.c						\
	as-enums.h						\
	as-icon.c						\
	as-icon.h						\
	as-image.c						\
	as-image.h						\
	as-inf.c						\
	as-inf.h						\
	as-markup.c						\
	as-markup.h						\
	as-node.c						\
	as-node.h						\
	as-problem.c						\
	as-problem.h						\
	as-provide.c						\
	as-provide.h						\
	as-release.c						\
	as-release.h						\
	as-review.c						\
	as-review.h						\
	as-screenshot.c						\
	as-screenshot.h						\
	as-store.c						\
	as-store.h						\
	as-suggest.c						\
	as-suggest.h						\
	as-tag.c						\
	as-tag.h						\
	as-translation.c					\
	as-translation.h					\
	as-utils.c						\
	as-utils.h						\
	as-version.h

AppStreamGlib-1.0.gir: libappstream-glib.la
AppStreamGlib_1_0_gir_INCLUDES = GObject-2.0 Gio-2.0 GdkPixbuf-2.0
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
	appstream-glib.gresource.xml				\
	appstream-glib.pc.in					\
	as-category-ids.txt					\
	as-environment-ids.txt					\
	as-license-ids.txt					\
	as-stock-icons.txt					\
	as-tag.gperf						\
	as-version.h.in

massif:
	valgrind --tool=massif --massif-out-file=massif.log .libs/lt-as-self-test;	\
	ms_print massif.log > massif.txt

clean-local:
	rm -f *~
	rm -f $(CLEANFILES)

-include $(top_srcdir)/git.mk
