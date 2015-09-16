AM_CPPFLAGS =						\
	$(GDKPIXBUF_CFLAGS)				\
	$(GLIB_CFLAGS)					\
	$(GTK_CFLAGS)					\
	$(LIBARCHIVE_CFLAGS)				\
	$(SOUP_CFLAGS)					\
	$(RPM_CFLAGS)					\
	$(SQLITE_CFLAGS)				\
	$(FREETYPE_CFLAGS)				\
	-I$(top_srcdir)/libappstream-glib		\
	-I$(top_builddir)/libappstream-glib		\
	-I$(top_srcdir)/libappstream-builder		\
	-I$(top_builddir)/libappstream-builder		\
	-DG_LOG_DOMAIN=\"Asb\"

plugindir = $(libdir)/asb-plugins-$(AS_PLUGIN_VERSION)
plugin_LTLIBRARIES =					\
	libasb_plugin_absorb.la				\
	libasb_plugin_appdata.la			\
	libasb_plugin_blacklist.la			\
	libasb_plugin_dbus.la				\
	libasb_plugin_firmware.la			\
	libasb_plugin_desktop.la			\
	libasb_plugin_gir.la				\
	libasb_plugin_gresource.la			\
	libasb_plugin_font.la				\
	libasb_plugin_gettext.la			\
	libasb_plugin_gstreamer.la			\
	libasb_plugin_hardcoded.la			\
	libasb_plugin_kde_services.la			\
	libasb_plugin_kde_notifyrc.la			\
	libasb_plugin_nm.la

if HAVE_OSTREE
plugin_LTLIBRARIES +=					\
	libasb_plugin_ostree.la
endif

libasb_plugin_dbus_la_SOURCES = asb-plugin-dbus.c
libasb_plugin_dbus_la_LIBADD = $(GLIB_LIBS)
libasb_plugin_dbus_la_LDFLAGS = -module -avoid-version
libasb_plugin_dbus_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libasb_plugin_firmware_la_SOURCES = asb-plugin-firmware.c
libasb_plugin_firmware_la_LIBADD = $(GLIB_LIBS)
libasb_plugin_firmware_la_LDFLAGS = -module -avoid-version
libasb_plugin_firmware_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libasb_plugin_absorb_la_SOURCES = asb-plugin-absorb.c
libasb_plugin_absorb_la_LIBADD = $(GLIB_LIBS)
libasb_plugin_absorb_la_LDFLAGS = -module -avoid-version
libasb_plugin_absorb_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libasb_plugin_kde_services_la_SOURCES = asb-plugin-kde-services.c
libasb_plugin_kde_services_la_LIBADD = $(GLIB_LIBS)
libasb_plugin_kde_services_la_LDFLAGS = -module -avoid-version
libasb_plugin_kde_services_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libasb_plugin_kde_notifyrc_la_SOURCES = asb-plugin-kde-notifyrc.c
libasb_plugin_kde_notifyrc_la_LIBADD = $(GLIB_LIBS)
libasb_plugin_kde_notifyrc_la_LDFLAGS = -module -avoid-version
libasb_plugin_kde_notifyrc_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libasb_plugin_gstreamer_la_SOURCES = asb-plugin-gstreamer.c
libasb_plugin_gstreamer_la_LIBADD = $(GLIB_LIBS)
libasb_plugin_gstreamer_la_LDFLAGS = -module -avoid-version
libasb_plugin_gstreamer_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libasb_plugin_gettext_la_SOURCES = asb-plugin-gettext.c
libasb_plugin_gettext_la_LIBADD = $(GLIB_LIBS)
libasb_plugin_gettext_la_LDFLAGS = -module -avoid-version
libasb_plugin_gettext_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libasb_plugin_blacklist_la_SOURCES = asb-plugin-blacklist.c
libasb_plugin_blacklist_la_LIBADD = $(GLIB_LIBS)
libasb_plugin_blacklist_la_LDFLAGS = -module -avoid-version
libasb_plugin_blacklist_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libasb_plugin_hardcoded_la_SOURCES = asb-plugin-hardcoded.c
libasb_plugin_hardcoded_la_LIBADD = $(GLIB_LIBS)
libasb_plugin_hardcoded_la_LDFLAGS = -module -avoid-version
libasb_plugin_hardcoded_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libasb_plugin_desktop_la_SOURCES = asb-plugin-desktop.c
libasb_plugin_desktop_la_LIBADD = $(GLIB_LIBS) $(GDKPIXBUF_LIBS)
libasb_plugin_desktop_la_LDFLAGS = -module -avoid-version
libasb_plugin_desktop_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libasb_plugin_gir_la_SOURCES = asb-plugin-gir.c
libasb_plugin_gir_la_LIBADD = $(GLIB_LIBS) $(GDKPIXBUF_LIBS)
libasb_plugin_gir_la_LDFLAGS = -module -avoid-version
libasb_plugin_gir_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libasb_plugin_gresource_la_SOURCES = asb-plugin-gresource.c
libasb_plugin_gresource_la_LIBADD = $(GLIB_LIBS) $(GDKPIXBUF_LIBS)
libasb_plugin_gresource_la_LDFLAGS = -module -avoid-version
libasb_plugin_gresource_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libasb_plugin_appdata_la_SOURCES = asb-plugin-appdata.c
libasb_plugin_appdata_la_LIBADD = $(GLIB_LIBS) $(GDKPIXBUF_LIBS)
libasb_plugin_appdata_la_LDFLAGS = -module -avoid-version
libasb_plugin_appdata_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libasb_plugin_nm_la_SOURCES = asb-plugin-nm.c
libasb_plugin_nm_la_LIBADD = $(GLIB_LIBS)
libasb_plugin_nm_la_LDFLAGS = -module -avoid-version
libasb_plugin_nm_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)

libasb_plugin_font_la_SOURCES = asb-plugin-font.c
libasb_plugin_font_la_LIBADD = $(GLIB_LIBS) $(FREETYPE_LIBS) $(GDKPIXBUF_LIBS) $(GTK_LIBS)
libasb_plugin_font_la_LDFLAGS = -module -avoid-version
libasb_plugin_font_la_CFLAGS = $(GLIB_CFLAGS) $(GTK_CFLAGS) $(WARNINGFLAGS_C)

if HAVE_OSTREE
libasb_plugin_ostree_la_SOURCES = asb-plugin-ostree.c
libasb_plugin_ostree_la_LIBADD = $(GLIB_LIBS) $(FREETYPE_LIBS) $(GDKPIXBUF_LIBS)
libasb_plugin_ostree_la_LDFLAGS = -module -avoid-version
libasb_plugin_ostree_la_CFLAGS = $(GLIB_CFLAGS) $(WARNINGFLAGS_C)
endif

-include $(top_srcdir)/git.mk
