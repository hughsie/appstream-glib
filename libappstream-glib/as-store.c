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
#include "as-cleanup.h"
#include "as-node-private.h"
#include "as-problem.h"
#include "as-store.h"
#include "as-utils-private.h"
#include "as-yaml.h"

#define AS_API_VERSION_NEWEST	0.6

typedef enum {
	AS_STORE_PROBLEM_NONE			= 0,
	AS_STORE_PROBLEM_LEGACY_ROOT		= 1 << 0,
	AS_STORE_PROBLEM_LAST
} AsStoreProblems;

typedef struct _AsStorePrivate	AsStorePrivate;
struct _AsStorePrivate
{
	gchar			*destdir;
	gchar			*origin;
	gchar			*builder_id;
	gdouble			 api_version;
	GPtrArray		*array;		/* of AsApp */
	GHashTable		*hash_id;	/* of AsApp{id} */
	GHashTable		*hash_pkgname;	/* of AsApp{pkgname} */
	GPtrArray		*file_monitors;	/* of GFileMonitor */
	GHashTable		*metadata_indexes;	/* GHashTable{key} */
	AsStoreAddFlags		 add_flags;
	AsStoreProblems		 problems;
	guint32			 filter;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsStore, as_store, G_TYPE_OBJECT)

enum {
	SIGNAL_CHANGED,
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
GQuark
as_store_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AsStoreError");
	return quark;
}

/**
 * as_store_finalize:
 **/
static void
as_store_finalize (GObject *object)
{
	AsStore *store = AS_STORE (object);
	AsStorePrivate *priv = GET_PRIVATE (store);

	g_free (priv->destdir);
	g_free (priv->origin);
	g_free (priv->builder_id);
	g_ptr_array_unref (priv->array);
	g_ptr_array_unref (priv->file_monitors);
	g_hash_table_unref (priv->hash_id);
	g_hash_table_unref (priv->hash_pkgname);
	g_hash_table_unref (priv->metadata_indexes);

	G_OBJECT_CLASS (as_store_parent_class)->finalize (object);
}

/**
 * as_store_init:
 **/
static void
as_store_init (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	priv->api_version = AS_API_VERSION_NEWEST;
	priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->hash_id = g_hash_table_new_full (g_str_hash,
					       g_str_equal,
					       NULL,
					       NULL);
	priv->hash_pkgname = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    g_free,
						    (GDestroyNotify) g_object_unref);
	priv->file_monitors = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->metadata_indexes = g_hash_table_new_full (g_str_hash,
							  g_str_equal,
							  g_free,
							  (GDestroyNotify) g_hash_table_unref);
}

/**
 * as_store_class_init:
 **/
