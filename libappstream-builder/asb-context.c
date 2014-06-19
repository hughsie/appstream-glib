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
 * SECTION:asb-context
 * @short_description: High level interface for creating metadata.
 * @stability: Unstable
 *
 * This high level object can be used to build metadata given some package
 * filenames.
 */

#include "config.h"

#include <stdlib.h>
#include <appstream-glib.h>

#include "as-cleanup.h"
#include "asb-context.h"
#include "asb-context-private.h"
#include "asb-plugin.h"
#include "asb-plugin-loader.h"
#include "asb-task.h"
#include "asb-utils.h"

#ifdef HAVE_RPM
#include "asb-package-rpm.h"
#endif

#include "asb-package-deb.h"

typedef struct _AsbContextPrivate	AsbContextPrivate;
struct _AsbContextPrivate
{
	AsStore			*old_md_cache;
	GList			*apps;			/* of AsbApp */
	GMutex			 apps_mutex;		/* for ->apps */
	GPtrArray		*extra_pkgs;		/* of AsbGlobValue */
	GPtrArray		*file_globs;		/* of AsbPackage */
	GPtrArray		*packages;		/* of AsbPackage */
	AsbPluginLoader		*plugin_loader;
	gboolean		 add_cache_id;
	gboolean		 extra_checks;
	gboolean		 no_net;
	gboolean		 use_package_cache;
	guint			 max_threads;
	gdouble			 api_version;
	gchar			*old_metadata;
	gchar			*extra_appstream;
	gchar			*extra_appdata;
	gchar			*extra_screenshots;
	gchar			*screenshot_uri;
	gchar			*log_dir;
	gchar			*cache_dir;
	gchar			*temp_dir;
	gchar			*output_dir;
	gchar			*basename;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsbContext, asb_context, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (asb_context_get_instance_private (o))

/**
 * asb_context_set_no_net:
 * @ctx: A #AsbContext
 * @no_net: if network is disallowed
 *
 * Sets if network access is disallowed.
 *
 * Since: 0.1.0
 **/
void
asb_context_set_no_net (AsbContext *ctx, gboolean no_net)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->no_net = no_net;
}

/**
 * asb_context_set_api_version:
 * @ctx: A #AsbContext
 * @api_version: the AppStream API version
 *
 * Sets the version of the metadata to write.
 *
 * Since: 0.1.0
 **/
void
asb_context_set_api_version (AsbContext *ctx, gdouble api_version)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->api_version = api_version;
}

/**
 * asb_context_set_add_cache_id:
 * @ctx: A #AsbContext
 * @add_cache_id: boolean
 *
 * Sets if the cache id should be included in the metadata.
 *
 * Since: 0.1.0
 **/
void
asb_context_set_add_cache_id (AsbContext *ctx, gboolean add_cache_id)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->add_cache_id = add_cache_id;
}

/**
 * asb_context_set_extra_checks:
 * @ctx: A #AsbContext
 * @extra_checks: boolean
 *
 * Sets if extra checks should be performed when building the metadata.
 * Doing this requires internet access and may take a lot longer.
 *
 * Since: 0.1.0
 **/
void
asb_context_set_extra_checks (AsbContext *ctx, gboolean extra_checks)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->extra_checks = extra_checks;
}

/**
 * asb_context_set_use_package_cache:
 * @ctx: A #AsbContext
 * @use_package_cache: boolean
 *
 * Sets if the package cache should be used.
 *
 * Since: 0.1.0
 **/
void
asb_context_set_use_package_cache (AsbContext *ctx, gboolean use_package_cache)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->use_package_cache = use_package_cache;
}

/**
 * asb_context_set_max_threads:
 * @ctx: A #AsbContext
 * @max_threads: integer
 *
 * Sets the maximum number of threads to use when processing packages.
 *
 * Since: 0.1.0
 **/
void
asb_context_set_max_threads (AsbContext *ctx, guint max_threads)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->max_threads = max_threads;
}

/**
 * asb_context_set_old_metadata:
 * @ctx: A #AsbContext
 * @old_metadata: filename, or %NULL
 *
 * Sets the filename location of the old metadata file.
 * Note: the old metadata must have been built with cache-ids to be useful.
 *
 * Since: 0.1.0
 **/
