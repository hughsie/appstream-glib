/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013-2016 Richard Hughes <richard@hughsie.com>
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
#include "as-problem.h"
#include "as-profile.h"
#include "as-monitor.h"
#include "as-ref-string.h"
#include "as-stemmer.h"
#include "as-store.h"
#include "as-utils-private.h"
#include "as-yaml.h"

#ifdef HAVE_GCAB
#include "as-store-cab.h"
#endif

#define AS_API_VERSION_NEWEST	0.8

typedef enum {
	AS_STORE_PROBLEM_NONE			= 0,
	AS_STORE_PROBLEM_LEGACY_ROOT		= 1 << 0,
	AS_STORE_PROBLEM_LAST
} AsStoreProblems;

typedef struct
{
	gchar			*destdir;
	gchar			*origin;
	gchar			*builder_id;
	gdouble			 api_version;
	GPtrArray		*array;		/* of AsApp */
	GHashTable		*hash_id;	/* of GPtrArray of AsApp{id} */
	GHashTable		*hash_merge_id;	/* of GPtrArray of AsApp{id} */
	GHashTable		*hash_unique_id;	/* of AsApp{unique_id} */
	GHashTable		*hash_pkgname;	/* of AsApp{pkgname} */
	AsMonitor		*monitor;
	GHashTable		*metadata_indexes;	/* GHashTable{key} */
	GHashTable		*appinfo_dirs;	/* GHashTable{path:AsStorePathData} */
	GHashTable		*search_blacklist;	/* GHashTable{AsRefString:1} */
	guint32			 add_flags;
	guint32			 watch_flags;
	guint32			 problems;
	guint16			 search_match;
	guint32			 filter;
	guint			 changed_block_refcnt;
	gboolean		 is_pending_changed_signal;
	AsProfile		*profile;
	AsStemmer		*stemmer;
} AsStorePrivate;

typedef struct {
	AsAppScope		 scope;
	gchar			*arch;
} AsStorePathData;

G_DEFINE_TYPE_WITH_PRIVATE (AsStore, as_store, G_TYPE_OBJECT)

enum {
	SIGNAL_CHANGED,
	SIGNAL_APP_ADDED,
	SIGNAL_APP_REMOVED,
	SIGNAL_APP_CHANGED,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

#define GET_PRIVATE(o) (as_store_get_instance_private (o))

/**
 * as_store_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.1.2
 **/
G_DEFINE_QUARK (as-store-error-quark, as_store_error)

static gboolean	as_store_from_file_internal (AsStore *store,
					     GFile *file,
					     AsAppScope scope,
					     const gchar *arch,
					     guint32 load_flags,
					     guint32 watch_flags,
					     GCancellable *cancellable,
					     GError **error);

static void
as_store_finalize (GObject *object)
{
	AsStore *store = AS_STORE (object);
	AsStorePrivate *priv = GET_PRIVATE (store);

	g_free (priv->destdir);
	g_free (priv->origin);
	g_free (priv->builder_id);
	g_ptr_array_unref (priv->array);
	g_object_unref (priv->monitor);
	g_object_unref (priv->profile);
	g_object_unref (priv->stemmer);
	g_hash_table_unref (priv->hash_id);
	g_hash_table_unref (priv->hash_merge_id);
	g_hash_table_unref (priv->hash_unique_id);
	g_hash_table_unref (priv->hash_pkgname);
	g_hash_table_unref (priv->metadata_indexes);
	g_hash_table_unref (priv->appinfo_dirs);
	g_hash_table_unref (priv->search_blacklist);

	G_OBJECT_CLASS (as_store_parent_class)->finalize (object);
}

static void
as_store_class_init (AsStoreClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/**
	 * AsStore::changed:
	 * @store: the #AsStore instance that emitted the signal
	 *
	 * The ::changed signal is emitted when components have been added
	 * or removed from the store.
	 *
	 * Since: 0.1.2
	 **/
	signals [SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (AsStoreClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	/**
	 * AsStore::app-added:
	 * @store: the #AsStore instance that emitted the signal
	 * @app: the #AsApp instance
	 *
	 * The ::app-added signal is emitted when a component has been added to
	 * the store.
	 *
	 * Since: 0.6.5
	 **/
	signals [SIGNAL_APP_ADDED] =
		g_signal_new ("app-added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (AsStoreClass, app_added),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, AS_TYPE_APP);

	/**
	 * AsStore::app-removed:
	 * @store: the #AsStore instance that emitted the signal
	 * @app: the #AsApp instance
	 *
	 * The ::app-removed signal is emitted when a component has been removed
	 * from the store.
	 *
	 * Since: 0.6.5
	 **/
	signals [SIGNAL_APP_REMOVED] =
		g_signal_new ("app-removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (AsStoreClass, app_removed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, AS_TYPE_APP);

	/**
	 * AsStore::app-changed:
	 * @store: the #AsStore instance that emitted the signal
	 * @app: the #AsApp instance
	 *
	 * The ::app-changed signal is emitted when a component has been changed
	 * in the store.
	 *
	 * Since: 0.6.5
	 **/
	signals [SIGNAL_APP_CHANGED] =
		g_signal_new ("app-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (AsStoreClass, app_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, AS_TYPE_APP);

	object_class->finalize = as_store_finalize;
}

static void
as_store_perhaps_emit_changed (AsStore *store, const gchar *details)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	if (priv->changed_block_refcnt > 0) {
		priv->is_pending_changed_signal = TRUE;
		return;
	}
	if (!priv->is_pending_changed_signal) {
		priv->is_pending_changed_signal = TRUE;
		return;
	}
	g_debug ("Emitting ::changed() [%s]", details);
	g_signal_emit (store, signals[SIGNAL_CHANGED], 0);
	priv->is_pending_changed_signal = FALSE;
}

static guint32 *
as_store_changed_inhibit (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	priv->changed_block_refcnt++;
	return &priv->changed_block_refcnt;
}

static void
as_store_changed_uninhibit (guint32 **tok)
{
	if (tok == NULL || *tok == NULL)
		return;
	if (*(*tok) == 0) {
		g_critical ("changed_block_refcnt already zero");
		return;
	}
	(*(*tok))--;
	*tok = NULL;
}

static void
as_store_changed_uninhibit_cb (void *v)
{
	as_store_changed_uninhibit ((guint32 **)v);
}

#define _cleanup_uninhibit_ __attribute__ ((cleanup(as_store_changed_uninhibit_cb)))

/**
 * as_store_add_filter:
 * @store: a #AsStore instance.
 * @kind: a #AsAppKind, e.g. %AS_APP_KIND_FIRMWARE
 *
 * Adds a filter to the store so that only components of this type are
 * loaded into the store. This may be useful if the client is only interested
 * in certain types of component, or not interested in loading components
 * it cannot process.
 *
 * If no filter is set then all types of components are loaded.
 *
 * Since: 0.3.5
 **/
void
as_store_add_filter (AsStore *store, AsAppKind kind)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	priv->filter |= 1u << kind;
}

/**
 * as_store_remove_filter:
 * @store: a #AsStore instance.
 * @kind: a #AsAppKind, e.g. %AS_APP_KIND_FIRMWARE
 *
 * Removed a filter from the store so that components of this type are no longer
 * loaded into the store. This may be useful if the client is only interested
 * in certain types of component.
 *
 * If all filters are removed then all types of components are loaded.
 *
 * Since: 0.3.5
 **/
void
as_store_remove_filter (AsStore *store, AsAppKind kind)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	priv->filter &= ~(1u << kind);
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
 * as_store_remove_all:
 * @store: a #AsStore instance.
 *
 * Removes all applications from the store.
 *
 * Since: 0.2.5
 **/
void
as_store_remove_all (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_return_if_fail (AS_IS_STORE (store));
	g_ptr_array_set_size (priv->array, 0);
	g_hash_table_remove_all (priv->hash_id);
	g_hash_table_remove_all (priv->hash_merge_id);
	g_hash_table_remove_all (priv->hash_unique_id);
	g_hash_table_remove_all (priv->hash_pkgname);
}

static void
as_store_regen_metadata_index_key (AsStore *store, const gchar *key)
{
	AsApp *app;
	AsStorePrivate *priv = GET_PRIVATE (store);
	GHashTable *md;
	GPtrArray *apps;
	const gchar *tmp;
	guint i;

	/* regenerate cache */
	md = g_hash_table_new_full (g_str_hash, g_str_equal,
				    g_free, (GDestroyNotify) g_ptr_array_unref);
	for (i = 0; i < priv->array->len; i++) {
		app = g_ptr_array_index (priv->array, i);

		/* no data */
		tmp = as_app_get_metadata_item (app, key);
		if (tmp == NULL)
			continue;

		/* seen before */
		apps = g_hash_table_lookup (md, tmp);
		if (apps != NULL) {
			g_ptr_array_add (apps, g_object_ref (app));
			continue;
		}

		/* never seen before */
		apps = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
		g_ptr_array_add (apps, g_object_ref (app));
		g_hash_table_insert (md, g_strdup (tmp), apps);

	}
	g_hash_table_insert (priv->metadata_indexes, g_strdup (key), md);
}

/**
 * as_store_get_apps_by_metadata:
 * @store: a #AsStore instance.
 * @key: metadata key
 * @value: metadata value
 *
 * Gets an array of all the applications that match a specific metadata element.
 *
 * Returns: (element-type AsApp) (transfer container): an array
 *
 * Since: 0.1.4
 **/
GPtrArray *
as_store_get_apps_by_metadata (AsStore *store,
			       const gchar *key,
			       const gchar *value)
{
	AsApp *app;
	AsStorePrivate *priv = GET_PRIVATE (store);
	GHashTable *index;
	GPtrArray *apps;
	guint i;

	g_return_val_if_fail (AS_IS_STORE (store), NULL);

	/* do we have this indexed? */
	index = g_hash_table_lookup (priv->metadata_indexes, key);
	if (index != NULL) {
		if (g_hash_table_size (index) == 0) {
			as_store_regen_metadata_index_key (store, key);
			index = g_hash_table_lookup (priv->metadata_indexes, key);
		}
		apps = g_hash_table_lookup (index, value);
		if (apps != NULL)
			return g_ptr_array_ref (apps);
		return g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	}

	/* find all the apps with this specific metadata key */
	apps = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	for (i = 0; i < priv->array->len; i++) {
		app = g_ptr_array_index (priv->array, i);
		if (g_strcmp0 (as_app_get_metadata_item (app, key), value) != 0)
			continue;
		g_ptr_array_add (apps, g_object_ref (app));
	}
	return apps;
}


/**
 * as_store_get_apps_by_id:
 * @store: a #AsStore instance.
 * @id: the application full ID.
 *
 * Gets an array of all the applications that match a specific ID,
 * ignoring the prefix type.
 *
 * Returns: (element-type AsApp) (transfer container): an array
 *
 * Since: 0.5.12
 **/
GPtrArray *
as_store_get_apps_by_id (AsStore *store, const gchar *id)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	GPtrArray *apps;
	g_return_val_if_fail (AS_IS_STORE (store), NULL);
	apps = g_hash_table_lookup (priv->hash_id, id);
	if (apps != NULL)
		return g_ptr_array_ref (apps);
	return g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

/**
 * as_store_get_apps_by_id_merge:
 * @store: a #AsStore instance.
 * @id: the application full ID.
 *
 * Gets an array of all the merge applications that match a specific ID.
 *
 * Returns: (element-type AsApp) (transfer none): an array
 *
 * Since: 0.7.0
 **/
GPtrArray *
as_store_get_apps_by_id_merge (AsStore *store, const gchar *id)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_return_val_if_fail (AS_IS_STORE (store), NULL);
	return g_hash_table_lookup (priv->hash_merge_id, id);
}

/**
 * as_store_add_metadata_index:
 * @store: a #AsStore instance.
 * @key: the metadata key.
 *
 * Adds a metadata index key.
 *
 * NOTE: if applications are removed *all* the indexes will be invalid and
 * will have to be re-added.
 *
 * Since: 0.3.0
 **/
void
as_store_add_metadata_index (AsStore *store, const gchar *key)
{
	as_store_regen_metadata_index_key (store, key);
}

/**
 * as_store_get_app_by_id:
 * @store: a #AsStore instance.
 * @id: the application full ID.
 *
 * Finds an application in the store by ID.
 * If more than one application exists matching the specific ID,
 * (for instance when using %AS_STORE_ADD_FLAG_USE_UNIQUE_ID) then the
 * first item that was added is returned.
 *
 * Returns: (transfer none): a #AsApp or %NULL
 *
 * Since: 0.1.0
 **/
AsApp *
as_store_get_app_by_id (AsStore *store, const gchar *id)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	GPtrArray *apps;
	g_return_val_if_fail (AS_IS_STORE (store), NULL);
	apps = g_hash_table_lookup (priv->hash_id, id);
	if (apps == NULL)
		return NULL;
	return g_ptr_array_index (apps, 0);
}

static AsApp *
_as_app_new_from_unique_id (const gchar *unique_id)
{
	g_auto(GStrv) split = NULL;
	g_autoptr(AsApp) app = as_app_new ();

	split = g_strsplit (unique_id, "/", -1);
	if (g_strv_length (split) != AS_UTILS_UNIQUE_ID_PARTS)
		return NULL;
	if (g_strcmp0 (split[0], AS_APP_UNIQUE_WILDCARD) != 0)
		as_app_set_scope (app, as_app_scope_from_string (split[0]));
	if (g_strcmp0 (split[1], AS_APP_UNIQUE_WILDCARD) != 0) {
		if (g_strcmp0 (split[1], "package") == 0) {
			as_app_add_pkgname (app, "");
		} else {
			AsBundleKind kind = as_bundle_kind_from_string (split[1]);
			if (kind != AS_BUNDLE_KIND_UNKNOWN) {
				g_autoptr(AsBundle) bundle = as_bundle_new ();
				as_bundle_set_kind (bundle, kind);
				as_app_add_bundle (app, bundle);
			}
		}
	}
	if (g_strcmp0 (split[2], AS_APP_UNIQUE_WILDCARD) != 0)
		as_app_set_origin (app, split[2]);
	if (g_strcmp0 (split[3], AS_APP_UNIQUE_WILDCARD) != 0)
		as_app_set_kind (app, as_app_kind_from_string (split[3]));
	if (g_strcmp0 (split[4], AS_APP_UNIQUE_WILDCARD) != 0)
		as_app_set_id (app, split[4]);
	if (g_strcmp0 (split[5], AS_APP_UNIQUE_WILDCARD) != 0)
		as_app_set_branch (app, split[5]);

	return g_steal_pointer (&app);
}

static AsApp *
as_store_get_app_by_app (AsStore *store, AsApp *app)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	guint i;

	for (i = 0; i < priv->array->len; i++) {
		AsApp *app_tmp = g_ptr_array_index (priv->array, i);
		if (as_app_equal (app_tmp, app))
			return app_tmp;
	}
	return NULL;
}

/**
 * as_store_get_app_by_unique_id:
 * @store: a #AsStore instance.
 * @unique_id: the application unique ID, e.g.
 *      `user/flatpak/gnome-apps-nightly/app/gimp.desktop/master`
 * @search_flags: the search flags, e.g. %AS_STORE_SEARCH_FLAG_USE_WILDCARDS
 *
 * Finds an application in the store by matching the unique ID.
 *
 * Returns: (transfer none): a #AsApp or %NULL
 *
 * Since: 0.6.1
 **/
AsApp *
as_store_get_app_by_unique_id (AsStore *store,
			       const gchar *unique_id,
			       guint32 search_flags)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_autoptr(AsApp) app_tmp = NULL;

