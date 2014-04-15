AM_CPPFLAGS =						\
	$(PIE_CFLAGS)					\
	$(GLIB_CFLAGS)					\
	$(LIBARCHIVE_CFLAGS)				\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-I$(top_srcdir)/libappstream-glib		\
	-DG_LOG_DOMAIN=\"As\"				\
	-DVERSION="\"$(VERSION)\""			\
	-DLOCALEDIR=\""$(localedir)"\"

AS_GLIB_LIBS =						\
	$(top_builddir)/libappstream-glib/libappstream-glib.la

bin_PROGRAMS =						\
	appstream-util

appstream_util_SOURCES =				\
	as-util.c
appstream_util_LDADD =					\
	$(AS_GLIB_LIBS)					\
	$(GLIB_LIBS)					\
	$(LIBARCHIVE_LIBS)
appstream_util_LDFLAGS =				\
	$(PIE_LDFLAGS)
appstream_util_CFLAGS =					\
	$(WARNINGFLAGS_C)

-include $(top_srcdir)/git.mk
