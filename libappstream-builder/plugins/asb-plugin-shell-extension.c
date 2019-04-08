/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include <config.h>
#include <fnmatch.h>
#include <json-glib/json-glib.h>

#include <asb-plugin.h>

const gchar *
asb_plugin_get_name (void)
{
	return "shell-extension";
}

void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/share/gnome-shell/extensions/*/metadata.json");
}

static gboolean
_asb_plugin_check_filename (const gchar *filename)
{
	if (asb_plugin_match_glob ("/usr/share/gnome-shell/extensions/*/metadata.json", filename))
		return TRUE;
	return FALSE;
}

gboolean
asb_plugin_check_filename (AsbPlugin *plugin, const gchar *filename)
{
	return _asb_plugin_check_filename (filename);
}

static gboolean
as_app_parse_shell_extension_data (AsbPlugin *plugin,
				   AsApp *app,
				   const gchar *data,
				   gsize len,
				   GError **error)
{
	JsonArray *json_array;
	JsonNode *json_root;
	JsonObject *json_obj;
	const gchar *tmp;
	g_autoptr(JsonParser) json_parser = NULL;

	/* parse the data */
	json_parser = json_parser_new ();
	if (!json_parser_load_from_data (json_parser, data, (gssize) len, error))
		return FALSE;
	json_root = json_parser_get_root (json_parser);
	if (json_root == NULL) {
		g_set_error_literal (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "no root");
		return FALSE;
	}
	json_obj = json_node_get_object (json_root);
	if (json_obj == NULL) {
		g_set_error_literal (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "no object");
		return FALSE;
	}

	as_app_set_kind (app, AS_APP_KIND_SHELL_EXTENSION);
	as_app_set_comment (app, NULL, "GNOME Shell Extension");
	if (asb_context_get_flag (plugin->ctx, ASB_CONTEXT_FLAG_ADD_DEFAULT_ICONS)) {
		as_app_add_category (AS_APP (app), "Addons");
		as_app_add_category (AS_APP (app), "ShellExtensions");
	}
	tmp = json_object_get_string_member (json_obj, "uuid");
	if (tmp != NULL) {
		g_autofree gchar *id = NULL;
		id = as_utils_appstream_id_build (tmp);
		as_app_set_id (app, id);
		as_app_add_metadata (AS_APP (app), "shell-extensions::uuid", tmp);
	}
	if (json_object_has_member (json_obj, "gettext-domain")) {
		tmp = json_object_get_string_member (json_obj, "gettext-domain");
		if (tmp != NULL) {
			g_autoptr(AsTranslation) transaction = NULL;
			transaction = as_translation_new ();
			as_translation_set_kind (transaction, AS_TRANSLATION_KIND_GETTEXT);
			as_translation_set_id (transaction, tmp);
			as_app_add_translation (app, transaction);
		}
	}
	if (json_object_has_member (json_obj, "name")) {
		tmp = json_object_get_string_member (json_obj, "name");
		if (tmp != NULL)
			as_app_set_name (app, NULL, tmp);
	}
	if (json_object_has_member (json_obj, "description")) {
		tmp = json_object_get_string_member (json_obj, "description");
		if (tmp != NULL) {
			g_autofree gchar *desc = NULL;
			desc = as_markup_import (tmp,
						 AS_MARKUP_CONVERT_FORMAT_SIMPLE,
						 error);
			if (desc == NULL)
				return FALSE;
			as_app_set_description (app, NULL, desc);
		}
	}
	if (json_object_has_member (json_obj, "url")) {
		tmp = json_object_get_string_member (json_obj, "url");
		if (tmp != NULL)
			as_app_add_url (app, AS_URL_KIND_HOMEPAGE, tmp);
	}
	if (json_object_has_member (json_obj, "original-authors")) {
		json_array = json_object_get_array_member (json_obj, "original-authors");
		if (json_array != NULL) {
			tmp = json_array_get_string_element (json_array, 0);
			as_app_set_developer_name (app, NULL, tmp);
		}
	}
	if (as_app_get_release_default (app) == NULL &&
	    json_object_has_member (json_obj, "shell-version")) {
		json_array = json_object_get_array_member (json_obj, "shell-version");
		if (json_array != NULL) {
			g_autoptr(AsRelease) release = NULL;
			tmp = json_array_get_string_element (json_array, 0);
			release = as_release_new ();
			as_release_set_state (release, AS_RELEASE_STATE_INSTALLED);
			as_release_set_version (release, tmp);
			as_app_add_release (app, release);
		}
	}

	/* use a stock icon */
	if (asb_context_get_flag (plugin->ctx, ASB_CONTEXT_FLAG_ADD_DEFAULT_ICONS)) {
		g_autoptr(AsIcon) ic = as_icon_new ();
		as_icon_set_kind (ic, AS_ICON_KIND_STOCK);
		as_icon_set_name (ic, "application-x-addon-symbolic");
		as_app_add_icon (app, ic);
	}
	return TRUE;
}

static gboolean
asb_plugin_process_filename (AsbPlugin *plugin,
			     AsbPackage *pkg,
			     const gchar *filename,
			     GList **apps,
			     GError **error)
{
	gsize len;
	g_autoptr(AsbApp) app = NULL;
	g_autofree gchar *data = NULL;

	app = asb_app_new (pkg, NULL);
	if (!g_file_get_contents (filename, &data, &len, error))
		return FALSE;
	if (!as_app_parse_shell_extension_data (plugin, AS_APP (app), data, len, error))
		return FALSE;
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