	g_return_val_if_fail (AS_IS_STORE (store), NULL);
	g_return_val_if_fail (unique_id != NULL, NULL);

	/* no globs */
	if ((search_flags & AS_STORE_SEARCH_FLAG_USE_WILDCARDS) == 0)
		return g_hash_table_lookup (priv->hash_unique_id, unique_id);

	/* create virtual app using scope/system/origin/kind/id/branch */
	app_tmp = _as_app_new_from_unique_id (unique_id);
	if (app_tmp == NULL)
		return NULL;
	return as_store_get_app_by_app (store, app_tmp);
}

/**
 * as_store_get_app_by_provide:
 * @store: a #AsStore instance.
 * @kind: the #AsProvideKind
 * @value: the provide value, e.g. "com.hughski.ColorHug2.firmware"
 *
 * Finds an application in the store by something that it provides.
 *
 * Returns: (transfer none): a #AsApp or %NULL
 *
 * Since: 0.5.0
 **/
AsApp *
as_store_get_app_by_provide (AsStore *store, AsProvideKind kind, const gchar *value)
{
	AsApp *app;
	AsProvide *tmp;
	AsStorePrivate *priv = GET_PRIVATE (store);
	guint i;
	guint j;
	GPtrArray *provides;

	g_return_val_if_fail (AS_IS_STORE (store), NULL);
	g_return_val_if_fail (kind != AS_PROVIDE_KIND_UNKNOWN, NULL);
	g_return_val_if_fail (value != NULL, NULL);

	/* find an application that provides something */
	for (i = 0; i < priv->array->len; i++) {
		app = g_ptr_array_index (priv->array, i);
		provides = as_app_get_provides (app);
		for (j = 0; j < provides->len; j++) {
			tmp = g_ptr_array_index (provides, j);
			if (kind != as_provide_get_kind (tmp))
				continue;
			if (g_strcmp0 (as_provide_get_value (tmp), value) != 0)
				continue;
			return app;
		}
	}
	return NULL;

}

/**
 * as_store_get_app_by_launchable:
 * @store: a #AsStore instance.
 * @kind: the #AsLaunchableKind
 * @value: the provide value, e.g. "gimp.desktop"
 *
 * Finds an application in the store that provides a specific launchable.
 *
 * Returns: (transfer none): a #AsApp or %NULL
 *
 * Since: 0.7.8
 **/
AsApp *
as_store_get_app_by_launchable (AsStore *store, AsLaunchableKind kind, const gchar *value)
{
	AsStorePrivate *priv = GET_PRIVATE (store);

	g_return_val_if_fail (AS_IS_STORE (store), NULL);
	g_return_val_if_fail (kind != AS_LAUNCHABLE_KIND_UNKNOWN, NULL);
	g_return_val_if_fail (value != NULL, NULL);

	for (guint i = 0; i < priv->array->len; i++) {
		AsApp *app = g_ptr_array_index (priv->array, i);
		GPtrArray *launchables = as_app_get_launchables (app);
		for (guint j = 0; j < launchables->len; j++) {
			AsLaunchable *tmp = g_ptr_array_index (launchables, j);
			if (kind != as_launchable_get_kind (tmp))
				continue;
			if (g_strcmp0 (as_launchable_get_value (tmp), value) != 0)
				continue;
			return app;
		}
	}
	return NULL;

}

/**
 * as_store_get_apps_by_provide:
 * @store: a #AsStore instance.
 * @kind: the #AsProvideKind
 * @value: the provide value, e.g. "com.hughski.ColorHug2.firmware"
 *
 * Finds any applications in the store by something that they provides.
 *
 * Returns: (transfer container) (element-type AsApp): an array of applications
 *
 * Since: 0.7.5
 **/
GPtrArray *
as_store_get_apps_by_provide (AsStore *store, AsProvideKind kind, const gchar *value)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	GPtrArray *apps = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	g_return_val_if_fail (AS_IS_STORE (store), NULL);
	g_return_val_if_fail (kind != AS_PROVIDE_KIND_UNKNOWN, NULL);
	g_return_val_if_fail (value != NULL, NULL);

	/* find an application that provides something */
	for (guint i = 0; i < priv->array->len; i++) {
		AsApp *app = g_ptr_array_index (priv->array, i);
		GPtrArray *provides = as_app_get_provides (app);
		for (guint j = 0; j < provides->len; j++) {
			AsProvide *tmp = g_ptr_array_index (provides, j);
			if (kind != as_provide_get_kind (tmp))
				continue;
			if (g_strcmp0 (as_provide_get_value (tmp), value) != 0)
				continue;
			g_ptr_array_add (apps, g_object_ref (app));
		}
	}
	return apps;
}

/**
 * as_store_get_app_by_id_ignore_prefix:
 * @store: a #AsStore instance.
 * @id: the application full ID.
 *
 * Finds an application in the store ignoring the prefix type.
 *
 * Returns: (transfer none): a #AsApp or %NULL
 *
 * Since: 0.5.12
 **/