static void
as_store_class_init (AsStoreClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/**
	 * AsStore::changed:
	 * @device: the #AsStore instance that emitted the signal
	 *
	 * The ::changed signal is emitted when the files backing the store
	 * have changed.
	 *
	 * Since: 0.1.2
	 **/
	signals [SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (AsStoreClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	object_class->finalize = as_store_finalize;
}

/**
 * as_store_add_filter:
 * @store: a #AsStore instance.
 * @kind: a #AsIdKind, e.g. %AS_ID_KIND_FIRMWARE
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
as_store_add_filter (AsStore *store, AsIdKind kind)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	priv->filter |= 1 << kind;
}

/**
 * as_store_remove_filter:
 * @store: a #AsStore instance.
 * @kind: a #AsIdKind, e.g. %AS_ID_KIND_FIRMWARE
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
as_store_remove_filter (AsStore *store, AsIdKind kind)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	priv->filter &= ~(1 << kind);
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
	g_hash_table_remove_all (priv->hash_pkgname);
}

/**
 * as_store_regen_metadata_index_key:
 **/
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
		{ "cheese.desktop",		"org.gnome.Cheese.desktop" },
		{ "devhelp.desktop",		"org.gnome.Devhelp.desktop" },
		{ "file-roller.desktop",	"org.gnome.FileRoller.desktop" },
		{ "gcalctool.desktop",		"gnome-calculator.desktop" },
		{ "gedit.desktop",		"org.gnome.gedit.desktop" },
		{ "glchess.desktop",		"gnome-chess.desktop" },
		{ "glines.desktop",		"five-or-more.desktop" },
		{ "gnect.desktop",		"four-in-a-row.desktop" },
		{ "gnibbles.desktop",		"gnome-nibbles.desktop" },
		{ "gnobots2.desktop",		"gnome-robots.desktop" },
		{ "gnome-2048.desktop",		"org.gnome.gnome-2048.desktop" },
		{ "gnome-boxes.desktop",	"org.gnome.Boxes.desktop" },
		{ "gnome-clocks.desktop",	"org.gnome.clocks.desktop" },
		{ "gnome-contacts.desktop",	"org.gnome.Contacts.desktop" },
		{ "gnome-dictionary.desktop",	"org.gnome.Dictionary.desktop" },
		{ "gnome-disks.desktop",	"org.gnome.DiskUtility.desktop" },
		{ "gnome-documents.desktop",	"org.gnome.Documents.desktop" },
		{ "gnome-font-viewer.desktop",	"org.gnome.font-viewer.desktop" },
		{ "gnome-maps.desktop",		"org.gnome.Maps.desktop" },
		{ "gnome-photos.desktop",	"org.gnome.Photos.desktop" },
		{ "gnome-screenshot.desktop",	"org.gnome.Screenshot.desktop" },
		{ "gnome-software.desktop",	"org.gnome.Software.desktop" },
		{ "gnome-sound-recorder.desktop", "org.gnome.SoundRecorder.desktop" },
		{ "gnome-terminal.desktop",	"org.gnome.Terminal.desktop" },
		{ "gnome-weather.desktop",	"org.gnome.Weather.Application.desktop" },
		{ "gnomine.desktop",		"gnome-mines.desktop" },
		{ "gnotravex.desktop",		"gnome-tetravex.desktop" },
		{ "gnotski.desktop",		"gnome-klotski.desktop" },
		{ "gtali.desktop",		"tali.desktop" },
		{ "nautilus.desktop",		"org.gnome.Nautilus.desktop" },
		{ "polari.desktop",		"org.gnome.Polari.desktop" },
		{ "totem.desktop",		"org.gnome.Totem.desktop" },

		/* KDE */
		{ "blinken.desktop",		"org.kde.blinken.desktop" },
		{ "cantor.desktop",		"org.kde.cantor.desktop" },
		{ "filelight.desktop",		"org.kde.filelight.desktop" },
		{ "gwenview.desktop",		"org.kde.gwenview.desktop" },
		{ "kalgebra.desktop",		"org.kde.kalgebra.desktop" },
		{ "kanagram.desktop",		"org.kde.kanagram.desktop" },
		{ "kapman.desktop",		"org.kde.kapman.desktop" },
		{ "kbruch.desktop",		"org.kde.kbruch.desktop" },
		{ "kgeography.desktop",		"org.kde.kgeography.desktop" },
		{ "khangman.desktop",		"org.kde.khangman.desktop" },
		{ "kiten.desktop",		"org.kde.kiten.desktop" },
		{ "klettres.desktop",		"org.kde.klettres.desktop" },
		{ "klipper.desktop",		"org.kde.klipper.desktop" },
		{ "kmplot.desktop",		"org.kde.kmplot.desktop" },
		{ "kollision.desktop",		"org.kde.kollision.desktop" },
		{ "konsole.desktop",		"org.kde.konsole.desktop" },
		{ "kstars.desktop",		"org.kde.kstars.desktop" },
		{ "ktp-log-viewer.desktop",	"org.kde.ktplogviewer.desktop" },
		{ "kturtle.desktop",		"org.kde.kturtle.desktop" },
		{ "kwordquiz.desktop",		"org.kde.kwordquiz.desktop" },
		{ "okteta.desktop",		"org.kde.okteta.desktop" },
		{ "parley.desktop",		"org.kde.parley.desktop" },
		{ "partitionmanager.desktop",	"org.kde.PartitionManager.desktop" },
		{ "step.desktop",		"org.kde.step.desktop" },

		/* others */
		{ "colorhug-ccmx.desktop",	"com.hughski.ColorHug.CcmxLoader.desktop" },
		{ "colorhug-flash.desktop",	"com.hughski.ColorHug.FlashLoader.desktop" },
		{ "dconf-editor.desktop",	"ca.desrt.dconf-editor.desktop" },

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
	g_hash_table_remove (priv->hash_id, as_app_get_id (app));
	g_ptr_array_remove (priv->array, app);
	g_hash_table_remove_all (priv->metadata_indexes);
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
		g_ptr_array_remove (priv->array, app);
	}
	g_hash_table_remove_all (priv->metadata_indexes);
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
	AsStorePrivate *priv = GET_PRIVATE (store);
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
	item = g_hash_table_lookup (priv->hash_id, id);
	if (item != NULL) {

		/* the previously stored app is what we actually want */
		if ((priv->add_flags & AS_STORE_ADD_FLAG_PREFER_LOCAL) > 0) {

			if (as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_APPSTREAM &&
			    as_app_get_source_kind (item) == AS_APP_SOURCE_KIND_APPDATA) {
				g_debug ("ignoring AppStream entry as AppData exists: %s", id);
				return;
			}
			if (as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_APPSTREAM &&
			    as_app_get_source_kind (item) == AS_APP_SOURCE_KIND_DESKTOP) {
				g_debug ("ignoring AppStream entry as desktop exists: %s", id);
				return;
			}
			if (as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_APPDATA &&
			    as_app_get_source_kind (item) == AS_APP_SOURCE_KIND_DESKTOP) {
				g_debug ("merging duplicate AppData:desktop entries: %s", id);
				as_app_subsume_full (app, item, AS_APP_SUBSUME_FLAG_BOTH_WAYS);
				/* promote the desktop source to AppData */
				as_app_set_source_kind (item, AS_APP_SOURCE_KIND_APPDATA);
				return;
			}
			if (as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_DESKTOP &&
			    as_app_get_source_kind (item) == AS_APP_SOURCE_KIND_APPDATA) {
				g_debug ("merging duplicate desktop:AppData entries: %s", id);
				as_app_subsume_full (app, item, AS_APP_SUBSUME_FLAG_BOTH_WAYS);
				return;
			}

		} else {
			if (as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_APPDATA &&
			    as_app_get_source_kind (item) == AS_APP_SOURCE_KIND_APPSTREAM) {
				as_app_set_state (item, AS_APP_STATE_INSTALLED);
				g_debug ("ignoring AppData entry as AppStream exists: %s", id);
				return;
			}
			if (as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_DESKTOP &&
			    as_app_get_source_kind (item) == AS_APP_SOURCE_KIND_APPSTREAM) {
				as_app_set_state (item, AS_APP_STATE_INSTALLED);
				g_debug ("ignoring desktop entry as AppStream exists: %s", id);
				return;
			}

			/* the previously stored app is higher priority */
			if (as_app_get_priority (item) >
			    as_app_get_priority (app)) {
				g_debug ("ignoring duplicate %s:%s entry: %s",
					 as_app_source_kind_to_string (as_app_get_source_kind (app)),
					 as_app_source_kind_to_string (as_app_get_source_kind (item)),
					 id);
				return;
			}

			/* same priority */
			if (as_app_get_priority (item) ==
			    as_app_get_priority (app)) {
				g_debug ("merging duplicate %s:%s entries: %s",
					 as_app_source_kind_to_string (as_app_get_source_kind (app)),
					 as_app_source_kind_to_string (as_app_get_source_kind (item)),
					 id);
				as_app_subsume_full (app, item,
						     AS_APP_SUBSUME_FLAG_BOTH_WAYS);

				/* promote the desktop source to AppData */
				if (as_app_get_source_kind (item) == AS_APP_SOURCE_KIND_DESKTOP &&
				    as_app_get_source_kind (app) == AS_APP_SOURCE_KIND_APPDATA)
					as_app_set_source_kind (item, AS_APP_SOURCE_KIND_APPDATA);
				return;
			}
		}

		/* this new item has a higher priority than the one we've
		 * previously stored */
		g_debug ("removing %s entry: %s",
			 as_app_source_kind_to_string (as_app_get_source_kind (item)),
			 id);
		g_hash_table_remove (priv->hash_id, id);
		g_ptr_array_remove (priv->array, item);
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
 * as_store_match_addons:
 **/
static void
as_store_match_addons (AsStore *store)
{
	AsApp *app;
	AsApp *parent;
	AsStorePrivate *priv = GET_PRIVATE (store);
	GPtrArray *plugin_ids;
	const gchar *tmp;
	guint i;
	guint j;

	for (i = 0; i < priv->array->len; i++) {
		app = g_ptr_array_index (priv->array, i);
		if (as_app_get_id_kind (app) != AS_ID_KIND_ADDON)
			continue;
		plugin_ids = as_app_get_extends (app);
		if (plugin_ids->len == 0) {
			g_warning ("%s was of type addon but had no extends",
				   as_app_get_id (app));
			continue;
		}
		for (j = 0; j < plugin_ids->len; j++) {
			tmp = g_ptr_array_index (plugin_ids, j);
			parent = g_hash_table_lookup (priv->hash_id, tmp);
			if (parent == NULL)
				continue;
			as_app_add_addon (parent, app);
		}
	}
}

/**
 * as_store_from_root:
 **/
static gboolean
as_store_from_root (AsStore *store,
		    GNode *root,
		    const gchar *icon_root,
		    const gchar *source_filename,
		    GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	GNode *apps;
	GNode *n;
	const gchar *tmp;
	_cleanup_free_ AsNodeContext *ctx = NULL;
	_cleanup_free_ gchar *icon_path = NULL;

	g_return_val_if_fail (AS_IS_STORE (store), FALSE);

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

	/* set in the XML file */
	tmp = as_node_get_attribute (apps, "builder_id");
	if (tmp != NULL)
		as_store_set_builder_id (store, tmp);

	/* if we have an origin either from the XML or _set_origin() */
	if (priv->origin != NULL) {
		if (icon_root == NULL)
			icon_root = "/usr/share/app-info/icons/";
		icon_path = g_build_filename (icon_root,
					      priv->origin,
					      NULL);
	}
	ctx = as_node_context_new ();
	for (n = apps->children; n != NULL; n = n->next) {
		_cleanup_error_free_ GError *error_local = NULL;
		_cleanup_object_unref_ AsApp *app = NULL;
		if (as_node_get_tag (n) != AS_TAG_APPLICATION)
			continue;

		/* do the filtering here */
		if (priv->filter != 0) {
			if (g_strcmp0 (as_node_get_name (n), "component") == 0) {
				AsIdKind kind_tmp;
				tmp = as_node_get_attribute (n, "type");
				kind_tmp = as_id_kind_from_string (tmp);
				if ((priv->filter & (1 << kind_tmp)) == 0)
					continue;
			}
		}

		app = as_app_new ();
		if (icon_path != NULL)
			as_app_set_icon_path (app, icon_path, -1);
		as_app_set_source_kind (app, AS_APP_SOURCE_KIND_APPSTREAM);
		if (!as_app_node_parse (app, n, ctx, &error_local)) {
			g_set_error (error,
				     AS_STORE_ERROR,
				     AS_STORE_ERROR_FAILED,
				     "Failed to parse root: %s",
				     error_local->message);
			return FALSE;
		}
		as_app_set_origin (app, priv->origin);
		if (source_filename != NULL)
			as_app_set_source_file (app, source_filename);
		as_store_add_app (store, app);
	}

	/* add addon kinds to their parent AsApp */
	as_store_match_addons (store);

	return TRUE;
}

/**
 * as_store_load_yaml_file:
 **/
static gboolean
as_store_load_yaml_file (AsStore *store,
			 GFile *file,
			 const gchar *icon_root,
			 GCancellable *cancellable,
			 GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	GNode *app_n;
	GNode *n;
	const gchar *tmp;
	_cleanup_free_ AsNodeContext *ctx = NULL;
	_cleanup_free_ gchar *icon_path = NULL;
	_cleanup_yaml_unref_ GNode *root = NULL;

	/* load file */
	root = as_yaml_from_file (file, cancellable, error);
	if (root == NULL)
		return FALSE;

	/* get header information */
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
	}

	/* if we have an origin either from the YAML or _set_origin() */
	if (priv->origin != NULL) {
		if (icon_root == NULL)
			icon_root = "/usr/share/app-info/icons/";
		icon_path = g_build_filename (icon_root,
					      priv->origin,
					      NULL);
	}

	/* parse applications */
	ctx = as_node_context_new ();
	for (app_n = root->children->next; app_n != NULL; app_n = app_n->next) {
		_cleanup_object_unref_ AsApp *app = NULL;
		if (app_n->children == NULL)
			continue;
		app = as_app_new ();
		if (icon_path != NULL)
			as_app_set_icon_path (app, icon_path, -1);
		as_app_set_source_kind (app, AS_APP_SOURCE_KIND_APPSTREAM);
		if (!as_app_node_parse_dep11 (app, app_n, ctx, error))
			return FALSE;
		as_app_set_origin (app, priv->origin);
		if (as_app_get_id (app) != NULL)
			as_store_add_app (store, app);
	}
	return TRUE;
}

/**
 * as_store_from_file:
 * @store: a #AsStore instance.
 * @file: a #GFile.
 * @icon_root: the icon path, or %NULL for the default.
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
		    const gchar *icon_root,
		    GCancellable *cancellable,
		    GError **error)
{
	_cleanup_free_ gchar *filename = NULL;
	_cleanup_error_free_ GError *error_local = NULL;
	_cleanup_node_unref_ GNode *root = NULL;

	g_return_val_if_fail (AS_IS_STORE (store), FALSE);

	/* a DEP-11 file */
	filename = g_file_get_path (file);
	if (g_strstr_len (filename, -1, ".yml") != NULL)
		return as_store_load_yaml_file (store, file, icon_root,
						cancellable, error);

	/* an AppStream XML file */
	root = as_node_from_file (file,
				  AS_NODE_FROM_XML_FLAG_LITERAL_TEXT,
				  cancellable,
				  &error_local);
	if (root == NULL) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to parse %s file: %s",
			     filename, error_local->message);
		return FALSE;
	}
	return as_store_from_root (store, root, icon_root, filename, error);
}

