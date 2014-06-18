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

#include <config.h>
#include <fnmatch.h>

#include <asb-plugin.h>

/**
 * asb_plugin_get_name:
 */
const gchar *
asb_plugin_get_name (void)
{
	return "metainfo";
}

/**
 * asb_plugin_add_globs:
 */
void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/share/appdata/*.metainfo.xml");
}

/**
 * _asb_plugin_check_filename:
 */
static gboolean
_asb_plugin_check_filename (const gchar *filename)
{
	if (fnmatch ("/usr/share/appdata/*.metainfo.xml", filename, 0) == 0)
		return TRUE;
	return FALSE;
}

/**
 * asb_plugin_check_filename:
 */
gboolean
asb_plugin_check_filename (AsbPlugin *plugin, const gchar *filename)
{
	return _asb_plugin_check_filename (filename);
}

/**
 * asb_plugin_process_filename:
 */
static gboolean
asb_plugin_process_filename (AsbPlugin *plugin,
			     AsbPackage *pkg,
			     const gchar *filename,
			     GList **apps,
			     GError **error)
{
	_cleanup_object_unref_ AsbApp *app = NULL;

	app = asb_app_new (pkg, NULL);
	if (!as_app_parse_file (AS_APP (app), filename,
				AS_APP_PARSE_FLAG_APPEND_DATA,
				error))
		return FALSE;
	if (as_app_get_id_kind (AS_APP (app)) != AS_ID_KIND_ADDON) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "%s is not an addon",
			     as_app_get_id_full (AS_APP (app)));
		return FALSE;
	}
	asb_app_set_requires_appdata (app, FALSE);
	asb_plugin_add_app (apps, app);
	return TRUE;
}

/**
 * asb_plugin_process:
 */
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
		_cleanup_free_ gchar *filename_tmp = NULL;
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

/**
 * asb_plugin_metainfo_absorb:
 */
static void
asb_plugin_metainfo_absorb (AsApp *app, AsApp *donor)
{
	GPtrArray *mimetypes;
	const gchar *tmp;
	guint i;

	mimetypes = as_app_get_mimetypes (donor);
	for (i = 0; i < mimetypes->len; i++) {
		tmp = g_ptr_array_index (mimetypes, i);
		as_app_add_mimetype (app, tmp, -1);
	}
}

/**
 * asb_plugin_merge:
 */
void
asb_plugin_merge (AsbPlugin *plugin, GList **list)
{
	AsApp *app;
	AsApp *found;
	GList *l;
	GList *list_new = NULL;
	_cleanup_hashtable_unref_ GHashTable *hash;

	/* make a hash table of ID->AsApp */
	hash = g_hash_table_new_full (g_str_hash, g_str_equal,
				      g_free, (GDestroyNotify) g_object_unref);
	for (l = *list; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		if (as_app_get_id_kind (app) != AS_ID_KIND_DESKTOP)
			continue;
		g_hash_table_insert (hash,
				     g_strdup (as_app_get_id_full (app)),
				     g_object_ref (app));
	}

	/* add addons where the pkgname is different from the
	 * main package */
	for (l = *list; l != NULL; l = l->next) {
		if (!ASB_IS_APP (l->data)) {
			asb_plugin_add_app (&list_new, l->data);
			continue;
		}
		app = AS_APP (l->data);
		if (as_app_get_id_kind (app) != AS_ID_KIND_ADDON) {
			asb_plugin_add_app (&list_new, l->data);
			continue;
		}
		found = g_hash_table_lookup (hash, as_app_get_id_full (app));
		if (found == NULL) {
			asb_plugin_add_app (&list_new, l->data);
			continue;
		}
		if (g_strcmp0 (as_app_get_pkgname_default (app),
			       as_app_get_pkgname_default (found)) == 0) {
			g_debug ("absorbing addon %s shipped in main package %s",
				 as_app_get_id_full (app),
				 as_app_get_pkgname_default (app));
			asb_plugin_metainfo_absorb (found, app);
			continue;
		}
		asb_plugin_add_app (&list_new, ASB_APP (app));
	}

	/* success */
	g_list_free_full (*list, (GDestroyNotify) g_object_unref);
	*list = list_new;
}