void
asb_context_set_old_metadata (AsbContext *ctx, const gchar *old_metadata)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->old_metadata = g_strdup (old_metadata);
}

/**
 * asb_context_set_extra_appstream:
 * @ctx: A #AsbContext
 * @extra_appstream: directory name, or %NULL
 *
 * Sets the location of a directory which is used for supplimental AppStream
 * files.
 *
 * Since: 0.1.0
 **/
void
asb_context_set_extra_appstream (AsbContext *ctx, const gchar *extra_appstream)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->extra_appstream = g_strdup (extra_appstream);
}

/**
 * asb_context_set_extra_appdata:
 * @ctx: A #AsbContext
 * @extra_appdata: directory name, or %NULL
 *
 * Sets the location of a directory which is used for supplimental AppData
 * files.
 *
 * Since: 0.1.0
 **/
void
asb_context_set_extra_appdata (AsbContext *ctx, const gchar *extra_appdata)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->extra_appdata = g_strdup (extra_appdata);
}

/**
 * asb_context_set_extra_screenshots:
 * @ctx: A #AsbContext
 * @extra_screenshots: directory name, or %NULL
 *
 * Sets the location of a directory which is used for supplimental screenshot
 * files.
 *
 * Since: 0.1.0
 **/
void
asb_context_set_extra_screenshots (AsbContext *ctx, const gchar *extra_screenshots)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->extra_screenshots = g_strdup (extra_screenshots);
}

/**
 * asb_context_set_screenshot_uri:
 * @ctx: A #AsbContext
 * @screenshot_uri: Remote URI root, e.g. "http://www.mysite.com/screenshots/"
 *
 * Sets the remote screenshot URI for screenshots.
 *
 * Since: 0.1.0
 **/
void
asb_context_set_screenshot_uri (AsbContext *ctx, const gchar *screenshot_uri)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->screenshot_uri = g_strdup (screenshot_uri);
}

/**
 * asb_context_set_log_dir:
 * @ctx: A #AsbContext
 * @log_dir: directory
 *
 * Sets the log directory to use when building metadata.
 *
 * Since: 0.1.0
 **/
void
asb_context_set_log_dir (AsbContext *ctx, const gchar *log_dir)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->log_dir = g_strdup (log_dir);
}

/**
 * asb_context_set_cache_dir:
 * @ctx: A #AsbContext
 * @cache_dir: directory
 *
 * Sets the cache directory to use when building metadata.
 *
 * Since: 0.1.0
 **/
void
asb_context_set_cache_dir (AsbContext *ctx, const gchar *cache_dir)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->cache_dir = g_strdup (cache_dir);
}

/**
 * asb_context_set_temp_dir:
 * @ctx: A #AsbContext
 * @temp_dir: directory
 *
 * Sets the temporary directory to use when building metadata.
 *
 * Since: 0.1.0
 **/
void
asb_context_set_temp_dir (AsbContext *ctx, const gchar *temp_dir)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->temp_dir = g_strdup (temp_dir);
}

/**
 * asb_context_set_output_dir:
 * @ctx: A #AsbContext
 * @output_dir: directory
 *
 * Sets the output directory to use when building metadata.
 *
 * Since: 0.1.0
 **/
void
asb_context_set_output_dir (AsbContext *ctx, const gchar *output_dir)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->output_dir = g_strdup (output_dir);
}

/**
 * asb_context_set_basename:
 * @ctx: A #AsbContext
 * @basename: AppStream basename, e.g. "fedora-21"
 *
 * Sets the basename for the two metadata files.
 *
 * Since: 0.1.0
 **/
void
asb_context_set_basename (AsbContext *ctx, const gchar *basename)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->basename = g_strdup (basename);
}

/**
 * asb_context_get_extra_package:
 * @ctx: A #AsbContext
 * @pkgname: package name
 *
 * Gets an extra package that should be used when processing an application.
 *
 * Returns: a pakage name, or %NULL
 *
 * Since: 0.1.0
 **/