/**
 * as_store_from_xml:
 * @store: a #AsStore instance.
 * @data: XML data
 * @data_len: Length of @data, or -1 if NULL terminated
 * @icon_root: the icon path, or %NULL for the default.
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
		   gssize data_len,
		   const gchar *icon_root,
		   GError **error)
{
	_cleanup_error_free_ GError *error_local = NULL;
	_cleanup_node_unref_ GNode *root = NULL;

	g_return_val_if_fail (AS_IS_STORE (store), FALSE);

	root = as_node_from_xml (data, data_len,
				 AS_NODE_FROM_XML_FLAG_LITERAL_TEXT,
				 &error_local);
	if (root == NULL) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to parse XML: %s",
			     error_local->message);
		return TRUE;
	}
	return as_store_from_root (store, root, icon_root, NULL, error);
}

/**
 * as_store_apps_sort_cb:
 **/
static gint
as_store_apps_sort_cb (gconstpointer a, gconstpointer b)
{
	return g_strcmp0 (as_app_get_id (AS_APP (*(AsApp **) a)),
			  as_app_get_id (AS_APP (*(AsApp **) b)));
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
	gchar version[6];
	_cleanup_free_ AsNodeContext *ctx = NULL;

	/* get XML text */
	node_root = as_node_new ();
	if (priv->api_version >= 0.6) {
		node_apps = as_node_insert (node_root, "components", NULL, 0, NULL);
	} else {
		node_apps = as_node_insert (node_root, "applications", NULL, 0, NULL);
	}

	/* set origin attribute */
	if (priv->origin != NULL)
		as_node_add_attribute (node_apps, "origin", priv->origin, -1);

	/* set origin attribute */
	if (priv->builder_id != NULL)
		as_node_add_attribute (node_apps, "builder_id", priv->builder_id, -1);

	/* set version attribute */
	if (priv->api_version > 0.1f) {
		g_ascii_formatd (version, sizeof (version),
				 "%.1f", priv->api_version);
		as_node_add_attribute (node_apps, "version", version, -1);
	}

	/* sort by ID */
	g_ptr_array_sort (priv->array, as_store_apps_sort_cb);

	/* add applications */
	ctx = as_node_context_new ();
	as_node_context_set_version (ctx, priv->api_version);
	as_node_context_set_output (ctx, AS_APP_SOURCE_KIND_APPSTREAM);
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
		  AsNodeToXmlFlags flags,
		  GCancellable *cancellable,
		  GError **error)
{
	_cleanup_error_free_ GError *error_local = NULL;
	_cleanup_object_unref_ GOutputStream *out2 = NULL;
	_cleanup_object_unref_ GOutputStream *out = NULL;
	_cleanup_object_unref_ GZlibCompressor *compressor = NULL;
	_cleanup_string_free_ GString *xml = NULL;
	_cleanup_free_ gchar *basename = NULL;

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
AsStoreAddFlags
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
as_store_set_add_flags (AsStore *store, AsStoreAddFlags add_flags)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	priv->add_flags = add_flags;
}

