AM_CPPFLAGS =						\
	$(PIE_CFLAGS)					\
	$(GLIB_CFLAGS)					\
	$(GDKPIXBUF_CFLAGS)				\
	$(LIBARCHIVE_CFLAGS)				\
	$(SOUP_CFLAGS)					\
	$(OSTREE_CFLAGS)				\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-I$(top_srcdir)/libappstream-glib		\
	-I$(top_builddir)/libappstream-glib		\
	-I$(top_srcdir)/libappstream-builder		\
	-I$(top_builddir)/libappstream-builder		\
	-DG_LOG_DOMAIN=\"As\"				\
	-DVERSION="\"$(VERSION)\""			\
	-DLOCALEDIR=\""$(localedir)"\"

AS_GLIB_LIBS =						\
	$(top_builddir)/libappstream-glib/libappstream-glib.la
AS_BUILDER_LIBS =					\
	$(top_builddir)/libappstream-builder/libappstream-builder.la

bin_PROGRAMS =						\
	appstream-builder				\
	appstream-util

appstream_util_SOURCES =				\
	as-util.c
appstream_util_LDADD =					\
	$(AS_GLIB_LIBS)					\
	$(GLIB_LIBS)					\
	$(GDKPIXBUF_LIBS)				\
	$(SOUP_LIBS)					\
	$(LIBARCHIVE_LIBS)
appstream_util_LDFLAGS =				\
	$(PIE_LDFLAGS)
appstream_util_CFLAGS =					\
	$(WARNINGFLAGS_C)

appstream_builder_SOURCES =				\
	as-builder.c
appstream_builder_LDADD =				\
	$(AS_GLIB_LIBS)					\
	$(AS_BUILDER_LIBS)				\
	$(GLIB_LIBS)					\
	$(OSTREE_LIBS)					\
	$(SOUP_LIBS)					\
	$(LIBARCHIVE_LIBS)
appstream_builder_LDFLAGS =				\
	$(PIE_LDFLAGS)
appstream_builder_CFLAGS =				\
	$(WARNINGFLAGS_C)

# ship this obsolete binary
bin_PROGRAMS +=						\
	appdata-validate
appdata_validate_SOURCES =				\
	as-validate.c
appdata_validate_LDADD =				\
	$(AS_GLIB_LIBS)					\
	$(GLIB_LIBS)
appdata_validate_CFLAGS =				\
	$(WARNINGFLAGS_C)

-include $(top_srcdir)/git.mk