const gchar *
asb_context_get_extra_package (AsbContext *ctx, const gchar *pkgname)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	return asb_glob_value_search (priv->extra_pkgs, pkgname);
}

/**
 * asb_context_get_use_package_cache:
 * @ctx: A #AsbContext
 *
 * Gets if the package cache should be used.
 *
 * Returns: boolean
 *
 * Since: 0.1.0
 **/
gboolean
asb_context_get_use_package_cache (AsbContext *ctx)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->use_package_cache;
}

/**
 * asb_context_get_extra_checks:
 * @ctx: A #AsbContext
 *
 * Gets if extra checks should be performed.
 *
 * Returns: boolean
 *
 * Since: 0.1.0
 **/
gboolean
asb_context_get_extra_checks (AsbContext *ctx)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->extra_checks;
}

/**
 * asb_context_get_add_cache_id:
 * @ctx: A #AsbContext
 *
 * Gets if the cache_id should be added to the metadata.
 *
 * Returns: boolean
 *
 * Since: 0.1.0
 **/
gboolean
asb_context_get_add_cache_id (AsbContext *ctx)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->add_cache_id;
}

/**
 * asb_context_get_temp_dir:
 * @ctx: A #AsbContext
 *
 * Gets the temporary directory to use
 *
 * Returns: directory
 *
 * Since: 0.1.0
 **/
const gchar *
asb_context_get_temp_dir (AsbContext *ctx)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->temp_dir;
}

/**
 * asb_context_get_plugin_loader:
 * @ctx: A #AsbContext
 *
 * Gets the plugins available to the metadata extractor.
 *
 * Returns: (transfer none): the plugin loader in use
 *
 * Since: 0.1.0
 **/
AsbPluginLoader *
asb_context_get_plugin_loader (AsbContext *ctx)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->plugin_loader;
}

/**
 * asb_context_get_packages:
 * @ctx: A #AsbContext
 *
 * Returns the packages already added to the context.
 *
 * Returns: (transfer none) (element-type AsbPackage): array of packages
 *
 * Since: 0.1.0
 **/
GPtrArray *
asb_context_get_packages (AsbContext *ctx)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->packages;
}

/**
 * asb_context_add_filename:
 * @ctx: A #AsbContext
 * @filename: package filename
 * @error: A #GError or %NULL
 *
 * Adds a filename to the list of packages to be processed
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_context_add_filename (AsbContext *ctx, const gchar *filename, GError **error)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	_cleanup_object_unref_ AsbPackage *pkg = NULL;

	/* open */
#if HAVE_RPM
	if (g_str_has_suffix (filename, ".rpm"))
		pkg = asb_package_rpm_new ();
#endif
	if (g_str_has_suffix (filename, ".deb"))
		pkg = asb_package_deb_new ();
	if (pkg == NULL) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "No idea how to handle %s",
			     filename);
		return FALSE;
	}
	if (!asb_package_open (pkg, filename, error))
		return FALSE;

	/* add to array */
	g_ptr_array_add (priv->packages, g_object_ref (pkg));
	return TRUE;
}

/**
 * asb_context_get_file_globs:
 * @ctx: A #AsbContext
 *
 * Gets the list of file globs added by plugins.
 *
 * Returns: file globs
 *
 * Since: 0.1.0
 **/
GPtrArray *
asb_context_get_file_globs (AsbContext *ctx)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->file_globs;
}

/**
 * asb_context_setup:
 * @ctx: A #AsbContext
 * @error: A #GError or %NULL
 *
 * Sets up the context ready for use.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_context_setup (AsbContext *ctx, GError **error)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);

	/* load plugins */
	if (!asb_plugin_loader_setup (priv->plugin_loader, error))
		return FALSE;

	/* get a cache of the file globs */
	priv->file_globs = asb_plugin_loader_get_globs (priv->plugin_loader);

	/* add old metadata */
	if (priv->old_metadata != NULL) {
		_cleanup_object_unref_ GFile *file = NULL;
		file = g_file_new_for_path (priv->old_metadata);
		if (!as_store_from_file (priv->old_md_cache, file,
					 NULL, NULL, error))
			return FALSE;
	}

	/* add any extra applications */
	if (priv->extra_appstream != NULL &&
	    g_file_test (priv->extra_appstream, G_FILE_TEST_EXISTS)) {
		if (!asb_utils_add_apps_from_dir (&priv->apps,
						  priv->extra_appstream,
						  error))
			return FALSE;
		g_print ("Added extra %i apps\n", g_list_length (priv->apps));
	}

	return TRUE;
}