/**
 * as_store_guess_origin_fallback:
 */
static gboolean
as_store_guess_origin_fallback (AsStore *store,
				const gchar *filename,
				GError **error)
{
	gchar *tmp;
	_cleanup_free_ gchar *origin_fallback = NULL;

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

/**
 * as_store_load_app_info_file:
 */
static gboolean
as_store_load_app_info_file (AsStore *store,
			     const gchar *path_xml,
			     const gchar *icon_root,
			     GCancellable *cancellable,
			     GError **error)
{
	_cleanup_object_unref_ GFile *file = NULL;

	/* guess this based on the name */
	if (!as_store_guess_origin_fallback (store, path_xml, error))
		return FALSE;

	/* load this specific file */
	g_debug ("Loading AppStream XML %s with icon path %s",
		 path_xml, icon_root);
	file = g_file_new_for_path (path_xml);
	return as_store_from_file (store,
				   file,
				   icon_root,
				   cancellable,
				   error);
}

/**
 * as_store_cache_changed_cb:
 */
static void
as_store_cache_changed_cb (GFileMonitor *monitor,
			   GFile *file, GFile *other_file,
			   GFileMonitorEvent event_type,
			   AsStore *store)
{
	g_debug ("Emitting ::changed()");
	g_signal_emit (store, signals[SIGNAL_CHANGED], 0);
}

/**
 * as_store_monitor_directory:
 **/
static gboolean
as_store_monitor_directory (AsStore *store,
			    const gchar *path,
			    GCancellable *cancellable,
			    GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	_cleanup_error_free_ GError *error_local = NULL;
	_cleanup_object_unref_ GFile *file = NULL;
	_cleanup_object_unref_ GFileMonitor *monitor = NULL;

	file = g_file_new_for_path (path);
	monitor = g_file_monitor_directory (file,
					    G_FILE_MONITOR_NONE,
					    cancellable,
					    &error_local);
	if (monitor == NULL) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to monitor %s: %s",
			     path, error_local->message);
		return FALSE;
	}
	g_signal_connect (monitor, "changed",
			  G_CALLBACK (as_store_cache_changed_cb),
			  store);
	g_ptr_array_add (priv->file_monitors, g_object_ref (monitor));
	return TRUE;
}