AsApp *
as_store_get_app_by_id_ignore_prefix (AsStore *store, const gchar *id)
{
	AsApp *app;
	AsStorePrivate *priv = GET_PRIVATE (store);
	guint i;

	g_return_val_if_fail (AS_IS_STORE (store), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	/* find an application that provides something */
	for (i = 0; i < priv->array->len; i++) {
		app = g_ptr_array_index (priv->array, i);
		if (g_strcmp0 (as_app_get_id_no_prefix (app), id) == 0)
			return app;
	}
	return NULL;
}

/**
 * as_store_get_app_by_id_with_fallbacks:
 * @store: a #AsStore instance.
 * @id: the application full ID.
 *
 * Finds an application in the store by either by the current desktop ID
 * or a desktop ID that it has used previously. This allows upstream software
 * to change their ID (e.g. from cheese.desktop to org.gnome.Cheese.desktop)
 * without us duplicating entries in the software center.
 *
 * Returns: (transfer none): a #AsApp or %NULL
 *
 * Since: 0.4.1
 **/
AsApp *
as_store_get_app_by_id_with_fallbacks (AsStore *store, const gchar *id)
{
	AsApp *app;
	guint i;
	const struct {
		const gchar	*old;
		const gchar	*new;
	} id_map[] = {
		/* GNOME */
		{ "baobab.desktop",		"org.gnome.baobab.desktop" },
		{ "bijiben.desktop",		"org.gnome.bijiben.desktop" },
		{ "cheese.desktop",		"org.gnome.Cheese.desktop" },
		{ "devhelp.desktop",		"org.gnome.Devhelp.desktop" },
		{ "epiphany.desktop",		"org.gnome.Epiphany.desktop" },
		{ "file-roller.desktop",	"org.gnome.FileRoller.desktop" },
		{ "font-manager.desktop",	"org.gnome.FontManager.desktop" },
		{ "gcalctool.desktop",		"gnome-calculator.desktop" },
		{ "gcm-viewer.desktop",		"org.gnome.ColorProfileViewer.desktop" },
		{ "geary.desktop",		"org.gnome.Geary.desktop" },
		{ "gedit.desktop",		"org.gnome.gedit.desktop" },
		{ "glchess.desktop",		"gnome-chess.desktop" },
		{ "glines.desktop",		"five-or-more.desktop" },
		{ "gnect.desktop",		"four-in-a-row.desktop" },
		{ "gnibbles.desktop",		"gnome-nibbles.desktop" },
		{ "gnobots2.desktop",		"gnome-robots.desktop" },
		{ "gnome-2048.desktop",		"org.gnome.gnome-2048.desktop" },
		{ "gnome-boxes.desktop",	"org.gnome.Boxes.desktop" },
		{ "gnome-calculator.desktop",	"org.gnome.Calculator.desktop" },
		{ "gnome-clocks.desktop",	"org.gnome.clocks.desktop" },
		{ "gnome-contacts.desktop",	"org.gnome.Contacts.desktop" },
		{ "gnome-dictionary.desktop",	"org.gnome.Dictionary.desktop" },
		{ "gnome-disks.desktop",	"org.gnome.DiskUtility.desktop" },
		{ "gnome-documents.desktop",	"org.gnome.Documents.desktop" },
		{ "gnome-font-viewer.desktop",	"org.gnome.font-viewer.desktop" },
		{ "gnome-maps.desktop",		"org.gnome.Maps.desktop" },
		{ "gnome-nibbles.desktop",	"org.gnome.Nibbles.desktop" },
		{ "gnome-photos.desktop",	"org.gnome.Photos.desktop" },
		{ "gnome-power-statistics.desktop", "org.gnome.PowerStats.desktop" },
		{ "gnome-screenshot.desktop",	"org.gnome.Screenshot.desktop" },
		{ "gnome-software.desktop",	"org.gnome.Software.desktop" },
		{ "gnome-sound-recorder.desktop", "org.gnome.SoundRecorder.desktop" },
		{ "gnome-terminal.desktop",	"org.gnome.Terminal.desktop" },
		{ "gnome-weather.desktop",	"org.gnome.Weather.Application.desktop" },
		{ "gnomine.desktop",		"gnome-mines.desktop" },
		{ "gnotravex.desktop",		"gnome-tetravex.desktop" },
		{ "gnotski.desktop",		"gnome-klotski.desktop" },
		{ "gtali.desktop",		"tali.desktop" },
		{ "hitori.desktop",		"org.gnome.Hitori.desktop" },
		{ "latexila.desktop",		"org.gnome.latexila.desktop" },
		{ "lollypop.desktop",		"org.gnome.Lollypop.desktop" },
		{ "nautilus.desktop",		"org.gnome.Nautilus.desktop" },
		{ "polari.desktop",		"org.gnome.Polari.desktop" },
		{ "sound-juicer.desktop",	"org.gnome.SoundJuicer.desktop" },
		{ "totem.desktop",		"org.gnome.Totem.desktop" },

		/* KDE */
		{ "akregator.desktop",		"org.kde.akregator.desktop" },
		{ "apper.desktop",		"org.kde.apper.desktop" },
		{ "ark.desktop",		"org.kde.ark.desktop" },
		{ "blinken.desktop",		"org.kde.blinken.desktop" },
		{ "cantor.desktop",		"org.kde.cantor.desktop" },
		{ "digikam.desktop",		"org.kde.digikam.desktop" },
		{ "dolphin.desktop",		"org.kde.dolphin.desktop" },
		{ "dragonplayer.desktop",	"org.kde.dragonplayer.desktop" },
		{ "filelight.desktop",		"org.kde.filelight.desktop" },
		{ "gwenview.desktop",		"org.kde.gwenview.desktop" },
		{ "juk.desktop",		"org.kde.juk.desktop" },
		{ "kajongg.desktop",		"org.kde.kajongg.desktop" },
		{ "kalgebra.desktop",		"org.kde.kalgebra.desktop" },
		{ "kalzium.desktop",		"org.kde.kalzium.desktop" },
		{ "kamoso.desktop",		"org.kde.kamoso.desktop" },
		{ "kanagram.desktop",		"org.kde.kanagram.desktop" },
		{ "kapman.desktop",		"org.kde.kapman.desktop" },
		{ "kapptemplate.desktop",	"org.kde.kapptemplate.desktop" },
		{ "kbruch.desktop",		"org.kde.kbruch.desktop" },
		{ "kdevelop.desktop",		"org.kde.kdevelop.desktop" },
		{ "kfind.desktop",		"org.kde.kfind.desktop" },
		{ "kgeography.desktop",		"org.kde.kgeography.desktop" },
		{ "kgpg.desktop",		"org.kde.kgpg.desktop" },
		{ "khangman.desktop",		"org.kde.khangman.desktop" },
		{ "kig.desktop",		"org.kde.kig.desktop" },
		{ "kiriki.desktop",		"org.kde.kiriki.desktop" },
		{ "kiten.desktop",		"org.kde.kiten.desktop" },
		{ "klettres.desktop",		"org.kde.klettres.desktop" },
		{ "klipper.desktop",		"org.kde.klipper.desktop" },
		{ "KMail2.desktop",		"org.kde.kmail.desktop" },
		{ "kmplot.desktop",		"org.kde.kmplot.desktop" },
		{ "kollision.desktop",		"org.kde.kollision.desktop" },
		{ "kolourpaint.desktop",	"org.kde.kolourpaint.desktop" },
		{ "konsole.desktop",		"org.kde.konsole.desktop" },
		{ "Kontact.desktop",		"org.kde.kontact.desktop" },
		{ "korganizer.desktop",		"org.kde.korganizer.desktop" },
		{ "krita.desktop",		"org.kde.krita.desktop" },
		{ "kshisen.desktop",		"org.kde.kshisen.desktop" },
		{ "kstars.desktop",		"org.kde.kstars.desktop" },
		{ "ksudoku.desktop",		"org.kde.ksudoku.desktop" },
		{ "ktouch.desktop",		"org.kde.ktouch.desktop" },
		{ "ktp-log-viewer.desktop",	"org.kde.ktplogviewer.desktop" },
		{ "kturtle.desktop",		"org.kde.kturtle.desktop" },
		{ "kwordquiz.desktop",		"org.kde.kwordquiz.desktop" },
		{ "marble.desktop",		"org.kde.marble.desktop" },
		{ "okteta.desktop",		"org.kde.okteta.desktop" },
		{ "parley.desktop",		"org.kde.parley.desktop" },
		{ "partitionmanager.desktop",	"org.kde.PartitionManager.desktop" },
		{ "picmi.desktop",		"org.kde.picmi.desktop" },
		{ "rocs.desktop",		"org.kde.rocs.desktop" },
		{ "showfoto.desktop",		"org.kde.showfoto.desktop" },
		{ "skrooge.desktop",		"org.kde.skrooge.desktop" },
		{ "step.desktop",		"org.kde.step.desktop" },
		{ "yakuake.desktop",		"org.kde.yakuake.desktop" },

		/* others */
		{ "colorhug-ccmx.desktop",	"com.hughski.ColorHug.CcmxLoader.desktop" },
		{ "colorhug-flash.desktop",	"com.hughski.ColorHug.FlashLoader.desktop" },
		{ "dconf-editor.desktop",	"ca.desrt.dconf-editor.desktop" },
		{ "feedreader.desktop",		"org.gnome.FeedReader.desktop" },
		{ "qtcreator.desktop",		"org.qt-project.qtcreator.desktop" },

		{ NULL, NULL }
	};

	/* trivial case */
	app = as_store_get_app_by_id (store, id);
	if (app != NULL)
		return app;

	/* has the application ID been renamed */
	for (i = 0; id_map[i].old != NULL; i++) {
		if (g_strcmp0 (id, id_map[i].old) == 0)
			return as_store_get_app_by_id (store, id_map[i].new);
		if (g_strcmp0 (id, id_map[i].new) == 0)
			return as_store_get_app_by_id (store, id_map[i].old);
	}

	return NULL;
}

static gboolean
as_app_has_pkgname (AsApp *app, const gchar *pkgname)
{
	guint i;
	GPtrArray *pkgnames;

	pkgnames = as_app_get_pkgnames (app);
	for (i = 0; i < pkgnames->len; i++) {
		const gchar *tmp = g_ptr_array_index (pkgnames, i);
		if (g_strcmp0 (tmp, pkgname) == 0)
			return TRUE;
	}
	return FALSE;
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
	AsApp *app;
	AsStorePrivate *priv = GET_PRIVATE (store);
	guint i;

	g_return_val_if_fail (AS_IS_STORE (store), NULL);

	/* in most cases, we can use the cache */
	app = g_hash_table_lookup (priv->hash_pkgname, pkgname);
	if (app != NULL)
		return app;

	/* fall back in case the user adds to app to the store, *then*
	 * uses as_app_add_pkgname() on the app */
	for (i = 0; i < priv->array->len; i++) {
		app = g_ptr_array_index (priv->array, i);
		if (as_app_has_pkgname (app, pkgname))
			return app;
	}

	/* not found */
	return NULL;
}

/**
 * as_store_get_app_by_pkgnames:
 * @store: a #AsStore instance.
 * @pkgnames: the package names to find.
 *
 * Finds an application in the store by any of the possible package names.
 *
 * Returns: (transfer none): a #AsApp or %NULL
 *
 * Since: 0.4.1
 **/
AsApp *
as_store_get_app_by_pkgnames (AsStore *store, gchar **pkgnames)
{
	AsApp *app;
	AsStorePrivate *priv = GET_PRIVATE (store);
	guint i;

	g_return_val_if_fail (AS_IS_STORE (store), NULL);
	g_return_val_if_fail (pkgnames != NULL, NULL);

	for (i = 0; pkgnames[i] != NULL; i++) {
		app = g_hash_table_lookup (priv->hash_pkgname, pkgnames[i]);
		if (app != NULL)
			return app;
	}
	return NULL;
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
	GPtrArray *apps;

	/* emit before removal */
	g_signal_emit (store, signals[SIGNAL_APP_REMOVED], 0, app);

	/* only remove this specific unique app */
	apps = g_hash_table_lookup (priv->hash_id, as_app_get_id (app));
	if (apps != NULL) {
		g_ptr_array_remove (apps, app);

		/* remove the array as well if it was the last app as the
		 * AsRefString with the app ID may get freed now */
		if (apps->len == 0)
			g_hash_table_remove (priv->hash_id, as_app_get_id (app));
	}

	g_hash_table_remove (priv->hash_unique_id, as_app_get_unique_id (app));
	g_ptr_array_remove (priv->array, app);
	g_hash_table_remove_all (priv->metadata_indexes);

	/* removed */
	as_store_perhaps_emit_changed (store, "remove-app");
}

/**
 * as_store_remove_app_by_id:
 * @store: a #AsStore instance.
 * @id: an application id
 *
 * Removes an application from the store if it exists.
 *
 * Since: 0.3.0
 **/
void
as_store_remove_app_by_id (AsStore *store, const gchar *id)
{
	AsApp *app;
	AsStorePrivate *priv = GET_PRIVATE (store);
	guint i;

	if (!g_hash_table_remove (priv->hash_id, id))
		return;
	for (i = 0; i < priv->array->len; i++) {
		app = g_ptr_array_index (priv->array, i);
		if (g_strcmp0 (id, as_app_get_id (app)) != 0)
			continue;

		/* emit before removal */
		g_signal_emit (store, signals[SIGNAL_APP_REMOVED], 0, app);

		g_ptr_array_remove (priv->array, app);
		g_hash_table_remove (priv->hash_unique_id,
				     as_app_get_unique_id (app));
	}
	g_hash_table_remove_all (priv->metadata_indexes);

	/* removed */
	as_store_perhaps_emit_changed (store, "remove-app-by-id");
}

static gboolean
_as_app_is_perhaps_merge_component (AsApp *app)
{
	if (as_app_get_kind (app) != AS_APP_KIND_DESKTOP)
		return FALSE;
	if (as_app_get_format_by_kind (app, AS_FORMAT_KIND_APPSTREAM) == NULL)
		return FALSE;
	if (as_app_get_bundle_kind (app) != AS_BUNDLE_KIND_UNKNOWN)
		return FALSE;
	if (as_app_get_name (app, NULL) != NULL)
		return FALSE;
	return TRUE;
}

/**
 * as_store_add_apps:
 * @store: a #AsStore instance.
 * @apps: (element-type AsApp): an array of apps
 *
 * Adds several applications to the store.
 *
 * Additionally only applications where the kind is known will be added.
 *
 * Since: 0.6.4
 **/
void
as_store_add_apps (AsStore *store, GPtrArray *apps)
{
	guint i;
	_cleanup_uninhibit_ guint32 *tok = NULL;

	g_return_if_fail (AS_IS_STORE (store));

	/* emit once when finished */
	tok = as_store_changed_inhibit (store);
	for (i = 0; i < apps->len; i++) {
		AsApp *app = g_ptr_array_index (apps, i);
		as_store_add_app (store, app);
	}

	/* this store has changed */
	as_store_changed_uninhibit (&tok);
	as_store_perhaps_emit_changed (store, "add-apps");
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
	AsApp *item = NULL;
	AsStorePrivate *priv = GET_PRIVATE (store);
	GPtrArray *apps;
	GPtrArray *pkgnames;
	const gchar *id;
	const gchar *pkgname;
	guint i;

	/* have we recorded this before? */
	id = as_app_get_id (app);
	if (id == NULL) {
		g_warning ("application has no ID set");
		return;
	}

	/* use some hacky logic to support older files */
	if ((priv->add_flags & AS_STORE_ADD_FLAG_USE_MERGE_HEURISTIC) > 0 &&
	    _as_app_is_perhaps_merge_component (app)) {
		as_app_set_merge_kind (app, AS_APP_MERGE_KIND_APPEND);
	}

	/* FIXME: deal with the differences between append and replace */
	if (as_app_get_merge_kind (app) == AS_APP_MERGE_KIND_APPEND ||
	    as_app_get_merge_kind (app) == AS_APP_MERGE_KIND_REPLACE)
		as_app_add_quirk (app, AS_APP_QUIRK_MATCH_ANY_PREFIX);

	/* ensure app has format set */
	if (as_app_get_format_default (app) == NULL) {
		g_autoptr(AsFormat) format = as_format_new ();
		as_format_set_kind (format, AS_FORMAT_KIND_UNKNOWN);
		as_app_add_format (app, format);
	}

	/* this is a special merge component */
	if (as_app_has_quirk (app, AS_APP_QUIRK_MATCH_ANY_PREFIX)) {
		guint64 flags = AS_APP_SUBSUME_FLAG_MERGE;
		AsAppMergeKind merge_kind = as_app_get_merge_kind (app);

		apps = g_hash_table_lookup (priv->hash_merge_id, id);
		if (apps == NULL) {
			apps = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
			g_hash_table_insert (priv->hash_merge_id,
					     g_strdup (as_app_get_id (app)),
					     apps);
		}
		g_debug ("added %s merge component: %s",
			 as_app_merge_kind_to_string (merge_kind),
			 as_app_get_unique_id (app));
		g_ptr_array_add (apps, g_object_ref (app));

		/* apply to existing components */
		flags |= AS_APP_SUBSUME_FLAG_NO_OVERWRITE;
		if (merge_kind == AS_APP_MERGE_KIND_REPLACE)
			flags |= AS_APP_SUBSUME_FLAG_REPLACE;
		for (i = 0; i < priv->array->len; i++) {
			AsApp *app_tmp = g_ptr_array_index (priv->array, i);
			if (g_strcmp0 (as_app_get_id (app_tmp), id) != 0)
				continue;
			g_debug ("using %s merge component %s on %s",
				 as_app_merge_kind_to_string (merge_kind),
				 id, as_app_get_unique_id (app_tmp));
			as_app_subsume_full (app_tmp, app, flags);

			/* emit after changes have been made */
			g_signal_emit (store, signals[SIGNAL_APP_CHANGED],
				       0, app_tmp);
		}
		return;
	}

	/* is there any merge components to add to this app */
	apps = g_hash_table_lookup (priv->hash_merge_id, id);
	if (apps != NULL) {
		for (i = 0; i < apps->len; i++) {
			AsApp *app_tmp = g_ptr_array_index (apps, i);
			AsAppMergeKind merge_kind = as_app_get_merge_kind (app_tmp);
			guint64 flags = AS_APP_SUBSUME_FLAG_MERGE;
			g_debug ("using %s merge component %s on %s",
				 as_app_merge_kind_to_string (merge_kind),
				 as_app_get_unique_id (app_tmp),
				 as_app_get_unique_id (app));
			flags |= AS_APP_SUBSUME_FLAG_NO_OVERWRITE;
			if (merge_kind == AS_APP_MERGE_KIND_REPLACE)
				flags |= AS_APP_SUBSUME_FLAG_REPLACE;
			as_app_subsume_full (app, app_tmp, flags);
		}
	}

	/* find the item */
	if (priv->add_flags & AS_STORE_ADD_FLAG_USE_UNIQUE_ID) {
		item = as_store_get_app_by_app (store, app);
	} else {
		apps = g_hash_table_lookup (priv->hash_id, id);
		if (apps != NULL && apps->len > 0)
			item = g_ptr_array_index (apps, 0);
	}
	if (item != NULL) {
		AsFormat *app_format = as_app_get_format_default (app);
		AsFormat *item_format = as_app_get_format_default (item);

		/* sanity check */
		if (app_format == NULL) {
			g_warning ("no format specified in %s",
				   as_app_get_unique_id (app));
			return;
		}
		if (item_format == NULL) {
			g_warning ("no format specified in %s",
				   as_app_get_unique_id (item));
			return;
		}

		/* the previously stored app is what we actually want */
		if ((priv->add_flags & AS_STORE_ADD_FLAG_PREFER_LOCAL) > 0) {

			if (as_format_get_kind (app_format) == AS_FORMAT_KIND_APPSTREAM &&
			    as_format_get_kind (item_format) == AS_FORMAT_KIND_APPDATA) {
				g_debug ("ignoring AppStream entry as AppData exists: %s:%s",
					 as_app_get_unique_id (app),
					 as_app_get_unique_id (item));
				as_app_subsume_full (app, item,
						     AS_APP_SUBSUME_FLAG_FORMATS |
						     AS_APP_SUBSUME_FLAG_RELEASES);
				return;
			}
			if (as_format_get_kind (app_format) == AS_FORMAT_KIND_APPSTREAM &&
			    as_format_get_kind (item_format) == AS_FORMAT_KIND_DESKTOP) {
				g_debug ("ignoring AppStream entry as desktop exists: %s:%s",
					 as_app_get_unique_id (app),
					 as_app_get_unique_id (item));
				return;
			}
			if (as_format_get_kind (app_format) == AS_FORMAT_KIND_APPDATA &&
			    as_format_get_kind (item_format) == AS_FORMAT_KIND_DESKTOP) {
				g_debug ("merging duplicate AppData:desktop entries: %s:%s",
					 as_app_get_unique_id (app),
					 as_app_get_unique_id (item));
				as_app_subsume_full (app, item,
						     AS_APP_SUBSUME_FLAG_BOTH_WAYS |
						     AS_APP_SUBSUME_FLAG_DEDUPE);
				return;
			}
			if (as_format_get_kind (app_format) == AS_FORMAT_KIND_DESKTOP &&
			    as_format_get_kind (item_format) == AS_FORMAT_KIND_APPDATA) {
				g_debug ("merging duplicate desktop:AppData entries: %s:%s",
					 as_app_get_unique_id (app),
					 as_app_get_unique_id (item));
				as_app_subsume_full (app, item,
						     AS_APP_SUBSUME_FLAG_BOTH_WAYS |
						     AS_APP_SUBSUME_FLAG_DEDUPE);
				return;
			}

			/* xxx */
			as_app_subsume_full (app, item,
					     AS_APP_SUBSUME_FLAG_FORMATS |
					     AS_APP_SUBSUME_FLAG_RELEASES);

		} else {
			if (as_format_get_kind (app_format) == AS_FORMAT_KIND_APPDATA &&
			    as_format_get_kind (item_format) == AS_FORMAT_KIND_APPSTREAM &&
			    as_app_get_scope (app) == AS_APP_SCOPE_SYSTEM) {
				g_debug ("ignoring AppData entry as AppStream exists: %s:%s",
					 as_app_get_unique_id (app),
					 as_app_get_unique_id (item));
				as_app_subsume_full (item, app,
						     AS_APP_SUBSUME_FLAG_FORMATS |
						     AS_APP_SUBSUME_FLAG_RELEASES);
				return;
			}
			if (as_format_get_kind (app_format) == AS_FORMAT_KIND_DESKTOP &&
			    as_format_get_kind (item_format) == AS_FORMAT_KIND_APPSTREAM &&
			    as_app_get_scope (app) == AS_APP_SCOPE_SYSTEM) {
				g_debug ("ignoring desktop entry as AppStream exists: %s:%s",
					 as_app_get_unique_id (app),
					 as_app_get_unique_id (item));
				as_app_subsume_full (item, app,
						     AS_APP_SUBSUME_FLAG_FORMATS);
				return;
			}

			/* the previously stored app is higher priority */
			if (as_app_get_priority (item) >
			    as_app_get_priority (app)) {
				g_debug ("ignoring duplicate %s:%s entry: %s:%s",
					 as_format_kind_to_string (as_format_get_kind (app_format)),
					 as_format_kind_to_string (as_format_get_kind (item_format)),
					 as_app_get_unique_id (app),
					 as_app_get_unique_id (item));
				as_app_subsume_full (item, app,
						     AS_APP_SUBSUME_FLAG_FORMATS |
						     AS_APP_SUBSUME_FLAG_RELEASES);
				return;
			}

			/* same priority */
			if (as_app_get_priority (item) ==
			    as_app_get_priority (app)) {
				g_debug ("merging duplicate %s:%s entries: %s:%s",
					 as_format_kind_to_string (as_format_get_kind (app_format)),
					 as_format_kind_to_string (as_format_get_kind (item_format)),
					 as_app_get_unique_id (app),
					 as_app_get_unique_id (item));
				as_app_subsume_full (app, item,
						     AS_APP_SUBSUME_FLAG_BOTH_WAYS |
						     AS_APP_SUBSUME_FLAG_DEDUPE);
				return;
			}
		}

		/* this new item has a higher priority than the one we've
		 * previously stored */
		g_debug ("removing %s entry: %s",
			 as_format_kind_to_string (as_format_get_kind (item_format)),
			 as_app_get_unique_id (item));
		as_app_subsume_full (app, item,
				     AS_APP_SUBSUME_FLAG_FORMATS |
				     AS_APP_SUBSUME_FLAG_RELEASES);
		as_store_remove_app (store, item);
	}

	/* create hash of id:[apps] if required */
	apps = g_hash_table_lookup (priv->hash_id, id);
	if (apps == NULL) {
		apps = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
		g_hash_table_insert (priv->hash_id,
				     g_strdup (as_app_get_id (app)),
				     apps);
	}
	g_ptr_array_add (apps, g_object_ref (app));

	/* success, add to array */
	g_ptr_array_add (priv->array, g_object_ref (app));
	g_hash_table_insert (priv->hash_unique_id,
			     g_strdup (as_app_get_unique_id (app)),
			     g_object_ref (app));
	pkgnames = as_app_get_pkgnames (app);
	for (i = 0; i < pkgnames->len; i++) {
		pkgname = g_ptr_array_index (pkgnames, i);
		g_hash_table_insert (priv->hash_pkgname,
				     g_strdup (pkgname),
				     g_object_ref (app));
	}

	/* add helper objects */
	as_app_set_stemmer (app, priv->stemmer);
	as_app_set_search_blacklist (app, priv->search_blacklist);
	as_app_set_search_match (app, priv->search_match);

	/* added */
	g_signal_emit (store, signals[SIGNAL_APP_ADDED], 0, app);
	as_store_perhaps_emit_changed (store, "add-app");
}

static void
as_store_match_addons_app (AsStore *store, AsApp *app)
{
	GPtrArray *plugin_ids;
	guint i;
	guint j;

	plugin_ids = as_app_get_extends (app);
	if (plugin_ids->len == 0) {
		g_warning ("%s was of type addon but had no extends",
			   as_app_get_id (app));
		return;
	}
	for (j = 0; j < plugin_ids->len; j++) {
		g_autoptr(GPtrArray) parents = NULL;
		const gchar *tmp = g_ptr_array_index (plugin_ids, j);

		/* restrict to same scope and bundle kind */
		parents = as_store_get_apps_by_id (store, tmp);
		for (i = 0; i < parents->len;  i++) {
			AsApp *parent = g_ptr_array_index (parents, i);
			if (as_app_get_scope (app) != as_app_get_scope (parent))
				continue;
			if (as_app_get_bundle_kind (app) != as_app_get_bundle_kind (parent))
				continue;
			as_app_add_addon (parent, app);
		}
	}
}

static void
as_store_match_addons (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	guint i;
	g_autoptr(AsProfileTask) ptask = NULL;

	/* profile */
	ptask = as_profile_start_literal (priv->profile, "AsStore:match-addons");
	g_assert (ptask != NULL);
	for (i = 0; i < priv->array->len; i++) {
		AsApp *app = g_ptr_array_index (priv->array, i);
		if (as_app_get_kind (app) != AS_APP_KIND_ADDON)
			continue;
		as_store_match_addons_app (store, app);
	}
}

/**
 * as_store_fixup_id_prefix:
 *
 * When we lived in a world where all software got installed to /usr we could
 * continue to use the application ID as the primary identifier.
 *
 * Now we support installing things per-user, and also per-system and per-user
 * flatpak (not even including jhbuild) we need to use the id prefix to
 * disambiguate the different applications according to a 'scope'.
 *
 * This means when we launch a specific application in the software center
 * we know what desktop file to use, and we can also then support different
 * versions of applications installed system wide and per-user.
 **/
static void
as_store_fixup_id_prefix (AsApp *app, const gchar *id_prefix)
{
	g_autofree gchar *id = NULL;

	/* ignore this for compatibility reasons */
	if (id_prefix == NULL || g_strcmp0 (id_prefix, "system") == 0)
		return;
	id = g_strdup_printf ("%s:%s", id_prefix, as_app_get_id (app));
	as_app_set_id (app, id);
}

static gboolean
as_store_from_root (AsStore *store,
		    AsNode *root,
		    AsAppScope scope,
		    const gchar *icon_prefix,
		    const gchar *source_filename,
		    const gchar *arch,
		    guint32 load_flags,
		    GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	AsNode *apps;
	AsNode *n;
	const gchar *tmp;
	const gchar *origin_delim = ":";
	gchar *str;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autofree gchar *icon_path = NULL;
	g_autofree gchar *id_prefix_app = NULL;
	g_autofree gchar *origin_app = NULL;
	g_autofree gchar *origin_app_icons = NULL;
	_cleanup_uninhibit_ guint32 *tok = NULL;
	g_autoptr(AsFormat) format = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;
	g_autoptr(AsRefString) icon_path_str = NULL;
	g_autoptr(AsRefString) origin_str = NULL;
	gboolean origin_is_flatpak;

	g_return_val_if_fail (AS_IS_STORE (store), FALSE);

	/* make throws us under a bus, yet again */
	tmp = g_getenv ("AS_SELF_TEST_PREFIX_DELIM");
	if (tmp != NULL)
		origin_delim = tmp;

	/* profile */
	ptask = as_profile_start_literal (priv->profile, "AsStore:store-from-root");
	g_assert (ptask != NULL);

	/* emit once when finished */
	tok = as_store_changed_inhibit (store);

	apps = as_node_find (root, "components");
	if (apps == NULL) {
		apps = as_node_find (root, "applications");
		if (apps == NULL) {
			g_set_error_literal (error,
					     AS_STORE_ERROR,
					     AS_STORE_ERROR_FAILED,
					     "No valid root node specified");
			return FALSE;
		}
		priv->problems |= AS_STORE_PROBLEM_LEGACY_ROOT;
	}

	/* get version */
	tmp = as_node_get_attribute (apps, "version");
	if (tmp != NULL)
		priv->api_version = g_ascii_strtod (tmp, NULL);

	/* set in the XML file */
	tmp = as_node_get_attribute (apps, "origin");
	if (tmp != NULL)
		as_store_set_origin (store, tmp);

	/* origin has prefix already specified in the XML */
	if (priv->origin != NULL) {
		str = g_strstr_len (priv->origin, -1, origin_delim);
		if (str != NULL) {
			id_prefix_app = g_strdup (priv->origin);
			str = g_strstr_len (id_prefix_app, -1, origin_delim);
			if (str != NULL) {
				str[0] = '\0';
				origin_app = g_strdup (str + 1);
				origin_app_icons = g_strdup (str + 1);
			}
		}
	}

	origin_is_flatpak = g_strcmp0 (priv->origin, "flatpak") == 0;

	/* special case flatpak symlinks -- scope:name.xml.gz */
	if (origin_app == NULL &&
	    origin_is_flatpak &&
	    source_filename != NULL &&
	    g_file_test (source_filename, G_FILE_TEST_IS_SYMLINK)) {
		g_autofree gchar *source_basename = NULL;

		/* get the origin */
		source_basename = g_path_get_basename (source_filename);
		str = g_strrstr (source_basename, ".xml");
		if (str != NULL) {
			str[0] = '\0';
			origin_app_icons = g_strdup (source_basename);
		}

		/* get the id-prefix */
		str = g_strstr_len (source_basename, -1, origin_delim);
		if (str != NULL) {
			str[0] = '\0';
			origin_app = g_strdup (str + 1);
			id_prefix_app = g_strdup (source_basename);
		}

		/* although in ~, this is a system scope app */
		if (g_strcmp0 (id_prefix_app, "flatpak") == 0)
			scope = AS_APP_SCOPE_SYSTEM;
	}

	/* fallback */
	if (origin_app == NULL && !origin_is_flatpak) {
		id_prefix_app = g_strdup (as_app_scope_to_string (scope));
		origin_app = g_strdup (priv->origin);
		origin_app_icons = g_strdup (priv->origin);
	}

	/* print what cleverness we did */
	if (g_strcmp0 (origin_app, priv->origin) != 0) {
		g_debug ("using app origin of '%s' rather than '%s'",
			 origin_app, priv->origin);
	}

	/* guess the icon path after we've read the origin and then look for
	 * ../icons/$origin if the topdir is 'xmls', falling back to ./icons */
	if (icon_prefix != NULL) {
		g_autofree gchar *topdir = NULL;
		topdir = g_path_get_basename (icon_prefix);
		if ((g_strcmp0 (topdir, "xmls") == 0 ||
		     g_strcmp0 (topdir, "yaml") == 0)
		    && origin_app_icons != NULL) {
			g_autofree gchar *dirname = NULL;
			dirname = g_path_get_dirname (icon_prefix);
			icon_path = g_build_filename (dirname,
						      "icons",
						      origin_app_icons,
						      NULL);
		} else {
			icon_path = g_build_filename (icon_prefix, "icons", NULL);
		}
	}
	g_debug ("using icon path %s", icon_path);

	/* set in the XML file */
	tmp = as_node_get_attribute (apps, "builder_id");
	if (tmp != NULL)
		as_store_set_builder_id (store, tmp);

	/* create refcounted versions */
	if (origin_app != NULL)
		origin_str = as_ref_string_new (origin_app);
	if (icon_path != NULL)
		icon_path_str = as_ref_string_new (icon_path);

	/* create format for all added apps */
	format = as_format_new ();
	as_format_set_kind (format, AS_FORMAT_KIND_APPSTREAM);
	if (source_filename != NULL)
		as_format_set_filename (format, source_filename);

	ctx = as_node_context_new ();
	for (n = apps->children; n != NULL; n = n->next) {
		g_autoptr(GError) error_local = NULL;
		g_autoptr(AsApp) app = NULL;
		if (as_node_get_tag (n) != AS_TAG_COMPONENT)
			continue;

		/* do the filtering here */
		if (priv->filter != 0) {
			if (g_strcmp0 (as_node_get_name (n), "component") == 0) {
				AsAppKind kind_tmp;
				tmp = as_node_get_attribute (n, "type");
				kind_tmp = as_app_kind_from_string (tmp);
				if ((priv->filter & (1u << kind_tmp)) == 0)
					continue;
			}
		}

		app = as_app_new ();
		if (icon_path_str != NULL)
			as_app_set_icon_path (app, icon_path_str);
		if (arch != NULL)
			as_app_add_arch (app, arch);
		as_app_add_format (app, format);
		as_app_set_scope (app, scope);
		if (!as_app_node_parse (app, n, ctx, &error_local)) {
			g_set_error (error,
				     AS_STORE_ERROR,
				     AS_STORE_ERROR_FAILED,
				     "Failed to parse root: %s",
				     error_local->message);
			return FALSE;
		}

		/* filter out non-merge types */
		if (load_flags & AS_STORE_LOAD_FLAG_ONLY_MERGE_APPS) {
			if (as_app_get_merge_kind (app) != AS_APP_MERGE_KIND_REPLACE &&
			    as_app_get_merge_kind (app) != AS_APP_MERGE_KIND_APPEND) {
				continue;
			}
		}

		/* set the ID prefix */
		if ((priv->add_flags & AS_STORE_ADD_FLAG_USE_UNIQUE_ID) == 0)
			as_store_fixup_id_prefix (app, id_prefix_app);

		if (origin_str != NULL)
			as_app_set_origin (app, origin_str);
		as_store_add_app (store, app);
	}

	/* add addon kinds to their parent AsApp */
	as_store_match_addons (store);

	/* this store has changed */
	as_store_changed_uninhibit (&tok);
	as_store_perhaps_emit_changed (store, "from-root");

	return TRUE;
}

static gboolean
as_store_load_yaml_file (AsStore *store,
			 GFile *file,
			 AsAppScope scope,
			 GCancellable *cancellable,
			 GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	AsNode *app_n;
	AsNode *n;
	AsYamlFromFlags flags = AS_YAML_FROM_FLAG_NONE;
	const gchar *tmp;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autofree gchar *icon_path = NULL;
	g_autofree gchar *source_filename = NULL;
	g_autoptr(AsYaml) root = NULL;
	g_autoptr(AsFormat) format = NULL;
	_cleanup_uninhibit_ guint32 *tok = NULL;

	/* load file */
	if (priv->add_flags & AS_STORE_ADD_FLAG_ONLY_NATIVE_LANGS)
		flags |= AS_YAML_FROM_FLAG_ONLY_NATIVE_LANGS;
	root = as_yaml_from_file (file, flags, cancellable, error);
	if (root == NULL)
		return FALSE;

	/* get header information */
	ctx = as_node_context_new ();
	for (n = root->children->children; n != NULL; n = n->next) {
		tmp = as_yaml_node_get_key (n);
		if (g_strcmp0 (tmp, "Origin") == 0) {
			as_store_set_origin (store, as_yaml_node_get_value (n));
			continue;
		}
		if (g_strcmp0 (tmp, "Version") == 0) {
			if (as_yaml_node_get_value (n) != NULL)
				as_store_set_api_version (store, g_ascii_strtod (as_yaml_node_get_value (n), NULL));
			continue;
		}
		if (g_strcmp0 (tmp, "MediaBaseUrl") == 0) {
			as_node_context_set_media_base_url (ctx, as_yaml_node_get_value (n));
			continue;
		}
	}

	/* if we have an origin either from the YAML or _set_origin() */
	if (priv->origin != NULL) {
		g_autofree gchar *filename = NULL;
		g_autofree gchar *icon_prefix1 = NULL;
		g_autofree gchar *icon_prefix2 = NULL;
		filename = g_file_get_path (file);
		icon_prefix1 = g_path_get_dirname (filename);
		icon_prefix2 = g_path_get_dirname (icon_prefix1);
		icon_path = g_build_filename (icon_prefix2,
					      "icons",
					      priv->origin,
					      NULL);
	}

	/* emit once when finished */
	tok = as_store_changed_inhibit (store);

	/* add format to each app */
	source_filename = g_file_get_path (file);
	if (source_filename != NULL) {
		format = as_format_new ();
		as_format_set_kind (format, AS_FORMAT_KIND_APPSTREAM);
		as_format_set_filename (format, source_filename);
	}

	/* parse applications */
	for (app_n = root->children->next; app_n != NULL; app_n = app_n->next) {
		g_autoptr(AsApp) app = NULL;
		if (app_n->children == NULL)
			continue;
		app = as_app_new ();

		/* do the filtering here */
		if (priv->filter != 0) {
			if ((priv->filter & (1u << as_app_get_kind (app))) == 0)
				continue;
		}

		if (icon_path != NULL)
			as_app_set_icon_path (app, icon_path);
		as_app_set_scope (app, scope);
		as_app_add_format (app, format);
		if (!as_app_node_parse_dep11 (app, app_n, ctx, error))
			return FALSE;
		as_app_set_origin (app, priv->origin);
		if (as_app_get_id (app) != NULL)
			as_store_add_app (store, app);
	}

	/* emit changed */
	as_store_changed_uninhibit (&tok);
	as_store_perhaps_emit_changed (store, "yaml-file");

	return TRUE;
}

static void
as_store_remove_by_source_file (AsStore *store, const gchar *filename)
{
	AsApp *app;
	GPtrArray *apps;
	guint i;
	const gchar *tmp;
	_cleanup_uninhibit_ guint32 *tok = NULL;
	g_autoptr(GPtrArray) ids = NULL;

	/* find any applications in the store with this source file */
	ids = g_ptr_array_new_with_free_func (g_free);
	apps = as_store_get_apps (store);
	for (i = 0; i < apps->len; i++) {
		AsFormat *format;
		app = g_ptr_array_index (apps, i);
		format = as_app_get_format_by_filename (app, filename);
		if (format == NULL)
			continue;
		as_app_remove_format (app, format);

		/* remove the app when all the formats have gone */
		if (as_app_get_formats(app)->len == 0) {
			g_debug ("no more formats for %s, deleting from store",
				 as_app_get_unique_id (app));
			g_ptr_array_add (ids, g_strdup (as_app_get_id (app)));
		}
	}

	/* remove these from the store */
	tok = as_store_changed_inhibit (store);
	for (i = 0; i < ids->len; i++) {
		tmp = g_ptr_array_index (ids, i);
		g_debug ("removing %s as %s invalid", tmp, filename);
		as_store_remove_app_by_id (store, tmp);
	}

	/* the store changed */
	as_store_changed_uninhibit (&tok);
	as_store_perhaps_emit_changed (store, "remove-by-source-file");
}

static void
as_store_watch_source_added (AsStore *store, const gchar *filename)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	AsStorePathData *path_data;
	g_autofree gchar *dirname = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) file = NULL;

	/* ignore directories */
	if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR))
		return;

	/* we helpfully saved this */
	dirname = g_path_get_dirname (filename);
	g_debug ("parsing new file %s from %s", filename, dirname);
	path_data = g_hash_table_lookup (priv->appinfo_dirs, filename);
	if (path_data == NULL)
		path_data = g_hash_table_lookup (priv->appinfo_dirs, dirname);
	if (path_data == NULL) {
		g_warning ("no path data for %s", dirname);
		return;
	}
	file = g_file_new_for_path (filename);
	/* Do not watch the file for changes: we're already watching its
	 * parent directory */
	if (!as_store_from_file_internal (store,
					  file,
					  path_data->scope,
					  path_data->arch,
					  AS_STORE_LOAD_FLAG_NONE,
					  AS_STORE_WATCH_FLAG_NONE,
					  NULL, /* cancellable */
					  &error)){
		g_warning ("failed to rescan: %s", error->message);
	}
}

