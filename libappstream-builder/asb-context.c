/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2015 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
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

#include "asb-context.h"
#include "asb-context-private.h"
#include "asb-plugin.h"
#include "asb-plugin-loader.h"
#include "asb-task.h"
#include "asb-utils.h"

#ifdef HAVE_RPM
#include "asb-package-rpm.h"
#endif
#ifdef HAVE_ALPM
#include "asb-package-alpm.h"
#endif
#include "asb-package-cab.h"
#include "asb-package-deb.h"

typedef struct
{
	AsStore			*store_failed;
	AsStore			*store_ignore;
	GList			*apps;			/* of AsbApp */
	GMutex			 apps_mutex;		/* for ->apps */
	GPtrArray		*file_globs;		/* of AsbPackage */
	GPtrArray		*packages;		/* of AsbPackage */
	AsbPluginLoader		*plugin_loader;
	AsbContextFlags		 flags;
	guint			 min_icon_size;
	gdouble			 api_version;
	gchar			*log_dir;
	gchar			*cache_dir;
	gchar			*temp_dir;
	gchar			*output_dir;
	gchar			*icons_dir;
	gchar			*basename;
	gchar			*origin;
} AsbContextPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsbContext, asb_context, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (asb_context_get_instance_private (o))

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
 * asb_context_set_flags:
 * @ctx: A #AsbContext
 * @flags: #AsbContextFlags, e.g. %ASB_CONTEXT_FLAG_NO_NETWORK
 *
 * Sets flags to be used when building the metadata.
 *
 * Since: 0.3.5
 **/
void
asb_context_set_flags (AsbContext *ctx, AsbContextFlags flags)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->flags = flags;
}

/**
 * asb_context_set_max_threads:
 * @ctx: A #AsbContext
 * @max_threads: integer
 *
 * Sets the maximum number of threads to use when processing packages.
 * This function now has no affect as only one thread is ever used.
 *
 * Since: 0.1.0
 **/
void
asb_context_set_max_threads (AsbContext *ctx, guint max_threads)
{
}

/**
 * asb_context_set_min_icon_size:
 * @ctx: A #AsbContext
 * @min_icon_size: integer
 *
 * Sets the smallest icon size in pixels supported.
 *
 * Since: 0.3.1
 **/
void
asb_context_set_min_icon_size (AsbContext *ctx, guint min_icon_size)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->min_icon_size = min_icon_size;
}

/**
 * asb_context_get_min_icon_size:
 * @ctx: A #AsbContext
 *
 * Gets the minimum icon size in pixels.
 *
 * Returns: size
 *
 * Since: 0.3.1
 **/
guint
asb_context_get_min_icon_size (AsbContext *ctx)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->min_icon_size;
}

/**
 * asb_context_set_old_metadata:
 * @ctx: A #AsbContext
 * @old_metadata: filename, or %NULL
 *
 * Sets the filename location of the old metadata file.
 * This function now has no affect as no cache ID is available.
 *
 * Since: 0.1.0
 **/
void
asb_context_set_old_metadata (AsbContext *ctx, const gchar *old_metadata)
{
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
 * asb_context_set_icons_dir:
 * @ctx: A #AsbContext
 * @icons_dir: directory
 *
 * Sets the icons directory to use when building metadata.
 *
 * Since: 0.3.5
 **/
void
asb_context_set_icons_dir (AsbContext *ctx, const gchar *icons_dir)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->icons_dir = g_strdup (icons_dir);
}

/**
 * asb_context_set_basename:
 * @ctx: A #AsbContext
 * @basename: AppStream file basename, e.g. "appstream"
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
 * asb_context_set_origin:
 * @ctx: A #AsbContext
 * @origin: AppStream origin, e.g. "fedora-21"
 *
 * Sets the origin for the two metadata files.
 *
 * Since: 0.3.4
 **/
