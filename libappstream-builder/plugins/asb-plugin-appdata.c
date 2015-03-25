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

#include <appstream-glib.h>
#include <libsoup/soup.h>

#include <asb-plugin.h>

struct AsbPluginPrivate {
	SoupSession	*session;
	GPtrArray	*filenames;
	GMutex		 filenames_mutex;
};

/**
 * asb_plugin_get_name:
 */
const gchar *
asb_plugin_get_name (void)
{
	return "appdata";
}

/**
 * asb_plugin_initialize:
 */
void
asb_plugin_initialize (AsbPlugin *plugin)
{
	plugin->priv = ASB_PLUGIN_GET_PRIVATE (AsbPluginPrivate);
	plugin->priv->filenames = g_ptr_array_new_with_free_func (g_free);
	g_mutex_init (&plugin->priv->filenames_mutex);
	plugin->priv->session = soup_session_new_with_options (SOUP_SESSION_USER_AGENT, "createrepo_as",
							       SOUP_SESSION_TIMEOUT, 10,
							       NULL);
	soup_session_add_feature_by_type (plugin->priv->session,
					  SOUP_TYPE_PROXY_RESOLVER_DEFAULT);
}

/**
 * asb_plugin_destroy:
 */
void
asb_plugin_destroy (AsbPlugin *plugin)
{
	const gchar *tmp;
	guint i;

	/* print out AppData files not used */
	if (g_getenv ("ASB_PERFORM_EXTRA_CHECKS") != NULL) {
		for (i = 0; i < plugin->priv->filenames->len; i++) {
			tmp = g_ptr_array_index (plugin->priv->filenames, i);
			g_debug ("%s was not used", tmp);
		}
	}
	g_ptr_array_unref (plugin->priv->filenames);
	g_mutex_clear (&plugin->priv->filenames_mutex);
	g_object_unref (plugin->priv->session);
}

/**
 * asb_plugin_add_globs:
 */
void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/share/appdata/*.appdata.xml");
	asb_plugin_add_glob (globs, "*.metainfo.xml");
}

/**
 * asb_plugin_appdata_add_path:
 */
static gboolean
asb_plugin_appdata_add_path (AsbPlugin *plugin, const gchar *path, GError **error)
{
	const gchar *tmp;
	_cleanup_dir_close_ GDir *dir = NULL;

	/* scan all the files */
	dir = g_dir_open (path, 0, error);
	if (dir == NULL)
		return FALSE;
	while ((tmp = g_dir_read_name (dir)) != NULL) {
		_cleanup_free_ gchar *filename = NULL;
		filename = g_build_filename (path, tmp, NULL);
		if (g_file_test (filename, G_FILE_TEST_IS_DIR)) {
			if (!asb_plugin_appdata_add_path (plugin, filename, error))
				return FALSE;
		} else {
			g_ptr_array_add (plugin->priv->filenames, g_strdup (filename));
		}
	}
	return TRUE;
}

/**
 * asb_plugin_appdata_add_files:
 */
static gboolean
asb_plugin_appdata_add_files (AsbPlugin *plugin, const gchar *path, GError **error)
{
	gboolean ret;

	/* already done */
	if (plugin->priv->filenames->len > 0)
		return TRUE;

	g_mutex_lock (&plugin->priv->filenames_mutex);
	ret = asb_plugin_appdata_add_path (plugin, path, error);
	g_mutex_unlock  (&plugin->priv->filenames_mutex);
	return ret;
}

/**
 * asb_plugin_appdata_remove_file:
 */
static void
asb_plugin_appdata_remove_file (AsbPlugin *plugin, const gchar *filename)
{
	const gchar *tmp;
	guint i;

	g_mutex_lock (&plugin->priv->filenames_mutex);
	for (i = 0; i < plugin->priv->filenames->len; i++) {
		tmp = g_ptr_array_index (plugin->priv->filenames, i);
		if (g_strcmp0 (tmp, filename) == 0) {
			g_ptr_array_remove_fast (plugin->priv->filenames,
						 (gpointer) tmp);
			break;
		}
	}
	g_mutex_unlock  (&plugin->priv->filenames_mutex);
}

/**
 * asb_plugin_appdata_log_overwrite:
 */
static void
asb_plugin_appdata_log_overwrite (AsbApp *app,
				  const gchar *property_name,
				  const gchar *old,
				  const gchar *new)
{
	/* does the value already exist with this value */
	if (g_strcmp0 (old, new) == 0)
		return;

	/* does the metadata exist with any value */
	if (old != NULL) {
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_INFO,
				 "AppData %s=%s->%s",
				 property_name, old, new);
	}
}

/**
 * asb_plugin_process_filename:
 */
