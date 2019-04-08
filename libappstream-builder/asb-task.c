/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2015 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:asb-task
 * @short_description: One specific task for building metadata.
 * @stability: Unstable
 *
 * This object represents a single task, typically a package which is created
 * and then processed. Typically this is done in a number of threads.
 */

#include "config.h"

#include "asb-context-private.h"
#include "asb-task.h"
#include "asb-package.h"
#include "asb-utils.h"
#include "asb-plugin.h"
#include "asb-plugin-loader.h"

typedef struct
{
	AsbContext		*ctx;
	AsbPackage		*pkg;
	GPtrArray		*plugins_to_run;
	gchar			*filename;
	gchar			*tmpdir;
} AsbTaskPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsbTask, asb_task, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (asb_task_get_instance_private (o))

static void
asb_task_add_suitable_plugins (AsbTask *task)
{
	AsbPluginLoader *plugin_loader;
	AsbPlugin *plugin;
	AsbTaskPrivate *priv = GET_PRIVATE (task);
	gchar **filelist;
	guint i;
	guint j;

	filelist = asb_package_get_filelist (priv->pkg);
	if (filelist == NULL)
		return;
	plugin_loader = asb_context_get_plugin_loader (priv->ctx);
	for (i = 0; filelist[i] != NULL; i++) {
		plugin = asb_plugin_loader_match_fn (plugin_loader, filelist[i]);
		if (plugin == NULL)
			continue;

		/* check not already added */
		for (j = 0; j < priv->plugins_to_run->len; j++) {
			if (g_ptr_array_index (priv->plugins_to_run, j) == plugin)
				break;
		}

		/* add */
		if (j == priv->plugins_to_run->len)
			g_ptr_array_add (priv->plugins_to_run, plugin);
	}
}

static gboolean
asb_task_explode_extra_package (AsbTask *task,
				const gchar *pkg_name,
				gboolean require_same_srpm,
				GError **error)
{
	AsbTaskPrivate *priv = GET_PRIVATE (task);
	AsbPackage *pkg_extra;

	/* if not found, that's fine */
	pkg_extra = asb_context_find_by_pkgname (priv->ctx, pkg_name);
	if (pkg_extra == NULL)
		return TRUE;

	/* check it's from the same source package */
	if (!asb_package_ensure (pkg_extra,
				 ASB_PACKAGE_ENSURE_SOURCE,
				 error))
		return FALSE;
	if (require_same_srpm &&
	    (g_strcmp0 (asb_package_get_source (pkg_extra),
		        asb_package_get_source (priv->pkg)) != 0))
		return TRUE;
	g_debug ("decompressing extra pkg %s", asb_package_get_name (pkg_extra));
	asb_package_log (priv->pkg,
			 ASB_PACKAGE_LOG_LEVEL_DEBUG,
			 "Adding extra package %s for %s",
			 asb_package_get_name (pkg_extra),
			 asb_package_get_name (priv->pkg));
	if (!asb_package_ensure (pkg_extra,
				 ASB_PACKAGE_ENSURE_FILES |
				 ASB_PACKAGE_ENSURE_DEPS,
				 error))
		return FALSE;
	if (!asb_package_explode (pkg_extra, priv->tmpdir,
				  asb_context_get_file_globs (priv->ctx),
				  error))
		return FALSE;

	/* free resources */
	if (!asb_package_close (pkg_extra, error))
		return FALSE;
	asb_package_clear (pkg_extra,
			   ASB_PACKAGE_ENSURE_DEPS |
			   ASB_PACKAGE_ENSURE_FILES);

	return TRUE;
}

typedef struct {
	GPtrArray	*results;
	GHashTable	*results_hash;
} AsbTaskExtraDeps;

