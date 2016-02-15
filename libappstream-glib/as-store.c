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
#include "as-problem.h"
#include "as-profile.h"
#include "as-monitor.h"
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
	GHashTable		*hash_id;	/* of AsApp{id} */
	GHashTable		*hash_pkgname;	/* of AsApp{pkgname} */
	AsMonitor		*monitor;
	GHashTable		*metadata_indexes;	/* GHashTable{key} */
	GHashTable		*appinfo_dirs;	/* GHashTable{path} */
	GHashTable		*xdg_app_dirs;	/* GHashTable{path} */
	AsStoreAddFlags		 add_flags;
	AsStoreWatchFlags	 watch_flags;
	AsStoreProblems		 problems;
	guint32			 filter;
	guint			 changed_block_refcnt;
	gboolean		 is_pending_changed_signal;
	AsProfile		*profile;
} AsStorePrivate;

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
G_DEFINE_QUARK (as-store-error-quark, as_store_error)

static void     as_store_add_path_xdg_app (AsStore           *store,
					   GPtrArray         *array,
					   const gchar       *base,
					   gboolean           monitor);
static gboolean as_store_load_app_info    (AsStore           *store,
					   const gchar       *path,
					   AsStoreLoadFlags   flags,
					   GCancellable      *cancellable,
					   GError           **error);

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
	g_object_unref (priv->monitor);
	g_object_unref (priv->profile);
	g_hash_table_unref (priv->hash_id);
	g_hash_table_unref (priv->hash_pkgname);
	g_hash_table_unref (priv->metadata_indexes);
	g_hash_table_unref (priv->appinfo_dirs);
	g_hash_table_unref (priv->xdg_app_dirs);

	G_OBJECT_CLASS (as_store_parent_class)->finalize (object);
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

	object_class->finalize = as_store_finalize;
}

/**
 * as_store_perhaps_emit_changed:
 */
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

/**
 * as_store_changed_inhibit:
 */
static guint32 *
as_store_changed_inhibit (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	priv->changed_block_refcnt++;
	return &priv->changed_block_refcnt;
}

/**
 * as_store_changed_uninhibit:
 */
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

/**
 * as_store_changed_uninhibit_cb:
 */
static void
as_store_changed_uninhibit_cb (void *v)
{
	as_store_changed_uninhibit ((guint32 **)v);
}

#define _cleanup_uninhibit_ __attribute__ ((cleanup(as_store_changed_uninhibit_cb)))

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
		{ "font-manager.desktop",	"org.gnome.FontManager.desktop" },
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
		{ "ark.desktop",		"org.kde.ark.desktop" },
		{ "blinken.desktop",		"org.kde.blinken.desktop" },
		{ "cantor.desktop",		"org.kde.cantor.desktop" },
		{ "dolphin.desktop",		"org.kde.dolphin.desktop" },
		{ "dragonplayer.desktop",	"org.kde.dragonplayer.desktop" },
		{ "filelight.desktop",		"org.kde.filelight.desktop" },
		{ "gwenview.desktop",		"org.kde.gwenview.desktop" },
		{ "kalgebra.desktop",		"org.kde.kalgebra.desktop" },
		{ "kanagram.desktop",		"org.kde.kanagram.desktop" },
		{ "kapman.desktop",		"org.kde.kapman.desktop" },
		{ "kbruch.desktop",		"org.kde.kbruch.desktop" },
		{ "kgeography.desktop",		"org.kde.kgeography.desktop" },
		{ "khangman.desktop",		"org.kde.khangman.desktop" },
		{ "kig.desktop",		"org.kde.kig.desktop" },
		{ "kiriki.desktop",		"org.kde.kiriki.desktop" },
		{ "kiten.desktop",		"org.kde.kiten.desktop" },
		{ "klettres.desktop",		"org.kde.klettres.desktop" },
		{ "klipper.desktop",		"org.kde.klipper.desktop" },
		{ "kmplot.desktop",		"org.kde.kmplot.desktop" },
		{ "kollision.desktop",		"org.kde.kollision.desktop" },
		{ "konsole.desktop",		"org.kde.konsole.desktop" },
		{ "kshisen.desktop",		"org.kde.kshisen.desktop" },
		{ "kstars.desktop",		"org.kde.kstars.desktop" },
		{ "ktp-log-viewer.desktop",	"org.kde.ktplogviewer.desktop" },
		{ "kturtle.desktop",		"org.kde.kturtle.desktop" },
		{ "kwordquiz.desktop",		"org.kde.kwordquiz.desktop" },
		{ "okteta.desktop",		"org.kde.okteta.desktop" },
		{ "parley.desktop",		"org.kde.parley.desktop" },
		{ "partitionmanager.desktop",	"org.kde.PartitionManager.desktop" },
		{ "picmi.desktop",		"org.kde.picmi.desktop" },
		{ "rocs.desktop",		"org.kde.rocs.desktop" },
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
		g_ptr_array_remove (priv->array, app);
	}
	g_hash_table_remove_all (priv->metadata_indexes);

	/* removed */
	as_store_perhaps_emit_changed (store, "remove-app-by-id");
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

	/* added */
	as_store_perhaps_emit_changed (store, "add-app");
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
	g_autoptr(AsProfileTask) ptask = NULL;

	/* profile */
	ptask = as_profile_start_literal (priv->profile, "AsStore:match-addons");

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
 * as_store_get_origin_for_xdg_app:
 **/
