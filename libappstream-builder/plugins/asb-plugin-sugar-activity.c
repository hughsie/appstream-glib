/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016 Sam Parkinson <sam@sam.today>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <config.h>
#include <string.h>
#include <fnmatch.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <asb-plugin.h>

#define __APPSTREAM_GLIB_PRIVATE_H
#include <as-utils-private.h>
#include <as-app-private.h>

/**
 * asb_plugin_get_name:
 */
const gchar *
asb_plugin_get_name (void)
{
	return "sugar-activity";
}

#define _SUGAR_ACTIVITY_GLOB "/usr/share/sugar/activities/*/activity/activity.info"
#define _SUGAR_ACTIVITY_ICON_GLOB "/usr/share/sugar/activities/*/activity/*"
#define _SUGAR_ACTIVITY_LINFO_GLOB "/usr/share/sugar/activities/*/locale/*/activity.linfo"

/**
 * asb_plugin_add_globs:
 */
void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	// the _SUGAR_ACTIVITY_GLOB is a subset of the icon glob
	asb_plugin_add_glob (globs, _SUGAR_ACTIVITY_ICON_GLOB);
	asb_plugin_add_glob (globs, _SUGAR_ACTIVITY_LINFO_GLOB);
}

/**
 * _asb_plugin_is_actinfo
 */
static gboolean
_asb_plugin_is_actinfo (const gchar *filename)
{
	if (asb_plugin_match_glob (_SUGAR_ACTIVITY_GLOB, filename))
		return TRUE;
	return FALSE;
}

/**
 * _asb_plugin_is_linfo
 */
static gboolean
_asb_plugin_is_linfo (const gchar *filename)
{
	if (asb_plugin_match_glob (_SUGAR_ACTIVITY_LINFO_GLOB, filename))
		return TRUE;
	return FALSE;
}

/**
 * asb_plugin_check_filename:
 */
gboolean
asb_plugin_check_filename (AsbPlugin *plugin, const gchar *filename)
{
	return _asb_plugin_is_linfo (filename) ||
	       _asb_plugin_is_actinfo (filename);
}

static gboolean
_asb_plugin_load_sugar_icon (const gchar *path,
			     AsIcon *ic,
			     AsApp *app,
			     AsbPlugin *plugin) {
	gsize len;
	gboolean is_hidpi;
	gchar *data, *name;
	GBytes *bytes = NULL;
	GString *string = NULL;
	GRegex *regex = NULL;
	GdkPixbufLoader *pbl = NULL;

	if (!g_file_get_contents (path, &data, &len, NULL))
		return FALSE;

	// Sugar icons have 2 XML entities, stroke_color and fill_color
	regex = g_regex_new ("<!ENTITY stroke_color \"[^\"]+\">",
			     0, 0, NULL);
	data = g_regex_replace_literal (regex, data, len, 0,
					"<!ENTITY stroke_color \"#282828\">",
					0, NULL);
	g_free (regex);

	regex = g_regex_new ("<!ENTITY fill_color \"[^\"]+\">",
			     0, 0, NULL);
	data = g_regex_replace_literal (regex, data, -1, 0,
					"<!ENTITY fill_color \"#FFFFFF\">",
					0, NULL);
	g_free (regex);

	// The PBL takes bytes/length, not null terminated string
	string = g_string_new (data);
	bytes = g_string_free_to_bytes (string);

	is_hidpi = asb_context_get_flag (plugin->ctx, ASB_CONTEXT_FLAG_HIDPI_ICONS);
	for (gint factor = 1; factor <= (is_hidpi ? 2 : 1); factor++) {
		pbl = gdk_pixbuf_loader_new_with_type ("svg", NULL);
		gdk_pixbuf_loader_set_size (pbl, 64 * factor, 64 * factor);
		gdk_pixbuf_loader_write_bytes (pbl, bytes, NULL);
		if (!gdk_pixbuf_loader_close (pbl, NULL))
			return FALSE;

		as_icon_set_kind (ic, AS_ICON_KIND_CACHED);
		as_icon_set_prefix (ic, as_app_get_icon_path (AS_APP (app)));
		as_icon_set_pixbuf (ic, gdk_pixbuf_loader_get_pixbuf (pbl));
		as_icon_set_width (ic, 64 * factor);
		as_icon_set_height (ic, 64 * factor);
		if (is_hidpi) {
			name = g_strdup_printf ("%ix%i/%s.png",
			    64 * factor, 64 * factor,
			    as_app_get_id_filename (AS_APP (app)));
		} else {
			name = g_strdup_printf ("%s.png",
			    as_app_get_id_filename (AS_APP (app)));
		}
		as_icon_set_name (ic, name);
	}

	return TRUE;
}

/**
 * asb_plugin_process_actinfo:
 */
