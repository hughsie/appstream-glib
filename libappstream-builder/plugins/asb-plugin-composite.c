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
#include <string.h>

#include <asb-plugin.h>

/**
 * asb_plugin_get_name:
 */
const gchar *
asb_plugin_get_name (void)
{
	return "composite";
}

/**
 * asb_plugin_composite_app:
 */
static gboolean
asb_plugin_composite_app (AsApp *app, AsApp *donor, GError **error)
{
	AsApp *tmp;
	gint rc;
	_cleanup_free_ gchar *id = NULL;

	/* check this makes sense */
	if (as_app_get_id_kind (app) != as_app_get_id_kind (donor)) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_INVALID_TYPE,
			     "Cannot composite apps %s:%s of different id kind",
			     as_app_get_id (app),
			     as_app_get_id (donor));
		return FALSE;
	}

	/* the ID, name with the shortest length wins */
	rc = strlen (as_app_get_id (app)) - strlen (as_app_get_id (donor));
	if (rc == 0) {
		rc = strlen (as_app_get_name (app, "C")) -
		     strlen (as_app_get_name (donor, "C"));
	}
	if (rc > 0) {
		tmp = app;
		app = donor;
		donor = tmp;
	}

	/* set the new composite string */
	id = as_utils_get_string_overlap (as_app_get_id (app), as_app_get_id (donor));
	if (id == NULL || strlen (id) < 12) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_INVALID_TYPE,
			     "Cannot composite apps %s:%s as no ID overlap",
			     as_app_get_id (app),
			     as_app_get_id (donor));
		return FALSE;
	}
	as_app_set_id (app, id, -1);

	/* add some easily merged properties */
	as_app_subsume_full (app, donor, AS_APP_SUBSUME_FLAG_PARTIAL);
	as_app_add_veto (donor, "absorbed into %s", as_app_get_id (app));
	return TRUE;
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

	/* add all packages to the hash */
	hash = g_hash_table_new_full (g_str_hash, g_str_equal,
				      g_free, (GDestroyNotify) g_object_unref);
	for (l = list; l != NULL; l = l->next) {
		app = AS_APP (l->data);
		if (as_app_get_vetos (app)->len > 0)
			continue;
		tmp = as_app_get_pkgname_default (app);
		if (tmp == NULL)
			continue;
		found = g_hash_table_lookup (hash, tmp);
		if (found != NULL) {
			_cleanup_error_free_ GError *error = NULL;
			if (!asb_plugin_composite_app (app, found, &error)) {
				g_warning ("Failed to composite %s:%s",
					   as_app_get_id (app),
					   as_app_get_id (found));
				continue;
			}
			continue;
		}
		g_hash_table_insert (hash,
				     g_strdup (as_app_get_pkgname_default (app)),
				     g_object_ref (app));
	}
}
