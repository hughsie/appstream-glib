AM_CPPFLAGS =						\
	$(GDKPIXBUF_CFLAGS)				\
	$(GLIB_CFLAGS)					\
	$(GTK_CFLAGS)					\
	$(LIBARCHIVE_CFLAGS)				\
	$(SOUP_CFLAGS)					\
	$(RPM_CFLAGS)					\
	$(JSON_GLIB_CFLAGS)				\
	$(FREETYPE_CFLAGS)				\
	-I$(top_srcdir)/libappstream-glib		\
	-I$(top_builddir)/libappstream-glib		\
	-I$(top_srcdir)/libappstream-builder		\
	-I$(top_builddir)/libappstream-builder		\
	-DG_LOG_DOMAIN=\"Asb\"

plugindir = $(libdir)/asb-plugins-$(AS_PLUGIN_VERSION)
plugin_LTLIBRARIES =					\
	libasb_plugin_appdata.la			\
	libasb_plugin_shell_extension.la		\
	libasb_plugin_desktop.la			\
	libasb_plugin_sugar-activity.la			\
	libasb_plugin_gettext.la			\
	libasb_plugin_hardcoded.la

if HAVE_FONTS
plugin_LTLIBRARIES +=					\
	libasb_plugin_font.la
endif

libasb_plugin_gettext_la_SOURCES = asb-plugin-gettext.c
libasb_plugin_gettext_la_LIBADD = $(GLIB_LIBS)
libasb_plugin_gettext_la_LDFLAGS = -module -avoid-version
libasb_plugin_gettext_la_CFLAGS = $(GLIB_CFLAGS) $(WARN_CFLAGS)

libasb_plugin_hardcoded_la_SOURCES = asb-plugin-hardcoded.c
libasb_plugin_hardcoded_la_LIBADD = $(GLIB_LIBS)
libasb_plugin_hardcoded_la_LDFLAGS = -module -avoid-version
libasb_plugin_hardcoded_la_CFLAGS = $(GLIB_CFLAGS) $(WARN_CFLAGS)

libasb_plugin_desktop_la_SOURCES = asb-plugin-desktop.c
libasb_plugin_desktop_la_LIBADD = $(GLIB_LIBS) $(GDKPIXBUF_LIBS)
libasb_plugin_desktop_la_LDFLAGS = -module -avoid-version
libasb_plugin_desktop_la_CFLAGS = $(GLIB_CFLAGS) $(WARN_CFLAGS)

libasb_plugin_sugar_activity_la_SOURCES = asb-plugin-sugar-activity.c
libasb_plugin_sugar_activity_la_LIBADD = $(GLIB_LIBS) $(GDKPIXBUF_LIBS)
libasb_plugin_sugar_activity_la_LDFLAGS = -module -avoid-version
libasb_plugin_sugar_activity_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libasb_plugin_appdata_la_SOURCES = asb-plugin-appdata.c
libasb_plugin_appdata_la_LIBADD = $(GLIB_LIBS) $(GDKPIXBUF_LIBS)
libasb_plugin_appdata_la_LDFLAGS = -module -avoid-version
libasb_plugin_appdata_la_CFLAGS = $(GLIB_CFLAGS) $(WARN_CFLAGS)

libasb_plugin_shell_extension_la_SOURCES = asb-plugin-shell-extension.c
libasb_plugin_shell_extension_la_LIBADD = $(GLIB_LIBS) $(JSON_GLIB_LIBS)
libasb_plugin_shell_extension_la_LDFLAGS = -module -avoid-version
libasb_plugin_shell_extension_la_CFLAGS = $(GLIB_CFLAGS) $(WARN_CFLAGS)

if HAVE_FONTS
libasb_plugin_font_la_SOURCES = asb-plugin-font.c
libasb_plugin_font_la_LIBADD = $(GLIB_LIBS) $(FREETYPE_LIBS) $(GDKPIXBUF_LIBS) $(GTK_LIBS)
libasb_plugin_font_la_LDFLAGS = -module -avoid-version
libasb_plugin_font_la_CFLAGS = $(GLIB_CFLAGS) $(GTK_CFLAGS) $(WARN_CFLAGS)
endif

-include $(top_srcdir)/git.mk