void
asb_context_set_origin (AsbContext *ctx, const gchar *origin)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	priv->origin = g_strdup (origin);
}

/**
 * asb_context_get_flags:
 * @ctx: A #AsbContext
 *
 * Gets the build flags.
 *
 * Returns: #AsbContextFlags
 *
 * Since: 0.3.5
 **/
AsbContextFlags
asb_context_get_flags (AsbContext *ctx)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->flags;
}

/**
 * asb_context_get_flag:
 * @ctx: A #AsbContext
 * @flag: A #AsbContextFlags
 *
 * Gets one specific build flag.
 *
 * Returns: %TRUE if the flag was set
 *
 * Since: 0.3.5
 **/
gboolean
asb_context_get_flag (AsbContext *ctx, AsbContextFlags flag)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	return (priv->flags & flag) > 0;
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
 * asb_context_get_cache_dir:
 * @ctx: A #AsbContext
 *
 * Gets the screenshot directory to use
 *
 * Returns: directory
 *
 * Since: 0.3.6
 **/
const gchar *
asb_context_get_cache_dir (AsbContext *ctx)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	return priv->cache_dir;
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
 * asb_context_add_package:
 * @ctx: A #AsbContext
 * @pkg: A #AsbPackage
 *
 * Adds a package to the list of packages to be processed
 *
 * Since: 0.3.5
 **/
void
asb_context_add_package (AsbContext *ctx, AsbPackage *pkg)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	g_ptr_array_add (priv->packages, g_object_ref (pkg));
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
	g_autoptr(AsbPackage) pkg = NULL;

	/* open */
#ifdef HAVE_RPM
	if (g_str_has_suffix (filename, ".rpm"))
		pkg = asb_package_rpm_new ();
#endif
#ifdef HAVE_ALPM
	if (g_str_has_suffix (filename, ".pkg.tar") ||
	    g_str_has_suffix (filename, ".pkg.tar.xz"))
		pkg = asb_package_alpm_new ();
#endif
	if (g_str_has_suffix (filename, ".cab"))
		pkg = asb_package_cab_new ();
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

	/* add to array */
	asb_package_set_filename (pkg, filename);

	/* failed to guess the nevra */
	if (asb_package_get_name (pkg) == NULL) {
		if (!asb_package_open (pkg, filename, error))
			return FALSE;
	}

	asb_context_add_package (ctx, pkg);
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
	g_autofree gchar *icons_dir = NULL;
	g_autofree gchar *screenshot_dir1 = NULL;
	g_autofree gchar *screenshot_dir2 = NULL;

	/* required stuff set */
	if (priv->origin == NULL) {
		g_set_error_literal (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "origin not set!");
		return FALSE;
	}
	if (priv->output_dir == NULL) {
		g_set_error_literal (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "output_dir not set!");
		return FALSE;
	}
	if (priv->icons_dir == NULL) {
		g_set_error_literal (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "icons_dir not set!");
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
	if (!asb_utils_ensure_exists (priv->output_dir, error))
		return FALSE;
	screenshot_dir1 = g_build_filename (priv->temp_dir, "screenshots", NULL);
	if (!asb_utils_ensure_exists_and_empty (screenshot_dir1, error))
		return FALSE;
	screenshot_dir2 = g_build_filename (priv->cache_dir, "screenshots", NULL);
	if (!asb_utils_ensure_exists (screenshot_dir2, error))
		return FALSE;
	if (priv->log_dir != NULL) {
		if (!asb_utils_ensure_exists (priv->log_dir, error))
			return FALSE;
	}

	/* icons is nuked; we can re-decompress from the -icons.tar.gz */
	if (!asb_utils_ensure_exists (priv->icons_dir, error))
		return FALSE;
	if (priv->flags & ASB_CONTEXT_FLAG_HIDPI_ICONS) {
		g_autofree gchar *icons_dir_hidpi = NULL;
		g_autofree gchar *icons_dir_lodpi = NULL;
		icons_dir_lodpi = g_build_filename (priv->icons_dir, "64x64", NULL);
		if (!asb_utils_ensure_exists (icons_dir_lodpi, error))
			return FALSE;
		icons_dir_hidpi = g_build_filename (priv->icons_dir, "128x128", NULL);
		if (!asb_utils_ensure_exists (icons_dir_hidpi, error))
			return FALSE;
	}

	/* load plugins */
	if (!asb_plugin_loader_setup (priv->plugin_loader, error))
		return FALSE;

	/* get a cache of the file globs */
	priv->file_globs = asb_plugin_loader_get_globs (priv->plugin_loader);

	return TRUE;
}