static void
as_store_watch_source_changed (AsStore *store, const gchar *filename)
{
	/* remove all the apps provided by the source file then re-add them */
	g_debug ("re-parsing changed file %s", filename);
	as_store_remove_by_source_file (store, filename);
	as_store_watch_source_added (store, filename);
}

static void
as_store_monitor_changed_cb (AsMonitor *monitor,
			     const gchar *filename,
			     AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	_cleanup_uninhibit_ guint32 *tok = NULL;

	/* reload, or emit a signal */
	tok = as_store_changed_inhibit (store);
	if (priv->watch_flags & AS_STORE_WATCH_FLAG_ADDED)
		as_store_watch_source_changed (store, filename);
	as_store_changed_uninhibit (&tok);
	as_store_perhaps_emit_changed (store, "file changed");
}

static void
as_store_monitor_added_cb (AsMonitor *monitor,
			     const gchar *filename,
			     AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	_cleanup_uninhibit_ guint32 *tok = NULL;

	/* reload, or emit a signal */
	tok = as_store_changed_inhibit (store);
	if (priv->watch_flags & AS_STORE_WATCH_FLAG_ADDED)
		as_store_watch_source_added (store, filename);
	as_store_changed_uninhibit (&tok);
	as_store_perhaps_emit_changed (store, "file added");
}