/**
 * asb_task_process_func:
 **/
static void
asb_task_process_func (gpointer data, gpointer user_data)
{
	AsbTask *task = (AsbTask *) data;
	_cleanup_error_free_ GError *error = NULL;

	/* just run the task */
	if (!asb_task_process (task, &error))
		g_warning ("Failed to run task: %s", error->message);
}

/**
 * asb_context_write_icons:
 **/
static gboolean
asb_context_write_icons (AsbContext *ctx,
			 const gchar *temp_dir,
			 const gchar *output_dir,
			 const gchar *basename,
			 GError **error)
{
	_cleanup_free_ gchar *filename;
	_cleanup_free_ gchar *icons_dir;

	icons_dir = g_build_filename (temp_dir, "icons", NULL);
	filename = g_strdup_printf ("%s/%s-icons.tar.gz", output_dir, basename);
	g_print ("Writing %s...\n", filename);
	return asb_utils_write_archive_dir (filename, icons_dir, error);
}

/**
 * asb_context_write_xml:
 **/
static gboolean
asb_context_write_xml (AsbContext *ctx,
		       const gchar *output_dir,
		       const gchar *basename,
		       GError **error)
{
	AsApp *app;
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	GList *l;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_object_unref_ AsStore *store;
	_cleanup_object_unref_ GFile *file;

	store = as_store_new ();
	for (l = priv->apps; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		if (ASB_IS_APP (app)) {
			if (asb_app_get_vetos(ASB_APP(app))->len > 0)
				continue;
		}
		as_store_add_app (store, app);
	}
	filename = g_strdup_printf ("%s/%s.xml.gz", output_dir, basename);
	file = g_file_new_for_path (filename);

	g_print ("Writing %s...\n", filename);
	as_store_set_origin (store, basename);
	as_store_set_api_version (store, priv->api_version);
	return as_store_to_file (store,
				 file,
				 AS_NODE_TO_XML_FLAG_ADD_HEADER |
				 AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
				 AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE,
				 NULL, error);
}