static gboolean
asb_context_write_icons (AsbContext *ctx,
			 const gchar *temp_dir,
			 GError **error)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	g_autofree gchar *filename = NULL;

	/* not enabled */
	if (priv->flags & ASB_CONTEXT_FLAG_UNCOMPRESSED_ICONS)
		return TRUE;

	if (!g_file_test (priv->icons_dir, G_FILE_TEST_EXISTS))
		return TRUE;
	filename = g_strdup_printf ("%s/%s-icons.tar.gz",
				    priv->output_dir, priv->basename);
	g_print ("Writing %s...\n", filename);
	return asb_utils_write_archive_dir (filename, priv->icons_dir, error);
}

static gboolean
asb_context_write_screenshots (AsbContext *ctx,
			       const gchar *temp_dir,
			       GError **error)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	g_autofree gchar *filename = NULL;
	g_autofree gchar *screenshot_dir = NULL;

	/* not enabled */
	if (priv->flags & ASB_CONTEXT_FLAG_UNCOMPRESSED_ICONS)
		return TRUE;

	screenshot_dir = g_build_filename (temp_dir, "screenshots", NULL);
	if (!g_file_test (screenshot_dir, G_FILE_TEST_EXISTS))
		return TRUE;
	filename = g_strdup_printf ("%s/%s-screenshots.tar",
				    priv->output_dir, priv->basename);
	g_print ("Writing %s...\n", filename);
	return asb_utils_write_archive_dir (filename, screenshot_dir, error);
}

static gboolean
asb_context_write_xml (AsbContext *ctx, GError **error)
{
	AsApp *app;
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	GList *l;
	g_autofree gchar *filename = NULL;
	g_autoptr(AsStore) store = NULL;
	g_autoptr(GFile) file = NULL;

	/* convert any vetod applications into dummy components */
	for (l = priv->apps; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		if (!ASB_IS_APP (app))
			continue;
		if (as_app_get_vetos(app)->len == 0)
			continue;
		asb_context_add_app_ignore (ctx, asb_app_get_package (ASB_APP (app)));
	}

	/* add any non-vetoed applications */
	store = as_store_new ();
	for (l = priv->apps; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		if (as_app_get_vetos(app)->len > 0)
			continue;
		as_store_add_app (store, app);
		as_store_remove_app (priv->store_failed, app);

		/* remove from the ignore list if the application was useful */
		if (ASB_IS_APP (app)) {
			AsbPackage *pkg = asb_app_get_package (ASB_APP (app));
			g_autofree gchar *name_arch = NULL;
			name_arch = g_strdup_printf ("%s.%s",
						     asb_package_get_name (pkg),
						     asb_package_get_arch (pkg));
			as_store_remove_app_by_id (priv->store_ignore, name_arch);
		}

	}
	filename = g_strdup_printf ("%s/%s.xml.gz",
				    priv->output_dir,
				    priv->basename);
	file = g_file_new_for_path (filename);

	g_print ("Writing %s...\n", filename);
	as_store_set_origin (store, priv->origin);
	as_store_set_api_version (store, priv->api_version);
	return as_store_to_file (store,
				 file,
				 AS_NODE_TO_XML_FLAG_ADD_HEADER |
				 AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
				 AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE,
				 NULL, error);
}

