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

#include "asb-plugin-loader.h"
#include "asb-plugin.h"

typedef struct
{
	GPtrArray		*plugins;
	AsbContext		*ctx;
	gchar			*plugin_dir;
} AsbPluginLoaderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsbPluginLoader, asb_plugin_loader, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (asb_plugin_loader_get_instance_private (o))

static void
asb_plugin_loader_run (AsbPluginLoader *plugin_loader, const gchar *function_name)
{
	AsbPluginLoaderPrivate *priv = GET_PRIVATE (plugin_loader);
	AsbPluginFunc plugin_func = NULL;
	AsbPlugin *plugin;
	gboolean ret;
	guint i;

	/* run each plugin */
	for (i = 0; i < priv->plugins->len; i++) {
		plugin = g_ptr_array_index (priv->plugins, i);
		ret = g_module_symbol (plugin->module,
				       function_name,
				       (gpointer *) &plugin_func);
		if (!ret)
			continue;
		plugin_func (plugin);
	}
}

static void
asb_plugin_loader_finalize (GObject *object)
{
	AsbPluginLoader *plugin_loader = ASB_PLUGIN_LOADER (object);
	AsbPluginLoaderPrivate *priv = GET_PRIVATE (plugin_loader);

	asb_plugin_loader_run (plugin_loader, "asb_plugin_destroy");

	if (priv->ctx != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (priv->ctx),
					      (gpointer*) &priv->ctx);
	}
	g_ptr_array_unref (priv->plugins);
	g_free (priv->plugin_dir);

	G_OBJECT_CLASS (asb_plugin_loader_parent_class)->finalize (object);
}

static void
asb_plugin_loader_plugin_free (AsbPlugin *plugin)
{
	g_free (plugin->priv);
	g_free (plugin->name);
	g_module_close (plugin->module);
	g_slice_free (AsbPlugin, plugin);
}

static void
asb_plugin_loader_init (AsbPluginLoader *plugin_loader)
{
	AsbPluginLoaderPrivate *priv = GET_PRIVATE (plugin_loader);
	priv->plugins = g_ptr_array_new_with_free_func ((GDestroyNotify) asb_plugin_loader_plugin_free);
}

/**
 * asb_plugin_loader_match_fn:
 * @plugin_loader: A #AsbPluginLoader
 * @filename: filename
 *
 * Processes the list of plugins finding a plugin that can process the filename.
 *
 * Returns: (transfer none): a plugin, or %NULL
 *
 * Since: 0.2.1
 **/
