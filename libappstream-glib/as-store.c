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

/**
 * SECTION:as-store
 * @short_description: a hashed array store of applications
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * This store contains both an array of #AsApp's but also a pair of hashes
 * to quickly retrieve an application from the ID or package name.
 *
 * Applications can also be removed, and the whole store can be loaded and
 * saved to a compressed XML file.
 *
 * See also: #AsApp
 */

#include "config.h"

#include "as-app-private.h"
#include "as-node-private.h"
#include "as-store.h"

typedef struct _AsStorePrivate	AsStorePrivate;
struct _AsStorePrivate
{
	gchar			*origin;
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

	g_free (priv->origin);
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
						    (GDestroyNotify) g_object_unref);
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
 * Returns: (transfer none): a #AsApp or %NULL
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
 * Returns: (transfer none): a #AsApp or %NULL
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
 * as_store_remove_app:
 * @store: a #AsStore instance.
 * @app: a #AsApp instance.
 *
 * Removes an application from the store if it exists.
 *
 * Since: 0.1.0
 **/
void
as_store_remove_app (AsStore *store, AsApp *app)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_hash_table_remove (priv->hash_id, as_app_get_id (app));
	g_ptr_array_remove (priv->array, app);
}

/**
 * as_store_add_app:
 * @store: a #AsStore instance.
 * @app: a #AsApp instance.
 *
 * Adds an application to the store. If a lower priority application has already
 * been added then this new application will replace it.
 *
 * Additionally only applications where the kind is known will be added.
 *
 * Since: 0.1.0
 **/
void
as_store_add_app (AsStore *store, AsApp *app)
{
	AsApp *item;
	AsIdKind id_kind;
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
	if (id_kind == AS_ID_KIND_UNKNOWN) {
		g_debug ("No idea how to handle AppStream entry: %s", id);
		return;
	}

	/* success, add to array */
	g_ptr_array_add (priv->array, g_object_ref (app));
	g_hash_table_insert (priv->hash_id,
			     (gpointer) as_app_get_id (app),
			     app);
	pkgnames = as_app_get_pkgnames (app);
	for (i = 0; i < pkgnames->len; i++) {
		pkgname = g_ptr_array_index (pkgnames, i);
		g_hash_table_insert (priv->hash_pkgname,
				     g_strdup (pkgname),
				     g_object_ref (app));
	}
}

/**
 * as_store_from_file:
 * @store: a #AsStore instance.
 * @file: a #GFile.
 * @path_icons: the icon path for the applications, or %NULL for the default.
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
as_store_from_file (AsStore *store,
		    GFile *file,
		    const gchar *path_icons,
		    GCancellable *cancellable,
		    GError **error)
{
	AsApp *app;
	GNode *apps;
	GNode *n;
	GNode *root;
	const gchar *origin;
	gboolean ret = TRUE;
	gchar *fullpath = NULL;

	g_return_val_if_fail (AS_IS_STORE (store), FALSE);

	root = as_node_from_file (file,
				  AS_NODE_FROM_XML_FLAG_NONE,
				  cancellable,
				  error);
	if (root == NULL) {
		ret = FALSE;
		goto out;
	}
	apps = as_node_find (root, "applications");
	if (apps == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_FAILED,
				     "No valid root node specified");
		goto out;
	}
	if (path_icons == NULL) {
		origin = as_node_get_attribute (apps, "origin");
		as_store_set_origin (store, origin);
		if (origin != NULL) {
			fullpath = g_build_filename ("/usr/share/app-info/icons/",
						     origin,
						     NULL);
		}
	} else {
		fullpath = g_strdup (path_icons);
	}
	for (n = apps->children; n != NULL; n = n->next) {
		app = as_app_new ();
		if (fullpath != NULL)
			as_app_set_icon_path (app, fullpath, -1);
		ret = as_app_node_parse (app, n, error);
		if (!ret) {
			g_object_unref (app);
			goto out;
		}
		as_store_add_app (store, app);
		g_object_unref (app);
	}
out:
	if (root != NULL)
		as_node_unref (root);
	g_free (fullpath);
	return ret;
}

/**
 * as_store_to_xml:
 * @store: a #AsStore instance.
 * @flags: the AsNodeToXmlFlags, e.g. %AS_NODE_INSERT_FLAG_NONE.
 *
 * Outputs an XML representation of all the applications in the store.
 *
 * Returns: A #GString
 *
 * Since: 0.1.0
 **/
GString *
as_store_to_xml (AsStore *store, AsNodeToXmlFlags flags)
{
	AsApp *app;
	AsStorePrivate *priv = GET_PRIVATE (store);
	GNode *node_apps;
	GNode *node_root;
	GString *xml;
	guint i;

	/* get XML text */
	node_root = as_node_new ();
	node_apps = as_node_insert (node_root, "applications", NULL, 0,
				    "version", "0.4",
				    NULL);
	if (priv->origin != NULL)
		as_node_add_attribute (node_root, "origin", priv->origin, -1);
	for (i = 0; i < priv->array->len; i++) {
		app = g_ptr_array_index (priv->array, i);
		as_app_node_insert (app, node_apps);
	}
	xml = as_node_to_xml (node_root, flags);
	as_node_unref (node_root);
	return xml;
}

/**
 * as_store_to_file:
 * @store: a #AsStore instance.
 * @file: file
 * @flags: the AsNodeToXmlFlags, e.g. %AS_NODE_INSERT_FLAG_NONE.
 * @cancellable: A #GCancellable, or %NULL
 * @error: A #GError or %NULL
 *
 * Outputs a compressed XML file of all the applications in the store.
 *
 * Returns: A #GString
 *
 * Since: 0.1.0
 **/
gboolean
as_store_to_file (AsStore *store,
		  GFile *file,
		  AsNodeToXmlFlags flags,
		  GCancellable *cancellable,
		  GError **error)
{
	GOutputStream *out;
	GOutputStream *out2;
	GString *xml;
	GZlibCompressor *compressor;
	gboolean ret;

	/* compress as a gzip file */
	compressor = g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP, -1);
	out = g_memory_output_stream_new_resizable ();
	out2 = g_converter_output_stream_new (out, G_CONVERTER (compressor));
	xml = as_store_to_xml (store, flags);
	ret = g_output_stream_write_all (out2, xml->str, xml->len,
					 NULL, NULL, error);
	if (!ret)
		goto out;
	ret = g_output_stream_close (out2, NULL, error);
	if (!ret)
		goto out;

	/* write file */
	ret = g_file_replace_contents (file,
		g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (out)),
		g_memory_output_stream_get_size (G_MEMORY_OUTPUT_STREAM (out)),
				       NULL,
				       FALSE,
				       G_FILE_CREATE_NONE,
				       NULL,
				       cancellable,
				       error);
	if (!ret)
		goto out;
out:
	g_object_unref (compressor);
	g_object_unref (out);
	g_object_unref (out2);
	g_string_free (xml, TRUE);
	return ret;
}

/**
 * as_store_get_origin:
 * @store: a #AsStore instance.
 *
 * Gets the metadata origin, which is used to locate icons.
 *
 * Returns: the origin string, or %NULL if unset
 *
 * Since: 0.1.1
 **/
const gchar *
as_store_get_origin (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	return priv->origin;
}

/**
 * as_store_set_origin:
 * @store: a #AsStore instance.
 * @origin: the origin, e.g. "fedora-21"
 *
 * Sets the metadata origin, which is used to locate icons.
 *
 * Since: 0.1.1
 **/
void
as_store_set_origin (AsStore *store, const gchar *origin)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_free (priv->origin);
	priv->origin = g_strdup (origin);
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