static gchar *
as_store_get_origin_for_xdg_app (const gchar *fn)
{
	g_auto(GStrv) split = g_strsplit (fn, "/", -1);
	guint chunks = g_strv_length (split);
	if (chunks < 5)
		return NULL;
	return g_strdup (split[chunks - 4]);
}

/**
 * as_store_from_root:
 **/
static gboolean
as_store_from_root (AsStore *store,
		    AsNode *root,
		    const gchar *icon_prefix,
		    const gchar *source_filename,
		    GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	AsNode *apps;
	AsNode *n;
	const gchar *tmp;
	g_autofree AsNodeContext *ctx = NULL;
	g_autofree gchar *icon_path = NULL;
	g_autofree gchar *origin_app = NULL;
	_cleanup_uninhibit_ guint32 *tok = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;

	g_return_val_if_fail (AS_IS_STORE (store), FALSE);

	/* profile */
	ptask = as_profile_start_literal (priv->profile, "AsStore:store-from-root");

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

	/* special case xdg-apps */
	if (g_strcmp0 (priv->origin, "xdg-app") == 0) {
		origin_app = as_store_get_origin_for_xdg_app (source_filename);
		g_debug ("using app origin of '%s' rather than '%s'",
			 origin_app, priv->origin);
	} else {
		origin_app = g_strdup (priv->origin);
	}

	/* guess the icon path after we've read the origin and then look for
	 * ../icons/$origin if the topdir is 'xmls', falling back to ./icons */
	if (icon_prefix != NULL) {
		g_autofree gchar *topdir = NULL;
		topdir = g_path_get_basename (icon_prefix);
		if ((g_strcmp0 (topdir, "xmls") == 0 ||
		     g_strcmp0 (topdir, "yaml") == 0)
		    && priv->origin != NULL) {
			g_autofree gchar *dirname = NULL;
			dirname = g_path_get_dirname (icon_prefix);
			icon_path = g_build_filename (dirname,
						      "icons",
						      priv->origin,
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

	ctx = as_node_context_new ();
	for (n = apps->children; n != NULL; n = n->next) {
		g_autoptr(GError) error_local = NULL;
		g_autoptr(AsApp) app = NULL;
		if (as_node_get_tag (n) != AS_TAG_COMPONENT)
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
			as_app_set_icon_path (app, icon_path);
		as_app_set_source_kind (app, AS_APP_SOURCE_KIND_APPSTREAM);
		if (!as_app_node_parse (app, n, ctx, &error_local)) {
			g_set_error (error,
				     AS_STORE_ERROR,
				     AS_STORE_ERROR_FAILED,
				     "Failed to parse root: %s",
				     error_local->message);
			return FALSE;
		}
		if (origin_app != NULL)
			as_app_set_origin (app, origin_app);
		if (source_filename != NULL)
			as_app_set_source_file (app, source_filename);
		as_store_add_app (store, app);
	}

	/* add addon kinds to their parent AsApp */
	as_store_match_addons (store);

	/* this store has changed */
	as_store_changed_uninhibit (&tok);
	as_store_perhaps_emit_changed (store, "from-root");

	return TRUE;
}

/**
 * as_store_load_yaml_file:
 **/
static gboolean
as_store_load_yaml_file (AsStore *store,
			 GFile *file,
			 GCancellable *cancellable,
			 GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	AsNode *app_n;
	AsNode *n;
	const gchar *tmp;
	g_autofree AsNodeContext *ctx = NULL;
	g_autofree gchar *icon_path = NULL;
	g_autoptr(AsYaml) root = NULL;
	_cleanup_uninhibit_ guint32 *tok = NULL;

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

	/* parse applications */
	ctx = as_node_context_new ();
	for (app_n = root->children->next; app_n != NULL; app_n = app_n->next) {
		g_autoptr(AsApp) app = NULL;
		if (app_n->children == NULL)
			continue;
		app = as_app_new ();
		if (icon_path != NULL)
			as_app_set_icon_path (app, icon_path);
		as_app_set_source_kind (app, AS_APP_SOURCE_KIND_APPSTREAM);
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

/**
 * as_store_rescan_xdg_app_dir:
 */
static void
as_store_rescan_xdg_app_dir (AsStore *store, const gchar *filename)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_autoptr(GPtrArray) app_info = NULL;
	const gchar *tmp;
	guint i;

	g_debug ("rescanning xdg-app dir %s", filename);

	app_info = g_ptr_array_new_with_free_func (g_free);
	as_store_add_path_xdg_app (store, app_info, filename, FALSE);

	for (i = 0; i < app_info->len; i++) {
		g_autofree gchar *dest = NULL;
		g_autoptr(GError) error_local = NULL;
		tmp = g_ptr_array_index (app_info, i);
		dest = g_build_filename (priv->destdir ? priv->destdir : "/", tmp, NULL);
		if (!g_file_test (dest, G_FILE_TEST_EXISTS))
			continue;
		if (!as_store_load_app_info (store, dest, AS_STORE_LOAD_FLAG_IGNORE_INVALID, NULL, &error_local))
			g_warning ("Can't load app info: %s\n", error_local->message);
	}
}

/**
 * as_store_remove_by_source_file:
 */
static void
as_store_remove_by_source_file (AsStore *store, const gchar *filename)
{
	AsApp *app;
	GPtrArray *apps;
	guint i;
	const gchar *tmp;
	g_autoptr(GPtrArray) ids = NULL;

	/* find any applications in the store with this source file */
	ids = g_ptr_array_new_with_free_func (g_free);
	apps = as_store_get_apps (store);
	for (i = 0; i < apps->len; i++) {
		app = g_ptr_array_index (apps, i);
		if (g_strcmp0 (as_app_get_source_file (app), filename) != 0)
			continue;
		g_ptr_array_add (ids, g_strdup (as_app_get_id (app)));
	}

	/* remove these from the store */
	for (i = 0; i < ids->len; i++) {
		tmp = g_ptr_array_index (ids, i);
		g_debug ("removing %s as %s invalid", tmp, filename);
		as_store_remove_app_by_id (store, tmp);
	}

	/* the store changed */
	as_store_perhaps_emit_changed (store, "remove-by-source-file");
}

/**
 * as_store_monitor_changed_cb:
 */
static void
as_store_monitor_changed_cb (AsMonitor *monitor,
			     const gchar *filename,
			     AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);

	/* reload, or emit a signal */
	if (priv->watch_flags & AS_STORE_WATCH_FLAG_ADDED) {
		_cleanup_uninhibit_ guint32 *tok = NULL;
		tok = as_store_changed_inhibit (store);

		if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
			g_autoptr(GError) error = NULL;
			g_autoptr(GFile) file = NULL;
			as_store_remove_by_source_file (store, filename);
			g_debug ("rescanning %s", filename);
			file = g_file_new_for_path (filename);
			if (!as_store_from_file (store, file, NULL, NULL, &error))
				g_warning ("failed to rescan: %s", error->message);
		} else if (g_hash_table_contains (priv->xdg_app_dirs, filename)) {
			as_store_rescan_xdg_app_dir (store, filename);
		}
	}
	as_store_perhaps_emit_changed (store, "file changed");
}

/**
 * as_store_monitor_added_cb:
 */
static void
as_store_monitor_added_cb (AsMonitor *monitor,
			     const gchar *filename,
			     AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);

	/* reload, or emit a signal */
	if (priv->watch_flags & AS_STORE_WATCH_FLAG_ADDED) {
		_cleanup_uninhibit_ guint32 *tok = NULL;
		tok = as_store_changed_inhibit (store);
		if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
			g_autoptr(GError) error = NULL;
			g_autoptr(GFile) file = NULL;

			g_debug ("scanning %s", filename);
			file = g_file_new_for_path (filename);
			if (!as_store_from_file (store, file, NULL, NULL, &error))
				g_warning ("failed to rescan: %s", error->message);
		} else if (g_hash_table_contains (priv->xdg_app_dirs, filename)) {
			as_store_rescan_xdg_app_dir (store, filename);
		}

	}
	as_store_perhaps_emit_changed (store, "file added");
}