/**
 * asb_context_process:
 * @ctx: A #AsbContext
 * @error: A #GError or %NULL
 *
 * Processes all the packages that have been added to the context.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_context_process (AsbContext *ctx, GError **error)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	AsbPackage *pkg;
	AsbTask *task;
	GThreadPool *pool;
	gboolean ret = FALSE;
	guint i;
	_cleanup_ptrarray_unref_ GPtrArray *tasks = NULL;

	/* create thread pool */
	pool = g_thread_pool_new (asb_task_process_func,
				  ctx,
				  priv->max_threads,
				  TRUE,
				  error);
	if (pool == NULL)
		goto out;

	/* add each package */
	g_print ("Processing packages...\n");
	tasks = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	for (i = 0; i < priv->packages->len; i++) {
		pkg = g_ptr_array_index (priv->packages, i);
		if (!asb_package_get_enabled (pkg)) {
			asb_package_log (pkg,
					 ASB_PACKAGE_LOG_LEVEL_DEBUG,
					 "%s is not enabled",
					 asb_package_get_nevr (pkg));
			asb_package_log_flush (pkg, NULL);
			continue;
		}

		/* set locations of external resources */
		asb_package_set_config (pkg, "AppDataExtra", priv->extra_appdata);
		asb_package_set_config (pkg, "ScreenshotsExtra", priv->extra_screenshots);
		asb_package_set_config (pkg, "MirrorURI", priv->screenshot_uri);
		asb_package_set_config (pkg, "LogDir", priv->log_dir);
		asb_package_set_config (pkg, "CacheDir", priv->cache_dir);
		asb_package_set_config (pkg, "TempDir", priv->temp_dir);
		asb_package_set_config (pkg, "OutputDir", priv->output_dir);

		/* create task */
		task = asb_task_new (ctx);
		asb_task_set_id (task, i);
		asb_task_set_package (task, pkg);
		g_ptr_array_add (tasks, task);

		/* add task to pool */
		ret = g_thread_pool_push (pool, task, error);
		if (!ret)
			goto out;
	}

	/* wait for them to finish */
	g_thread_pool_free (pool, FALSE, TRUE);

	/* merge */
	g_print ("Merging applications...\n");
	asb_plugin_loader_merge (priv->plugin_loader, &priv->apps);

	/* write XML file */
	ret = asb_context_write_xml (ctx, priv->output_dir, priv->basename, error);
	if (!ret)
		goto out;

	/* write icons archive */
	ret = asb_context_write_icons (ctx,
				       priv->temp_dir,
				       priv->output_dir,
				       priv->basename,
				       error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * asb_context_disable_older_pkgs:
 * @ctx: A #AsbContext
 *
 * Disable older packages that have been added to the context.
 *
 * Since: 0.1.0
 **/
void
asb_context_disable_older_pkgs (AsbContext *ctx)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	AsbPackage *found;
	AsbPackage *pkg;
	const gchar *key;
	guint i;
	_cleanup_hashtable_unref_ GHashTable *newest;

	newest = g_hash_table_new_full (g_str_hash, g_str_equal,
					g_free, (GDestroyNotify) g_object_unref);
	for (i = 0; i < priv->packages->len; i++) {
		pkg = ASB_PACKAGE (g_ptr_array_index (priv->packages, i));
		key = asb_package_get_name (pkg);
		if (key == NULL)
			continue;
		found = g_hash_table_lookup (newest, key);
		if (found != NULL) {
			if (asb_package_compare (pkg, found) < 0) {
				asb_package_set_enabled (pkg, FALSE);
				continue;
			}
			asb_package_set_enabled (found, FALSE);
		}
		g_hash_table_insert (newest, g_strdup (key), g_object_ref (pkg));
	}
}

/**
 * asb_context_find_in_cache:
 * @ctx: A #AsbContext
 * @filename: cache-id
 *
 * Finds an application in the cache. This will only return results if
 * asb_context_set_old_metadata() has been used.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_context_find_in_cache (AsbContext *ctx, const gchar *filename)
{
	AsApp *app;
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	guint i;
	_cleanup_free_ gchar *cache_id;
	_cleanup_ptrarray_unref_ GPtrArray *apps;

	cache_id = asb_utils_get_cache_id_for_filename (filename);
	apps = as_store_get_apps_by_metadata (priv->old_md_cache,
					      "X-CreaterepoAsCacheID",
					      cache_id);
	if (apps->len == 0)
		return FALSE;
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		asb_context_add_app (ctx, (AsbApp *) app);
	}
	return TRUE;
}

/**
 * asb_context_find_by_pkgname:
 * @ctx: A #AsbContext
 * @pkgname: a package name
 *
 * Find a package from its name.
 *
 * Returns: (transfer none): a #AsbPackage, or %NULL for not found.
 *
 * Since: 0.1.0
 **/
AsbPackage *
asb_context_find_by_pkgname (AsbContext *ctx, const gchar *pkgname)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	AsbPackage *pkg;
	guint i;

	for (i = 0; i < priv->packages->len; i++) {
		pkg = g_ptr_array_index (priv->packages, i);
		if (g_strcmp0 (asb_package_get_name (pkg), pkgname) == 0)
			return pkg;
	}
	return NULL;
}

/**
 * asb_context_add_extra_pkg:
 **/
static void
asb_context_add_extra_pkg (AsbContext *ctx, const gchar *pkg1, const gchar *pkg2)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	g_ptr_array_add (priv->extra_pkgs, asb_glob_value_new (pkg1, pkg2));
}

/**
 * asb_context_add_app:
 * @ctx: A #AsbContext
 * @app: A #AsbApp
 *
 * Adds an application to the context.
 *
 * Since: 0.1.0
 **/