static void
as_store_monitor_removed_cb (AsMonitor *monitor,
			     const gchar *filename,
			     AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	/* remove, or emit a signal */
	if (priv->watch_flags & AS_STORE_WATCH_FLAG_REMOVED) {
		as_store_remove_by_source_file (store, filename);
	} else {
		as_store_perhaps_emit_changed (store, "file removed");
	}
}

/**
 * as_store_add_path_data:
 *
 * Save the path data so we can add any newly-discovered applications with the
 * correct prefix and architecture.
 **/
static void
as_store_add_path_data (AsStore *store,
			const gchar *path,
			AsAppScope scope,
			const gchar *arch)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	AsStorePathData *path_data;

	/* don't scan non-existent directories */
	if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
		return;
	}

	/* check not already exists */
	path_data = g_hash_table_lookup (priv->appinfo_dirs, path);
	if (path_data != NULL) {
		if (path_data->scope != scope ||
		    g_strcmp0 (path_data->arch, arch) != 0) {
			g_warning ("already added path %s [%s:%s] vs new [%s:%s]",
				   path,
				   as_app_scope_to_string (path_data->scope),
				   path_data->arch,
				   as_app_scope_to_string (scope),
				   arch);
		} else {
			g_debug ("already added path %s [%s:%s]",
				 path,
				 as_app_scope_to_string (path_data->scope),
				 path_data->arch);
		}
		return;
	}

	/* create new */
	path_data = g_slice_new0 (AsStorePathData);
	path_data->scope = scope;
	path_data->arch = g_strdup (arch);
	g_hash_table_insert (priv->appinfo_dirs, g_strdup (path), path_data);
}

static gboolean
as_store_from_file_internal (AsStore *store,
			     GFile *file,
			     AsAppScope scope,
			     const gchar *arch,
			     guint32 load_flags,
			     guint32 watch_flags,
			     GCancellable *cancellable,
			     GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	guint32 flags = AS_NODE_FROM_XML_FLAG_LITERAL_TEXT;
	g_autofree gchar *filename = NULL;
	g_autofree gchar *icon_prefix = NULL;
	g_autoptr(GError) error_local = NULL;
	g_autoptr(AsNode) root = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;

	g_return_val_if_fail (AS_IS_STORE (store), FALSE);

	/* profile */
	filename = g_file_get_path (file);
	ptask = as_profile_start (priv->profile,
				  "AsStore:store-from-file{%s}",
				  filename);
	g_assert (ptask != NULL);

	/* a DEP-11 file */
	if (g_strstr_len (filename, -1, ".yml") != NULL) {
		return as_store_load_yaml_file (store,
						file,
						scope,
						cancellable,
						error);
	}
#ifdef HAVE_GCAB
	/* a cab archive */
	if (g_str_has_suffix (filename, ".cab"))
		return as_store_cab_from_file (store, file, cancellable, error);
#endif

	/* an AppStream XML file */
	if (priv->add_flags & AS_STORE_ADD_FLAG_ONLY_NATIVE_LANGS)
		flags |= AS_NODE_FROM_XML_FLAG_ONLY_NATIVE_LANGS;
	root = as_node_from_file (file, flags, cancellable, &error_local);
	if (root == NULL) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to parse %s file: %s",
			     filename, error_local->message);
		return FALSE;
	}

	/* watch for file changes */
	if (watch_flags > 0) {
		as_store_add_path_data (store, filename, scope, arch);
		if (!as_monitor_add_file (priv->monitor,
					  filename,
					  cancellable,
					  error))
			return FALSE;
	}

	/* icon prefix is the directory the XML has been found in */
	icon_prefix = g_path_get_dirname (filename);
	return as_store_from_root (store, root, scope,
				   icon_prefix, filename, arch, load_flags,
				   error);
}

/**
 * as_store_from_file:
 * @store: a #AsStore instance.
 * @file: a #GFile.
 * @icon_root: (nullable): the icon path, or %NULL for the default (unused)
 * @cancellable: a #GCancellable.
 * @error: A #GError or %NULL.
 *
 * Parses an AppStream XML or DEP-11 YAML file and adds any valid applications
 * to the store.
 *
 * If the root node does not have a 'origin' attribute, then the method
 * as_store_set_origin() should be called *before* this function if cached
 * icons are required.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.0
 **/
gboolean
as_store_from_file (AsStore *store,
		    GFile *file,
		    const gchar *icon_root, /* unused */
		    GCancellable *cancellable,
		    GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);

	return as_store_from_file_internal (store, file,
					    AS_APP_SCOPE_UNKNOWN,
					    NULL, /* arch */
					    AS_STORE_LOAD_FLAG_NONE,
					    priv->watch_flags,
					    cancellable, error);
}

/**
 * as_store_from_bytes:
 * @store: a #AsStore instance.
 * @bytes: a #GBytes.
 * @cancellable: a #GCancellable.
 * @error: A #GError or %NULL.
 *
 * Parses an appstream store presented as an archive. This is typically
 * a .cab file containing firmware files.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.5.2
 **/
gboolean
as_store_from_bytes (AsStore *store,
		     GBytes *bytes,
		     GCancellable *cancellable,
		     GError **error)
{
	g_autofree gchar *content_type = NULL;
	gconstpointer data;
	gsize size;

	/* find content type */
	data = g_bytes_get_data (bytes, &size);
	content_type = g_content_type_guess (NULL, data, size, NULL);

	/* is an AppStream file */
	if (g_strcmp0 (content_type, "application/xml") == 0) {
		g_autofree gchar *tmp = g_strndup (data, size);
		return as_store_from_xml (store, tmp, NULL, error);
	}

	/* is firmware */
	if (g_strcmp0 (content_type, "application/vnd.ms-cab-compressed") == 0) {
#ifdef HAVE_GCAB
		return as_store_cab_from_bytes (store, bytes, cancellable, error);
#else
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "not supported, compiled without gcab");
		return FALSE;
#endif
	}

	/* not sure what to do */
	g_set_error (error,
		     AS_STORE_ERROR,
		     AS_STORE_ERROR_FAILED,
		     "cannot load store of type %s",
		     content_type);
	return FALSE;
}

/**
 * as_store_from_xml:
 * @store: a #AsStore instance.
 * @data: XML data
 * @icon_root: (nullable): the icon path, or %NULL for the default.
 * @error: A #GError or %NULL.
 *
 * Parses AppStream XML file and adds any valid applications to the store.
 *
 * If the root node does not have a 'origin' attribute, then the method
 * as_store_set_origin() should be called *before* this function if cached
 * icons are required.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.1
 **/
gboolean
as_store_from_xml (AsStore *store,
		   const gchar *data,
		   const gchar *icon_root,
		   GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	guint32 flags = AS_NODE_FROM_XML_FLAG_LITERAL_TEXT;
	g_autoptr(GError) error_local = NULL;
	g_autoptr(AsNode) root = NULL;

	g_return_val_if_fail (AS_IS_STORE (store), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	/* ignore empty file */
	if (data[0] == '\0')
		return TRUE;

	/* load XML data */
	if (priv->add_flags & AS_STORE_ADD_FLAG_ONLY_NATIVE_LANGS)
		flags |= AS_NODE_FROM_XML_FLAG_ONLY_NATIVE_LANGS;
	root = as_node_from_xml (data, flags, &error_local);
	if (root == NULL) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to parse XML: %s",
			     error_local->message);
		return FALSE;
	}
	return as_store_from_root (store, root,
				   AS_APP_SCOPE_UNKNOWN,
				   icon_root,
				   NULL, /* filename */
				   NULL, /* arch */
				   AS_STORE_LOAD_FLAG_NONE,
				   error);
}

static gint
as_store_apps_sort_cb (gconstpointer a, gconstpointer b)
{
	return g_strcmp0 (as_app_get_id (AS_APP (*(AsApp **) a)),
			  as_app_get_id (AS_APP (*(AsApp **) b)));
}

static void
as_store_check_app_for_veto (AsApp *app)
{
	/* these categories need AppData files */
	if (as_app_get_description_size (app) == 0) {
		guint i;
		const gchar *cats_require_appdata[] = {
			"ConsoleOnly",
			"DesktopSettings",
			"Settings",
			NULL };
		for (i = 0; cats_require_appdata[i] != NULL; i++) {
			if (as_app_has_category (app, cats_require_appdata[i])) {
				as_app_add_veto (app, "%s requires an AppData file",
						 cats_require_appdata[i]);
			}
		}
	}
}