static gboolean
asb_plugin_process_filename (AsbPlugin *plugin,
			     AsbApp *app,
			     const gchar *filename,
			     GError **error)
{
	AsProblemKind problem_kind;
	AsProblem *problem;
	const gchar *key;
	const gchar *old;
	const gchar *tmp;
	gboolean ret;
	GHashTable *hash;
	GPtrArray *array;
	GList *l;
	GList *list;
	guint i;
	_cleanup_object_unref_ AsApp *appdata = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *problems = NULL;

	/* validate */
	appdata = as_app_new ();
	ret = as_app_parse_file (appdata, filename,
				 AS_APP_PARSE_FLAG_NONE,
				 error);
	if (!ret)
		return FALSE;
	problems = as_app_validate (appdata,
				    AS_APP_VALIDATE_FLAG_NO_NETWORK |
				    AS_APP_VALIDATE_FLAG_RELAX,
				    error);
	if (problems == NULL)
		return FALSE;
	for (i = 0; i < problems->len; i++) {
		problem = g_ptr_array_index (problems, i);
		problem_kind = as_problem_get_kind (problem);
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "AppData problem: %s : %s",
				 as_problem_kind_to_string (problem_kind),
				 as_problem_get_message (problem));
	}

	/* check app id */
	tmp = as_app_get_id (appdata);
	if (tmp == NULL) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "AppData %s has no ID",
			     filename);
		return FALSE;
	}
	if (g_strcmp0 (tmp, as_app_get_id (AS_APP (app))) != 0) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "AppData %s does not match '%s':'%s'",
			     filename,
			     tmp,
			     as_app_get_id (AS_APP (app)));
		return FALSE;
	}

	/* check license */
	tmp = as_app_get_metadata_license (appdata);
	if (tmp == NULL) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "AppData %s has no licence",
			     filename);
		return FALSE;
	}
	if (!as_utils_is_spdx_license (tmp)) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "AppData %s license '%s' invalid",
			     filename, tmp);
		return FALSE;
	}

	/* other optional data */
	tmp = as_app_get_url_item (appdata, AS_URL_KIND_HOMEPAGE);
	if (tmp != NULL)
		as_app_add_url (AS_APP (app), AS_URL_KIND_HOMEPAGE, tmp, -1);
	tmp = as_app_get_project_group (appdata);
	if (tmp != NULL) {
		/* check the category is valid */
		if (!as_utils_is_environment_id (tmp)) {
			asb_package_log (asb_app_get_package (app),
					 ASB_PACKAGE_LOG_LEVEL_WARNING,
					 "AppData project group invalid, "
					 "so ignoring: %s", tmp);
		} else {
			as_app_set_project_group (AS_APP (app), tmp, -1);
		}
	}
	array = as_app_get_compulsory_for_desktops (appdata);
	if (array->len > 0) {
		tmp = g_ptr_array_index (array, 0);
		as_app_add_compulsory_for_desktop (AS_APP (app), tmp, -1);
	}

	/* perhaps get name */
	hash = as_app_get_names (appdata);
	list = g_hash_table_get_keys (hash);
	for (l = list; l != NULL; l = l->next) {
		key = l->data;
		tmp = g_hash_table_lookup (hash, key);
		if (g_strcmp0 (key, "C") == 0) {
			old = as_app_get_name (AS_APP (app), key);
			asb_plugin_appdata_log_overwrite (app, "name",
							  old, tmp);
		}
		as_app_set_name (AS_APP (app), key, tmp, -1);
	}
	if (g_list_length (list) == 1) {
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "AppData 'name' has no translations");
	}
	g_list_free (list);

	/* perhaps get summary */
	hash = as_app_get_comments (appdata);
	list = g_hash_table_get_keys (hash);
	for (l = list; l != NULL; l = l->next) {
		key = l->data;
		tmp = g_hash_table_lookup (hash, key);
		if (g_strcmp0 (key, "C") == 0) {
			old = as_app_get_comment (AS_APP (app), key);
			asb_plugin_appdata_log_overwrite (app, "summary",
							  old, tmp);
		}
		as_app_set_comment (AS_APP (app), key, tmp, -1);
	}
	if (g_list_length (list) == 1) {
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "AppData 'summary' has no translations");
	}
	g_list_free (list);

	/* get descriptions */
	hash = as_app_get_descriptions (appdata);
	list = g_hash_table_get_keys (hash);
	for (l = list; l != NULL; l = l->next) {
		key = l->data;
		tmp = g_hash_table_lookup (hash, key);
		as_app_set_description (AS_APP (app), key, tmp, -1);
	}
	if (g_list_length (list) == 1) {
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "AppData 'description' has no translations");
	}
	g_list_free (list);

	/* add screenshots if not already added */
	array = as_app_get_screenshots (AS_APP (app));
	if (array->len == 0) {
		/* just use the upstream locations */
		array = as_app_get_screenshots (appdata);
		for (i = 0; i < array->len; i++) {
			AsScreenshot *ss;
			ss = g_ptr_array_index (array, i);
			as_app_add_screenshot (AS_APP (app), ss);
		}
	} else {
		array = as_app_get_screenshots (appdata);
		if (array->len > 0) {
			asb_package_log (asb_app_get_package (app),
					 ASB_PACKAGE_LOG_LEVEL_INFO,
					 "AppData screenshots ignored");
		}
	}

	/* add metadata */
	hash = as_app_get_metadata (appdata);
	list = g_hash_table_get_keys (hash);
	for (l = list; l != NULL; l = l->next) {
		key = l->data;
		tmp = g_hash_table_lookup (hash, key);
		old = as_app_get_metadata_item (AS_APP (app), key);
		asb_plugin_appdata_log_overwrite (app, "metadata", old, tmp);
		as_app_add_metadata (AS_APP (app), key, tmp, -1);
	}

	/* add releases */
	array = as_app_get_releases (appdata);
	for (i = 0; i < array->len; i++) {
		AsRelease *rel = g_ptr_array_index (array, i);
		as_app_add_release (AS_APP (app), rel);
	}

	/* log updateinfo */
	tmp = as_app_get_update_contact (AS_APP (appdata));
	if (tmp != NULL) {
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_INFO,
				 "Upstream contact <%s>", tmp);
	}

	/* success */
	asb_app_set_requires_appdata (app, FALSE);
	return TRUE;
}