void
asb_context_add_app (AsbContext *ctx, AsbApp *app)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	g_mutex_lock (&priv->apps_mutex);
	asb_plugin_add_app (&priv->apps, app);
	g_mutex_unlock (&priv->apps_mutex);
}

/**
 * asb_context_finalize:
 **/
static void
asb_context_finalize (GObject *object)
{
	AsbContext *ctx = ASB_CONTEXT (object);
	AsbContextPrivate *priv = GET_PRIVATE (ctx);

	g_object_unref (priv->old_md_cache);
	g_object_unref (priv->plugin_loader);
	g_ptr_array_unref (priv->packages);
	g_ptr_array_unref (priv->extra_pkgs);
	g_list_foreach (priv->apps, (GFunc) g_object_unref, NULL);
	g_list_free (priv->apps);
	g_ptr_array_unref (priv->file_globs);
	g_mutex_clear (&priv->apps_mutex);
	g_free (priv->old_metadata);
	g_free (priv->extra_appstream);
	g_free (priv->extra_appdata);
	g_free (priv->extra_screenshots);
	g_free (priv->screenshot_uri);
	g_free (priv->log_dir);
	g_free (priv->cache_dir);
	g_free (priv->temp_dir);
	g_free (priv->output_dir);
	g_free (priv->basename);

	G_OBJECT_CLASS (asb_context_parent_class)->finalize (object);
}

/**
 * asb_context_init:
 **/
static void
asb_context_init (AsbContext *ctx)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);

	priv->plugin_loader = asb_plugin_loader_new (ctx);
	priv->packages = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->extra_pkgs = asb_glob_value_array_new ();
	g_mutex_init (&priv->apps_mutex);
	priv->old_md_cache = as_store_new ();
	priv->max_threads = 1;

	/* add extra data */
	asb_context_add_extra_pkg (ctx, "alliance-libs", "alliance");
	asb_context_add_extra_pkg (ctx, "beneath-a-steel-sky*", "scummvm");
	asb_context_add_extra_pkg (ctx, "coq-coqide", "coq");
	asb_context_add_extra_pkg (ctx, "drascula*", "scummvm");
	asb_context_add_extra_pkg (ctx, "efte-*", "efte-common");
	asb_context_add_extra_pkg (ctx, "fcitx-*", "fcitx-data");
	asb_context_add_extra_pkg (ctx, "flight-of-the-amazon-queen", "scummvm");
	asb_context_add_extra_pkg (ctx, "gcin", "gcin-data");
	asb_context_add_extra_pkg (ctx, "hotot-gtk", "hotot-common");
	asb_context_add_extra_pkg (ctx, "hotot-qt", "hotot-common");
	asb_context_add_extra_pkg (ctx, "java-1.7.0-openjdk-devel", "java-1.7.0-openjdk");
	asb_context_add_extra_pkg (ctx, "kchmviewer-qt", "kchmviewer");
	asb_context_add_extra_pkg (ctx, "libreoffice-*", "libreoffice-core");
	asb_context_add_extra_pkg (ctx, "lure", "scummvm");
	asb_context_add_extra_pkg (ctx, "nntpgrab-gui", "nntpgrab-core");
	asb_context_add_extra_pkg (ctx, "projectM-*", "libprojectM-qt");
	asb_context_add_extra_pkg (ctx, "scummvm-tools", "scummvm");
	asb_context_add_extra_pkg (ctx, "speed-dreams", "speed-dreams-robots-base");
	asb_context_add_extra_pkg (ctx, "switchdesk-gui", "switchdesk");
	asb_context_add_extra_pkg (ctx, "transmission-*", "transmission-common");
	asb_context_add_extra_pkg (ctx, "calligra-krita", "calligra-core");
}

/**
 * asb_context_class_init:
 **/
static void
asb_context_class_init (AsbContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asb_context_finalize;
}

/**
 * asb_context_new:
 *
 * Creates a new high-level instance.
 *
 * Returns: a #AsbContext
 *
 * Since: 0.1.0
 **/
AsbContext *
asb_context_new (void)
{
	AsbContext *context;
	context = g_object_new (ASB_TYPE_CONTEXT, NULL);
	return ASB_CONTEXT (context);
}