static gboolean
asb_plugin_process_actinfo (AsbPlugin *plugin,
			    AsbPackage *pkg,
			    const gchar *filename,
			    AsApp *app,
			    GError **error)
{
	gsize len;
	const gchar *tmp;
	g_autofree gchar *data = NULL;
	g_autofree gchar *id = NULL;
	g_autoptr(GKeyFile) actinfo = NULL;

	if (!g_file_get_contents (filename, &data, &len, error))
		return FALSE;

	actinfo = g_key_file_new ();
	if (!g_key_file_load_from_data (actinfo, data, len,
					G_KEY_FILE_NONE, error))
		return FALSE;

	if (!g_key_file_has_group (actinfo, "Activity"))
		return FALSE;
	if (!g_key_file_has_key (actinfo, "Activity", "bundle_id", error))
		return FALSE;

	as_app_set_kind (app, AS_APP_KIND_DESKTOP);

	tmp = g_key_file_get_string (actinfo, "Activity", "bundle_id", NULL);
	id = g_strdup_printf ("%s.activity.desktop", tmp);
	as_app_set_id (app, id);

	tmp = g_key_file_get_string (actinfo, "Activity", "name", NULL);
	if (tmp != NULL)
		as_app_set_name (app, NULL, tmp);

	tmp = g_key_file_get_string (actinfo, "Activity", "summary", NULL);
	if (tmp != NULL) {
		as_app_set_comment (app, NULL, tmp);
		g_autofree gchar *desc = NULL;
		desc = as_markup_import (tmp,
					 AS_MARKUP_CONVERT_FORMAT_SIMPLE,
					 error);
		if (desc != NULL)
			as_app_set_description (app, NULL, desc);
	}

	tmp = g_key_file_get_string (actinfo, "Activity", "license", NULL);
	if (tmp != NULL) {
		as_app_set_project_license (app, tmp);
	}

	tmp = g_key_file_get_string (actinfo, "Activity", "icon", NULL);
	if (tmp != NULL) {
		g_autoptr(AsIcon) ic = NULL;
		const gchar *directory;
		const gchar *path;

		directory = g_path_get_dirname (filename);

		// Icon might be "icon.svg" or might already have .svg suffix
		path = g_build_filename (directory, tmp, NULL);
		if (!g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
			const gchar *with_dotsvg;
			with_dotsvg = g_strdup_printf ("%s.svg", tmp);
			path = g_build_filename (directory, with_dotsvg, NULL);
		}

		if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
			ic = as_icon_new ();
			if (_asb_plugin_load_sugar_icon (
				    path, ic, AS_APP (app), plugin))
				as_app_add_icon (app, ic);
		}
	}
	return TRUE;
}

/**
 * asb_plugin_process_actinfo:
 */
static gboolean
asb_plugin_process_linfo (AsbPlugin *plugin,
			    AsbPackage *pkg,
			    const gchar *filename,
			    AsApp *app,
			    GError **error)
{
	gsize len;
	const gchar *tmp;
	const gchar *locale;
	g_autofree gchar *data = NULL;
	g_autoptr(GKeyFile) linfo = NULL;

	// filename ~= "/usr/share/sugar/activities/*/locale/*/activity.linfo"
	// locale is the 2nd wildcard section
	tmp = g_path_get_dirname (filename);
	locale = g_path_get_basename (tmp);

	if (!g_file_get_contents (filename, &data, &len, error))
		return FALSE;

	linfo = g_key_file_new ();
	if (!g_key_file_load_from_data (linfo, data, len,
					G_KEY_FILE_NONE, error))
		return FALSE;
	if (!g_key_file_has_group (linfo, "Activity"))
		return FALSE;

	tmp = g_key_file_get_string (linfo, "Activity", "name", NULL);
	if (tmp != NULL)
		as_app_set_name (app, locale, tmp);

	tmp = g_key_file_get_string (linfo, "Activity", "summary", NULL);
	if (tmp != NULL) {
		as_app_set_comment (app, locale, tmp);
		g_autofree gchar *desc = NULL;
		desc = as_markup_import (tmp,
					 AS_MARKUP_CONVERT_FORMAT_SIMPLE,
					 error);
		if (desc != NULL)
			as_app_set_description (app, locale, desc);
	}

	return TRUE;
}

/**
 * asb_plugin_process:
 */
GList *
asb_plugin_process (AsbPlugin *plugin,
		    AsbPackage *pkg,
		    const gchar *tmpdir,
		    GError **error)
{
	gboolean ret;
	GList *apps = NULL;
	guint i;
	gchar **filelist;
	g_autoptr(AsbApp) app = NULL;

	app = asb_app_new (pkg, NULL);

	filelist = asb_package_get_filelist (pkg);
	for (i = 0; filelist[i] != NULL; i++) {
		g_autofree gchar *filename_tmp = NULL;
		filename_tmp = g_build_filename (tmpdir, filelist[i], NULL);

		if (_asb_plugin_is_actinfo (filelist[i])) {
			ret = asb_plugin_process_actinfo (
			    plugin, pkg, filename_tmp, AS_APP (app), error);
			if (!ret) {
				g_set_error (error,
					     ASB_PLUGIN_ERROR,
					     ASB_PLUGIN_ERROR_FAILED,
					     "bad activity.info in %s",
					     asb_package_get_basename (pkg));
				return NULL;
			}
			asb_plugin_add_app (&apps, AS_APP (app));
		} else if (_asb_plugin_is_linfo (filelist[i])) {
			ret = asb_plugin_process_linfo (
			    plugin, pkg, filename_tmp, AS_APP (app), error);

			if (!ret)
			    g_debug ("Bad activity.linfo: %s\n", filelist[i]);
		}
	}

	return apps;
}