/**
 * as_store_load_app_info:
 **/
static gboolean
as_store_load_app_info (AsStore *store,
			const gchar *path,
			const gchar *format,
			GCancellable *cancellable,
			GError **error)
{
	const gchar *tmp;
	_cleanup_dir_close_ GDir *dir = NULL;
	_cleanup_error_free_ GError *error_local = NULL;
	_cleanup_free_ gchar *icon_root = NULL;
	_cleanup_free_ gchar *path_md = NULL;

	/* search all files */
	path_md = g_build_filename (path, format, NULL);
	if (!g_file_test (path_md, G_FILE_TEST_EXISTS))
		return TRUE;
	dir = g_dir_open (path_md, 0, &error_local);
	if (dir == NULL) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to open %s: %s",
			     path_md, error_local->message);
		return FALSE;
	}
	icon_root = g_build_filename (path, "icons", NULL);
	while ((tmp = g_dir_read_name (dir)) != NULL) {
		_cleanup_free_ gchar *filename_md = NULL;
		filename_md = g_build_filename (path_md, tmp, NULL);
		if (!as_store_load_app_info_file (store,
						  filename_md,
						  icon_root,
						  cancellable,
						  error))
			return FALSE;
	}

	/* watch the directories for changes */
	if (!as_store_monitor_directory (store, path_md, cancellable, error))
		return FALSE;
	if (!as_store_monitor_directory (store, icon_root, cancellable, error))
		return FALSE;

	return TRUE;
}

