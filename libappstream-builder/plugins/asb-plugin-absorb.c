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

#include <asb-plugin.h>
#include <fnmatch.h>

/**
 * asb_plugin_get_name:
 */
const gchar *
asb_plugin_get_name (void)
{
	return "absorb";
}

/**
 * asb_plugin_absorb_parent_for_pkgname:
 */
static void
asb_plugin_absorb_parent_for_pkgname (GList *list, AsApp *parent, const gchar *pkgname)
{
	AsApp *app;
	GList *l;

	for (l = list; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		if (as_app_get_id_kind (app) != AS_ID_KIND_ADDON)
			continue;
		if (g_strcmp0 (as_app_get_pkgname_default (app), pkgname) != 0)
			continue;
		if (as_app_get_vetos(app)->len > 0)
			continue;
		g_debug ("Adding X-Merge-With-Parent on %s as %s depends on %s",
			 as_app_get_id (app),
			 as_app_get_pkgname_default (parent),
			 as_app_get_pkgname_default (app));
		as_app_add_metadata (app,
				     "X-Merge-With-Parent",
				     as_app_get_id (parent));
	}
}

/**
 * asb_plugin_merge_prepare_deps:
 */
static void
asb_plugin_merge_prepare_deps (GList *list)
{
	AsApp *app;
	AsbPackage *pkg;
	GList *l;

	for (l = list; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		if (as_app_get_id_kind (app) != AS_ID_KIND_DESKTOP)
			continue;
		if (!ASB_IS_APP (app))
			continue;
		if (as_app_get_vetos(app)->len > 0)
			continue;
		pkg = asb_app_get_package (ASB_APP (app));
		asb_plugin_absorb_parent_for_pkgname (list, app, asb_package_get_name (pkg));
	}
}

/**
 * asb_plugin_merge:
 */
void
asb_plugin_merge (AsbPlugin *plugin, GList *list)
{
	AsApp *app;
	AsApp *found;
	GList *l;
	const gchar *tmp;
	_cleanup_hashtable_unref_ GHashTable *hash = NULL;

	/* add X-Merge-With-Parent on any metainfo files that are in a package
	 * required by a desktop package */
	asb_plugin_merge_prepare_deps (list);

	/* add all packages to the hash */
	hash = g_hash_table_new_full (g_str_hash, g_str_equal,
				      g_free, (GDestroyNotify) g_object_unref);
	for (l = list; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		g_hash_table_insert (hash,
				     g_strdup (as_app_get_id (app)),
				     g_object_ref (app));
	}

	/* absorb some apps into their parent */
	for (l = list; l != NULL; l = l->next) {
		app = AS_APP (l->data);

		/* no absorb metadata */
		tmp = as_app_get_metadata_item (app, "X-Merge-With-Parent");
		if (tmp == NULL)
			continue;

		/* find the parent app */
		found = g_hash_table_lookup (hash, tmp);
		if (found == NULL) {
			as_app_add_veto (app, "No referenced '%s'", tmp);
			continue;
		}

		/* partially absorb */
		as_app_add_veto (app, "partially absorbing %s into %s",
				 as_app_get_id (app), as_app_get_id (found));
		as_app_subsume_full (found, app, AS_APP_SUBSUME_FLAG_PARTIAL);
	}
}
