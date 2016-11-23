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
	appstream-compose				\
	appstream-util

if HAVE_BUILDER
bin_PROGRAMS +=						\
	appstream-builder
endif

appstream_util_SOURCES =				\
	as-util.c
appstream_util_LDADD =					\
	$(AS_GLIB_LIBS)					\
	$(GLIB_LIBS)					\
	$(GDKPIXBUF_LIBS)				\
	$(SOUP_LIBS)					\
	$(LIBARCHIVE_LIBS)
appstream_util_LDFLAGS =				\
	$(RELRO_LDFLAGS)				\
	$(PIE_LDFLAGS)
appstream_util_CFLAGS =					\
	$(WARN_CFLAGS)

if HAVE_BUILDER
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
	$(RELRO_LDFLAGS)				\
	$(PIE_LDFLAGS)
appstream_builder_CFLAGS =				\
	$(WARN_CFLAGS)
endif

appstream_compose_SOURCES =				\
	as-compose.c
appstream_compose_LDADD =				\
	$(AS_GLIB_LIBS)					\
	$(GLIB_LIBS)					\
	$(SOUP_LIBS)					\
	$(GDKPIXBUF_LIBS)				\
	$(LIBARCHIVE_LIBS)
appstream_compose_LDFLAGS =				\
	$(RELRO_LDFLAGS)				\
	$(PIE_LDFLAGS)
appstream_compose_CFLAGS =				\
	$(WARN_CFLAGS)

-include $(top_srcdir)/git.mk