static void
as_store_check_apps_for_veto (AsStore *store)
{
	guint i;
	AsApp *app;
	AsStorePrivate *priv = GET_PRIVATE (store);

	/* add any vetos */
	for (i = 0; i < priv->array->len; i++) {
		app = g_ptr_array_index (priv->array, i);
		as_store_check_app_for_veto (app);
	}
}

/**
 * as_store_remove_apps_with_veto:
 * @store: a #AsStore instance.
 *
 * Removes any applications from the store if they have any vetos.
 *
 * Since: 0.5.13
 **/
void
as_store_remove_apps_with_veto (AsStore *store)
{
	guint i;
	AsApp *app;
	AsStorePrivate *priv = GET_PRIVATE (store);
	_cleanup_uninhibit_ guint32 *tok = NULL;

	/* don't shortcut the list as we have to use as_store_remove_app()
	 * rather than just removing from the GPtrArray */
	tok = as_store_changed_inhibit (store);
	do {
		for (i = 0; i < priv->array->len; i++) {
			app = g_ptr_array_index (priv->array, i);
			if (as_app_get_vetos (app)->len > 0) {
				g_debug ("removing %s as vetoed",
					 as_app_get_id (app));
				as_store_remove_app (store, app);
				break;
			}
		}
	} while (i < priv->array->len);
	as_store_changed_uninhibit (&tok);
	as_store_perhaps_emit_changed (store, "remove-apps-with-veto");
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
as_store_to_xml (AsStore *store, guint32 flags)
{
	AsApp *app;
	AsStorePrivate *priv = GET_PRIVATE (store);
	AsNode *node_apps;
	AsNode *node_root;
	GString *xml;
	gboolean output_trusted = FALSE;
	guint i;
	gchar version[6];
	g_autoptr(AsNodeContext) ctx = NULL;

	/* check categories of apps about to be written */
	as_store_check_apps_for_veto (store);

	/* get XML text */
	node_root = as_node_new ();
	node_apps = as_node_insert (node_root, "components", NULL, 0, NULL);

	/* set origin attribute */
	if (priv->origin != NULL)
		as_node_add_attribute (node_apps, "origin", priv->origin);

	/* set origin attribute */
	if (priv->builder_id != NULL)
		as_node_add_attribute (node_apps, "builder_id", priv->builder_id);

	/* set version attribute */
	if (priv->api_version > 0.1f) {
		g_ascii_formatd (version, sizeof (version),
				 "%.1f", priv->api_version);
		as_node_add_attribute (node_apps, "version", version);
	}

	/* sort by ID */
	g_ptr_array_sort (priv->array, as_store_apps_sort_cb);

	/* output is trusted, so include update_contact */
	if (g_getenv ("APPSTREAM_GLIB_OUTPUT_TRUSTED") != NULL)
		output_trusted = TRUE;

	/* add applications */
	ctx = as_node_context_new ();
	as_node_context_set_version (ctx, priv->api_version);
	as_node_context_set_output (ctx, AS_FORMAT_KIND_APPSTREAM);
	as_node_context_set_output_trusted (ctx, output_trusted);
	for (i = 0; i < priv->array->len; i++) {
		app = g_ptr_array_index (priv->array, i);
		as_app_node_insert (app, node_apps, ctx);
	}
	xml = as_node_to_xml (node_root, flags);
	as_node_unref (node_root);
	return xml;
}

/**
 * as_store_convert_icons:
 * @store: a #AsStore instance.
 * @kind: the AsIconKind, e.g. %AS_ICON_KIND_EMBEDDED.
 * @error: A #GError or %NULL
 *
 * Converts all the icons in the store to a specific kind.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.3.1
 **/
gboolean
as_store_convert_icons (AsStore *store, AsIconKind kind, GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	AsApp *app;
	guint i;

	/* convert application icons */
	for (i = 0; i < priv->array->len; i++) {
		app = g_ptr_array_index (priv->array, i);
		if (!as_app_convert_icons (app, kind, error))
			return FALSE;
	}
	return TRUE;
}

/**
 * as_store_to_file:
 * @store: a #AsStore instance.
 * @file: file
 * @flags: the AsNodeToXmlFlags, e.g. %AS_NODE_INSERT_FLAG_NONE.
 * @cancellable: A #GCancellable, or %NULL
 * @error: A #GError or %NULL
 *
 * Outputs an optionally compressed XML file of all the applications in the store.
 *
 * Returns: A #GString
 *
 * Since: 0.1.0
 **/
gboolean
as_store_to_file (AsStore *store,
		  GFile *file,
		  guint32 flags,
		  GCancellable *cancellable,
		  GError **error)
{
	g_autoptr(GError) error_local = NULL;
	g_autoptr(GOutputStream) out2 = NULL;
	g_autoptr(GOutputStream) out = NULL;
	g_autoptr(GZlibCompressor) compressor = NULL;
	g_autoptr(GString) xml = NULL;
	g_autofree gchar *basename = NULL;

	/* check if compressed */
	basename = g_file_get_basename (file);
	if (g_strstr_len (basename, -1, ".gz") == NULL) {
		xml = as_store_to_xml (store, flags);
		if (!g_file_replace_contents (file, xml->str, xml->len,
					      NULL,
					      FALSE,
					      G_FILE_CREATE_NONE,
					      NULL,
					      cancellable,
					      &error_local)) {
			g_set_error (error,
				     AS_STORE_ERROR,
				     AS_STORE_ERROR_FAILED,
				     "Failed to write file: %s",
				     error_local->message);
			return FALSE;
		}
		return TRUE;
	}

	/* compress as a gzip file */
	compressor = g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP, -1);
	out = g_memory_output_stream_new_resizable ();
	out2 = g_converter_output_stream_new (out, G_CONVERTER (compressor));
	xml = as_store_to_xml (store, flags);
	if (!g_output_stream_write_all (out2, xml->str, xml->len,
					NULL, NULL, &error_local)) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to write stream: %s",
			     error_local->message);
		return FALSE;
	}
	if (!g_output_stream_close (out2, NULL, &error_local)) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to close stream: %s",
			     error_local->message);
		return FALSE;
	}

	/* write file */
	if (!g_file_replace_contents (file,
		g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (out)),
		g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (out)),
				      NULL,
				      FALSE,
				      G_FILE_CREATE_NONE,
				      NULL,
				      cancellable,
				      &error_local)) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to write file: %s",
			     error_local->message);
		return FALSE;
	}
	return TRUE;
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
 * as_store_get_builder_id:
 * @store: a #AsStore instance.
 *
 * Gets the metadata builder identifier, which is used to work out if old
 * metadata is compatible with this builder.
 *
 * Returns: the builder_id string, or %NULL if unset
 *
 * Since: 0.2.5
 **/
const gchar *
as_store_get_builder_id (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	return priv->builder_id;
}

/**
 * as_store_set_builder_id:
 * @store: a #AsStore instance.
 * @builder_id: the builder_id, e.g. "appstream-glib:1"
 *
 * Sets the metadata builder identifier, which is used to work out if old
 * metadata can be used.
 *
 * Since: 0.2.5
 **/
void
as_store_set_builder_id (AsStore *store, const gchar *builder_id)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_free (priv->builder_id);
	priv->builder_id = g_strdup (builder_id);
}

/**
 * as_store_set_destdir:
 * @store: a #AsStore instance.
 * @destdir: the destdir, e.g. "/tmp"
 *
 * Sets the destdir, which is used to prefix usr.
 *
 * Since: 0.2.4
 **/
void
as_store_set_destdir (AsStore *store, const gchar *destdir)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_free (priv->destdir);
	priv->destdir = g_strdup (destdir);
}

/**
 * as_store_get_destdir:
 * @store: a #AsStore instance.
 *
 * Gets the destdir, which is used to prefix usr.
 *
 * Returns: the destdir path, or %NULL if unset
 *
 * Since: 0.2.4
 **/
const gchar *
as_store_get_destdir (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	return priv->destdir;
}

/**
 * as_store_get_api_version:
 * @store: a #AsStore instance.
 *
 * Gets the AppStream API version.
 *
 * Returns: the #AsNodeInsertFlags, or 0 if unset
 *
 * Since: 0.1.1
 **/
gdouble
as_store_get_api_version (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	return priv->api_version;
}

/**
 * as_store_set_api_version:
 * @store: a #AsStore instance.
 * @api_version: the API version
 *
 * Sets the AppStream API version.
 *
 * Since: 0.1.1
 **/
void
as_store_set_api_version (AsStore *store, gdouble api_version)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	priv->api_version = api_version;
}

/**
 * as_store_get_add_flags:
 * @store: a #AsStore instance.
 *
 * Gets the flags used for adding applications to the store.
 *
 * Returns: the #AsStoreAddFlags, or 0 if unset
 *
 * Since: 0.2.2
 **/
guint32
as_store_get_add_flags (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	return priv->add_flags;
}

/**
 * as_store_set_add_flags:
 * @store: a #AsStore instance.
 * @add_flags: the #AsStoreAddFlags, e.g. %AS_STORE_ADD_FLAG_NONE
 *
 * Sets the flags used when adding applications to the store.
 *
 * NOTE: Using %AS_STORE_ADD_FLAG_PREFER_LOCAL may be a privacy risk depending on
 * your level of paranoia, and should not be used by default.
 *
 * Since: 0.2.2
 **/
void
as_store_set_add_flags (AsStore *store, guint32 add_flags)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	priv->add_flags = add_flags;
}

/**
 * as_store_get_watch_flags:
 * @store: a #AsStore instance.
 *
 * Gets the flags used for adding files to the store.
 *
 * Returns: the #AsStoreWatchFlags, or 0 if unset
 *
 * Since: 0.4.2
 **/
guint32
as_store_get_watch_flags (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	return priv->watch_flags;
}

/**
 * as_store_set_watch_flags:
 * @store: a #AsStore instance.
 * @watch_flags: the #AsStoreWatchFlags, e.g. %AS_STORE_WATCH_FLAG_NONE
 *
 * Sets the flags used when adding files to the store.
 *
 * Since: 0.4.2
 **/
void
as_store_set_watch_flags (AsStore *store, guint32 watch_flags)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	priv->watch_flags = watch_flags;
}

static gboolean
as_store_guess_origin_fallback (AsStore *store,
				const gchar *filename,
				GError **error)
{
	gchar *tmp;
	g_autofree gchar *origin_fallback = NULL;

	/* the first component of the file (e.g. "fedora-20.xml.gz)
	 * is used for the icon directory as we might want to clean up
	 * the icons manually if they are installed in /var/cache */
	origin_fallback = g_path_get_basename (filename);
	tmp = g_strstr_len (origin_fallback, -1, ".xml");
	if (tmp == NULL)
		tmp = g_strstr_len (origin_fallback, -1, ".yml");
	if (tmp == NULL) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "AppStream metadata name %s not valid, "
			     "expected .xml[.*] or .yml[.*]",
			     filename);
		return FALSE;
	}
	tmp[0] = '\0';

	/* load this specific file */
	as_store_set_origin (store, origin_fallback);
	return TRUE;
}

static gboolean
as_store_load_app_info_file (AsStore *store,
			     AsAppScope scope,
			     const gchar *path_xml,
			     const gchar *arch,
			     guint32 flags,
			     GCancellable *cancellable,
			     GError **error)
{
	g_autoptr(GFile) file = NULL;

	/* ignore large compressed files */
	if (flags & AS_STORE_LOAD_FLAG_ONLY_UNCOMPRESSED &&
	    g_str_has_suffix (path_xml, ".gz")) {
		g_debug ("ignoring compressed file %s", path_xml);
		return TRUE;
	}

	/* guess this based on the name */
	if (!as_store_guess_origin_fallback (store, path_xml, error))
		return FALSE;

	/* load without adding monitor */
	file = g_file_new_for_path (path_xml);
	return as_store_from_file_internal (store,
					    file,
					    scope,
					    arch,
					    flags,
					    AS_STORE_WATCH_FLAG_NONE,
					    cancellable,
					    error);
}

static gboolean
as_store_load_app_info (AsStore *store,
			AsAppScope scope,
			const gchar *path,
			const gchar *arch,
			guint32 flags,
			GCancellable *cancellable,
			GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	_cleanup_uninhibit_ guint32 *tok = NULL;

	/* Don't add the same dir twice, we're monitoring it for changes anyway */
	if (g_hash_table_contains (priv->appinfo_dirs, path))
		return TRUE;

	/* emit once when finished */
	tok = as_store_changed_inhibit (store);

	/* search all files, if the location already exists */
	if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
		const gchar *tmp;
		g_autoptr(GDir) dir = NULL;
		g_autoptr(GError) error_local = NULL;
		dir = g_dir_open (path, 0, &error_local);
		if (dir == NULL) {
			if (flags & AS_STORE_LOAD_FLAG_IGNORE_INVALID) {
				g_warning ("ignoring invalid AppStream path %s: %s",
					   path, error_local->message);
				return TRUE;
			}
			g_set_error (error,
				     AS_STORE_ERROR,
				     AS_STORE_ERROR_FAILED,
				     "Failed to open %s: %s",
				     path, error_local->message);
			return FALSE;
		}

		while ((tmp = g_dir_read_name (dir)) != NULL) {
			GError *error_store = NULL;
			g_autofree gchar *filename_md = NULL;
			if (g_strcmp0 (tmp, "icons") == 0)
				continue;
			filename_md = g_build_filename (path, tmp, NULL);
			if (!as_store_load_app_info_file (store,
							  scope,
							  filename_md,
							  arch,
							  flags,
							  cancellable,
							  &error_store)) {
				if (flags & AS_STORE_LOAD_FLAG_IGNORE_INVALID) {
					g_warning ("Ignoring invalid AppStream file %s: %s",
						   filename_md, error_store->message);
					g_clear_error (&error_store);
				} else {
					g_propagate_error (error, error_store);
					return FALSE;
				}
			}
		}
	}

	/* watch the directories for changes, even if it does not exist yet */
	as_store_add_path_data (store, path, scope, arch);
	if (!as_monitor_add_directory (priv->monitor,
				       path,
				       cancellable,
				       error))
		return FALSE;

	/* emit changed */
	as_store_changed_uninhibit (&tok);
	as_store_perhaps_emit_changed (store, "load-app-info");

	return TRUE;
}

static void
as_store_set_app_installed (AsApp *app)
{
	GPtrArray *releases = as_app_get_releases (app);
	for (guint i = 0; i < releases->len; i++) {
		AsRelease *rel = g_ptr_array_index (releases, i);
		as_release_set_state (rel, AS_RELEASE_STATE_INSTALLED);
	}
}

