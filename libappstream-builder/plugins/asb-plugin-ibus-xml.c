/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
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
#include <appstream-glib.h>

#include <asb-plugin.h>

/**
 * asb_plugin_get_name:
 */
const gchar *
asb_plugin_get_name (void)
{
	return "ibus-xml";
}

/**
 * asb_plugin_add_globs:
 */
void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/share/ibus/component/*.xml");
}

/**
 * _asb_plugin_check_filename:
 */
static gboolean
_asb_plugin_check_filename (const gchar *filename)
{
	if (fnmatch ("/usr/share/ibus/component/*.xml", filename, 0) == 0)
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
	GNode *root = NULL;
	GString *valid_xml;
	const gchar *tmp;
	const GNode *n;
	gboolean found_header = FALSE;
	gboolean ret;
	guint i;
	_cleanup_free_ gchar *basename = NULL;
	_cleanup_free_ gchar *data = NULL;
	_cleanup_free_ gchar *filename_tmp;
	_cleanup_object_unref_ AsbApp *app = NULL;
	_cleanup_object_unref_ AsIcon *icon = NULL;
	_cleanup_strv_free_ gchar **languages = NULL;
	_cleanup_strv_free_ gchar **lines = NULL;

	/* open file */
	filename_tmp = g_build_filename (tmpdir, filename, NULL);
	ret = g_file_get_contents (filename_tmp, &data, NULL, error);
	if (!ret)
		goto out;

	/* some components start with a comment (invalid XML) and some
	 * don't even have '<?xml' -- try to fix up best we can */
	valid_xml = g_string_new ("");
	lines = g_strsplit (data, "\n", -1);
	for (i = 0; lines[i] != NULL; i++) {
		if (g_str_has_prefix (lines[i], "<?xml") ||
		    g_str_has_prefix (lines[i], "<component>"))
			found_header = TRUE;
		if (found_header)
			g_string_append_printf (valid_xml, "%s\n", lines[i]);
	}

	/* parse contents */
	root = as_node_from_xml (valid_xml->str, -1,
				 AS_NODE_FROM_XML_FLAG_NONE,
				 error);
	if (!ret)
		goto out;

	/* create new app */
	basename = g_path_get_basename (filename);
	app = asb_app_new (pkg, basename);
	as_app_set_id_kind (AS_APP (app), AS_ID_KIND_INPUT_METHOD);
	as_app_add_category (AS_APP (app), "Addons", -1);
	as_app_add_category (AS_APP (app), "InputSources", -1);
	asb_app_set_requires_appdata (app, TRUE);
	asb_app_set_hidpi_enabled (app, asb_context_get_hidpi_enabled (plugin->ctx));

	/* add icon */
	icon = as_icon_new ();
	as_icon_set_kind (icon, AS_ICON_KIND_STOCK);
	as_icon_set_name (icon, "system-run-symbolic", -1);
	as_app_add_icon (AS_APP (app), icon);

	/* read the component header which all input methods have */
	n = as_node_find (root, "component/description");
	if (n != NULL) {
		as_app_set_name (AS_APP (app), "C", as_node_get_data (n), -1);
		as_app_set_comment (AS_APP (app), "C", as_node_get_data (n), -1);
	}
	n = as_node_find (root, "component/homepage");
	if (n != NULL) {
		as_app_add_url (AS_APP (app),
				AS_URL_KIND_HOMEPAGE,
				as_node_get_data (n), -1);
	}

	/* do we have a engine section we can use? */
	n = as_node_find (root, "component/engines/engine/longname");
	if (n != NULL)
		as_app_set_name (AS_APP (app), "C", as_node_get_data (n), -1);
	n = as_node_find (root, "component/engines/engine/description");
	if (n != NULL)
		as_app_set_comment (AS_APP (app), "C", as_node_get_data (n), -1);
	n = as_node_find (root, "component/engines/engine/symbol");
	if (n != NULL) {
		tmp = as_node_get_data (n);
		if (tmp != NULL && tmp[0] != '\0') {
			as_app_add_metadata (AS_APP (app),
					     "X-IBus-Symbol",
					     tmp, -1);
		}
	}
	n = as_node_find (root, "component/engines/engine/language");
	if (n != NULL) {
		tmp = as_node_get_data (n);
		if (tmp != NULL) {
			languages = g_strsplit (tmp, ",", -1);
			for (i = 0; languages[i] != NULL; i++) {
				if (g_strcmp0 (languages[i], "other") == 0)
					continue;
				as_app_add_language (AS_APP (app),
						     100, languages[i], -1);
			}
		}
	}

	/* add */
	asb_plugin_add_app (apps, AS_APP (app));
out:
	if (root != NULL)
		as_node_unref (root);
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
			return NULL;
		}
	}

	/* no desktop files we care about */
	if (apps == NULL) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "nothing interesting in %s",
			     asb_package_get_basename (pkg));
		return NULL;
	}
	return apps;
}
