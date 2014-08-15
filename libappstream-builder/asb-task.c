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
 * SECTION:asb-task
 * @short_description: One specific task for building metadata.
 * @stability: Unstable
 *
 * Thsi object represents a single task, typically a package which is created
 * and then processed. Typically this is done in a number of threads.
 */

#include "config.h"

#include "as-cleanup.h"
#include "asb-context-private.h"
#include "asb-task.h"
#include "asb-package.h"
#include "asb-utils.h"
#include "asb-plugin.h"
#include "asb-plugin-loader.h"

typedef struct _AsbTaskPrivate	AsbTaskPrivate;
struct _AsbTaskPrivate
{
	AsbContext		*ctx;
	AsbPackage		*pkg;
	AsbPanel		*panel;
	GPtrArray		*plugins_to_run;
	gchar			*filename;
	gchar			*tmpdir;
	guint			 id;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsbTask, asb_task, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (asb_task_get_instance_private (o))

/**
 * asb_task_add_suitable_plugins:
 **/
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

/**
 * asb_task_explode_extra_package:
 **/
static gboolean
asb_task_explode_extra_package (AsbTask *task,
				const gchar *pkg_name,
				gboolean require_same_srpm)
{
	AsbTaskPrivate *priv = GET_PRIVATE (task);
	AsbPackage *pkg_extra;
	gboolean ret = TRUE;
	_cleanup_error_free_ GError *error = NULL;

	/* if not found, that's fine */
	pkg_extra = asb_context_find_by_pkgname (priv->ctx, pkg_name);
	if (pkg_extra == NULL)
		return TRUE;

	/* check it's from the same source package */
	if (require_same_srpm &&
	    (g_strcmp0 (asb_package_get_source (pkg_extra),
		        asb_package_get_source (priv->pkg)) != 0))
		return TRUE;

	asb_panel_set_status (priv->panel, "Decompressing extra pkg %s",
			      asb_package_get_name (pkg_extra));
	asb_package_log (priv->pkg,
			 ASB_PACKAGE_LOG_LEVEL_INFO,
			 "Adding extra package %s for %s",
			 asb_package_get_name (pkg_extra),
			 asb_package_get_name (priv->pkg));
	ret = asb_package_explode (pkg_extra, priv->tmpdir,
				   asb_context_get_file_globs (priv->ctx),
				   &error);
	if (!ret) {
		asb_package_log (priv->pkg,
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "Failed to explode extra file: %s",
				 error->message);
	}
	return ret;
}

/**
 * asb_task_explode_extra_packages:
 **/
static gboolean
asb_task_explode_extra_packages (AsbTask *task)
{
	AsbTaskPrivate *priv = GET_PRIVATE (task);
	const gchar *ignore[] = { "rtld", NULL };
	const gchar *tmp;
	gchar **deps;
	guint i;
	_cleanup_hashtable_unref_ GHashTable *hash;
	_cleanup_ptrarray_unref_ GPtrArray *array;
	_cleanup_ptrarray_unref_ GPtrArray *icon_themes;

	/* anything the package requires */
	hash = g_hash_table_new (g_str_hash, g_str_equal);
	for (i = 0; ignore[i] != NULL; i++) {
		g_hash_table_insert (hash,
				     (gchar *) ignore[i],
				     GINT_TO_POINTER (1));
	}
	array = g_ptr_array_new_with_free_func (g_free);
	icon_themes = g_ptr_array_new_with_free_func (g_free);
	deps = asb_package_get_deps (priv->pkg);
	for (i = 0; deps[i] != NULL; i++) {
		if (g_strstr_len (deps[i], -1, " ") != NULL)
			continue;
		if (g_strstr_len (deps[i], -1, ".so") != NULL)
			continue;
		if (g_str_has_prefix (deps[i], "/"))
			continue;
		if (g_hash_table_lookup (hash, deps[i]) != NULL)
			continue;
		/* if an app depends on kde-runtime, that means the
		 * oxygen icon set is available to them */
		if (g_strcmp0 (deps[i], "oxygen-icon-theme") == 0 ||
		    g_strcmp0 (deps[i], "kde-runtime") == 0) {
			g_hash_table_insert (hash, (gpointer) "oxygen-icon-theme",
					     GINT_TO_POINTER (1));
			g_ptr_array_add (icon_themes,
					 g_strdup ("oxygen-icon-theme"));
		} else {
			g_ptr_array_add (array, g_strdup (deps[i]));
		}
		g_hash_table_insert (hash, deps[i], GINT_TO_POINTER (1));
	}

	/* explode any potential packages */
	for (i = 0; i < array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		if (!asb_task_explode_extra_package (task, tmp, TRUE))
			return FALSE;
	}

	/* explode any icon themes */
	for (i = 0; i < icon_themes->len; i++) {
		tmp = g_ptr_array_index (icon_themes, i);
		if (!asb_task_explode_extra_package (task, tmp, FALSE))
			return FALSE;
	}
	return TRUE;
}

/**
 * asb_task_process:
 * @task: A #AsbTask
 * @error_not_used: A #GError or %NULL
 *
 * Processes the task.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_task_process (AsbTask *task, GError **error_not_used)
{
	AsRelease *release;
	AsbApp *app;
	AsbPlugin *plugin = NULL;
	AsbTaskPrivate *priv = GET_PRIVATE (task);
	GList *apps = NULL;
	GList *l;
	GPtrArray *array;
	gboolean ret;
	gchar *cache_id;
	gchar *tmp;
	guint i;
	guint nr_added = 0;
	_cleanup_error_free_ GError *error = NULL;
	_cleanup_free_ gchar *basename = NULL;

	/* reset the profile timer */
	asb_package_log_start (priv->pkg);