static gboolean
asb_context_convert_icons (AsbContext *ctx, GError **error)
{
	AsApp *app;
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	GList *l;

	/* not enabled */
	if ((priv->flags & ASB_CONTEXT_FLAG_EMBEDDED_ICONS) == 0)
		return TRUE;

	/* convert each one before saving resources */
	for (l = priv->apps; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		if (as_app_get_vetos(app)->len > 0)
			continue;
		if (!as_app_convert_icons (app, AS_ICON_KIND_EMBEDDED, error))
			return FALSE;
	}
	return TRUE;
}

static gboolean
asb_context_save_resources (AsbContext *ctx, GError **error)
{
	AsApp *app;
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	GList *l;

	for (l = priv->apps; l != NULL; l = l->next) {
		app = AS_APP (l->data);

		/* save icon */
		if (as_app_get_vetos(app)->len > 0)
			continue;
		if (!ASB_IS_APP (app))
			continue;
		if (!asb_app_save_resources (ASB_APP (app),
					     ASB_APP_SAVE_FLAG_ICONS,
					     error))
			return FALSE;
	}
	return TRUE;
}

static gboolean
asb_context_detect_pkgname_dups (AsbContext *ctx, GError **error)
{
	AsApp *app;
	AsApp *found;
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	GList *l;
	const gchar *pkgname;
	g_autoptr(GHashTable) hash = NULL;

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
				 as_app_get_id (app),
				 as_app_get_id (found), pkgname);
			continue;
		}
		g_hash_table_insert (hash, (gpointer) pkgname, app);
	}
	return TRUE;
}

static void
asb_context_write_app_xml (AsbContext *ctx)
{
	AsbApp *app;
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	GList *l;

	/* log the XML in the log file */
	for (l = priv->apps; l != NULL; l = l->next) {
		g_autoptr(GString) xml = NULL;
		g_autoptr(AsStore) store = NULL;

		/* we have an open log file? */
		if (!ASB_IS_APP (l->data))
			continue;

		/* just log raw XML */
		app = ASB_APP (l->data);
		store = as_store_new ();
		as_store_set_api_version (store, 1.0f);
		as_store_add_app (store, AS_APP (app));
		xml = as_store_to_xml (store,
				       AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
				       AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_NONE, "%s", xml->str);
	}
}

static gboolean
asb_context_detect_missing_data (AsbContext *ctx, GError **error)
{
	AsApp *app;
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	GList *l;

	/* look for the thing that an addon extends */
	for (l = priv->apps; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		if (as_app_get_name (AS_APP (app), "C") == NULL)
			as_app_add_veto (AS_APP (app), "No <name> in AppData");
		if (as_app_get_comment (AS_APP (app), "C") == NULL)
			as_app_add_veto (AS_APP (app), "No <summary> in AppData");
		switch (as_app_get_kind (AS_APP (app))) {
		case AS_APP_KIND_ADDON:
		case AS_APP_KIND_FIRMWARE:
		case AS_APP_KIND_DRIVER:
		case AS_APP_KIND_GENERIC:
		case AS_APP_KIND_LOCALIZATION:
		case AS_APP_KIND_CODEC:
		case AS_APP_KIND_INPUT_METHOD:
		case AS_APP_KIND_SHELL_EXTENSION:
			break;
		default:
			if (as_app_get_icon_default (AS_APP (app)) == NULL)
				as_app_add_veto (AS_APP (app), "Has no Icon");
			break;
		}
	}
	return TRUE;
}