static void
asb_task_extra_deps_free (AsbTaskExtraDeps *extra_deps)
{
	g_ptr_array_unref (extra_deps->results);
	g_hash_table_unref (extra_deps->results_hash);
	g_slice_free (AsbTaskExtraDeps, extra_deps);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(AsbTaskExtraDeps, asb_task_extra_deps_free);

static gboolean
asb_task_get_extra_deps_recursive (AsbTask *task,
                                   const gchar *dep,
                                   AsbTaskExtraDeps *extra_deps,
                                   GError **error)
{
	AsbTaskPrivate *priv = GET_PRIVATE (task);
	AsbPackage *subpkg;
	GPtrArray *subpkg_deps;

	subpkg = asb_context_find_by_pkgname (priv->ctx, dep);
	if (subpkg == NULL)
		return TRUE;

	if (!asb_package_ensure (subpkg,
	                         ASB_PACKAGE_ENSURE_DEPS |
	                         ASB_PACKAGE_ENSURE_SOURCE,
	                         error))
		return FALSE;

	/* check it's from the same source package */
	if (g_strcmp0 (asb_package_get_source (subpkg),
	               asb_package_get_source (priv->pkg)) != 0)
		return TRUE;

	subpkg_deps = asb_package_get_deps (subpkg);
	for (guint i = 0; i < subpkg_deps->len; i++) {
		const gchar *subpkg_dep = g_ptr_array_index (subpkg_deps, i);

		/* already processed? */
		if (g_hash_table_lookup (extra_deps->results_hash, subpkg_dep) != NULL)
			continue;

		/* process recursively */
		g_hash_table_insert (extra_deps->results_hash,
				     g_strdup (subpkg_dep),
				     GINT_TO_POINTER (1));
		if (!asb_task_get_extra_deps_recursive (task, subpkg_dep, extra_deps, error))
			return FALSE;

		/* add to results */
		g_ptr_array_add (extra_deps->results, g_strdup (subpkg_dep));
	}

	return TRUE;
}

static gboolean
asb_task_add_extra_deps (AsbTask *task, GError **error)
{
	AsbTaskPrivate *priv = GET_PRIVATE (task);
	GPtrArray *deps;
	g_autoptr(AsbTaskExtraDeps) extra_deps = NULL;

	extra_deps = g_slice_new0 (AsbTaskExtraDeps);
	extra_deps->results = g_ptr_array_new_with_free_func (g_free);
	extra_deps->results_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	/* recursively get extra package deps */
	deps = asb_package_get_deps (priv->pkg);
	for (guint i = 0; i < deps->len; i++) {
		const gchar *dep = g_ptr_array_index (deps, i);
		if (!asb_task_get_extra_deps_recursive (task, dep, extra_deps, error))
			return FALSE;
	}

	/* copy all the extra package deps into the main package */
	for (guint i = 0; i < extra_deps->results->len; i++) {
		const gchar *extra_dep = g_ptr_array_index (extra_deps->results, i);
		asb_package_add_dep (priv->pkg, extra_dep);
	}

	return TRUE;
}

static gboolean
asb_task_explode_extra_packages (AsbTask *task, GError **error)
{
	AsbTaskPrivate *priv = GET_PRIVATE (task);
	GPtrArray *deps;
	const gchar *ignore[] = { "rtld", NULL };
	const gchar *tmp;
	guint i;
	g_autoptr(GHashTable) hash = NULL;
	g_autoptr(GPtrArray) array = NULL;
	g_autoptr(GPtrArray) icon_themes = NULL;

	/* recursively copy all the extra package deps into the main package */
	if (!asb_task_add_extra_deps (task, error))
		return FALSE;

	/* anything the package requires */
	hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	for (i = 0; ignore[i] != NULL; i++) {
		g_hash_table_insert (hash,
				     g_strdup (ignore[i]),
				     GINT_TO_POINTER (1));
	}
	array = g_ptr_array_new_with_free_func (g_free);
	icon_themes = g_ptr_array_new_with_free_func (g_free);
	deps = asb_package_get_deps (priv->pkg);
	for (i = 0; i < deps->len; i++) {
		tmp = g_ptr_array_index (deps, i);
		if (g_strstr_len (tmp, -1, " ") != NULL)
			continue;
		if (g_strstr_len (tmp, -1, ".so") != NULL)
			continue;
		if (g_str_has_prefix (tmp, "/"))
			continue;
		if (g_hash_table_lookup (hash, tmp) != NULL)
			continue;
		if (g_strcmp0 (tmp, asb_package_get_name (priv->pkg)) == 0)
			continue;

		/* if an app depends on kde-runtime, that means the
		 * oxygen icon set is available to them */
		if (g_strcmp0 (tmp, "oxygen-icon-theme") == 0 ||
		    g_strcmp0 (tmp, "kde-runtime") == 0) {
			g_hash_table_insert (hash, g_strdup ("oxygen-icon-theme"),
					     GINT_TO_POINTER (1));
			g_ptr_array_add (icon_themes,
					 g_strdup ("oxygen-icon-theme"));
		/* Applications depending on yast2 have an implicit dependency
		 * on yast2-branding-openSUSE, which brings required icons in this case.
		 */
		} else if (g_strcmp0 (tmp, "yast2-branding-openSUSE") == 0 ||
			   g_strcmp0 (tmp, "yast2") == 0) {
			g_hash_table_insert (hash, g_strdup ("yast2-branding-openSUSE"),
					     GINT_TO_POINTER (1));
			g_ptr_array_add (icon_themes,
					 g_strdup ("yast2-branding-openSUSE"));
		} else {
			g_ptr_array_add (array, g_strdup (tmp));
		}
		g_hash_table_insert (hash, g_strdup (tmp), GINT_TO_POINTER (1));
	}

	/* explode any potential packages */
	for (i = 0; i < array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		if (!asb_task_explode_extra_package (task, tmp, TRUE, error))
			return FALSE;
	}

	/* explode any icon themes */
	for (i = 0; i < icon_themes->len; i++) {
		tmp = g_ptr_array_index (icon_themes, i);
		if (!asb_task_explode_extra_package (task, tmp, FALSE, error))
			return FALSE;
	}
	return TRUE;
}

/**
 * asb_task_process:
 * @task: A #AsbTask
 * @error: A #GError or %NULL
 *
 * Processes the task.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_task_process (AsbTask *task, GError **error)
{
	AsRelease *release;
	AsbApp *app;
	AsbPlugin *plugin = NULL;
	AsbTaskPrivate *priv = GET_PRIVATE (task);
	GList *apps = NULL;
	GPtrArray *array;
	gboolean ret;
	guint i;
	guint nr_added = 0;
	g_autofree gchar *basename = NULL;

	/* reset the profile timer */
	asb_package_log_start (priv->pkg);

	/* ensure nevra read */
	if (!asb_package_ensure (priv->pkg,
				 ASB_PACKAGE_ENSURE_NEVRA,
				 error))
		return FALSE;

	g_debug ("starting: %s", asb_package_get_name (priv->pkg));

	/* treat archive as a special case */
	if (g_str_has_suffix (priv->filename, ".cab")) {
		AsApp *app_tmp;
		GPtrArray *apps_tmp;
		g_autoptr(AsStore) store = as_store_new ();
		g_autoptr(GFile) file = g_file_new_for_path (priv->filename);
		if (!as_store_from_file (store, file, NULL, NULL, error)) {
			g_prefix_error (error, "Failed to parse %s: ",
					asb_package_get_filename (priv->pkg));
			return FALSE;
		}
		apps_tmp = as_store_get_apps (store);
		for (i = 0; i < apps_tmp->len; i++) {
			g_autoptr(AsbApp) app2 = NULL;
			app_tmp = AS_APP (g_ptr_array_index (apps_tmp, i));
			app2 = asb_app_new (priv->pkg, as_app_get_id (app_tmp));
			as_app_subsume (AS_APP (app2), app_tmp);
			asb_context_add_app (priv->ctx, app2);
			nr_added++;
		}
		g_debug ("added %u apps from archive", apps_tmp->len);
		goto skip;
	}

	/* ensure file list read */
	if (!asb_package_ensure (priv->pkg,
				 ASB_PACKAGE_ENSURE_FILES,
				 error))
		return FALSE;

	/* did we get a file match on any plugin */
	basename = g_path_get_basename (priv->filename);
	asb_package_log (priv->pkg,
			 ASB_PACKAGE_LOG_LEVEL_DEBUG,
			 "Getting filename match for %s",
			 basename);
	asb_task_add_suitable_plugins (task);
	if (priv->plugins_to_run->len == 0) {
		asb_context_add_app_ignore (priv->ctx, priv->pkg);
		asb_package_close (priv->pkg, NULL);
		asb_package_clear (priv->pkg,
				   ASB_PACKAGE_ENSURE_DEPS |
				   ASB_PACKAGE_ENSURE_FILES);
		return TRUE;
	}

	/* delete old tree if it exists */
	if (!asb_utils_ensure_exists_and_empty (priv->tmpdir, error)) {
		g_prefix_error (error, "Failed to clear: ");
		return FALSE;
	}

	/* explode tree */
	g_debug ("decompressing files: %s", asb_package_get_name (priv->pkg));
	asb_package_log (priv->pkg,
			 ASB_PACKAGE_LOG_LEVEL_DEBUG,
			 "Exploding tree for %s",
			 asb_package_get_name (priv->pkg));
	if (!asb_package_explode (priv->pkg,
				  priv->tmpdir,
				  asb_context_get_file_globs (priv->ctx),
				  error)) {
		g_prefix_error (error, "Failed to explode %s: ",
				asb_package_get_basename (priv->pkg));
		return FALSE;
	}

	/* add extra packages */
	if (!asb_package_ensure (priv->pkg,
				 ASB_PACKAGE_ENSURE_DEPS |
				 ASB_PACKAGE_ENSURE_SOURCE,
				 error))
		return FALSE;
	if (!asb_task_explode_extra_packages (task, error)) {
		g_prefix_error (error, "Failed to explode extra files: ");
		return FALSE;
	}

	/* run plugins */
	g_debug ("examining: %s", asb_package_get_name (priv->pkg));
	for (i = 0; i < priv->plugins_to_run->len; i++) {
		GList *apps_tmp = NULL;
		g_autoptr(GError) error_local = NULL;
		plugin = g_ptr_array_index (priv->plugins_to_run, i);
		asb_package_log (priv->pkg,
				 ASB_PACKAGE_LOG_LEVEL_DEBUG,
				 "Processing %s with %s",
				 basename,
				 plugin->name);
		apps_tmp = asb_plugin_process (plugin, priv->pkg, priv->tmpdir, &error_local);
		if (apps_tmp == NULL) {
			asb_package_log (priv->pkg,
					 ASB_PACKAGE_LOG_LEVEL_WARNING,
					 "Failed to run process '%s': %s",
					 plugin->name, error_local->message);
			continue;
		}
		for (GList *l = apps_tmp; l != NULL; l = l->next) {
			app = ASB_APP (l->data);
			asb_plugin_add_app (&apps, AS_APP (app));
		}
		g_list_free_full (apps_tmp, g_object_unref);
	}

	/* print */
	g_debug ("processing: %s", asb_package_get_name (priv->pkg));
	for (GList *l = apps; l != NULL; l = l->next) {
		g_autoptr(GError) error_local = NULL;
		app = l->data;

		/* never set */
		if (as_app_get_id (AS_APP (app)) == NULL) {
			asb_package_log (priv->pkg,
					 ASB_PACKAGE_LOG_LEVEL_INFO,
					 "app id not set for %s",
					 asb_package_get_name (priv->pkg));
			continue;
		}

		/* copy data from pkg into app */
		if (!asb_package_ensure (priv->pkg,
					 ASB_PACKAGE_ENSURE_LICENSE |
					 ASB_PACKAGE_ENSURE_RELEASES |
					 ASB_PACKAGE_ENSURE_VCS |
					 ASB_PACKAGE_ENSURE_URL,
					 error))
			return FALSE;
		if (asb_package_get_url (priv->pkg) != NULL &&
		    as_app_get_url_item (AS_APP (app), AS_URL_KIND_HOMEPAGE) == NULL) {
			as_app_add_url (AS_APP (app),
					AS_URL_KIND_HOMEPAGE,
					asb_package_get_url (priv->pkg));
		}
		if (asb_package_get_license (priv->pkg) != NULL &&
		    as_app_get_project_license (AS_APP (app)) == NULL) {
			as_app_set_project_license (AS_APP (app),
						    asb_package_get_license (priv->pkg));
		}

		/* add the source name so we can suggest these together */
		if (g_strcmp0 (asb_package_get_source_pkgname (priv->pkg),
			       asb_package_get_name (priv->pkg)) != 0) {
			as_app_set_source_pkgname (AS_APP (app),
						   asb_package_get_source_pkgname (priv->pkg));
		}

		/* set all the releases on the app */
		array = asb_package_get_releases (priv->pkg);
		if (as_app_get_kind (AS_APP (app)) != AS_APP_KIND_ADDON) {
			for (i = 0; i < array->len; i++) {
				release = g_ptr_array_index (array, i);
				as_app_add_release (AS_APP (app), release);
			}
		}

		/* run each refine plugin on each app */
		ret = asb_plugin_loader_process_app (asb_context_get_plugin_loader (priv->ctx),
						     priv->pkg,
						     app,
						     priv->tmpdir,
						     &error_local);
		if (!ret) {
			asb_package_log (priv->pkg,
					 ASB_PACKAGE_LOG_LEVEL_WARNING,
					 "Failed to run process on %s: %s",
					 as_app_get_id (AS_APP (app)),
					 error_local->message);
			continue;
		}

		/* set the VCS information into the metadata */
		if (asb_package_get_vcs (priv->pkg) != NULL) {
			as_app_add_metadata (AS_APP (app),
					     "VersionControlSystem",
					     asb_package_get_vcs (priv->pkg));
		}

		/* save any screenshots early */
		if (array->len == 0) {
			if (!asb_app_save_resources (ASB_APP (app),
						     ASB_APP_SAVE_FLAG_SCREENSHOTS,
						     error))
				return FALSE;
		}

		/* all okay */
		asb_context_add_app (priv->ctx, app);
		nr_added++;
	}
skip:
	/* delete tree */
	g_debug ("deleting temp files: %s", asb_package_get_name (priv->pkg));
	if (!asb_utils_rmtree (priv->tmpdir, error)) {
		g_prefix_error (error, "Failed to delete tree: ");
		return FALSE;
	}

	/* write log */
	g_debug ("writing log: %s", asb_package_get_name (priv->pkg));
	if (!asb_package_log_flush (priv->pkg, error)) {
		g_prefix_error (error, "Failed to write package log: ");
		return FALSE;
	}

	/* clear loaded resources */
	asb_package_close (priv->pkg, NULL);
	asb_package_clear (priv->pkg,
			   ASB_PACKAGE_ENSURE_DEPS |
			   ASB_PACKAGE_ENSURE_FILES);
	g_list_free_full (apps, (GDestroyNotify) g_object_unref);
	return TRUE;
}