/**
 * as_store_add_app_install_screenshot:
 **/
static void
as_store_add_app_install_screenshot (AsApp *app)
{
	GPtrArray *pkgnames;
	const gchar *pkgname;
	_cleanup_free_ gchar *url = NULL;
	_cleanup_object_unref_ AsImage *im = NULL;
	_cleanup_object_unref_ AsScreenshot *ss = NULL;

	/* get the default package name */
	pkgnames = as_app_get_pkgnames (app);
	if (pkgnames->len == 0)
		return;
	pkgname = g_ptr_array_index (pkgnames, 0);
	url = g_build_filename ("http://screenshots.debian.net/screenshot",
				pkgname, NULL);

	/* screenshots.debian.net doesn't specify a size, so this is a guess */
	im = as_image_new ();
	as_image_set_url (im, url, -1);
	as_image_set_width (im, 800);
	as_image_set_height (im, 600);

	/* add screenshot without a caption */
	ss = as_screenshot_new ();
	as_screenshot_add_image (ss, im);
	as_app_add_screenshot (app, ss);
}

/**
 * as_store_load_app_install_file:
 **/
static gboolean
as_store_load_app_install_file (AsStore *store,
				const gchar *filename,
				const gchar *path_icons,
				GError **error)
{
	AsIcon *icon;
	GPtrArray *icons;
	guint i;
	_cleanup_error_free_ GError *error_local = NULL;
	_cleanup_object_unref_ AsApp *app = NULL;

	app = as_app_new ();
	as_app_set_icon_path (app, path_icons, -1);
	if (!as_app_parse_file (app,
				filename,
				AS_APP_PARSE_FLAG_USE_HEURISTICS,
				&error_local)) {
		if (g_error_matches (error_local,
				     AS_APP_ERROR,
				     AS_APP_ERROR_INVALID_TYPE)) {
			/* Ubuntu include non-apps here too... */
			return TRUE;
		}
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to parse %s: %s",
			     filename,
			     error_local->message);
		return FALSE;
	}

	/* convert all the icons */
	icons = as_app_get_icons (app);
	for (i = 0; i < icons->len; i++) {
		icon = g_ptr_array_index (icons, i);
		if (as_icon_get_kind (icon) == AS_ICON_KIND_UNKNOWN)
			as_icon_set_kind (icon, AS_ICON_KIND_CACHED);
	}
	as_store_add_app_install_screenshot (app);
	as_store_add_app (store, app);

	/* this isn't strictly true, but setting it AS_APP_SOURCE_KIND_DESKTOP
	 * means that it's considered installed by the front-end */
	 as_app_set_source_kind (app, AS_APP_SOURCE_KIND_APPSTREAM);

	return TRUE;
}

