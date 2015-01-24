/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2015 Richard Hughes <richard@hughsie.com>
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
#include <fnmatch.h>
#include <sqlite3.h>

#include <asb-plugin.h>

/**
 * asb_plugin_get_name:
 */
const gchar *
asb_plugin_get_name (void)
{
	return "ibus-sqlite";
}

/**
 * asb_plugin_add_globs:
 */
void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/share/ibus-table/tables/*.db");
}

/**
 * _asb_plugin_check_filename:
 */
static gboolean
_asb_plugin_check_filename (const gchar *filename)
{
	if (asb_plugin_match_glob ("/usr/share/ibus-table/tables/*.db", filename))
		return TRUE;
	return FALSE;
}

/**
 * asb_plugin_check_filename:
 */
gboolean
asb_plugin_check_filename (AsbPlugin *plugin, const gchar *filename)
{
	return _asb_plugin_check_filename (filename);
}

/**
 * asb_plugin_sqlite_callback_cb:
 */
static int
asb_plugin_sqlite_callback_cb (void *user_data, int argc, char **argv, char **data)
{
	gchar **tmp = (gchar **) user_data;
	*tmp = g_strdup (argv[1]);
	return 0;
}

/**
 * asb_plugin_process_filename:
 */
static gboolean
asb_plugin_process_filename (AsbPlugin *plugin,
			     AsbPackage *pkg,
			     const gchar *filename,
			     GList **apps,
			     const gchar *tmpdir,
			     GError **error)
{
	gboolean ret = TRUE;
	gchar *error_msg = 0;
	gchar *filename_tmp;
	gint rc;
	guint i;
	sqlite3 *db = NULL;
	_cleanup_free_ gchar *basename = NULL;
	_cleanup_free_ gchar *description = NULL;
	_cleanup_free_ gchar *language_string = NULL;
	_cleanup_free_ gchar *name = NULL;
	_cleanup_free_ gchar *symbol = NULL;
	_cleanup_object_unref_ AsbApp *app = NULL;
	_cleanup_object_unref_ AsIcon *icon = NULL;
	_cleanup_strv_free_ gchar **languages = NULL;

	/* open IME database */
	filename_tmp = g_build_filename (tmpdir, filename, NULL);
	rc = sqlite3_open (filename_tmp, &db);
	if (rc) {
		ret = FALSE;
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "Can't open database: %s",
			     sqlite3_errmsg (db));
		goto out;
	}

	/* get name */
	rc = sqlite3_exec(db, "SELECT * FROM ime WHERE attr = 'name' LIMIT 1;",
			  asb_plugin_sqlite_callback_cb,
			  &name, &error_msg);
	if (rc != SQLITE_OK) {
		ret = FALSE;
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "Can't get IME name from %s: %s",
			     filename, error_msg);
		sqlite3_free(error_msg);
		goto out;
	}

	/* get symbol */
	rc = sqlite3_exec(db, "SELECT * FROM ime WHERE attr = 'symbol' LIMIT 1;",
			  asb_plugin_sqlite_callback_cb,
			  &symbol, &error_msg);
	if (rc != SQLITE_OK) {
		ret = FALSE;
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "Can't get IME symbol from %s: %s",
			     filename, error_msg);
		sqlite3_free(error_msg);
		goto out;
	}

	/* get languages */
	rc = sqlite3_exec(db, "SELECT * FROM ime WHERE attr = 'languages' LIMIT 1;",
			  asb_plugin_sqlite_callback_cb,
			  &language_string, &error_msg);
	if (rc != SQLITE_OK) {
		ret = FALSE;
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "Can't get IME languages from %s: %s",
			     filename, error_msg);
		sqlite3_free(error_msg);
		goto out;
	}

	/* get description */
	rc = sqlite3_exec(db, "SELECT * FROM ime WHERE attr = 'description' LIMIT 1;",
			  asb_plugin_sqlite_callback_cb,
			  &description, &error_msg);
	if (rc != SQLITE_OK) {
		ret = FALSE;
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "Can't get IME name from %s: %s",
			     filename, error_msg);
		sqlite3_free(error_msg);
		goto out;
	}

	/* this is _required_ */
	if (name == NULL || description == NULL) {
		ret = FALSE;
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "No 'name' and 'description' in %s",
			     filename);
		goto out;
	}

	/* create new app */
	basename = g_path_get_basename (filename);
	app = asb_app_new (pkg, basename);
	as_app_set_id_kind (AS_APP (app), AS_ID_KIND_INPUT_METHOD);
	as_app_add_category (AS_APP (app), "Addons", -1);
	as_app_add_category (AS_APP (app), "InputSources", -1);
	as_app_set_name (AS_APP (app), "C", name, -1);
	as_app_set_comment (AS_APP (app), "C", description, -1);
	if (symbol != NULL && symbol[0] != '\0')
		as_app_add_metadata (AS_APP (app), "X-IBus-Symbol", symbol, -1);
	if (language_string != NULL) {
		languages = g_strsplit (language_string, ",", -1);
		for (i = 0; languages[i] != NULL; i++) {
			if (g_strcmp0 (languages[i], "other") == 0)
				continue;
			as_app_add_language (AS_APP (app),
					     100, languages[i], -1);
		}
	}
	asb_app_set_requires_appdata (app, TRUE);
	asb_app_set_hidpi_enabled (app, asb_context_get_flag (plugin->ctx, ASB_CONTEXT_FLAG_HIDPI_ICONS));

	/* add icon */
	icon = as_icon_new ();
	as_icon_set_kind (icon, AS_ICON_KIND_STOCK);
	as_icon_set_name (icon, "system-run-symbolic", -1);
	as_app_add_icon (AS_APP (app), icon);

	asb_plugin_add_app (apps, AS_APP (app));
out:
	if (db != NULL)
		sqlite3_close (db);
	return ret;
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

	filelist = asb_package_get_filelist (pkg);
	for (i = 0; filelist[i] != NULL; i++) {
		if (!_asb_plugin_check_filename (filelist[i]))
			continue;
		ret = asb_plugin_process_filename (plugin,
						   pkg,
						   filelist[i],
						   &apps,
						   tmpdir,
						   error);
		if (!ret) {
			g_list_free_full (apps, (GDestroyNotify) g_object_unref);
			apps = NULL;
			goto out;
		}
	}

	/* no desktop files we care about */
	if (apps == NULL) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "nothing interesting in %s",
			     asb_package_get_basename (pkg));
		goto out;
	}
out:
	return apps;
}
