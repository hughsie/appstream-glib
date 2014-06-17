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

/**
 * SECTION:asb-plugin-loader
 * @short_description: Plugin loader.
 * @stability: Unstable
 *
 * This module provides an array of plugins which can operate on an exploded
 * package tree.
 */

#include "config.h"

#include <glib.h>

#include "as-cleanup.h"
#include "asb-plugin.h"
#include "asb-plugin-loader.h"

/**
 * asb_plugin_loader_plugin_free:
 **/
static void
asb_plugin_loader_plugin_free (AsbPlugin *plugin)
{
	g_free (plugin->priv);
	g_free (plugin->name);
	g_module_close (plugin->module);
	g_slice_free (AsbPlugin, plugin);
}

/**
 * asb_plugin_loader_match_fn:
 * @plugins: (element-type AsbPlugin): An array of plugins
 * @filename: filename
 *
 * Processes the list of plugins finding a plugin that can process the filename.
 *
 * Returns: (transfer none): a plugin, or %NULL
 *
 * Since: 0.1.0
 **/
AsbPlugin *
asb_plugin_loader_match_fn (GPtrArray *plugins, const gchar *filename)
{
	gboolean ret;
	AsbPluginCheckFilenameFunc plugin_func = NULL;
	AsbPlugin *plugin;
	guint i;

	/* run each plugin */
	for (i = 0; i < plugins->len; i++) {
		plugin = g_ptr_array_index (plugins, i);
		ret = g_module_symbol (plugin->module,
				       "asb_plugin_check_filename",
				       (gpointer *) &plugin_func);
		if (!ret)
			continue;
		if (plugin_func (plugin, filename))
			return plugin;
	}
	return NULL;
}

/**
 * asb_plugin_loader_process_app:
 * @plugins: (element-type AsbPlugin): An array of plugins
 * @pkg: The #AsbPackage
 * @app: The #AsbApp to refine
 * @tmpdir: A temporary location to use
 * @error: A #GError or %NULL
 *
 * Processes an application object, refining any available data.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_plugin_loader_process_app (GPtrArray *plugins,
			       AsbPackage *pkg,
			       AsbApp *app,
			       const gchar *tmpdir,
			       GError **error)
{
	gboolean ret;
	AsbPluginProcessAppFunc plugin_func = NULL;
	AsbPlugin *plugin;
	guint i;

	/* run each plugin */
	for (i = 0; i < plugins->len; i++) {
		plugin = g_ptr_array_index (plugins, i);
		ret = g_module_symbol (plugin->module,
				       "asb_plugin_process_app",
				       (gpointer *) &plugin_func);
		if (!ret)
			continue;
		asb_package_log (pkg,
				 ASB_PACKAGE_LOG_LEVEL_DEBUG,
				 "Running asb_plugin_process_app() from %s",
				 plugin->name);
		if (!plugin_func (plugin, pkg, app, tmpdir, error))
			return FALSE;
	}
	return TRUE;
}

/**
 * asb_plugin_loader_run:
 **/
static void
asb_plugin_loader_run (GPtrArray *plugins, const gchar *function_name)
{
	gboolean ret;
	AsbPluginFunc plugin_func = NULL;
	AsbPlugin *plugin;
	guint i;

	/* run each plugin */
	for (i = 0; i < plugins->len; i++) {
		plugin = g_ptr_array_index (plugins, i);
		ret = g_module_symbol (plugin->module,
				       function_name,
				       (gpointer *) &plugin_func);
		if (!ret)
			continue;
		plugin_func (plugin);
	}
}

/**
 * asb_plugin_loader_get_globs:
 * @plugins: (element-type AsbPlugin): An array of plugins
 *
 * Gets the list of globs.
 *
 * Returns: (transfer container) (element-type utf8): globs
 *
 * Since: 0.1.0
 **/
GPtrArray *
asb_plugin_loader_get_globs (GPtrArray *plugins)
{
	gboolean ret;
	AsbPluginGetGlobsFunc plugin_func = NULL;
	AsbPlugin *plugin;
	guint i;
	GPtrArray *globs;

	/* run each plugin */
	globs = asb_glob_value_array_new ();
	for (i = 0; i < plugins->len; i++) {
		plugin = g_ptr_array_index (plugins, i);
		ret = g_module_symbol (plugin->module,
				       "asb_plugin_add_globs",
				       (gpointer *) &plugin_func);
		if (!ret)
			continue;
		plugin_func (plugin, globs);
	}
	return globs;
}

/**
 * asb_plugin_loader_merge:
 * @plugins: (element-type AsbPlugin): An array of plugins
 * @apps: (element-type AsbApp): a list of applications that need merging
 *
 * Merge the list of applications using the plugins.
 *
 * Since: 0.1.0
 **/