static gboolean
asb_context_detect_missing_parents (AsbContext *ctx, GError **error)
{
	AsApp *app;
	AsApp *found;
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	GList *l;
	const gchar *tmp;
	g_autoptr(GHashTable) hash = NULL;

	/* add all desktop apps to the hash */
	hash = g_hash_table_new (g_str_hash, g_str_equal);
	for (l = priv->apps; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		if (!ASB_IS_APP (app))
			continue;
		if (as_app_get_pkgname_default (app) == NULL)
			continue;
		g_hash_table_insert (hash,
				     (gpointer) as_app_get_id (app),
				     app);
	}

	/* look for the thing that an addon extends */
	for (l = priv->apps; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		if (!ASB_IS_APP (app))
			continue;
		if (as_app_get_pkgname_default (app) == NULL)
			continue;
		if (as_app_get_kind (app) != AS_APP_KIND_ADDON)
			continue;
		if (as_app_get_extends(app)->len == 0)
			continue;
		tmp = g_ptr_array_index (as_app_get_extends(app), 0);
		found = g_hash_table_lookup (hash, tmp);
		if (found != NULL)
			continue;

		/* do not add the addon */
		as_app_add_veto (app, "%s has no parent of '%s'",
				 as_app_get_id (app), tmp);
		g_print ("WARNING: %s has no parent of '%s'\n",
			 as_app_get_id (app), tmp);
	}
	return TRUE;
}

static gboolean
asb_context_write_xml_fail (AsbContext *ctx, GError **error)
{
	AsApp *app;
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	GList *l;
	g_autofree gchar *basename_failed = NULL;
	g_autofree gchar *filename = NULL;
	g_autoptr(GFile) file = NULL;

	/* no need to create */
	if ((priv->flags & ASB_CONTEXT_FLAG_INCLUDE_FAILED) == 0)
		return TRUE;

	for (l = priv->apps; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		if (!ASB_IS_APP (app))
			continue;
		if (as_app_get_vetos(app)->len == 0)
			continue;
		if (as_store_get_app_by_id (priv->store_failed,
					    as_app_get_id (app)) != NULL)
			continue;
		as_store_add_app (priv->store_failed, app);
	}
	filename = g_strdup_printf ("%s/%s-failed.xml.gz",
				    priv->output_dir, priv->basename);
	file = g_file_new_for_path (filename);

	g_print ("Writing %s...\n", filename);
	basename_failed = g_strdup_printf ("%s-failed", priv->origin);
	as_store_set_origin (priv->store_failed, basename_failed);
	as_store_set_api_version (priv->store_failed, priv->api_version);
	return as_store_to_file (priv->store_failed,
				 file,
				 AS_NODE_TO_XML_FLAG_ADD_HEADER |
				 AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
				 AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE,
				 NULL, error);
}

static gboolean
asb_context_write_xml_ignore (AsbContext *ctx, GError **error)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	g_autofree gchar *basename_cache = NULL;
	g_autofree gchar *filename = NULL;
	g_autoptr(GFile) file = NULL;

	/* the store is already populated */
	filename = g_strdup_printf ("%s/%s-ignore.xml.gz",
				    priv->output_dir, priv->basename);
	file = g_file_new_for_path (filename);

	g_print ("Writing %s...\n", filename);
	basename_cache = g_strdup_printf ("%s-ignore", priv->origin);
	as_store_set_origin (priv->store_ignore, basename_cache);
	as_store_set_api_version (priv->store_ignore, priv->api_version);
	return as_store_to_file (priv->store_ignore,
				 file,
				 AS_NODE_TO_XML_FLAG_ADD_HEADER |
				 AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
				 AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE,
				 NULL, error);
}

