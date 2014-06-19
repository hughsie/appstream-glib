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
asb_task_explode_extra_package (AsbTask *task, const gchar *pkg_name)
{
	AsbTaskPrivate *priv = GET_PRIVATE (task);
	AsbPackage *pkg_extra;
	gboolean ret = TRUE;
	_cleanup_error_free_ GError *error = NULL;

	/* if not found, that's fine */
	pkg_extra = asb_context_find_by_pkgname (priv->ctx, pkg_name);
	if (pkg_extra == NULL)
		return TRUE;
	asb_package_log (priv->pkg,
			 ASB_PACKAGE_LOG_LEVEL_DEBUG,
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
	const gchar *tmp;
	guint i;
	_cleanup_ptrarray_unref_ GPtrArray *array;

	/* anything hardcoded */
	array = g_ptr_array_new_with_free_func (g_free);
	tmp = asb_context_get_extra_package (priv->ctx, asb_package_get_name (priv->pkg));
	if (tmp != NULL)
		g_ptr_array_add (array, g_strdup (tmp));

	/* add all variants of %NAME-common, %NAME-data etc */
	tmp = asb_package_get_name (priv->pkg);
	g_ptr_array_add (array, g_strdup_printf ("%s-data", tmp));
	g_ptr_array_add (array, g_strdup_printf ("%s-common", tmp));
	for (i = 0; i < array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		if (!asb_task_explode_extra_package (task, tmp))
			return FALSE;
	}
	return TRUE;
}

/**
 * asb_task_check_urls:
 **/
static void
asb_task_check_urls (AsApp *app, AsbPackage *pkg)
{
	const gchar *url;
	gboolean ret;
	guint i;
	_cleanup_error_free_ GError *error = NULL;

	for (i = 0; i < AS_URL_KIND_LAST; i++) {
		url = as_app_get_url_item (app, i);
		if (url == NULL)
			continue;
		ret = as_utils_check_url_exists (url, 5, &error);
		if (!ret) {
			asb_package_log (pkg,
					 ASB_PACKAGE_LOG_LEVEL_WARNING,
					 "%s URL %s invalid: %s",
					 as_url_kind_to_string (i),
					 url,
					 error->message);
			g_clear_error (&error);
		}
	}
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
	const gchar * const *kudos;
	gboolean ret;
	gboolean valid;
	gchar *cache_id;
	gchar *tmp;
	guint i;
	guint nr_added = 0;
	_cleanup_error_free_ GError *error = NULL;
	_cleanup_free_ gchar *basename = NULL;

	/* reset the profile timer */
	asb_package_log_start (priv->pkg);

	/* did we get a file match on any plugin */
	basename = g_path_get_basename (priv->filename);
	asb_package_log (priv->pkg,
			 ASB_PACKAGE_LOG_LEVEL_DEBUG,
			 "Getting filename match for %s",
			 basename);
	asb_task_add_suitable_plugins (task);
	if (priv->plugins_to_run->len == 0)
		goto out;

	/* delete old tree if it exists */
	if (!asb_context_get_use_package_cache (priv->ctx)) {
		ret = asb_utils_ensure_exists_and_empty (priv->tmpdir, &error);
		if (!ret) {
			asb_package_log (priv->pkg,
					 ASB_PACKAGE_LOG_LEVEL_WARNING,
					 "Failed to clear: %s", error->message);
			goto out;
		}
	}

	/* explode tree */
	asb_package_log (priv->pkg,
			 ASB_PACKAGE_LOG_LEVEL_DEBUG,
			 "Exploding tree for %s",
			 asb_package_get_name (priv->pkg));
	if (!asb_context_get_use_package_cache (priv->ctx) ||
	    !g_file_test (priv->tmpdir, G_FILE_TEST_EXISTS)) {
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
	}

	/* run plugins */
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
					 "Failed to run process: %s",
					 error->message);
			g_clear_error (&error);
		}
	}
	if (apps == NULL)
		goto skip;

	/* print */
	for (l = apps; l != NULL; l = l->next) {
		app = l->data;

		/* all apps assumed to be okay */
		valid = TRUE;

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
			asb_app_add_veto (app, "Has no Name");
		if (as_app_get_comment (AS_APP (app), "C") == NULL)
			asb_app_add_veto (app, "Has no Comment");

		/* don't include apps that have no icon */
		if (as_app_get_id_kind (AS_APP (app)) != AS_ID_KIND_ADDON) {
			if (as_app_get_icon (AS_APP (app)) == NULL)
				asb_app_add_veto (app, "Has no Icon");
		}

		/* list all the reasons we're ignoring the app */
		array = asb_app_get_vetos (app);
		if (array->len > 0) {
			asb_package_log (priv->pkg,
					 ASB_PACKAGE_LOG_LEVEL_WARNING,
					 "%s not included in metadata:",
					 as_app_get_id_full (AS_APP (app)));
			for (i = 0; i < array->len; i++) {
				tmp = g_ptr_array_index (array, i);
				asb_package_log (priv->pkg,
						 ASB_PACKAGE_LOG_LEVEL_WARNING,
						 " - %s", tmp);
			}
			valid = FALSE;
		}

		/* don't include apps that *still* require appdata */
		array = asb_app_get_requires_appdata (app);
		if (array->len > 0) {
			asb_package_log (priv->pkg,
					 ASB_PACKAGE_LOG_LEVEL_WARNING,
					 "%s required appdata but none provided",
					 as_app_get_id_full (AS_APP (app)));
			for (i = 0; i < array->len; i++) {
				tmp = g_ptr_array_index (array, i);
				if (tmp == NULL)
					continue;
				asb_package_log (priv->pkg,
						 ASB_PACKAGE_LOG_LEVEL_WARNING,
						 " - %s", tmp);
			}
			valid = FALSE;
		}
		if (!valid)
			continue;

		/* verify URLs still exist */
		if (asb_context_get_extra_checks (priv->ctx))
			asb_task_check_urls (AS_APP (app), priv->pkg);

		/* save icon and screenshots */
		ret = asb_app_save_resources (app, &error);
		if (!ret) {
			asb_package_log (priv->pkg,
					 ASB_PACKAGE_LOG_LEVEL_WARNING,
					 "Failed to save resources: %s",
					 error->message);
			g_clear_error (&error);
			goto skip;
		}

		/* print Kudos the might have */
		kudos = as_util_get_possible_kudos ();
		for (i = 0; kudos[i] != NULL; i++) {
			if (as_app_get_metadata_item (AS_APP (app), kudos[i]) != NULL)
				continue;
			asb_package_log (priv->pkg,
					 ASB_PACKAGE_LOG_LEVEL_INFO,
					 "Application does not have %s",
					 kudos[i]);
		}

		/* set cache-id in case we want to use the metadata directly */
		if (asb_context_get_add_cache_id (priv->ctx)) {
			cache_id = asb_utils_get_cache_id_for_filename (priv->filename);
			as_app_add_metadata (AS_APP (app),
					     "X-CreaterepoAsCacheID",
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
	if (asb_context_get_add_cache_id (priv->ctx) && nr_added == 0) {
		_cleanup_object_unref_ AsApp *dummy;
		dummy = as_app_new ();
		as_app_set_id_full (dummy, asb_package_get_name (priv->pkg), -1);
		cache_id = asb_utils_get_cache_id_for_filename (priv->filename);
		as_app_add_metadata (dummy,
				     "X-CreaterepoAsCacheID",
				     cache_id, -1);
		asb_context_add_app (priv->ctx, (AsbApp *) dummy);
		g_free (cache_id);
	}

	/* delete tree */
	if (!asb_context_get_use_package_cache (priv->ctx)) {
		if (!asb_utils_rmtree (priv->tmpdir, &error)) {
			asb_package_log (priv->pkg,
					 ASB_PACKAGE_LOG_LEVEL_WARNING,
					 "Failed to delete tree: %s",
					 error->message);
			goto out;
		}
	}

	/* write log */
	if (!asb_package_log_flush (priv->pkg, &error)) {
		asb_package_log (priv->pkg,
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "Failed to write package log: %s",
				 error->message);
		goto out;
	}

	/* update UI */
	g_print ("Processed %i/%i %s\n",
		 priv->id + 1,
		 asb_context_get_packages(priv->ctx)->len,
		 asb_package_get_name (priv->pkg));
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
