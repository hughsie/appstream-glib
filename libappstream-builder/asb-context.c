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
#include "asb-panel.h"
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
	GPtrArray		*file_globs;		/* of AsbPackage */
	GPtrArray		*packages;		/* of AsbPackage */
	AsbPanel		*panel;
	AsbPluginLoader		*plugin_loader;
	gboolean		 add_cache_id;
	gboolean		 no_net;
	guint			 max_threads;
	gdouble			 api_version;
	gchar			*old_metadata;
	gchar			*extra_appstream;
	gchar			*extra_appdata;
	gchar			*extra_screenshots;
	gchar			*screenshot_uri;
	gchar			*log_dir;
	gchar			*screenshot_dir;
	gchar			*cache_dir;
	gchar			*temp_dir;
	gchar			*output_dir;
	gchar			*basename;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsbContext, asb_context, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (asb_context_get_instance_private (o))

/**
 * asb_context_realpath:
 **/
static gchar *
asb_context_realpath (const gchar *path)
{
	char *temp;
	gchar *real;

	/* don't trust realpath one little bit */
	if (path == NULL)
		return NULL;

	/* glibc allocates us a buffer to try and fix some brain damage */
	temp = realpath (path, NULL);
	if (temp == NULL)
		return NULL;
	real = g_strdup (temp);
	free (temp);
	return real;
}

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
	priv->old_metadata = asb_context_realpath (old_metadata);
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
	priv->extra_appstream = asb_context_realpath (extra_appstream);
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
	priv->extra_appdata = asb_context_realpath (extra_appdata);
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
	priv->extra_screenshots = asb_context_realpath (extra_screenshots);
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
	priv->log_dir = asb_context_realpath (log_dir);
}

/**
 * asb_context_set_screenshot_dir:
 * @ctx: A #AsbContext
 * @screenshot_dir: directory
 *
 * Sets the screenshot directory to use when building metadata.
 *
 * Since: 0.2.2
 **/
void
asb_context_set_screenshot_dir (AsbContext *ctx, const gchar *screenshot_dir)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->screenshot_dir = asb_context_realpath (screenshot_dir);
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
 * asb_context_get_no_net:
 * @ctx: A #AsbContext
 *
 * Gets if network access is forbidden.
 *
 * Returns: boolean
 *
 * Since: 0.2.5
 **/
gboolean
asb_context_get_no_net (AsbContext *ctx)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->no_net;
}

/**
 * asb_context_get_api_version:
 * @ctx: A #AsbContext
 *
 * Gets the target metadata API version.
 *
 * Returns: floating point
 *
 * Since: 0.1.0
 **/
gdouble
asb_context_get_api_version (AsbContext *ctx)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->api_version;
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

	/* can find in existing metadata */
	if (asb_context_find_in_cache (ctx, filename)) {
		g_debug ("Found %s in old metadata", filename);
		return TRUE;
	}

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
 * asb_context_load_extra_screenshots:
 **/