	asb_panel_set_job_number (priv->panel, priv->id + 1);
	asb_panel_set_title (priv->panel, asb_package_get_name (priv->pkg));
	asb_panel_set_status (priv->panel, "Starting");

	/* did we get a file match on any plugin */
	basename = g_path_get_basename (priv->filename);
	asb_package_log (priv->pkg,
			 ASB_PACKAGE_LOG_LEVEL_DEBUG,
			 "Getting filename match for %s",
			 basename);
	asb_task_add_suitable_plugins (task);
	if (priv->plugins_to_run->len == 0) {
		asb_context_add_app_dummy (priv->ctx, priv->pkg);
		goto out;
	}

	/* delete old tree if it exists */
	ret = asb_utils_ensure_exists_and_empty (priv->tmpdir, &error);
	if (!ret) {
		asb_package_log (priv->pkg,
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "Failed to clear: %s", error->message);
		goto out;
	}

	/* explode tree */
	asb_panel_set_status (priv->panel, "Decompressing files");
	asb_package_log (priv->pkg,
			 ASB_PACKAGE_LOG_LEVEL_DEBUG,
			 "Exploding tree for %s",
			 asb_package_get_name (priv->pkg));
	ret = asb_package_explode (priv->pkg,
				   priv->tmpdir,
				   asb_context_get_file_globs (priv->ctx),
				   &error);
	if (!ret) {
		asb_package_log (priv->pkg,
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "Failed to explode: %s", error->message);
		g_clear_error (&error);
		goto skip;
	}

	/* add extra packages */
	ret = asb_task_explode_extra_packages (task);
	if (!ret)
		goto skip;

	/* run plugins */
	asb_panel_set_status (priv->panel, "Examining");
	for (i = 0; i < priv->plugins_to_run->len; i++) {
		plugin = g_ptr_array_index (priv->plugins_to_run, i);
		asb_package_log (priv->pkg,
				 ASB_PACKAGE_LOG_LEVEL_DEBUG,
				 "Processing %s with %s",
				 basename,
				 plugin->name);
		apps = asb_plugin_process (plugin, priv->pkg, priv->tmpdir, &error);
		if (apps == NULL) {
			asb_package_log (priv->pkg,
					 ASB_PACKAGE_LOG_LEVEL_WARNING,
					 "Failed to run process '%s': %s",
					 plugin->name, error->message);
			g_clear_error (&error);
		}
	}
	if (apps == NULL)
		goto skip;