/**
 * as_store_monitor_removed_cb:
 */
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
 * as_store_from_file:
 * @store: a #AsStore instance.
 * @file: a #GFile.
 * @icon_root: the icon path, or %NULL for the default (unused)
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
	g_autofree gchar *filename = NULL;
	g_autofree gchar *icon_prefix = NULL;
	g_autoptr(GError) error_local = NULL;
	g_autoptr(AsNode) root = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;

	g_return_val_if_fail (AS_IS_STORE (store), FALSE);

	/* profile */
	ptask = as_profile_start_literal (priv->profile, "AsStore:store-from-file");

	/* a DEP-11 file */
	filename = g_file_get_path (file);
	if (g_strstr_len (filename, -1, ".yml") != NULL)
		return as_store_load_yaml_file (store, file, cancellable, error);

#ifdef HAVE_GCAB
	/* a cab archive */
	if (g_str_has_suffix (filename, ".cab"))
		return as_store_cab_from_file (store, file, cancellable, error);
#endif

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

	/* watch for file changes */
	if (priv->watch_flags > 0) {
		if (!as_monitor_add_file (priv->monitor,
					  filename,
					  cancellable,
					  error))
			return FALSE;
	}

	/* icon prefix is the directory the XML has been found in */
	icon_prefix = g_path_get_dirname (filename);
	return as_store_from_root (store, root, icon_prefix, filename, error);
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
#ifdef HAVE_GCAB
	/* lets assume this is a .cab file for now */
	return as_store_cab_from_bytes (store, bytes, cancellable, error);