/**
 * as_store_load_app_install:
 **/
static gboolean
as_store_load_app_install (AsStore *store,
			   const gchar *path,
			   GCancellable *cancellable,
			   GError **error)
{
	const gchar *tmp;
	_cleanup_dir_close_ GDir *dir = NULL;
	_cleanup_error_free_ GError *error_local = NULL;
	_cleanup_free_ gchar *path_desktop = NULL;
	_cleanup_free_ gchar *path_icons = NULL;

	path_desktop = g_build_filename (path, "desktop", NULL);
	if (!g_file_test (path_desktop, G_FILE_TEST_EXISTS))
		return TRUE;
	dir = g_dir_open (path_desktop, 0, &error_local);
	if (dir == NULL) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to open %s: %s",
			     path_desktop,
			     error_local->message);
		return FALSE;
	}

	path_icons = g_build_filename (path, "icons", NULL);
	while ((tmp = g_dir_read_name (dir)) != NULL) {
		_cleanup_free_ gchar *filename = NULL;
		if (!g_str_has_suffix (tmp, ".desktop"))
			continue;
		filename = g_build_filename (path_desktop, tmp, NULL);
		if (!as_store_load_app_install_file (store,
						     filename,
						     path_icons,
						     error))
			return FALSE;
	}
	return TRUE;
}

/**
 * as_store_load_installed:
 **/