static void
asb_context_disable_older_pkgs (AsbContext *ctx)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	AsbPackage *found;
	AsbPackage *pkg;
	const gchar *key;
	guint i;
	g_autoptr(GHashTable) newest = NULL;

	newest = g_hash_table_new_full (g_str_hash, g_str_equal,
					g_free, (GDestroyNotify) g_object_unref);
	for (i = 0; i < priv->packages->len; i++) {
		pkg = ASB_PACKAGE (g_ptr_array_index (priv->packages, i));
		if (!asb_package_get_enabled (pkg))
			continue;
		key = asb_package_get_name (pkg);
		if (key == NULL)
			continue;
		switch (asb_package_get_kind (pkg)) {
		case ASB_PACKAGE_KIND_DEFAULT:
		case ASB_PACKAGE_KIND_BUNDLE:
			found = g_hash_table_lookup (newest, key);
			if (found != NULL) {
				if (asb_package_compare (pkg, found) <= 0) {
					asb_package_set_enabled (pkg, FALSE);
					continue;
				}
				asb_package_set_enabled (found, FALSE);
			}
			break;
		default:
			/* allow multiple versions */
			break;
		}
		g_hash_table_insert (newest, g_strdup (key), g_object_ref (pkg));
	}
}

/* return the first package in the repo that matches the name and arch */
static AsbPackage *
asb_context_get_package_by_name_arch (AsbContext *ctx,
				      const gchar *name,
				      const gchar *arch)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	guint i;
	for (i = 0; i < priv->packages->len; i++) {
		AsbPackage *pkg = ASB_PACKAGE (g_ptr_array_index (priv->packages, i));
		if (g_strcmp0 (asb_package_get_name (pkg), name) == 0 &&
		    g_strcmp0 (asb_package_get_arch (pkg), arch) == 0) {
			return pkg;
		}
	}
	return NULL;
}

static void
asb_context_disable_multiarch_pkgs (AsbContext *ctx)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	guint i;

	/* are there any non 64-bit packages in the repo with 64-bit versions */
	for (i = 0; i < priv->packages->len; i++) {
		AsbPackage *pkg = ASB_PACKAGE (g_ptr_array_index (priv->packages, i));
		AsbPackage *pkg64;
		const gchar *arch = asb_package_get_arch (pkg);
		const gchar *name = asb_package_get_name (pkg);
		if (arch == NULL)
			continue;
		if (g_strcmp0 (arch, "x86_64") == 0)
			continue;
		if (g_strcmp0 (arch, "noarch") == 0)
			continue;
		pkg64 = asb_context_get_package_by_name_arch (ctx,
							      name,
							      "x86_64");
		if (pkg64 == NULL)
			continue;
		g_debug ("disabling alternate-arch %s as native exists %s",
			 asb_package_get_filename (pkg),
			 asb_package_get_filename (pkg64));
		asb_package_set_enabled (pkg, FALSE);
	}
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

	/* only process the newest packages */
	asb_context_disable_multiarch_pkgs (ctx);
	asb_context_disable_older_pkgs (ctx);

	/* add each package */
	g_print ("Processing packages...\n");
	for (guint i = 0; i < priv->packages->len; i++) {
		g_autoptr(AsbTask) task = NULL;
		AsbPackage *pkg = g_ptr_array_index (priv->packages, i);
		if (!asb_package_get_enabled (pkg)) {
			asb_package_log (pkg,
					 ASB_PACKAGE_LOG_LEVEL_DEBUG,
					 "%s is not enabled",
					 asb_package_get_nevr (pkg));
			asb_context_add_app_ignore (ctx, pkg);
			asb_package_log_flush (pkg, NULL);
			continue;
		}

		/* set locations of external resources */
		asb_package_set_config (pkg, "LogDir", priv->log_dir);
		asb_package_set_config (pkg, "TempDir", priv->temp_dir);
		asb_package_set_config (pkg, "IconsDir", priv->icons_dir);
		asb_package_set_config (pkg, "OutputDir", priv->output_dir);

		/* create task */
		task = asb_task_new (ctx);
		asb_task_set_package (task, pkg);

		/* run the task */
		if (!asb_task_process (task, error))
			return FALSE;
	}

	/* merge */
	g_print ("Merging applications...\n");
	asb_plugin_loader_merge (priv->plugin_loader, priv->apps);

	/* print any warnings */
	if ((priv->flags & ASB_CONTEXT_FLAG_IGNORE_MISSING_INFO) == 0) {
		if (!asb_context_detect_missing_data (ctx, error))
			return FALSE;
	}
	if ((priv->flags & ASB_CONTEXT_FLAG_IGNORE_MISSING_PARENTS) == 0) {
		if (!asb_context_detect_missing_parents (ctx, error))
			return FALSE;
	}
	if (!asb_context_detect_pkgname_dups (ctx, error))
		return FALSE;
	if (!asb_context_convert_icons (ctx, error))
		return FALSE;
	if (!asb_context_save_resources (ctx, error))
		return FALSE;

	/* write the application XML to the log file */
	asb_context_write_app_xml (ctx);

	/* write XML file */
	if (!asb_context_write_xml (ctx, error))
		return FALSE;

	/* write XML file */
	if (!asb_context_write_xml_fail (ctx, error))
		return FALSE;

	/* write XML file */
	if (!asb_context_write_xml_ignore (ctx, error))
		return FALSE;

	/* write icons archive */
	if (!asb_context_write_icons (ctx, priv->temp_dir, error))
		return FALSE;

	/* write screenshots archive */
	if (!asb_context_write_screenshots (ctx, priv->temp_dir, error))
		return FALSE;

	/* ensure all packages are flushed */
	for (guint i = 0; i < priv->packages->len; i++) {
		AsbPackage *pkg = g_ptr_array_index (priv->packages, i);
		if (!asb_package_log_flush (pkg, error))
			return FALSE;
	}
	return TRUE;
}