#else
	g_set_error (error,
		     AS_STORE_ERROR,
		     AS_STORE_ERROR_FAILED,
		     "no firmware support, compiled with --disable-firmware");
	return FALSE;
#endif
}

/**
 * as_store_from_xml:
 * @store: a #AsStore instance.
 * @data: XML data
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
		   const gchar *icon_root,
		   GError **error)
{
	g_autoptr(GError) error_local = NULL;
	g_autoptr(AsNode) root = NULL;

	g_return_val_if_fail (AS_IS_STORE (store), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	/* ignore empty file */
	if (data[0] == '\0')
		return TRUE;

	root = as_node_from_xml (data,
				 AS_NODE_FROM_XML_FLAG_LITERAL_TEXT,
				 &error_local);
	if (root == NULL) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to parse XML: %s",
			     error_local->message);
		return FALSE;
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
	AsNode *node_apps;
	AsNode *node_root;
	GString *xml;
	guint i;
	gchar version[6];
	g_autofree AsNodeContext *ctx = NULL;

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
 * as_store_get_watch_flags:
 * @store: a #AsStore instance.
 *
 * Gets the flags used for adding files to the store.
 *
 * Returns: the #AsStoreWatchFlags, or 0 if unset
 *
 * Since: 0.4.2
 **/
AsStoreWatchFlags
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
as_store_set_watch_flags (AsStore *store, AsStoreWatchFlags watch_flags)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	priv->watch_flags = watch_flags;
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