static gboolean
asb_context_load_extra_screenshots (AsbContext *ctx, AsApp *app, GError **error)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	const gchar *tmp;
	_cleanup_dir_close_ GDir *dir = NULL;
	_cleanup_free_ gchar *path = NULL;
	_cleanup_object_unref_ AsbApp *app_build = NULL;
	_cleanup_object_unref_ AsbPackage *pkg = NULL;

	/* are there any extra screenshots for this app */
	if (priv->extra_screenshots == NULL ||
	    priv->screenshot_uri == NULL ||
	    priv->screenshot_dir == NULL)
		return TRUE;
	path = g_build_filename (priv->extra_screenshots, as_app_get_id (app), NULL);
	if (!g_file_test (path, G_FILE_TEST_EXISTS))
		return TRUE;

	/* create a virtual package */
	pkg = asb_package_new ();
	asb_package_set_name (pkg, as_app_get_id (app));
	asb_package_set_config (pkg, "MirrorURI", priv->screenshot_uri);
	asb_package_set_config (pkg, "ScreenshotDir", priv->screenshot_dir);

	/* create a new AsbApp and add all the extra screenshots */
	app_build = asb_app_new (pkg, as_app_get_id_full (app));
	as_app_subsume_full (AS_APP (app_build), app,
			     AS_APP_SUBSUME_FLAG_NO_OVERWRITE);
	dir = g_dir_open (path, 0, error);
	if (dir == NULL)
		return FALSE;
	while ((tmp = g_dir_read_name (dir)) != NULL) {
		_cleanup_free_ gchar *filename = NULL;
		filename = g_build_filename (path, tmp, NULL);
		if (!asb_app_add_screenshot_source (app_build, filename, error))
			return FALSE;
	}
	as_app_subsume_full (app, AS_APP (app_build),
			     AS_APP_SUBSUME_FLAG_NO_OVERWRITE);

	/* save all the screenshots to disk */
	return asb_app_save_resources (app_build, error);
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
	AsApp *app;
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	GList *l;
	guint i;
	const gchar *ss_sizes[] = { "112x63",
				    "624x351",
				    "752x423",
				    "source",
				    NULL };
	_cleanup_free_ gchar *icons_dir = NULL;

	/* required stuff set */
	if (priv->basename == NULL) {
		g_set_error_literal (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "basename not set!");
		return FALSE;
	}
	if (priv->output_dir == NULL) {
		g_set_error_literal (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "output_dir not set!");
		return FALSE;
	}
	if (priv->temp_dir == NULL) {
		g_set_error_literal (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "temp_dir not set!");
		return FALSE;
	}
	if (priv->cache_dir == NULL) {
		g_set_error_literal (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "cache_dir not set!");
		return FALSE;
	}

	/* create temp space */
	if (!asb_utils_ensure_exists_and_empty (priv->temp_dir, error))
		return FALSE;
	if (!asb_utils_ensure_exists (priv->output_dir, error))
		return FALSE;
	if (!asb_utils_ensure_exists (priv->cache_dir, error))
		return FALSE;

	/* icons is nuked; we can re-decompress from the -icons.tar.gz */
	icons_dir = g_build_filename (priv->temp_dir, "icons", NULL);
	if (!asb_utils_ensure_exists (icons_dir, error))
		return FALSE;

	/* create all the screenshot sizes */
	if (priv->screenshot_dir != NULL) {
		for (i = 0; ss_sizes[i] != NULL; i++) {
			_cleanup_free_ gchar *ss_dir = NULL;
			ss_dir = g_build_filename (priv->screenshot_dir,
						   ss_sizes[i], NULL);
			if (!asb_utils_ensure_exists (ss_dir, error))
				return FALSE;
		}
	}

	/* decompress the icons */
	if (priv->old_metadata != NULL) {
		_cleanup_free_ gchar *icons_fn = NULL;
		icons_fn = g_strdup_printf ("%s/%s-icons.tar.gz",
					    priv->old_metadata,
					    priv->basename);
		if (!asb_utils_explode (icons_fn, icons_dir, NULL, error))
			return FALSE;
	}

	/* load plugins */
	if (!asb_plugin_loader_setup (priv->plugin_loader, error))
		return FALSE;

	/* get a cache of the file globs */
	priv->file_globs = asb_plugin_loader_get_globs (priv->plugin_loader);

	/* add any extra applications and resize screenshots */
	if (priv->extra_appstream != NULL &&
	    g_file_test (priv->extra_appstream, G_FILE_TEST_EXISTS)) {
		if (!asb_utils_add_apps_from_dir (&priv->apps,
						  priv->extra_appstream,
						  error))
			return FALSE;
		for (l = priv->apps; l != NULL; l = l->next) {
			app = AS_APP (l->data);
			if (!asb_context_load_extra_screenshots (ctx, app, error))
				return FALSE;
		}
		g_print ("Added extra %i apps\n", g_list_length (priv->apps));
	}

	/* add old metadata */
	if (priv->old_metadata != NULL) {
		_cleanup_free_ gchar *xml_fn = NULL;
		_cleanup_object_unref_ GFile *file = NULL;
		xml_fn = g_strdup_printf ("%s/%s.xml.gz",
					  priv->old_metadata,
					  priv->basename);
		file = g_file_new_for_path (xml_fn);
		if (!as_store_from_file (priv->old_md_cache, file,
					 NULL, NULL, error))
			return FALSE;
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
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_free_ gchar *icons_dir = NULL;

	icons_dir = g_build_filename (temp_dir, "icons", NULL);
	if (!g_file_test (icons_dir, G_FILE_TEST_EXISTS))
		return TRUE;
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
			if (as_app_get_vetos(app)->len > 0)
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
 * asb_context_detect_pkgname_dups:
 **/
static gboolean
asb_context_detect_pkgname_dups (AsbContext *ctx, GError **error)
{
	AsApp *app;
	AsApp *found;
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	GList *l;
	const gchar *pkgname;
	_cleanup_hashtable_unref_ GHashTable *hash = NULL;

	hash = g_hash_table_new (g_str_hash, g_str_equal);
	for (l = priv->apps; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		pkgname = as_app_get_pkgname_default (app);
		if (pkgname == NULL)
			continue;
		if (ASB_IS_APP (app) &&  as_app_get_vetos(app)->len > 0)
			continue;
		found = g_hash_table_lookup (hash, pkgname);
		if (found != NULL) {
			g_print ("WARNING: %s and %s share the package '%s'\n",
				 as_app_get_id_full (app),
				 as_app_get_id_full (found), pkgname);
			continue;
		}
		g_hash_table_insert (hash, (gpointer) pkgname, app);
	}
	return TRUE;
}

/**
 * asb_context_detect_missing_parents:
 **/
static gboolean
asb_context_detect_missing_parents (AsbContext *ctx, GError **error)
{
	AsApp *app;
	AsApp *found;
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	GList *l;
	const gchar *tmp;
	_cleanup_hashtable_unref_ GHashTable *hash = NULL;

	/* add all desktop apps to the hash */
	hash = g_hash_table_new (g_str_hash, g_str_equal);
	for (l = priv->apps; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		if (!ASB_IS_APP (app))
			continue;
		if (as_app_get_pkgname_default (app) == NULL)
			continue;
		if (as_app_get_id_kind (app) != AS_ID_KIND_DESKTOP)
			continue;
		g_hash_table_insert (hash,
				     (gpointer) as_app_get_id_full (app),
				     app);
	}

	/* look for the thing that an addon extends */
	for (l = priv->apps; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		if (!ASB_IS_APP (app))
			continue;
		if (as_app_get_pkgname_default (app) == NULL)
			continue;
		if (as_app_get_id_kind (app) != AS_ID_KIND_ADDON)
			continue;
		if (as_app_get_extends(app)->len == 0)
			continue;
		tmp = g_ptr_array_index (as_app_get_extends(app), 0);
		found = g_hash_table_lookup (hash, tmp);
		if (found != NULL)
			continue;

		/* do not add the addon */
		as_app_add_veto (app, "%s has no parent of '%s'\n",
				  as_app_get_id_full (app), tmp);
		g_print ("WARNING: %s has no parent of '%s'\n",
			 as_app_get_id_full (app), tmp);
		asb_app_set_veto_description (ASB_APP (app));
	}
	return TRUE;
}

/**
 * asb_context_write_xml_fail:
 **/
static gboolean
asb_context_write_xml_fail (AsbContext *ctx,
			     const gchar *output_dir,
			     const gchar *basename,
			     GError **error)
{
	AsApp *app;
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	GList *l;
	_cleanup_free_ gchar *basename_failed = NULL;
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_object_unref_ AsStore *store;
	_cleanup_object_unref_ GFile *file;

	store = as_store_new ();
	for (l = priv->apps; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		if (!ASB_IS_APP (app))
			continue;
		if (as_app_get_vetos(app)->len == 0)
			continue;
		if (as_app_get_metadata_item (app, "NoDisplay") != NULL)
			continue;
		as_store_add_app (store, app);
	}
	filename = g_strdup_printf ("%s/%s-failed.xml.gz", output_dir, basename);
	file = g_file_new_for_path (filename);

	g_print ("Writing %s...\n", filename);
	basename_failed = g_strdup_printf ("%s-failed", basename);
	as_store_set_origin (store, basename_failed);
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
	asb_panel_set_job_total (priv->panel, priv->packages->len);
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
		asb_package_set_config (pkg, "ScreenshotDir", priv->screenshot_dir);
		asb_package_set_config (pkg, "CacheDir", priv->cache_dir);
		asb_package_set_config (pkg, "TempDir", priv->temp_dir);
		asb_package_set_config (pkg, "OutputDir", priv->output_dir);

		/* create task */
		task = asb_task_new (ctx);
		asb_task_set_id (task, i);
		asb_task_set_package (task, pkg);
		asb_task_set_panel (task, priv->panel);
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

	/* print any warnings */
	ret = asb_context_detect_missing_parents (ctx, error);
	if (!ret)
		goto out;

	/* write XML file */
	ret = asb_context_write_xml (ctx, priv->output_dir, priv->basename, error);
	if (!ret)
		goto out;

	/* write XML file */
	ret = asb_context_write_xml_fail (ctx, priv->output_dir, priv->basename, error);
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

	/* print any warnings */
	ret = asb_context_detect_pkgname_dups (ctx, error);
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
	asb_plugin_add_app (&priv->apps, AS_APP (app));
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
	g_object_unref (priv->panel);
	g_ptr_array_unref (priv->packages);
	g_list_foreach (priv->apps, (GFunc) g_object_unref, NULL);
	g_list_free (priv->apps);
	if (priv->file_globs != NULL)
		g_ptr_array_unref (priv->file_globs);
	g_mutex_clear (&priv->apps_mutex);
	g_free (priv->old_metadata);
	g_free (priv->extra_appstream);
	g_free (priv->extra_appdata);
	g_free (priv->extra_screenshots);
	g_free (priv->screenshot_uri);
	g_free (priv->log_dir);
	g_free (priv->screenshot_dir);
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
	priv->panel = asb_panel_new ();
	priv->packages = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	g_mutex_init (&priv->apps_mutex);
	priv->old_md_cache = as_store_new ();
	priv->max_threads = 1;
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