	/* print */
	asb_panel_set_status (priv->panel, "Processing");
	for (l = apps; l != NULL; l = l->next) {
		app = l->data;

		/* never set */
		if (as_app_get_id_full (AS_APP (app)) == NULL) {
			asb_package_log (priv->pkg,
					 ASB_PACKAGE_LOG_LEVEL_INFO,
					 "app id not set for %s",
					 asb_package_get_name (priv->pkg));
			continue;
		}

		/* copy data from pkg into app */
		if (asb_package_get_url (priv->pkg) != NULL) {
			as_app_add_url (AS_APP (app),
					AS_URL_KIND_HOMEPAGE,
					asb_package_get_url (priv->pkg), -1);
		}
		if (asb_package_get_license (priv->pkg) != NULL)
			as_app_set_project_license (AS_APP (app),
						    asb_package_get_license (priv->pkg),
						    -1);

		/* add the source name so we can suggest these together */
		if (g_strcmp0 (asb_package_get_source_pkgname (priv->pkg),
			       asb_package_get_name (priv->pkg)) != 0) {
			as_app_set_source_pkgname (AS_APP (app),
						   asb_package_get_source_pkgname (priv->pkg),
						   -1);
		}

		/* set all the releases on the app */
		array = asb_package_get_releases (priv->pkg);
		for (i = 0; i < array->len; i++) {
			release = g_ptr_array_index (array, i);
			as_app_add_release (AS_APP (app), release);
		}

		/* run each refine plugin on each app */
		ret = asb_plugin_loader_process_app (asb_context_get_plugin_loader (priv->ctx),
						     priv->pkg,
						     app,
						     priv->tmpdir,
						     &error);
		if (!ret) {
			asb_package_log (priv->pkg,
					 ASB_PACKAGE_LOG_LEVEL_WARNING,
					 "Failed to run process on %s: %s",
					 as_app_get_id (AS_APP (app)),
					 error->message);
			g_clear_error (&error);
			goto skip;
		}

		/* don't include components that have no name or comment */
		if (as_app_get_name (AS_APP (app), "C") == NULL)
			as_app_add_veto (AS_APP (app), "No 'Name' in desktop or <name> in AppData");
		if (as_app_get_comment (AS_APP (app), "C") == NULL)
			as_app_add_veto (AS_APP (app), "No 'Comment' in desktop or <summary> in AppData");

		/* don't include apps that have no icon */
		if (as_app_get_id_kind (AS_APP (app)) != AS_ID_KIND_ADDON) {
			if (as_app_get_icon (AS_APP (app)) == NULL)
				as_app_add_veto (AS_APP (app), "Has no Icon");
		}

		/* veto apps that *still* require appdata */
		array = asb_app_get_requires_appdata (app);
		for (i = 0; i < array->len; i++) {
			tmp = g_ptr_array_index (array, i);
			if (tmp == NULL) {
				as_app_add_veto (AS_APP (app), "Required AppData");
				continue;
			}
			as_app_add_veto (AS_APP (app), "Required AppData: %s", tmp);
		}

		/* list all the reasons we're ignoring the app */
		asb_app_set_veto_description (app);

		/* save icon and screenshots */
		if (array->len == 0) {
			ret = asb_app_save_resources (app, &error);
			if (!ret) {
				asb_package_log (priv->pkg,
						 ASB_PACKAGE_LOG_LEVEL_WARNING,
						 "Failed to save resources: %s",
						 error->message);
				g_clear_error (&error);
				goto skip;
			}
		}

		/* set cache-id in case we want to use the metadata directly */
		if (asb_context_get_add_cache_id (priv->ctx)) {
			cache_id = asb_utils_get_cache_id_for_filename (priv->filename);
			as_app_add_metadata (AS_APP (app),
					     "X-CacheID",
					     cache_id, -1);
			g_free (cache_id);
		}

		/* all okay */
		asb_context_add_app (priv->ctx, app);
		nr_added++;

		/* log the XML in the log file */
		tmp = asb_app_to_xml (app);
		asb_package_log (priv->pkg, ASB_PACKAGE_LOG_LEVEL_NONE, "%s", tmp);
		g_free (tmp);
	}
skip:
	/* add a dummy element to the AppStream metadata so that we don't keep
	 * parsing this every time */
	if (asb_context_get_add_cache_id (priv->ctx) && nr_added == 0)
		asb_context_add_app_dummy (priv->ctx, priv->pkg);

	/* delete tree */
	asb_panel_set_status (priv->panel, "Deleting temp files");
	if (!asb_utils_rmtree (priv->tmpdir, &error)) {
		asb_package_log (priv->pkg,
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "Failed to delete tree: %s",
				 error->message);
		goto out;
	}

	/* write log */
	asb_panel_set_status (priv->panel, "Writing log");
	if (!asb_package_log_flush (priv->pkg, &error)) {
		asb_package_log (priv->pkg,
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "Failed to write package log: %s",
				 error->message);
		goto out;
	}

	/* update UI */
	asb_panel_remove (priv->panel);
out:
	g_list_free_full (apps, (GDestroyNotify) g_object_unref);
	return TRUE;
}

/**
 * asb_task_finalize:
 **/
static void
asb_task_finalize (GObject *object)
{
	AsbTask *task = ASB_TASK (object);
	AsbTaskPrivate *priv = GET_PRIVATE (task);

	g_object_unref (priv->ctx);
	g_ptr_array_unref (priv->plugins_to_run);
	if (priv->pkg != NULL)
		g_object_unref (priv->pkg);
	if (priv->panel != NULL)
		g_object_unref (priv->panel);
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

/**
 * asb_task_set_panel: (skip):
 * @task: A #AsbTask
 * @panel: A #AsbPanel
 *
 * Sets the panel used for the task.
 *
 * Since: 0.2.3
 **/
void
asb_task_set_panel (AsbTask *task, AsbPanel *panel)
{
	AsbTaskPrivate *priv = GET_PRIVATE (task);
	priv->panel = g_object_ref (panel);
}

/**
 * asb_task_set_id:
 * @task: A #AsbTask
 * @id: numeric identifier
 *
 * Sets the ID to use for the task.
 *
 * Since: 0.1.0
 **/
void
asb_task_set_id (AsbTask *task, guint id)
{
	AsbTaskPrivate *priv = GET_PRIVATE (task);
	priv->id = id;
}

/**
 * asb_task_init:
 **/
static void
asb_task_init (AsbTask *task)
{
	AsbTaskPrivate *priv = GET_PRIVATE (task);
	priv->plugins_to_run = g_ptr_array_new ();
}

/**
 * asb_task_class_init:
 **/
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
