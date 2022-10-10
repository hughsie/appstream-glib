/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2015 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include <config.h>
#include <fnmatch.h>

#include <asb-plugin.h>

const gchar *
asb_plugin_get_name (void)
{
	return "appdata";
}

void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/share/appdata/*.metainfo.xml");
	asb_plugin_add_glob (globs, "/usr/share/appdata/*.appdata.xml");
	asb_plugin_add_glob (globs, "/usr/share/metainfo/*.metainfo.xml");
	asb_plugin_add_glob (globs, "/usr/share/metainfo/*.appdata.xml");
}

static gboolean
_asb_plugin_check_filename (const gchar *filename)
{
	if (asb_plugin_match_glob ("*.metainfo.xml", filename) ||
	    asb_plugin_match_glob ("/usr/share/appdata/*.metainfo.xml", filename) ||
	    asb_plugin_match_glob ("/usr/share/appdata/*.appdata.xml", filename) ||
	    asb_plugin_match_glob ("/usr/share/metainfo/*.metainfo.xml", filename) ||
	    asb_plugin_match_glob ("/usr/share/metainfo/*.appdata.xml", filename))
		return TRUE;
	return FALSE;
}

gboolean
asb_plugin_check_filename (AsbPlugin *plugin, const gchar *filename)
{
	return _asb_plugin_check_filename (filename);
}

static gboolean
asb_plugin_process_filename (AsbPlugin *plugin,
			     AsbPackage *pkg,
			     const gchar *filename,
			     GList **apps,
			     GError **error)
{
	AsProblemKind problem_kind;
	AsProblem *problem;
	const gchar *tmp;
	guint i;
	g_autoptr(AsbApp) app = NULL;
	g_autoptr(GPtrArray) problems = NULL;

	app = asb_app_new (NULL, NULL);
	if (!as_app_parse_file (AS_APP (app), filename,
				AS_APP_PARSE_FLAG_USE_HEURISTICS,
				error))
		return FALSE;
	if (as_app_get_kind (AS_APP (app)) == AS_APP_KIND_UNKNOWN) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "%s has no recognised type",
			     as_app_get_id (AS_APP (app)));
		return FALSE;
	}

	/* validate */
	problems = as_app_validate (AS_APP (app),
				    AS_APP_VALIDATE_FLAG_NO_NETWORK |
				    AS_APP_VALIDATE_FLAG_RELAX,
				    error);
	if (problems == NULL)
		return FALSE;
	asb_app_set_package (app, pkg);
	for (i = 0; i < problems->len; i++) {
		problem = g_ptr_array_index (problems, i);
		problem_kind = as_problem_get_kind (problem);
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "AppData problem: %s : %s",
				 as_problem_kind_to_string (problem_kind),
				 as_problem_get_message (problem));
	}

	/* fix up the project license */
	tmp = as_app_get_project_license (AS_APP (app));
	if (tmp != NULL && !as_utils_is_spdx_license (tmp)) {
		g_autofree gchar *license_spdx = NULL;
		license_spdx = as_utils_license_to_spdx (tmp);
		if (as_utils_is_spdx_license (license_spdx)) {
			asb_package_log (asb_app_get_package (app),
					 ASB_PACKAGE_LOG_LEVEL_WARNING,
					 "project license fixup: %s -> %s",
					 tmp, license_spdx);
			as_app_set_project_license (AS_APP (app), license_spdx);
		} else {
			asb_package_log (asb_app_get_package (app),
					 ASB_PACKAGE_LOG_LEVEL_WARNING,
					 "project license is invalid: %s", tmp);
			as_app_set_project_license (AS_APP (app), NULL);
		}
	}

	/* check license */
	tmp = as_app_get_metadata_license (AS_APP (app));
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

	/* log updateinfo */
	tmp = as_app_get_update_contact (AS_APP (app));
	if (tmp != NULL) {
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_INFO,
				 "Upstream contact <%s>", tmp);
	}

	/* fix up various components as required */
	if (asb_context_get_flag (plugin->ctx, ASB_CONTEXT_FLAG_ADD_DEFAULT_ICONS)) {

		/* fix up input methods */
		if (as_app_get_kind (AS_APP (app)) == AS_APP_KIND_INPUT_METHOD) {
			as_app_add_category (AS_APP (app), "Addons");
			as_app_add_category (AS_APP (app), "InputSources");
		}

		/* fix up codecs */
		if (as_app_get_kind (AS_APP (app)) == AS_APP_KIND_CODEC) {
			as_app_add_category (AS_APP (app), "Addons");
			as_app_add_category (AS_APP (app), "Codecs");
		}
	}

	/* success */
	asb_plugin_add_app (apps, AS_APP (app));
	return TRUE;
}

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
		g_autofree gchar *filename_tmp = NULL;
		if (!_asb_plugin_check_filename (filelist[i]))
			continue;
		filename_tmp = g_build_filename (tmpdir, filelist[i], NULL);
		ret = asb_plugin_process_filename (plugin,
						   pkg,
						   filename_tmp,
						   &apps,
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

void
asb_plugin_merge (AsbPlugin *plugin, GList *list)
{
	AsApp *app;
	AsApp *found;
	GList *l;
	g_autoptr(GHashTable) hash = NULL;

	/* make a hash table of ID->AsApp */
	hash = g_hash_table_new_full (g_str_hash, g_str_equal,
				      g_free, (GDestroyNotify) g_object_unref);
	for (l = list; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		if (as_app_get_kind (app) != AS_APP_KIND_DESKTOP)
			continue;
		g_hash_table_insert (hash,
				     g_strdup (as_app_get_id (app)),
				     g_object_ref (app));
	}

	/* add addons where the pkgname is different from the
	 * main package */
	for (l = list; l != NULL; l = l->next) {
		if (!ASB_IS_APP (l->data))
			continue;
		app = AS_APP (l->data);
		if (as_app_get_kind (app) != AS_APP_KIND_ADDON)
			continue;
		found = g_hash_table_lookup (hash, as_app_get_id (app));
		if (found == NULL)
			continue;
		if (g_strcmp0 (as_app_get_pkgname_default (app),
			       as_app_get_pkgname_default (found)) != 0)
			continue;
		as_app_add_veto (app,
				 "absorbing addon %s shipped in "
				 "main package %s",
				 as_app_get_id (app),
				 as_app_get_pkgname_default (app));
		as_app_subsume_full (found, app, AS_APP_SUBSUME_FLAG_MERGE);
	}
}