static void
asb_task_finalize (GObject *object)
{
	AsbTask *task = ASB_TASK (object);
	AsbTaskPrivate *priv = GET_PRIVATE (task);

	g_object_unref (priv->ctx);
	g_ptr_array_unref (priv->plugins_to_run);
	if (priv->pkg != NULL)
		g_object_unref (priv->pkg);
	g_free (priv->filename);
	g_free (priv->tmpdir);

	G_OBJECT_CLASS (asb_task_parent_class)->finalize (object);
}

/**
 * asb_task_set_package:
 * @task: A #AsbTask
 * @pkg: A #AsbPackage
 *
 * Sets the package used for the task.
 *
 * Since: 0.1.0
 **/
void
asb_task_set_package (AsbTask *task, AsbPackage *pkg)
{
	AsbTaskPrivate *priv = GET_PRIVATE (task);
	priv->tmpdir = g_build_filename (asb_context_get_temp_dir (priv->ctx),
					 asb_package_get_nevr (pkg), NULL);
	priv->filename = g_strdup (asb_package_get_filename (pkg));
	priv->pkg = g_object_ref (pkg);
}

static void
asb_task_init (AsbTask *task)
{
	AsbTaskPrivate *priv = GET_PRIVATE (task);
	priv->plugins_to_run = g_ptr_array_new ();
}

static void
asb_task_class_init (AsbTaskClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asb_task_finalize;
}

/**
 * asb_task_new:
 * @ctx: A #AsbContext
 *
 * Creates a new task.
 *
 * Returns: A #AsbTask
 *
 * Since: 0.1.0
 **/
AsbTask *
asb_task_new (AsbContext *ctx)
{
	AsbTask *task;
	AsbTaskPrivate *priv;
	task = g_object_new (ASB_TYPE_TASK, NULL);
	priv = GET_PRIVATE (task);
	priv->ctx = g_object_ref (ctx);
	return ASB_TASK (task);
}
