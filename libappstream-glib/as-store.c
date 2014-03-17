/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013-2014 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "config.h"

#include "as-node.h"
#include "as-store.h"

typedef struct _AsStorePrivate	AsStorePrivate;
struct _AsStorePrivate
{
	GPtrArray		*array;		/* of AsApp */
	GHashTable		*hash_id;	/* of AsApp{id} */
	GHashTable		*hash_pkgname;	/* of AsApp{pkgname} */
};

G_DEFINE_TYPE_WITH_PRIVATE (AsStore, as_store, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_store_get_instance_private (o))

/**
 * as_store_finalize:
 **/
static void
as_store_finalize (GObject *object)
{
	AsStore *store = AS_STORE (object);
	AsStorePrivate *priv = GET_PRIVATE (store);

	g_ptr_array_unref (priv->array);
	g_hash_table_unref (priv->hash_id);
	g_hash_table_unref (priv->hash_pkgname);

	G_OBJECT_CLASS (as_store_parent_class)->finalize (object);
}

/**
 * as_store_init:
 **/
static void
as_store_init (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->hash_id = g_hash_table_new_full (g_str_hash,
					       g_str_equal,
					       NULL,
					       NULL);
	priv->hash_pkgname = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    g_free,
						    NULL);
}

/**
 * as_store_class_init:
 **/
static void
as_store_class_init (AsStoreClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_store_finalize;
}

/**
 * as_store_get_size:
 * @store: a #AsStore instance.
 *
 * Gets the size of the store after deduplication and prioritization has
 * taken place.
 *
 * Returns: the number of usable applications in the store
 *
 * Since: 0.1.0
 **/
guint
as_store_get_size (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_return_val_if_fail (AS_IS_STORE (store), 0);
	return priv->array->len;
}

/**
 * as_store_get_apps:
 * @store: a #AsStore instance.
 *
 * Gets an array of all the valid applications in the store.
 *
 * Returns: (element-type AsApp) (transfer none): an array
 *
 * Since: 0.1.0
 **/
GPtrArray *
as_store_get_apps (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_return_val_if_fail (AS_IS_STORE (store), NULL);
	return priv->array;
}

/**
 * as_store_get_app_by_id:
 * @store: a #AsStore instance.
 * @id: the application short ID.
 *
 * Finds an application in the store by ID.
 *
 * Returns: (transfer none): a #GsApp or %NULL
 *
 * Since: 0.1.0
 **/
AsApp *
as_store_get_app_by_id (AsStore *store, const gchar *id)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_return_val_if_fail (AS_IS_STORE (store), NULL);
	return g_hash_table_lookup (priv->hash_id, id);
}

/**
 * as_store_get_app_by_pkgname:
 * @store: a #AsStore instance.
 * @pkgname: the package name.
 *
 * Finds an application in the store by package name.
 *
 * Returns: (transfer none): a #GsApp or %NULL
 *
 * Since: 0.1.0
 **/
AsApp *
as_store_get_app_by_pkgname (AsStore *store, const gchar *pkgname)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_return_val_if_fail (AS_IS_STORE (store), NULL);
	return g_hash_table_lookup (priv->hash_pkgname, pkgname);
}

/**
 * as_store_add_app:
 **/
static void
as_store_add_app (AsStore *store, AsApp *app)
{
	AsApp *item;
	AsAppIdKind id_kind;
	AsStorePrivate *priv = GET_PRIVATE (store);
	GPtrArray *pkgnames;
	const gchar *id;
	const gchar *pkgname;
	guint i;

	/* have we recorded this before? */
	id = as_app_get_id (app);
	item = g_hash_table_lookup (priv->hash_id, id);
	if (item != NULL) {

		/* the previously stored app is higher priority */
		if (as_app_get_priority (item) >
		    as_app_get_priority (app)) {
			g_debug ("ignoring duplicate AppStream entry: %s", id);
			g_object_unref (app);
			return;
		}

		/* this new item has a higher priority than the one we've
		 * previously stored */
		g_debug ("replacing duplicate AppStream entry: %s", id);
		g_hash_table_remove (priv->hash_id, id);
		g_ptr_array_remove (priv->array, item);
	}

	/* this is a type we don't know how to handle */
	id_kind = as_app_get_id_kind (app);
	if (id_kind == AS_APP_ID_KIND_UNKNOWN) {
		g_debug ("No idea how to handle AppStream entry: %s", id);
		g_object_unref (app);
		return;
	}

	/* success, add to array */
	g_ptr_array_add (priv->array, app);
	g_hash_table_insert (priv->hash_id,
			     (gpointer) as_app_get_id (app),
			     app);
	pkgnames = as_app_get_pkgnames (app);
	for (i = 0; i < pkgnames->len; i++) {
		pkgname = g_ptr_array_index (pkgnames, i);
		g_hash_table_insert (priv->hash_pkgname,
				     g_strdup (pkgname),
				     app);
	}
}

/**
 * as_store_parse_file:
 * @store: a #AsStore instance.
 * @file: a #GFile.
 * @path_icons: the icon path for the applications, or %NULL.
 * @cancellable: a #GCancellable.
 * @error: A #GError or %NULL.
 *
 * Parses an AppStream XML file and adds any valid applications to the store.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.0
 **/
gboolean
as_store_parse_file (AsStore *store,
		     GFile *file,
		     const gchar *path_icons,
		     GCancellable *cancellable,
		     GError **error)
{
	AsApp *app;
	GNode *apps;
	GNode *n;
	GNode *root;
	gboolean ret = TRUE;

	g_return_val_if_fail (AS_IS_STORE (store), FALSE);

	root = as_node_from_file (file, cancellable, error);
	if (root == NULL) {
		ret = FALSE;
		goto out;
	}
	apps = as_node_find (root, "applications");
	for (n = apps->children; n != NULL; n = n->next) {
		app = as_app_new ();
		if (path_icons != NULL)
			as_app_set_icon_path (app, path_icons, -1);
		ret = as_app_node_parse (app, n, error);
		if (!ret) {
			g_object_unref (app);
			goto out;
		}
		as_store_add_app (store, app);
	}
out:
	if (root != NULL)
		as_node_unref (root);
	return ret;
}

/**
 * as_store_new:
 *
 * Creates a new #AsStore.
 *
 * Returns: (transfer full): a #AsStore
 *
 * Since: 0.1.0
 **/
AsStore *
as_store_new (void)
{
	AsStore *store;
	store = g_object_new (AS_TYPE_STORE, NULL);
	return AS_STORE (store);
}