/**
 * asb_plugin_appdata_get_fn_for_app:
 */
static gchar *
asb_plugin_appdata_get_fn_for_app (AsApp *app)
{
	gchar *fn = g_strdup (as_app_get_id (app));
	gchar *tmp;

	/* just cut off the last section without munging the name */
	tmp = g_strrstr (fn, ".");
	if (tmp != NULL)
		*tmp = '\0';
	return fn;
}

/**
 * asb_plugin_process_app:
 */
gboolean
asb_plugin_process_app (AsbPlugin *plugin,
			AsbPackage *pkg,
			AsbApp *app,
			const gchar *tmpdir,
			GError **error)
{
	GError *error_local = NULL;
	const gchar *kind_str;
	const gchar *tmp;
	_cleanup_free_ gchar *appdata_basename = NULL;
	_cleanup_free_ gchar *appdata_filename = NULL;
	_cleanup_free_ gchar *appdata_filename_extra = NULL;

	/* get possible sources */
	appdata_basename = asb_plugin_appdata_get_fn_for_app (AS_APP (app));
	appdata_filename = g_strdup_printf ("%s/files/share/appdata/%s.appdata.xml",
					    tmpdir, appdata_basename);
	if (!g_file_test (appdata_filename, G_FILE_TEST_EXISTS)) {
		g_free (appdata_filename);
		appdata_filename = g_strdup_printf ("%s/usr/share/appdata/%s.appdata.xml",
						    tmpdir, appdata_basename);
	}
	tmp = asb_package_get_config (pkg, "AppDataExtra");
	if (tmp != NULL && g_file_test (tmp, G_FILE_TEST_EXISTS)) {
		if (!asb_plugin_appdata_add_files (plugin, tmp, error))
			return FALSE;
		kind_str = as_id_kind_to_string (as_app_get_id_kind (AS_APP (app)));
		appdata_filename_extra = g_strdup_printf ("%s/%s/%s.appdata.xml",
							  tmp,
							  kind_str,
							  appdata_basename);
		if (g_file_test (appdata_filename, G_FILE_TEST_EXISTS) &&
		    g_file_test (appdata_filename_extra, G_FILE_TEST_EXISTS)) {
			asb_package_log (pkg,
					 ASB_PACKAGE_LOG_LEVEL_WARNING,
					 "extra AppData file %s overwrites upstream",
					 appdata_filename_extra);
		}

		/* we used this */
		asb_plugin_appdata_remove_file (plugin, appdata_filename_extra);
	}

	/* firmware */
	if (asb_package_get_kind (pkg) == ASB_PACKAGE_KIND_FIRMWARE) {
		appdata_filename = g_strdup_printf ("%s/%s.metainfo.xml",
						    tmpdir,
						    asb_package_get_source_pkgname (pkg));
	}

	/* any appdata-extra file */
	if (appdata_filename_extra != NULL &&
	    g_file_test (appdata_filename_extra, G_FILE_TEST_EXISTS)) {
		as_app_add_metadata (AS_APP (app), "DistroMetadata", NULL, -1);
		return asb_plugin_process_filename (plugin,
						    app,
						    appdata_filename_extra,
						    error);
	}

	/* any installed appdata file */
	if (g_file_test (appdata_filename, G_FILE_TEST_EXISTS)) {
		/* be understanding if upstream gets the AppData file
		 * wrong -- just fall back to the desktop file data */
		if (!asb_plugin_process_filename (plugin,
						  app,
						  appdata_filename,
						  &error_local)) {
			error_local->code = ASB_PLUGIN_ERROR_IGNORE;
			g_propagate_error (error, error_local);
			g_prefix_error (error,
					"AppData file '%s' invalid: ",
					appdata_filename);
			return FALSE;
		}
		return TRUE;
	}

	/* we're going to require this soon */
	if (as_app_get_id_kind (AS_APP (app)) == AS_ID_KIND_DESKTOP &&
	    as_app_get_metadata_item (AS_APP (app), "NoDisplay") == NULL) {
		asb_package_log (pkg,
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "desktop application %s has no AppData",
				 as_app_get_id (AS_APP (app)));
	}
	return TRUE;
}