AsbPlugin *
asb_plugin_loader_match_fn (AsbPluginLoader *plugin_loader, const gchar *filename)
{
	AsbPluginCheckFilenameFunc plugin_func = NULL;
	AsbPluginLoaderPrivate *priv = GET_PRIVATE (plugin_loader);
	AsbPlugin *plugin;
	gboolean ret;
	guint i;

	/* run each plugin */
	for (i = 0; i < priv->plugins->len; i++) {
		plugin = g_ptr_array_index (priv->plugins, i);
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
 * @plugin_loader: A #AsbPluginLoader
 * @pkg: The #AsbPackage
 * @app: The #AsbApp to refine
 * @tmpdir: A temporary location to use
 * @error: A #GError or %NULL
 *
 * Processes an application object, refining any available data.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.2.1
 **/
gboolean
asb_plugin_loader_process_app (AsbPluginLoader *plugin_loader,
			       AsbPackage *pkg,
			       AsbApp *app,
			       const gchar *tmpdir,
			       GError **error)
{
	AsbPluginLoaderPrivate *priv = GET_PRIVATE (plugin_loader);
	AsbPlugin *plugin;
	AsbPluginProcessAppFunc plugin_func = NULL;
	GError *error_local = NULL;
	gboolean ret;
	guint i;

	/* run each plugin */
	for (i = 0; i < priv->plugins->len; i++) {
		plugin = g_ptr_array_index (priv->plugins, i);
		ret = g_module_symbol (plugin->module,
				       "asb_plugin_process_app",
				       (gpointer *) &plugin_func);
		if (!ret)
			continue;
		asb_package_log (pkg,
				 ASB_PACKAGE_LOG_LEVEL_DEBUG,
				 "Running asb_plugin_process_app() from %s",
				 plugin->name);
		if (!plugin_func (plugin, pkg, app, tmpdir, &error_local)) {
			asb_package_log (pkg,
					 ASB_PACKAGE_LOG_LEVEL_WARNING,
					 "Ignoring: %s",
					 error_local->message);
			g_clear_error (&error_local);
		}
	}
	return TRUE;
}

/**
 * asb_plugin_loader_get_plugins:
 * @plugin_loader: A #AsbPluginLoader
 *
 * Gets the list of plugins.
 *
 * Returns: (transfer none) (element-type AsbPlugin): plugins
 *
 * Since: 0.2.1
 **/
GPtrArray *
asb_plugin_loader_get_plugins (AsbPluginLoader *plugin_loader)
{
	AsbPluginLoaderPrivate *priv = GET_PRIVATE (plugin_loader);
	return priv->plugins;
}

/**
 * asb_plugin_loader_get_globs:
 * @plugin_loader: A #AsbPluginLoader
 *
 * Gets the list of globs.
 *
 * Returns: (transfer container) (element-type utf8): globs
 *
 * Since: 0.2.1
 **/
GPtrArray *
asb_plugin_loader_get_globs (AsbPluginLoader *plugin_loader)
{
	AsbPluginGetGlobsFunc plugin_func = NULL;
	AsbPluginLoaderPrivate *priv = GET_PRIVATE (plugin_loader);
	AsbPlugin *plugin;
	GPtrArray *globs;
	gboolean ret;
	guint i;

	/* run each plugin */
	globs = asb_glob_value_array_new ();
	for (i = 0; i < priv->plugins->len; i++) {
		plugin = g_ptr_array_index (priv->plugins, i);
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
 * @plugin_loader: A #AsbPluginLoader
 * @apps: (element-type AsbApp): a list of applications that need merging
 *
 * Merge the list of applications using the plugins.
 *
 * Since: 0.2.5
 **/
void
asb_plugin_loader_merge (AsbPluginLoader *plugin_loader, GList *apps)
{
	AsbApp *app;
	AsbApp *found;
	AsbPluginLoaderPrivate *priv = GET_PRIVATE (plugin_loader);
	AsbPluginMergeFunc plugin_func = NULL;
	AsbPlugin *plugin;
	GList *l;
	const gchar *key;
	const gchar *tmp;
	gboolean ret;
	guint i;
	g_autoptr(GHashTable) hash = NULL;

	/* run each plugin */
	for (i = 0; i < priv->plugins->len; i++) {
		plugin = g_ptr_array_index (priv->plugins, i);
		ret = g_module_symbol (plugin->module,
				       "asb_plugin_merge",
				       (gpointer *) &plugin_func);
		if (!ret)
			continue;
		plugin_func (plugin, apps);
	}

	/* FIXME: move to font plugin */
	for (l = apps; l != NULL; l = l->next) {
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
	for (l = apps; l != NULL; l = l->next) {
		if (!ASB_IS_APP (l->data))
			continue;
		app = ASB_APP (l->data);
		if (as_app_get_vetos(AS_APP(app))->len > 0)
			continue;
		key = as_app_get_id (AS_APP (app));
		found = g_hash_table_lookup (hash, key);
		if (found == NULL) {
			g_hash_table_insert (hash,
					     (gpointer) key,
					     (gpointer) app);
			continue;
		}
		if (as_app_get_kind (AS_APP (app)) == AS_APP_KIND_FIRMWARE) {
			as_app_subsume_full (AS_APP (found), AS_APP (app),
					     AS_APP_SUBSUME_FLAG_MERGE);
		}
		tmp = asb_package_get_nevr (asb_app_get_package (found));
		as_app_add_veto (AS_APP (app), "duplicate of %s", tmp);
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "duplicate %s not included as added from %s",
				 key, tmp);
	}
}

static AsbPlugin *
asb_plugin_loader_open_plugin (AsbPluginLoader *plugin_loader,
			       const gchar *filename)
{
	AsbPluginGetNameFunc plugin_name = NULL;
	AsbPluginLoaderPrivate *priv = GET_PRIVATE (plugin_loader);
	AsbPlugin *plugin = NULL;
	GModule *module;
	gboolean ret;

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
	plugin->ctx = priv->ctx;
	plugin->module = module;
	plugin->name = g_strdup (plugin_name ());
	g_debug ("opened plugin %s: %s", filename, plugin->name);

	/* add to array */
	g_ptr_array_add (priv->plugins, plugin);
	return plugin;
}

static gint
asb_plugin_loader_sort_cb (gconstpointer a, gconstpointer b)
{
	AsbPlugin **plugin_a = (AsbPlugin **) a;
	AsbPlugin **plugin_b = (AsbPlugin **) b;
	return g_strcmp0 ((*plugin_a)->name, (*plugin_b)->name);
}

/**
 * asb_plugin_loader_get_dir:
 * @plugin_loader: A #AsbPluginLoader
 *
 * Gets the plugin location.
 *
 * Returns: the location of the plugins
 *
 * Since: 0.4.2
 **/
const gchar *
asb_plugin_loader_get_dir (AsbPluginLoader *plugin_loader)
{
	AsbPluginLoaderPrivate *priv = GET_PRIVATE (plugin_loader);
	return priv->plugin_dir;
}

/**
 * asb_plugin_loader_set_dir:
 * @plugin_loader: A #AsbPluginLoader
 * @plugin_dir: the location of the plugins
 *
 * Set the plugin location.
 *
 * Since: 0.4.2
 **/
void
asb_plugin_loader_set_dir (AsbPluginLoader *plugin_loader, const gchar *plugin_dir)
{
	AsbPluginLoaderPrivate *priv = GET_PRIVATE (plugin_loader);
	g_free (priv->plugin_dir);
	priv->plugin_dir = g_strdup (plugin_dir);
}

/**
 * asb_plugin_loader_setup:
 * @plugin_loader: A #AsbPluginLoader
 * @error: A #GError or %NULL
 *
 * Set up the plugin loader.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.2.1
 **/
gboolean
asb_plugin_loader_setup (AsbPluginLoader *plugin_loader, GError **error)
{
	AsbPluginLoaderPrivate *priv = GET_PRIVATE (plugin_loader);
	const gchar *filename_tmp;
	g_autoptr(GDir) dir = NULL;

	/* never set */
	if (priv->plugin_dir == NULL)
		priv->plugin_dir = g_strdup (ASB_PLUGIN_DIR);

	/* search in the plugin directory for plugins */
	dir = g_dir_open (priv->plugin_dir, 0, error);
	if (dir == NULL)
		return FALSE;

	/* try to open each plugin */
	g_debug ("searching for plugins in %s", priv->plugin_dir);
	do {
		g_autofree gchar *filename_plugin = NULL;
		filename_tmp = g_dir_read_name (dir);
		if (filename_tmp == NULL)
			break;
		if (!g_str_has_suffix (filename_tmp, ".so"))
			continue;
		filename_plugin = g_build_filename (priv->plugin_dir,
						    filename_tmp,
						    NULL);
		asb_plugin_loader_open_plugin (plugin_loader, filename_plugin);
	} while (TRUE);

	/* run the plugins */
	asb_plugin_loader_run (plugin_loader, "asb_plugin_initialize");
	g_ptr_array_sort (priv->plugins, asb_plugin_loader_sort_cb);
	return TRUE;
}


static void
asb_plugin_loader_class_init (AsbPluginLoaderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asb_plugin_loader_finalize;
}

/**
 * asb_plugin_loader_new:
 * @ctx: A #AsbContext, or %NULL
 *
 * Creates a new plugin loader instance.
 *
 * Returns: a #AsbPluginLoader
 *
 * Since: 0.2.1
 **/
AsbPluginLoader *
asb_plugin_loader_new (AsbContext *ctx)
{
	AsbPluginLoader *plugin_loader;
	AsbPluginLoaderPrivate *priv;
	plugin_loader = g_object_new (ASB_TYPE_PLUGIN_LOADER, NULL);
	priv = GET_PRIVATE (plugin_loader);
	if (ctx != NULL) {
		priv->ctx = ctx;
		g_object_add_weak_pointer (G_OBJECT (priv->ctx),
					   (gpointer*) &priv->ctx);
	}
	return ASB_PLUGIN_LOADER (plugin_loader);
}