/**
 * asb_context_find_in_cache:
 * @ctx: A #AsbContext
 * @filename: cache-id
 *
 * This function used to find an application in the cache, and now does nothing.
 *
 * Returns: always %FALSE
 *
 * Since: 0.1.0
 **/
gboolean
asb_context_find_in_cache (AsbContext *ctx, const gchar *filename)
{
	return FALSE;
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
		if (!asb_package_get_enabled (pkg))
			continue;
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

void
asb_context_add_app_ignore (AsbContext *ctx, AsbPackage *pkg)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);
	g_autofree gchar *name_arch = NULL;
	g_autoptr(AsApp) app = NULL;

	/* never encountered before, so add */
	app = as_app_new ();
	name_arch = g_strdup_printf ("%s.%s",
				     asb_package_get_name (pkg),
				     asb_package_get_arch (pkg));
	as_app_set_id (app, name_arch);
	as_app_add_pkgname (app, asb_package_get_name (pkg));
	as_store_add_app (priv->store_ignore, app);
}

static void
asb_context_finalize (GObject *object)
{
	AsbContext *ctx = ASB_CONTEXT (object);
	AsbContextPrivate *priv = GET_PRIVATE (ctx);

	g_object_unref (priv->store_failed);
	g_object_unref (priv->store_ignore);
	g_object_unref (priv->plugin_loader);
	g_ptr_array_unref (priv->packages);
	g_list_foreach (priv->apps, (GFunc) g_object_unref, NULL);
	g_list_free (priv->apps);
	if (priv->file_globs != NULL)
		g_ptr_array_unref (priv->file_globs);
	g_mutex_clear (&priv->apps_mutex);
	g_free (priv->log_dir);
	g_free (priv->cache_dir);
	g_free (priv->temp_dir);
	g_free (priv->output_dir);
	g_free (priv->icons_dir);
	g_free (priv->basename);
	g_free (priv->origin);

	G_OBJECT_CLASS (asb_context_parent_class)->finalize (object);
}

static void
asb_context_init (AsbContext *ctx)
{
	AsbContextPrivate *priv = GET_PRIVATE (ctx);

	priv->plugin_loader = asb_plugin_loader_new (ctx);
	priv->packages = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	g_mutex_init (&priv->apps_mutex);
	priv->store_failed = as_store_new ();
	priv->store_ignore = as_store_new ();
	priv->min_icon_size = 32;
}

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