/**
 * as_store_load_app_info_file:
 */
static gboolean
as_store_load_app_info_file (AsStore *store,
			     const gchar *path_xml,
			     GCancellable *cancellable,
			     GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_autoptr(GFile) file = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;

	/* profile */
	ptask = as_profile_start (priv->profile, "AsStore:app-info{%s}", path_xml);

	/* guess this based on the name */
	if (!as_store_guess_origin_fallback (store, path_xml, error))
		return FALSE;

	file = g_file_new_for_path (path_xml);
	return as_store_from_file (store,
				   file,
				   NULL, /* icon path */
				   cancellable,
				   error);
}

/**
 * as_store_load_app_info:
 **/
static gboolean
as_store_load_app_info (AsStore *store,
			const gchar *path,
			AsStoreLoadFlags flags,
			GCancellable *cancellable,
			GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	const gchar *tmp;
	g_autoptr(GDir) dir = NULL;
	g_autoptr(GError) error_local = NULL;
	_cleanup_uninhibit_ guint32 *tok = NULL;

	/* Don't add the same dir twice, we're monitoring it for changes anyway */
	if (g_hash_table_contains (priv->appinfo_dirs, path))
		return TRUE;

	/* emit once when finished */
	tok = as_store_changed_inhibit (store);

	/* search all files */
	if (!g_file_test (path, G_FILE_TEST_EXISTS))
		return TRUE;
	dir = g_dir_open (path, 0, &error_local);
	if (dir == NULL) {
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
						  filename_md,
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

	/* watch the directories for changes */
	if (!as_monitor_add_directory (priv->monitor,
				       path,
				       cancellable,
				       error))
		return FALSE;

	g_hash_table_insert (priv->appinfo_dirs, g_strdup (path), NULL);

	/* emit changed */
	as_store_changed_uninhibit (&tok);
	as_store_perhaps_emit_changed (store, "load-app-info");

	return TRUE;
}

/**
 * as_store_set_app_installed:
 **/
static void
as_store_set_app_installed (AsApp *app)
{
	AsRelease *rel;
	GPtrArray *releases;
	guint i;

	/* releases */
	releases = as_app_get_releases (app);
	for (i = 0; i < releases->len; i++) {
		rel = g_ptr_array_index (releases, i);
		as_release_set_state (rel, AS_RELEASE_STATE_INSTALLED);
	}

	/* app itself */
	as_app_set_state (app, AS_APP_STATE_INSTALLED);
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
	g_autoptr(GDir) dir = NULL;
	_cleanup_uninhibit_ guint32 *tok = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;

	/* profile */
	ptask = as_profile_start (priv->profile, "AsStore:load-installed{%s}", path);

	dir = g_dir_open (path, 0, error);
	if (dir == NULL)
		return FALSE;

	/* watch the directories for changes */
	if (!as_monitor_add_directory (priv->monitor,
				       path,
				       cancellable,
				       error))
		return FALSE;

	/* emit once when finished */
	tok = as_store_changed_inhibit (store);

	/* relax the checks when parsing */
	if (flags & AS_STORE_LOAD_FLAG_ALLOW_VETO)
		parse_flags |= AS_APP_PARSE_FLAG_ALLOW_VETO;

	while ((tmp = g_dir_read_name (dir)) != NULL) {
		AsApp *app_tmp;
		g_autofree gchar *filename = NULL;
		g_autoptr(AsApp) app = NULL;
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
					path, cancellable, error);
}

/**
 * as_store_add_path_xdg_app:
 **/
static void
as_store_add_path_xdg_app (AsStore *store,
			   GPtrArray *array,
			   const gchar *base,
			   gboolean monitor)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	const gchar *filename;
	g_autoptr(GDir) dir = NULL;
	g_autoptr(GError) error_local = NULL;

	if (monitor) {
		/* mark and monitor directory so we can pick up later added remotes */
		g_hash_table_insert (priv->xdg_app_dirs, g_strdup (base), NULL);
		if (!as_monitor_add_file (priv->monitor,
					  base, NULL, &error_local))
			g_warning ("Can't monitor dir %s: %s",
				   base, error_local->message);
	}

	dir = g_dir_open (base, 0, NULL);
	if (dir == NULL)
		return;
	while ((filename = g_dir_read_name (dir)) != NULL) {
		g_autoptr(GError) error_local2 = NULL;
		gchar *fn = g_build_filename (base,
					      filename,
					      "x86_64",
					      "active",
					      NULL);
		g_ptr_array_add (array, fn);
	}
}