void
asb_plugin_loader_merge (GPtrArray *plugins, GList **apps)
{
	const gchar *key;
	const gchar *tmp;
	AsbApp *app;
	AsbApp *found;
	AsbPluginMergeFunc plugin_func = NULL;
	AsbPlugin *plugin;
	gboolean ret;
	GList *l;
	guint i;
	_cleanup_hashtable_unref_ GHashTable *hash;

	/* run each plugin */
	for (i = 0; i < plugins->len; i++) {
		plugin = g_ptr_array_index (plugins, i);
		ret = g_module_symbol (plugin->module,
				       "asb_plugin_merge",
				       (gpointer *) &plugin_func);
		if (!ret)
			continue;
		plugin_func (plugin, apps);
	}

	/* FIXME: move to font plugin */
	for (l = *apps; l != NULL; l = l->next) {
		if (!ASB_IS_APP (l->data))
			continue;
		app = ASB_APP (l->data);
		as_app_remove_metadata (AS_APP (app), "FontFamily");
		as_app_remove_metadata (AS_APP (app), "FontFullName");
		as_app_remove_metadata (AS_APP (app), "FontIconText");
		as_app_remove_metadata (AS_APP (app), "FontParent");
		as_app_remove_metadata (AS_APP (app), "FontSampleText");
		as_app_remove_metadata (AS_APP (app), "FontSubFamily");
		as_app_remove_metadata (AS_APP (app), "FontClassifier");
	}

	/* deduplicate */
	hash = g_hash_table_new (g_str_hash, g_str_equal);
	for (l = *apps; l != NULL; l = l->next) {
		if (!ASB_IS_APP (l->data))
			continue;
		app = ASB_APP (l->data);
		key = as_app_get_id_full (AS_APP (app));
		found = g_hash_table_lookup (hash, key);
		if (found == NULL) {
			g_hash_table_insert (hash,
					     (gpointer) key,
					     (gpointer) app);
			continue;
		}
		tmp = asb_package_get_nevr (asb_app_get_package (found));
		asb_app_add_veto (app, "duplicate of %s", tmp);
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "duplicate %s not included as added from %s",
				 key, tmp);
	}
}

/**
 * asb_plugin_loader_open_plugin:
 **/
static AsbPlugin *
asb_plugin_loader_open_plugin (GPtrArray *plugins,
			       const gchar *filename)
{
	gboolean ret;
	GModule *module;
	AsbPluginGetNameFunc plugin_name = NULL;
	AsbPlugin *plugin = NULL;

	module = g_module_open (filename, 0);
	if (module == NULL) {
		g_warning ("failed to open plugin %s: %s",
			   filename, g_module_error ());
		return NULL;
	}

	/* get description */
	ret = g_module_symbol (module,
			       "asb_plugin_get_name",
			       (gpointer *) &plugin_name);
	if (!ret) {
		g_warning ("Plugin %s requires name", filename);
		g_module_close (module);
		return NULL;
	}

	/* print what we know */
	plugin = g_slice_new0 (AsbPlugin);
	plugin->enabled = TRUE;
	plugin->module = module;
	plugin->name = g_strdup (plugin_name ());
	g_debug ("opened plugin %s: %s", filename, plugin->name);

	/* add to array */
	g_ptr_array_add (plugins, plugin);
	return plugin;
}

/**
 * asb_plugin_loader_sort_cb:
 **/
static gint
asb_plugin_loader_sort_cb (gconstpointer a, gconstpointer b)
{
	AsbPlugin **plugin_a = (AsbPlugin **) a;
	AsbPlugin **plugin_b = (AsbPlugin **) b;
	return -g_strcmp0 ((*plugin_a)->name, (*plugin_b)->name);
}

/**
 * asb_plugin_loader_setup:
 * @plugins: (element-type AsbPlugin): An array of plugins
 * @error: A #GError or %NULL
 *
 * Set up the plugin loader.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_plugin_loader_setup (GPtrArray *plugins, GError **error)
{
	const gchar *filename_tmp;
	const gchar *location = "./plugins/.libs/";
	_cleanup_dir_close_ GDir *dir;

	/* search system-wide if not found locally */
	if (!g_file_test (location, G_FILE_TEST_EXISTS))
		location = ASB_PLUGIN_DIR;

	/* search in the plugin directory for plugins */
	dir = g_dir_open (location, 0, error);
	if (dir == NULL)
		return FALSE;

	/* try to open each plugin */
	g_debug ("searching for plugins in %s", location);
	do {
		_cleanup_free_ gchar *filename_plugin = NULL;
		filename_tmp = g_dir_read_name (dir);
		if (filename_tmp == NULL)
			break;
		if (!g_str_has_suffix (filename_tmp, ".so"))
			continue;
		filename_plugin = g_build_filename (location,
						    filename_tmp,
						    NULL);
		asb_plugin_loader_open_plugin (plugins, filename_plugin);
	} while (TRUE);

	/* run the plugins */
	asb_plugin_loader_run (plugins, "asb_plugin_initialize");
	g_ptr_array_sort (plugins, asb_plugin_loader_sort_cb);
	return TRUE;
}

/**
 * asb_plugin_loader_new:
 *
 * Creates a new plugin loader interface.
 *
 * Returns: (transfer container) (element-type AsbPlugin): state
 *
 * Since: 0.1.0
 **/
GPtrArray *
asb_plugin_loader_new (void)
{
	return g_ptr_array_new_with_free_func ((GDestroyNotify) asb_plugin_loader_plugin_free);
}

/**
 * asb_plugin_loader_free:
 * @plugins: (element-type AsbPlugin): An array of plugins
 *
 * Destroy the plugin state.
 *
 * Since: 0.1.0
 **/
void
asb_plugin_loader_free (GPtrArray *plugins)
{
	asb_plugin_loader_run (plugins, "asb_plugin_destroy");
	g_ptr_array_unref (plugins);
}