static gboolean
as_store_load_installed (AsStore *store,
			 AsStoreLoadFlags flags,
			 const gchar *path,
			 GCancellable *cancellable,
			 GError **error)
{
	AsAppParseFlags parse_flags = AS_APP_PARSE_FLAG_USE_HEURISTICS;
	AsStorePrivate *priv = GET_PRIVATE (store);
	GError *error_local = NULL;
	const gchar *tmp;
	_cleanup_dir_close_ GDir *dir = NULL;

	dir = g_dir_open (path, 0, error);
	if (dir == NULL)
		return FALSE;

	/* relax the checks when parsing */
	if (flags & AS_STORE_LOAD_FLAG_ALLOW_VETO)
		parse_flags |= AS_APP_PARSE_FLAG_ALLOW_VETO;

	while ((tmp = g_dir_read_name (dir)) != NULL) {
		AsApp *app_tmp;
		_cleanup_free_ gchar *filename = NULL;
		_cleanup_object_unref_ AsApp *app = NULL;
		filename = g_build_filename (path, tmp, NULL);
		if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR))
			continue;
		if ((priv->add_flags & AS_STORE_ADD_FLAG_PREFER_LOCAL) == 0) {
			app_tmp = as_store_get_app_by_id (store, tmp);
			if (app_tmp != NULL &&
			    as_app_get_source_kind (app_tmp) == AS_APP_SOURCE_KIND_DESKTOP) {
				as_app_set_state (app_tmp, AS_APP_STATE_INSTALLED);
				g_debug ("not parsing %s as %s already exists",
					 filename, tmp);
				continue;
			}
		}
		app = as_app_new ();
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

		/* do not load applications with vetos */
		if ((flags & AS_STORE_LOAD_FLAG_ALLOW_VETO) == 0 &&
		    as_app_get_vetos(app)->len > 0)
			continue;

		/* set lower priority than AppStream entries */
		as_app_set_priority (app, -1);
		as_app_set_state (app, AS_APP_STATE_INSTALLED);
		as_store_add_app (store, app);
	}
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
					path, cancellable, error);
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
as_store_load (AsStore *store,
	       AsStoreLoadFlags flags,
	       GCancellable *cancellable,
	       GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	const gchar * const * data_dirs;
	const gchar *tmp;
	gchar *path;
	guint i;
	_cleanup_ptrarray_unref_ GPtrArray *app_info = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *installed = NULL;

	/* system locations */
	app_info = g_ptr_array_new_with_free_func (g_free);
	installed = g_ptr_array_new_with_free_func (g_free);
	data_dirs = g_get_system_data_dirs ();
	for (i = 0; data_dirs[i] != NULL; i++) {
		if ((flags & AS_STORE_LOAD_FLAG_APP_INFO_SYSTEM) > 0) {
			path = g_build_filename (data_dirs[i], "app-info", NULL);
			g_ptr_array_add (app_info, path);
		}
		if ((flags & AS_STORE_LOAD_FLAG_APPDATA) > 0) {
			path = g_build_filename (data_dirs[i], "appdata", NULL);
			g_ptr_array_add (installed, path);
		}
		if ((flags & AS_STORE_LOAD_FLAG_DESKTOP) > 0) {
			path = g_build_filename (data_dirs[i], "applications", NULL);
			g_ptr_array_add (installed, path);
		}
	}
	if ((flags & AS_STORE_LOAD_FLAG_APP_INFO_SYSTEM) > 0) {
		path = g_build_filename (LOCALSTATEDIR, "lib", "app-info", NULL);
		g_ptr_array_add (app_info, path);
		path = g_build_filename (LOCALSTATEDIR, "cache", "app-info", NULL);
		g_ptr_array_add (app_info, path);
		/* ignore the prefix; we actually want to use the
		 * distro-provided data in this case. */
		if (g_strcmp0 (LOCALSTATEDIR, "/var") != 0) {
			path = g_build_filename ("/var", "lib", "app-info", NULL);
			g_ptr_array_add (app_info, path);
			path = g_build_filename ("/var", "cache", "app-info", NULL);
			g_ptr_array_add (app_info, path);
		}
	}

	/* per-user locations */
	if ((flags & AS_STORE_LOAD_FLAG_APP_INFO_USER) > 0) {
		path = g_build_filename (g_get_user_data_dir (), "app-info", NULL);
		g_ptr_array_add (app_info, path);
	}
	if ((flags & AS_STORE_LOAD_FLAG_APPDATA) > 0) {
		path = g_build_filename (g_get_user_data_dir (), "appdata", NULL);
		g_ptr_array_add (installed, path);
	}
	if ((flags & AS_STORE_LOAD_FLAG_DESKTOP) > 0) {
		path = g_build_filename (g_get_user_data_dir (), "applications", NULL);
		g_ptr_array_add (installed, path);
	}

	/* load each app-info path if it exists */
	for (i = 0; i < app_info->len; i++) {
		_cleanup_free_ gchar *dest = NULL;
		tmp = g_ptr_array_index (app_info, i);
		dest = g_build_filename (priv->destdir ? priv->destdir : "/", tmp, NULL);
		if (!g_file_test (dest, G_FILE_TEST_EXISTS))
			continue;
		if (!as_store_load_app_info (store, dest, "xmls", cancellable, error))
			return FALSE;
		if (!as_store_load_app_info (store, dest, "yaml", cancellable, error))
			return FALSE;
	}

	/* load each appdata and desktop path if it exists */
	for (i = 0; i < installed->len; i++) {
		_cleanup_free_ gchar *dest = NULL;
		tmp = g_ptr_array_index (installed, i);
		dest = g_build_filename (priv->destdir ? priv->destdir : "/", tmp, NULL);
		if (!g_file_test (dest, G_FILE_TEST_EXISTS))
			continue;
		if (!as_store_load_installed (store, flags, dest, cancellable, error))
			return FALSE;
	}

	/* ubuntu specific */
	if ((flags & AS_STORE_LOAD_FLAG_APP_INSTALL) > 0) {
		_cleanup_free_ gchar *dest = NULL;
		dest = g_build_filename (priv->destdir ? priv->destdir : "/",
					 "/usr/share/app-install", NULL);
		if (!as_store_load_app_install (store, dest, cancellable, error))
			return FALSE;
	}

	/* match again, for applications extended from different roots */
	as_store_match_addons (store);

	return TRUE;
}

/**
 * as_store_validate_add:
 */
G_GNUC_PRINTF (3, 4) static void
as_store_validate_add (GPtrArray *problems, AsProblemKind kind, const gchar *fmt, ...)
{
	AsProblem *problem;
	guint i;
	va_list args;
	_cleanup_free_ gchar *str = NULL;

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
as_store_validate (AsStore *store, AsAppValidateFlags flags, GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	AsApp *app;
	GPtrArray *probs;
	guint i;

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

	/* check each application */
	for (i = 0; i < priv->array->len; i++) {
		AsProblem *prob;
		guint j;
		_cleanup_ptrarray_unref_ GPtrArray *probs_app = NULL;

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
						       "metadata version is v%.1f and "
						       "<description> requiring markup "
						       "was introduced in v0.6",
						       priv->api_version);
			}
		}
		if (priv->api_version < 0.7) {
			if (as_app_get_id_kind (app) == AS_ID_KIND_ADDON) {
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
	}
	return probs;
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