/**
 * as_store_add_path_both:
 **/
static void
as_store_add_path_both (GPtrArray *array, const gchar *base)
{
	g_ptr_array_add (array, g_build_filename (base, "yaml", NULL));
	g_ptr_array_add (array, g_build_filename (base, "xmls", NULL));
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
	/* load from multiple locations */
	AsStorePrivate *priv = GET_PRIVATE (store);
	const gchar * const * data_dirs;
	const gchar *tmp;
	gchar *path;
	guint i;
	g_autoptr(GPtrArray) app_info = NULL;
	g_autoptr(GPtrArray) installed = NULL;
	g_autoptr(AsProfileTask) ptask = NULL;
	_cleanup_uninhibit_ guint32 *tok = NULL;

	/* profile */
	ptask = as_profile_start_literal (priv->profile, "AsStore:load");

	/* system locations */
	app_info = g_ptr_array_new_with_free_func (g_free);
	installed = g_ptr_array_new_with_free_func (g_free);
	data_dirs = g_get_system_data_dirs ();
	for (i = 0; data_dirs[i] != NULL; i++) {
		if (g_strstr_len (data_dirs[i], -1, "xdg-app/exports") != NULL) {
			g_debug ("skipping %s as invalid", data_dirs[i]);
			continue;
		}
		if ((flags & AS_STORE_LOAD_FLAG_APP_INFO_SYSTEM) > 0) {
			g_autofree gchar *dest = NULL;
			dest = g_build_filename (data_dirs[i], "app-info", NULL);
			as_store_add_path_both (app_info, dest);
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
		g_autofree gchar *dest1 = NULL;
		g_autofree gchar *dest2 = NULL;
		dest1 = g_build_filename (LOCALSTATEDIR, "lib", "app-info", NULL);
		as_store_add_path_both (app_info, dest1);
		dest2 = g_build_filename (LOCALSTATEDIR, "cache", "app-info", NULL);
		as_store_add_path_both (app_info, dest2);
		/* ignore the prefix; we actually want to use the
		 * distro-provided data in this case. */
		if (g_strcmp0 (LOCALSTATEDIR, "/var") != 0) {
			g_autofree gchar *dest3 = NULL;
			g_autofree gchar *dest4 = NULL;
			dest3 = g_build_filename ("/var", "lib", "app-info", NULL);
			as_store_add_path_both (app_info, dest3);
			dest4 = g_build_filename ("/var", "cache", "app-info", NULL);
			as_store_add_path_both (app_info, dest4);
		}
	}

	/* per-user locations */
	if ((flags & AS_STORE_LOAD_FLAG_APP_INFO_USER) > 0) {
		g_autofree gchar *dest = NULL;
		dest = g_build_filename (g_get_user_data_dir (), "app-info", NULL);
		as_store_add_path_both (app_info, dest);
	}
	if ((flags & AS_STORE_LOAD_FLAG_APPDATA) > 0) {
		path = g_build_filename (g_get_user_data_dir (), "appdata", NULL);
		g_ptr_array_add (installed, path);
	}
	if ((flags & AS_STORE_LOAD_FLAG_DESKTOP) > 0) {
		path = g_build_filename (g_get_user_data_dir (), "applications", NULL);
		g_ptr_array_add (installed, path);
	}
	if ((flags & AS_STORE_LOAD_FLAG_XDG_APP_USER) > 0) {
		g_autofree gchar *dest = NULL;
		if ((flags & AS_STORE_LOAD_FLAG_APPDATA) > 0) {
			path = g_build_filename (g_get_user_data_dir (),
						 "xdg-app",
						 "exports",
						 "share",
						 "applications",
						 NULL);
			g_ptr_array_add (installed, path);
		}
		if ((flags & AS_STORE_LOAD_FLAG_DESKTOP) > 0) {
			path = g_build_filename (g_get_user_data_dir (),
						 "xdg-app",
						 "exports",
						 "share",
						 "appdata",
						 NULL);
			g_ptr_array_add (installed, path);
		}
		/* If we're running INSIDE the xdg-app environment we'll have the
		 * env var XDG_DATA_HOME set to "~/.var/app/org.gnome.Software/data"
		 * so specify the path manually to get the real data */
		dest = g_build_filename (g_get_home_dir (),
					 ".local",
					 "share",
					 "xdg-app",
					 "appstream",
					 NULL);
		as_store_add_path_xdg_app (store, app_info, dest, TRUE);
	}
	if ((flags & AS_STORE_LOAD_FLAG_XDG_APP_SYSTEM) > 0) {
		g_autofree gchar *dest = NULL;
		if ((flags & AS_STORE_LOAD_FLAG_APPDATA) > 0) {
			path = g_build_filename (LOCALSTATEDIR,
						 "xdg-app",
						 "exports",
						 "share",
						 "applications",
						 NULL);
			g_ptr_array_add (installed, path);
		}
		if ((flags & AS_STORE_LOAD_FLAG_DESKTOP) > 0) {
			path = g_build_filename (LOCALSTATEDIR,
						 "xdg-app",
						 "exports",
						 "share",
						 "appdata",
						 NULL);
			g_ptr_array_add (installed, path);
		}
		/* add AppStream */
		dest = g_build_filename (LOCALSTATEDIR,
					 "xdg-app",
					 "appstream",
					 NULL);
		as_store_add_path_xdg_app (store, app_info, dest, TRUE);
	}

	/* load each app-info path if it exists */
	tok = as_store_changed_inhibit (store);
	for (i = 0; i < app_info->len; i++) {
		g_autofree gchar *dest = NULL;
		tmp = g_ptr_array_index (app_info, i);
		dest = g_build_filename (priv->destdir ? priv->destdir : "/", tmp, NULL);
		if (!g_file_test (dest, G_FILE_TEST_EXISTS))
			continue;
		if (!as_store_load_app_info (store, dest, flags, cancellable, error))
			return FALSE;
	}

	/* load each appdata and desktop path if it exists */
	for (i = 0; i < installed->len; i++) {
		g_autofree gchar *dest = NULL;
		tmp = g_ptr_array_index (installed, i);
		dest = g_build_filename (priv->destdir ? priv->destdir : "/", tmp, NULL);
		if (!g_file_test (dest, G_FILE_TEST_EXISTS))
			continue;
		if (!as_store_load_installed (store, flags, dest, cancellable, error))
			return FALSE;
	}

	/* match again, for applications extended from different roots */
	as_store_match_addons (store);

	/* emit changed */
	as_store_changed_uninhibit (&tok);
	as_store_perhaps_emit_changed (store, "store-load");
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

/**
 * as_store_get_unique_name_app_key:
 */
static gchar *
as_store_get_unique_name_app_key (AsApp *app)
{
	g_autofree gchar *name_lower = NULL;
	name_lower = g_utf8_strdown (as_app_get_name (app, NULL), -1);
	return g_strdup_printf ("<%s:%s>",
				as_id_kind_to_string (as_app_get_id_kind (app)),
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
as_store_validate (AsStore *store, AsAppValidateFlags flags, GError **error)
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

		/* check uniqueness */
		app_key = as_store_get_unique_name_app_key (app);
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
	return probs;
}

/**
 * as_store_init:
 **/
static void
as_store_init (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	priv->profile = as_profile_new ();
	priv->api_version = AS_API_VERSION_NEWEST;
	priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->watch_flags = AS_STORE_WATCH_FLAG_NONE;
	priv->hash_id = g_hash_table_new_full (g_str_hash,
					       g_str_equal,
					       NULL,
					       NULL);
	priv->hash_pkgname = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    g_free,
						    (GDestroyNotify) g_object_unref);
	priv->appinfo_dirs = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    NULL,
						    NULL);
	priv->xdg_app_dirs = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    NULL,
						    NULL);
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