static gboolean
as_store_load_installed_file_is_valid (const gchar *filename)
{
	if (g_str_has_suffix (filename, ".desktop"))
		return TRUE;
	if (g_str_has_suffix (filename, ".metainfo.xml"))
		return TRUE;
	if (g_str_has_suffix (filename, ".appdata.xml"))
		return TRUE;
	g_debug ("ignoring filename with invalid suffix: %s", filename);
	return FALSE;
}

static gboolean
as_store_load_installed (AsStore *store,
			 guint32 flags,
			 AsAppScope scope,
			 const gchar *path,
			 GCancellable *cancellable,
			 GError **error)
{
	guint32 parse_flags = AS_APP_PARSE_FLAG_USE_HEURISTICS;
	AsStorePrivate *priv = GET_PRIVATE (store);
	GError *error_local = NULL;
	const gchar *tmp;
	g_autoptr(GDir) dir = NULL;
	_cleanup_uninhibit_ guint32 *tok = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;

	/* profile */
	ptask = as_profile_start (priv->profile, "AsStore:load-installed{%s}", path);
	g_assert (ptask != NULL);

	dir = g_dir_open (path, 0, error);
	if (dir == NULL)
		return FALSE;

	/* watch the directories for changes */
	as_store_add_path_data (store, path, scope, NULL);
	if (!as_monitor_add_directory (priv->monitor,
				       path,
				       cancellable,
				       error))
		return FALSE;

	/* emit once when finished */
	tok = as_store_changed_inhibit (store);

	/* always load all the .desktop 'X-' metadata */
	parse_flags |= AS_APP_PARSE_FLAG_ADD_ALL_METADATA;

	/* relax the checks when parsing */
	if (flags & AS_STORE_LOAD_FLAG_ALLOW_VETO)
		parse_flags |= AS_APP_PARSE_FLAG_ALLOW_VETO;

	/* propagate flag */
	if (priv->add_flags & AS_STORE_ADD_FLAG_ONLY_NATIVE_LANGS)
		parse_flags |= AS_APP_PARSE_FLAG_ONLY_NATIVE_LANGS;

	while ((tmp = g_dir_read_name (dir)) != NULL) {
		AsApp *app_tmp;
		GPtrArray *icons;
		guint i;
		g_autofree gchar *filename = NULL;
		g_autoptr(AsApp) app = NULL;
		filename = g_build_filename (path, tmp, NULL);
		if (!as_store_load_installed_file_is_valid (filename))
			continue;
		if ((priv->add_flags & AS_STORE_ADD_FLAG_PREFER_LOCAL) == 0) {
			app_tmp = as_store_get_app_by_id (store, tmp);
			if (app_tmp != NULL &&
			    as_app_get_format_by_kind (app_tmp, AS_FORMAT_KIND_DESKTOP) != NULL) {
				as_app_set_state (app_tmp, AS_APP_STATE_INSTALLED);
				g_debug ("not parsing %s as %s already exists",
					 filename, tmp);
				continue;
			}
		}
		app = as_app_new ();
		as_app_set_scope (app, scope);
		if (!as_app_parse_file (app, filename, parse_flags, &error_local)) {
			if (g_error_matches (error_local,
					     AS_APP_ERROR,
					     AS_APP_ERROR_INVALID_TYPE)) {
				g_debug ("Ignoring %s: %s", filename,
					 error_local->message);
				g_clear_error (&error_local);
				continue;
			}
			g_propagate_error (error, error_local);
			return FALSE;
		}

		/* convert any UNKNOWN icons to LOCAL */
		icons = as_app_get_icons (app);
		for (i = 0; i < icons->len; i++) {
			AsIcon *icon = g_ptr_array_index (icons, i);
			if (as_icon_get_kind (icon) == AS_ICON_KIND_UNKNOWN)
				as_icon_set_kind (icon, AS_ICON_KIND_STOCK);
		}

		/* set the ID prefix */
		if ((priv->add_flags & AS_STORE_ADD_FLAG_USE_UNIQUE_ID) == 0)
			as_store_fixup_id_prefix (app, as_app_scope_to_string (scope));

		/* do not load applications with vetos */
		if ((flags & AS_STORE_LOAD_FLAG_ALLOW_VETO) == 0 &&
		    as_app_get_vetos(app)->len > 0)
			continue;

		/* as these are added from installed AppData files then all the
		 * releases can also be marked as installed */
		as_store_set_app_installed (app);

		/* set lower priority than AppStream entries */
		as_app_set_priority (app, -1);
		as_store_add_app (store, app);
	}

	/* emit changed */
	as_store_changed_uninhibit (&tok);
	as_store_perhaps_emit_changed (store, "load-installed");

	return TRUE;
}

/**
 * as_store_load_path:
 * @store: a #AsStore instance.
 * @path: A path to load
 * @cancellable: a #GCancellable.
 * @error: A #GError or %NULL.
 *
 * Loads the store from a specific path.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.2.2
 **/
gboolean
as_store_load_path (AsStore *store, const gchar *path,
		    GCancellable *cancellable, GError **error)
{
	return as_store_load_installed (store, AS_STORE_LOAD_FLAG_NONE,
					AS_APP_SCOPE_UNKNOWN,
					path, cancellable, error);
}

static gboolean
as_store_search_installed (AsStore *store,
			   guint32 flags,
			   AsAppScope scope,
			   const gchar *path,
			   GCancellable *cancellable,
			   GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_autofree gchar *dest = NULL;
	dest = g_build_filename (priv->destdir ? priv->destdir : "/", path, NULL);
	g_debug ("searching path %s", dest);
	if (!g_file_test (dest, G_FILE_TEST_EXISTS))
		return TRUE;
	return as_store_load_installed (store, flags, scope,
					dest, cancellable, error);
}

static gboolean
as_store_search_app_info (AsStore *store,
			  guint32 flags,
			  AsAppScope scope,
			  const gchar *path,
			  GCancellable *cancellable,
			  GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	const gchar *supported_kinds[] = { "yaml", "xmls", NULL };
	guint i;

	for (i = 0; supported_kinds[i] != NULL; i++) {
		g_autofree gchar *dest = NULL;
		dest = g_build_filename (priv->destdir ? priv->destdir : "/",
					 path,
					 supported_kinds[i],
					 NULL);
		if (!as_store_load_app_info (store, scope, dest, NULL,
					     flags, cancellable, error))
			return FALSE;
	}
	return TRUE;
}

static gboolean
as_store_search_per_system (AsStore *store,
			    guint32 flags,
			    GCancellable *cancellable,
			    GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	const gchar * const * data_dirs;
	guint i;
	g_autoptr(AsProfileTask) ptask = NULL;

	/* profile */
	ptask = as_profile_start_literal (priv->profile, "AsStore:load{per-system}");
	g_assert (ptask != NULL);

	/* datadir AppStream, AppData and desktop */
	data_dirs = g_get_system_data_dirs ();
	for (i = 0; data_dirs[i] != NULL; i++) {
		if (g_strstr_len (data_dirs[i], -1, "flatpak/exports") != NULL) {
			g_debug ("skipping %s as invalid", data_dirs[i]);
			continue;
		}
		if (g_str_has_prefix (data_dirs[i], "/home/")) {
			g_debug ("skipping %s as invalid", data_dirs[i]);
			continue;
		}
		if (g_strstr_len (data_dirs[i], -1, "snapd/desktop") != NULL) {
			g_debug ("skippping %s as invalid", data_dirs[i]);
			continue;
		}
		if ((flags & AS_STORE_LOAD_FLAG_APP_INFO_SYSTEM) > 0) {
			g_autofree gchar *dest = NULL;
			dest = g_build_filename (data_dirs[i], "app-info", NULL);
			if (!as_store_search_app_info (store, flags, AS_APP_SCOPE_SYSTEM,
						       dest, cancellable, error))
				return FALSE;
		}
		if ((flags & AS_STORE_LOAD_FLAG_APPDATA) > 0) {
			g_autofree gchar *dest = NULL;
			dest = g_build_filename (data_dirs[i], "appdata", NULL);
			if (!as_store_search_installed (store, flags, AS_APP_SCOPE_SYSTEM,
							dest, cancellable, error))
				return FALSE;
		}
		if ((flags & AS_STORE_LOAD_FLAG_APPDATA) > 0) {
			g_autofree gchar *dest = NULL;
			dest = g_build_filename (data_dirs[i], "metainfo", NULL);
			if (!as_store_search_installed (store, flags, AS_APP_SCOPE_SYSTEM,
							dest, cancellable, error))
				return FALSE;
		}
		if ((flags & AS_STORE_LOAD_FLAG_DESKTOP) > 0) {
			g_autofree gchar *dest = NULL;
			dest = g_build_filename (data_dirs[i], "applications", NULL);
			if (!as_store_search_installed (store, flags, AS_APP_SCOPE_SYSTEM,
							dest, cancellable, error))
				return FALSE;
		}
	}

	/* cached AppStream, AppData and desktop */
	if ((flags & AS_STORE_LOAD_FLAG_APP_INFO_SYSTEM) > 0) {
		g_autofree gchar *dest1 = NULL;
		g_autofree gchar *dest2 = NULL;
		dest1 = g_build_filename (LOCALSTATEDIR, "lib", "app-info", NULL);
		if (!as_store_search_app_info (store, flags, AS_APP_SCOPE_SYSTEM, dest1,
					       cancellable, error))
			return FALSE;
		dest2 = g_build_filename (LOCALSTATEDIR, "cache", "app-info", NULL);
		if (!as_store_search_app_info (store, flags, AS_APP_SCOPE_SYSTEM, dest2,
					       cancellable, error))
			return FALSE;
		/* ignore the prefix; we actually want to use the
		 * distro-provided data in this case. */
		if (g_strcmp0 (LOCALSTATEDIR, "/var") != 0) {
			g_autofree gchar *dest3 = NULL;
			g_autofree gchar *dest4 = NULL;
			dest3 = g_build_filename ("/var", "lib", "app-info", NULL);
			if (!as_store_search_app_info (store, flags, AS_APP_SCOPE_SYSTEM,
						       dest3, cancellable, error))
				return FALSE;
			dest4 = g_build_filename ("/var", "cache", "app-info", NULL);
			if (!as_store_search_app_info (store, flags, AS_APP_SCOPE_SYSTEM,
						       dest4, cancellable, error))
				return FALSE;
		}
	}
	return TRUE;
}

static gboolean
as_store_search_per_user (AsStore *store,
			  guint32 flags,
			  GCancellable *cancellable,
			  GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_autoptr(AsProfileTask) ptask = NULL;

	/* profile */
	ptask = as_profile_start_literal (priv->profile, "AsStore:load{per-user}");
	g_assert (ptask != NULL);

	/* AppStream */
	if ((flags & AS_STORE_LOAD_FLAG_APP_INFO_USER) > 0) {
		g_autofree gchar *dest = NULL;
		dest = g_build_filename (g_get_user_data_dir (), "app-info", NULL);
		if (!as_store_search_app_info (store, flags, AS_APP_SCOPE_USER,
					       dest, cancellable, error))
			return FALSE;
	}

	/* AppData */
	if ((flags & AS_STORE_LOAD_FLAG_APPDATA) > 0) {
		g_autofree gchar *dest = NULL;
		dest = g_build_filename (g_get_user_data_dir (), "appdata", NULL);
		if (!as_store_search_installed (store, flags, AS_APP_SCOPE_USER,
						dest, cancellable, error))
			return FALSE;
	}

	/* MetaInfo */
	if ((flags & AS_STORE_LOAD_FLAG_APPDATA) > 0) {
		g_autofree gchar *dest = NULL;
		dest = g_build_filename (g_get_user_data_dir (), "metainfo", NULL);
		if (!as_store_search_installed (store, flags, AS_APP_SCOPE_USER,
						dest, cancellable, error))
			return FALSE;
	}

	/* desktop files */
	if ((flags & AS_STORE_LOAD_FLAG_DESKTOP) > 0) {
		g_autofree gchar *dest = NULL;
		dest = g_build_filename (g_get_user_data_dir (), "applications", NULL);
		if (!as_store_search_installed (store, flags, AS_APP_SCOPE_USER,
						dest, cancellable, error))
			return FALSE;
	}
	return TRUE;
}

static void
as_store_load_search_cache_cb (gpointer data, gpointer user_data)
{
	AsApp *app = AS_APP (data);
	as_app_search_matches (app, NULL);
}

/**
 * as_store_load_search_cache:
 * @store: a #AsStore instance.
 *
 * Populates the token cache for all applications in the store. This allows
 * all the search keywords for all applications in the store to be
 * pre-processed at one time in multiple threads rather than on demand.
 *
 * Note: Calling as_app_search_matches() automatically generates the search
 * cache for the #AsApp object if it has not already been generated.
 *
 * Since: 0.6.5
 **/
void
as_store_load_search_cache (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	guint i;
	GThreadPool *pool;
	g_autoptr(AsProfileTask) ptask = NULL;

	/* profile */
	ptask = as_profile_start_literal (priv->profile,
					  "AsStore:load-token-cache");
	as_profile_task_set_threaded (ptask, TRUE);

	/* load the token cache for each app in multiple threads */
	pool = g_thread_pool_new (as_store_load_search_cache_cb,
				  store, 4, TRUE, NULL);
	g_assert (pool != NULL);
	for (i = 0; i < priv->array->len; i++) {
		AsApp *app = g_ptr_array_index (priv->array, i);
		g_thread_pool_push (pool, app, NULL);
	}
	g_thread_pool_free (pool, FALSE, TRUE);
}

/**
 * as_store_load:
 * @store: a #AsStore instance.
 * @flags: #AsStoreLoadFlags, e.g. %AS_STORE_LOAD_FLAG_APP_INFO_SYSTEM
 * @cancellable: a #GCancellable.
 * @error: A #GError or %NULL.
 *
 * Loads the store from the default locations.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.2
 **/
gboolean
as_store_load (AsStore *store, guint32 flags, GCancellable *cancellable, GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_autoptr(AsProfileTask) ptask = NULL;
	_cleanup_uninhibit_ guint32 *tok = NULL;

	/* profile */
	ptask = as_profile_start_literal (priv->profile, "AsStore:load");
	g_assert (ptask != NULL);
	tok = as_store_changed_inhibit (store);

	/* per-user locations */
	if (!as_store_search_per_user (store, flags, cancellable, error))
		return FALSE;

	/* system locations */
	if (!as_store_search_per_system (store, flags, cancellable, error))
		return FALSE;

	/* find and remove any vetoed applications */
	as_store_check_apps_for_veto (store);
	if ((flags & AS_STORE_LOAD_FLAG_ALLOW_VETO) == 0)
		as_store_remove_apps_with_veto (store);

	/* match again, for applications extended from different roots */
	as_store_match_addons (store);

	/* emit changed */
	as_store_changed_uninhibit (&tok);
	as_store_perhaps_emit_changed (store, "store-load");
	return TRUE;
}

G_GNUC_PRINTF (3, 4) static void
as_store_validate_add (GPtrArray *problems, AsProblemKind kind, const gchar *fmt, ...)
{
	AsProblem *problem;
	guint i;
	va_list args;
	g_autofree gchar *str = NULL;

	va_start (args, fmt);
	str = g_strdup_vprintf (fmt, args);
	va_end (args);

	/* already added */
	for (i = 0; i < problems->len; i++) {
		problem = g_ptr_array_index (problems, i);
		if (g_strcmp0 (as_problem_get_message (problem), str) == 0)
			return;
	}

	/* add new problem to list */
	problem = as_problem_new ();
	as_problem_set_kind (problem, kind);
	as_problem_set_message (problem, str);
	g_ptr_array_add (problems, problem);
}

static gchar *
as_store_get_unique_name_app_key (AsApp *app)
{
	const gchar *name;
	g_autofree gchar *name_lower = NULL;
	name = as_app_get_name (app, NULL);
	if (name == NULL)
		return NULL;
	name_lower = g_utf8_strdown (name, -1);
	return g_strdup_printf ("<%s:%s>",
				as_app_kind_to_string (as_app_get_kind (app)),
				name_lower);
}

/**
 * as_store_validate:
 * @store: a #AsStore instance.
 * @flags: the #AsAppValidateFlags to use, e.g. %AS_APP_VALIDATE_FLAG_NONE
 * @error: A #GError or %NULL.
 *
 * Validates infomation in the store for data applicable to the defined
 * metadata version.
 *
 * Returns: (transfer container) (element-type AsProblem): A list of problems, or %NULL
 *
 * Since: 0.2.4
 **/
GPtrArray *
as_store_validate (AsStore *store, guint32 flags, GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	AsApp *app;
	GPtrArray *probs;
	guint i;
	g_autoptr(GHashTable) hash_names = NULL;

	g_return_val_if_fail (AS_IS_STORE (store), NULL);

	probs = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	/* check the root node */
	if (priv->api_version < 0.6) {
		if ((priv->problems & AS_STORE_PROBLEM_LEGACY_ROOT) == 0) {
			as_store_validate_add (probs,
					       AS_PROBLEM_KIND_TAG_INVALID,
					       "metadata version is v%.1f and "
					       "XML root is not <applications>",
					       priv->api_version);
		}
	} else {
		if ((priv->problems & AS_STORE_PROBLEM_LEGACY_ROOT) != 0) {
			as_store_validate_add (probs,
					       AS_PROBLEM_KIND_TAG_INVALID,
					       "metadata version is v%.1f and "
					       "XML root is not <components>",
					       priv->api_version);
		}
		if (priv->origin == NULL) {
			as_store_validate_add (probs,
					       AS_PROBLEM_KIND_TAG_MISSING,
					       "metadata version is v%.1f and "
					       "origin attribute is missing",
					       priv->api_version);
		}
	}

	/* check there exists only onle application with a specific name */
	hash_names = g_hash_table_new_full (g_str_hash, g_str_equal,
					    g_free, (GDestroyNotify) g_object_unref);

	/* check each application */
	for (i = 0; i < priv->array->len; i++) {
		AsApp *app_tmp;
		AsProblem *prob;
		guint j;
		g_autofree gchar *app_key = NULL;
		g_autoptr(GPtrArray) probs_app = NULL;

		app = g_ptr_array_index (priv->array, i);
		if (priv->api_version < 0.3) {
			if (as_app_get_source_pkgname (app) != NULL) {
				as_store_validate_add (probs,
						       AS_PROBLEM_KIND_TAG_INVALID,
						       "metadata version is v%.1f and "
						       "<source_pkgname> only introduced in v0.3",
						       priv->api_version);
			}
			if (as_app_get_priority (app) != 0) {
				as_store_validate_add (probs,
						       AS_PROBLEM_KIND_TAG_INVALID,
						       "metadata version is v%.1f and "
						       "<priority> only introduced in v0.3",
						       priv->api_version);
			}
		}
		if (priv->api_version < 0.4) {
			if (as_app_get_project_group (app) != NULL) {
				as_store_validate_add (probs,
						       AS_PROBLEM_KIND_TAG_INVALID,
						       "metadata version is v%.1f and "
						       "<project_group> only introduced in v0.4",
						       priv->api_version);
			}
			if (as_app_get_mimetypes(app)->len > 0) {
				as_store_validate_add (probs,
						       AS_PROBLEM_KIND_TAG_INVALID,
						       "metadata version is v%.1f and "
						       "<mimetypes> only introduced in v0.4",
						       priv->api_version);
			}
			if (as_app_get_screenshots(app)->len > 0) {
				as_store_validate_add (probs,
						       AS_PROBLEM_KIND_TAG_INVALID,
						       "metadata version is v%.1f and "
						       "<screenshots> only introduced in v0.4",
						       priv->api_version);
			}
			if (as_app_get_compulsory_for_desktops(app)->len > 0) {
				as_store_validate_add (probs,
						       AS_PROBLEM_KIND_TAG_INVALID,
						       "metadata version is v%.1f and "
						       "<compulsory_for_desktop> only introduced in v0.4",
						       priv->api_version);
			}
			if (g_list_length (as_app_get_languages(app)) > 0) {
				as_store_validate_add (probs,
						       AS_PROBLEM_KIND_TAG_INVALID,
						       "metadata version is v%.1f and "
						       "<languages> only introduced in v0.4",
						       priv->api_version);
			}
		}
		if (priv->api_version < 0.6) {
			if ((as_app_get_problems (app) & AS_APP_PROBLEM_PREFORMATTED_DESCRIPTION) == 0) {
				as_store_validate_add (probs,
						       AS_PROBLEM_KIND_TAG_INVALID,
						       "metadata version is v%.1f and "
						       "<description> markup "
						       "was introduced in v0.6",
						       priv->api_version);
			}
			if (as_app_get_architectures(app)->len > 0) {
				as_store_validate_add (probs,
						       AS_PROBLEM_KIND_TAG_INVALID,
						       "metadata version is v%.1f and "
						       "<architectures> only introduced in v0.6",
						       priv->api_version);
			}
			if (as_app_get_releases(app)->len > 0) {
				as_store_validate_add (probs,
						       AS_PROBLEM_KIND_TAG_INVALID,
						       "metadata version is v%.1f and "
						       "<releases> only introduced in v0.6",
						       priv->api_version);
			}
			if (as_app_get_provides(app)->len > 0) {
				as_store_validate_add (probs,
						       AS_PROBLEM_KIND_TAG_INVALID,
						       "metadata version is v%.1f and "
						       "<provides> only introduced in v0.6",
						       priv->api_version);
			}
		} else {
			if ((as_app_get_problems (app) & AS_APP_PROBLEM_PREFORMATTED_DESCRIPTION) != 0) {
				as_store_validate_add (probs,
						       AS_PROBLEM_KIND_TAG_INVALID,
						       "%s: metadata version is v%.1f and "
						       "<description> requiring markup "
						       "was introduced in v0.6",
						       as_app_get_id (app),
						       priv->api_version);
			}
		}
		if (priv->api_version < 0.7) {
			if (as_app_get_kind (app) == AS_APP_KIND_ADDON) {
				as_store_validate_add (probs,
						       AS_PROBLEM_KIND_TAG_INVALID,
						       "metadata version is v%.1f and "
						       "addon kinds only introduced in v0.7",
						       priv->api_version);
			}
			if (as_app_get_developer_name (app, NULL) != NULL) {
				as_store_validate_add (probs,
						       AS_PROBLEM_KIND_TAG_INVALID,
						       "metadata version is v%.1f and "
						       "<developer_name> only introduced in v0.7",
						       priv->api_version);
			}
			if (as_app_get_extends(app)->len > 0) {
				as_store_validate_add (probs,
						       AS_PROBLEM_KIND_TAG_INVALID,
						       "metadata version is v%.1f and "
						       "<extends> only introduced in v0.7",
						       priv->api_version);
			}
		}

		/* check for translations where there should be none */
		if ((as_app_get_problems (app) & AS_APP_PROBLEM_TRANSLATED_ID) != 0) {
			as_store_validate_add (probs,
					       AS_PROBLEM_KIND_TAG_INVALID,
					       "<id> values cannot be translated");
		}
		if ((as_app_get_problems (app) & AS_APP_PROBLEM_TRANSLATED_LICENSE) != 0) {
			as_store_validate_add (probs,
					       AS_PROBLEM_KIND_TAG_INVALID,
					       "<license> values cannot be translated");
		}
		if ((as_app_get_problems (app) & AS_APP_PROBLEM_TRANSLATED_PROJECT_GROUP) != 0) {
			as_store_validate_add (probs,
					       AS_PROBLEM_KIND_TAG_INVALID,
					       "<project_group> values cannot be translated");
		}

		/* validate each application */
		if (flags & AS_APP_VALIDATE_FLAG_ALL_APPS) {
			probs_app = as_app_validate (app, flags, error);
			if (probs_app == NULL)
				return NULL;
			for (j = 0; j < probs_app->len; j++) {
				prob = g_ptr_array_index (probs_app, j);
				as_store_validate_add (probs,
						       as_problem_get_kind (prob),
						       "%s: %s",
						       as_app_get_id (app),
						       as_problem_get_message (prob));
			}
		}

		/* check uniqueness */
		if (as_app_get_kind (app) != AS_APP_KIND_ADDON) {
			app_key = as_store_get_unique_name_app_key (app);
			if (app_key != NULL) {
				app_tmp = g_hash_table_lookup (hash_names, app_key);
				if (app_tmp != NULL) {
					as_store_validate_add (probs,
							       AS_PROBLEM_KIND_DUPLICATE_DATA,
							       "%s[%s] as the same name as %s[%s]: %s",
							       as_app_get_id (app),
							       as_app_get_pkgname_default (app),
							       as_app_get_id (app_tmp),
							       as_app_get_pkgname_default (app_tmp),
							       app_key);
				} else {
					g_hash_table_insert (hash_names,
							     g_strdup (app_key),
							     g_object_ref (app));
				}
			}
		}
	}
	return probs;
}

static void
as_store_path_data_free (AsStorePathData *path_data)
{
	g_free (path_data->arch);
	g_slice_free (AsStorePathData, path_data);
}

static void
as_store_create_search_blacklist (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	guint i;
	const gchar *blacklist[] = {
		"and", "the", "application", "for", "you", "your",
		"with", "can", "are", "from", "that", "use", "allows", "also",
		"this", "other", "all", "using", "has", "some", "like", "them",
		"well", "not", "using", "not", "but", "set", "its", "into",
		"such", "was", "they", "where", "want", "only", "about",
		"uses", "font", "features", "designed", "provides", "which",
		"many", "used", "org", "fonts", "open", "more", "based",
		"different", "including", "will", "multiple", "out", "have",
		"each", "when", "need", "most", "both", "their", "even",
		"way", "several", "been", "while", "very", "add", "under",
		"what", "those", "much", "either", "currently", "one",
		"support", "make", "over", "these", "there", "without", "etc",
		"main",
		NULL };
	for (i = 0; blacklist[i] != NULL; i++)  {
		g_hash_table_insert (priv->search_blacklist,
				     as_stemmer_process (priv->stemmer, blacklist[i]),
				     GUINT_TO_POINTER (1));
	}
}

/**
 * as_store_set_search_match:
 * @store: a #AsStore instance.
 * @search_match: the #AsAppSearchMatch, e.g. %AS_APP_SEARCH_MATCH_PKGNAME
 *
 * Sets the token match fields. The bitfield given here is used to choose what
 * is included in the token cache.
 *
 * Since: 0.6.5
 **/
void
as_store_set_search_match (AsStore *store, guint16 search_match)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	priv->search_match = search_match;
}

/**
 * as_store_get_search_match:
 * @store: a #AsStore instance.
 *
 * Gets the token match fields. The bitfield given here is used to choose what
 * is included in the token cache.
 *
 * Returns: a #AsAppSearchMatch, e.g. %AS_APP_SEARCH_MATCH_PKGNAME
 *
 * Since: 0.6.13
 **/
guint16
as_store_get_search_match (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	return priv->search_match;
}

static void
as_store_init (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	priv->profile = as_profile_new ();
	priv->stemmer = as_stemmer_new ();
	priv->api_version = AS_API_VERSION_NEWEST;
	priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->watch_flags = AS_STORE_WATCH_FLAG_NONE;
	priv->search_match = AS_APP_SEARCH_MATCH_LAST;
	priv->search_blacklist = g_hash_table_new_full (g_str_hash,
							g_str_equal,
							(GDestroyNotify) as_ref_string_unref,
							NULL);
	priv->hash_id = g_hash_table_new_full (g_str_hash,
					       g_str_equal,
					       g_free,
					       (GDestroyNotify) g_ptr_array_unref);
	priv->hash_merge_id = g_hash_table_new_full (g_str_hash,
						     g_str_equal,
						     g_free,
						     (GDestroyNotify) g_ptr_array_unref);
	priv->hash_unique_id = g_hash_table_new_full (g_str_hash,
						      g_str_equal,
						      g_free,
						      g_object_unref);
	priv->hash_pkgname = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    g_free,
						    (GDestroyNotify) g_object_unref);
	priv->appinfo_dirs = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    g_free,
						    (GDestroyNotify) as_store_path_data_free);
	priv->monitor = as_monitor_new ();
	g_signal_connect (priv->monitor, "changed",
			  G_CALLBACK (as_store_monitor_changed_cb),
			  store);
	g_signal_connect (priv->monitor, "added",
			  G_CALLBACK (as_store_monitor_added_cb),
			  store);
	g_signal_connect (priv->monitor, "removed",
			  G_CALLBACK (as_store_monitor_removed_cb),
			  store);
	priv->metadata_indexes = g_hash_table_new_full (g_str_hash,
							  g_str_equal,
							  g_free,
							  (GDestroyNotify) g_hash_table_unref);

	/* add stemmed keywords to the search blacklist */
	as_store_create_search_blacklist (store);
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
