/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2015 Richard Hughes <richard@hughsie.com>
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
 * SECTION:as-app
 * @short_description: An object for an AppStream application or add-on
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * This object represents the base object of all AppStream, the application.
 * Although called #AsApp, this object also represents components like fonts,
 * codecs and input methods.
 *
 * See also: #AsScreenshot, #AsRelease
 */

#include "config.h"

#include <string.h>

#include "as-app-private.h"
#include "as-bundle-private.h"
#include "as-enums.h"
#include "as-icon-private.h"
#include "as-node-private.h"
#include "as-provide-private.h"
#include "as-release-private.h"
#include "as-screenshot-private.h"
#include "as-tag.h"
#include "as-utils-private.h"
#include "as-yaml.h"

typedef struct
{
	AsAppProblems	 problems;
	AsIconKind	 icon_kind;
	AsIdKind	 id_kind;
	GHashTable	*comments;			/* of locale:string */
	GHashTable	*developer_names;		/* of locale:string */
	GHashTable	*descriptions;			/* of locale:string */
	GHashTable	*keywords;			/* of locale:GPtrArray */
	GHashTable	*languages;			/* of locale:string */
	GHashTable	*metadata;			/* of key:value */
	GHashTable	*names;				/* of locale:string */
	GHashTable	*urls;				/* of key:string */
	GPtrArray	*addons;			/* of AsApp */
	GPtrArray	*categories;			/* of string */
	GPtrArray	*compulsory_for_desktops;	/* of string */
	GPtrArray	*extends;			/* of string */
	GPtrArray	*kudos;				/* of string */
	GPtrArray	*permissions;				/* of string */
	GPtrArray	*mimetypes;			/* of string */
	GPtrArray	*pkgnames;			/* of string */
	GPtrArray	*architectures;			/* of string */
	GPtrArray	*releases;			/* of AsRelease */
	GPtrArray	*provides;			/* of AsProvide */
	GPtrArray	*screenshots;			/* of AsScreenshot */
	GPtrArray	*icons;				/* of AsIcon */
	GPtrArray	*bundles;			/* of AsBundle */
	GPtrArray	*vetos;				/* of string */
	AsAppSourceKind	 source_kind;
	AsAppState	 state;
	AsAppTrustFlags	 trust_flags;
	gchar		*icon_path;
	gchar		*id_filename;
	gchar		*id;
	gchar		*origin;
	gchar		*project_group;
	gchar		*project_license;
	gchar		*metadata_license;
	gchar		*source_pkgname;
	gchar		*update_contact;
	gchar		*source_file;
	gint		 priority;
	gsize		 token_cache_valid;
	GPtrArray	*token_cache;			/* of AsAppTokenItem */
} AsAppPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsApp, as_app, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_app_get_instance_private (o))

typedef struct {
	gchar		**values_ascii;
	gchar		**values_utf8;
	guint		  score;
} AsAppTokenItem;

/**
 * as_app_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.1.2
 **/
GQuark
as_app_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AsAppError");
	return quark;
}

/**
 * as_app_source_kind_from_string:
 * @source_kind: a source kind string
 *
 * Converts the text representation to an enumerated value.
 *
 * Return value: A #AsAppSourceKind, e.g. %AS_APP_SOURCE_KIND_APPSTREAM.
 *
 * Since: 0.2.2
 **/
AsAppSourceKind
as_app_source_kind_from_string (const gchar *source_kind)
{
	if (g_strcmp0 (source_kind, "appstream") == 0)
		return AS_APP_SOURCE_KIND_APPSTREAM;
	if (g_strcmp0 (source_kind, "appdata") == 0)
		return AS_APP_SOURCE_KIND_APPDATA;
	if (g_strcmp0 (source_kind, "metainfo") == 0)
		return AS_APP_SOURCE_KIND_METAINFO;
	if (g_strcmp0 (source_kind, "desktop") == 0)
		return AS_APP_SOURCE_KIND_DESKTOP;
	if (g_strcmp0 (source_kind, "inf") == 0)
		return AS_APP_SOURCE_KIND_INF;
	return AS_APP_SOURCE_KIND_UNKNOWN;
}

/**
 * as_app_source_kind_to_string:
 * @source_kind: the #AsAppSourceKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @source_kind, or %NULL for unknown
 *
 * Since: 0.2.2
 **/
const gchar *
as_app_source_kind_to_string (AsAppSourceKind source_kind)
{
	if (source_kind == AS_APP_SOURCE_KIND_APPSTREAM)
		return "appstream";
	if (source_kind == AS_APP_SOURCE_KIND_APPDATA)
		return "appdata";
	if (source_kind == AS_APP_SOURCE_KIND_METAINFO)
		return "metainfo";
	if (source_kind == AS_APP_SOURCE_KIND_DESKTOP)
		return "desktop";
	if (source_kind == AS_APP_SOURCE_KIND_INF)
		return "inf";
	return NULL;
}

/**
 * as_app_state_to_string:
 * @state: the #AsAppState.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @state, or %NULL for unknown
 *
 * Since: 0.2.2
 **/
const gchar *
as_app_state_to_string (AsAppState state)
{
	if (state == AS_APP_STATE_UNKNOWN)
		return "unknown";
	if (state == AS_APP_STATE_INSTALLED)
		return "installed";
	if (state == AS_APP_STATE_AVAILABLE)
		return "available";
	if (state == AS_APP_STATE_AVAILABLE_LOCAL)
		return "local";
	if (state == AS_APP_STATE_QUEUED_FOR_INSTALL)
		return "queued";
	if (state == AS_APP_STATE_INSTALLING)
		return "installing";
	if (state == AS_APP_STATE_REMOVING)
		return "removing";
	if (state == AS_APP_STATE_UPDATABLE)
		return "updatable";
	if (state == AS_APP_STATE_UPDATABLE_LIVE)
		return "updatable-live";
	if (state == AS_APP_STATE_UNAVAILABLE)
		return "unavailable";
	return NULL;
}

/**
 * as_app_guess_source_kind:
 * @filename: a file name
 *
 * Guesses the source kind based from the filename.
 *
 * Return value: A #AsAppSourceKind, e.g. %AS_APP_SOURCE_KIND_APPSTREAM.
 *
 * Since: 0.1.8
 **/
AsAppSourceKind
as_app_guess_source_kind (const gchar *filename)
{
	if (g_str_has_suffix (filename, ".xml.gz"))
		return AS_APP_SOURCE_KIND_APPSTREAM;
	if (g_str_has_suffix (filename, ".yml"))
		return AS_APP_SOURCE_KIND_APPSTREAM;
	if (g_str_has_suffix (filename, ".yml.gz"))
		return AS_APP_SOURCE_KIND_APPSTREAM;
#ifdef HAVE_GCAB
	if (g_str_has_suffix (filename, ".cab"))
		return AS_APP_SOURCE_KIND_APPSTREAM;
#endif
	if (g_str_has_suffix (filename, ".desktop"))
		return AS_APP_SOURCE_KIND_DESKTOP;
	if (g_str_has_suffix (filename, ".desktop.in"))
		return AS_APP_SOURCE_KIND_DESKTOP;
	if (g_str_has_suffix (filename, ".appdata.xml"))
		return AS_APP_SOURCE_KIND_APPDATA;
	if (g_str_has_suffix (filename, ".appdata.xml.in"))
		return AS_APP_SOURCE_KIND_APPDATA;
	if (g_str_has_suffix (filename, ".metainfo.xml"))
		return AS_APP_SOURCE_KIND_METAINFO;
	if (g_str_has_suffix (filename, ".metainfo.xml.in"))
		return AS_APP_SOURCE_KIND_METAINFO;
	if (g_str_has_suffix (filename, ".xml"))
		return AS_APP_SOURCE_KIND_APPSTREAM;
	if (g_str_has_suffix (filename, ".inf"))
		return AS_APP_SOURCE_KIND_INF;
	return AS_APP_SOURCE_KIND_UNKNOWN;
}

/**
 * as_app_finalize:
 **/
static void
as_app_finalize (GObject *object)
{
	AsApp *app = AS_APP (object);
	AsAppPrivate *priv = GET_PRIVATE (app);

	g_free (priv->icon_path);
	g_free (priv->id_filename);
	g_free (priv->id);
	g_free (priv->project_group);
	g_free (priv->project_license);
	g_free (priv->metadata_license);
	g_free (priv->origin);
	g_free (priv->source_pkgname);
	g_free (priv->update_contact);
	g_free (priv->source_file);
	g_hash_table_unref (priv->comments);
	g_hash_table_unref (priv->developer_names);
	g_hash_table_unref (priv->descriptions);
	g_hash_table_unref (priv->keywords);
	g_hash_table_unref (priv->languages);
	g_hash_table_unref (priv->metadata);
	g_hash_table_unref (priv->names);
	g_hash_table_unref (priv->urls);
	g_ptr_array_unref (priv->addons);
	g_ptr_array_unref (priv->categories);
	g_ptr_array_unref (priv->compulsory_for_desktops);
	g_ptr_array_unref (priv->extends);
	g_ptr_array_unref (priv->kudos);
	g_ptr_array_unref (priv->permissions);
	g_ptr_array_unref (priv->mimetypes);
	g_ptr_array_unref (priv->pkgnames);
	g_ptr_array_unref (priv->architectures);
	g_ptr_array_unref (priv->releases);
	g_ptr_array_unref (priv->provides);
	g_ptr_array_unref (priv->screenshots);
	g_ptr_array_unref (priv->icons);
	g_ptr_array_unref (priv->bundles);
	g_ptr_array_unref (priv->token_cache);
	g_ptr_array_unref (priv->vetos);

	G_OBJECT_CLASS (as_app_parent_class)->finalize (object);
}

/**
 * as_app_token_item_free:
 **/
static void
as_app_token_item_free (AsAppTokenItem *token_item)
{
	g_strfreev (token_item->values_ascii);
	g_strfreev (token_item->values_utf8);
	g_slice_free (AsAppTokenItem, token_item);
}

/**
 * as_app_init:
 **/
static void
as_app_init (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->categories = g_ptr_array_new_with_free_func (g_free);
	priv->compulsory_for_desktops = g_ptr_array_new_with_free_func (g_free);
	priv->extends = g_ptr_array_new_with_free_func (g_free);
	priv->keywords = g_hash_table_new_full (g_str_hash, g_str_equal,
						g_free, (GDestroyNotify) g_ptr_array_unref);
	priv->kudos = g_ptr_array_new_with_free_func (g_free);
	priv->permissions = g_ptr_array_new_with_free_func (g_free);
	priv->mimetypes = g_ptr_array_new_with_free_func (g_free);
	priv->pkgnames = g_ptr_array_new_with_free_func (g_free);
	priv->architectures = g_ptr_array_new_with_free_func (g_free);
	priv->addons = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->releases = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->provides = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->screenshots = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->icons = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->bundles = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->token_cache = g_ptr_array_new_with_free_func ((GDestroyNotify) as_app_token_item_free);
	priv->vetos = g_ptr_array_new_with_free_func (g_free);

	priv->comments = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->developer_names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->descriptions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->languages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	priv->metadata = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->urls = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

/**
 * as_app_class_init:
 **/
static void
as_app_class_init (AsAppClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_app_finalize;
}

/******************************************************************************/

/**
 * as_app_get_id:
 * @app: a #AsApp instance.
 *
 * Gets the full ID value.
 *
 * Returns: the ID, e.g. "org.gnome.Software.desktop"
 *
 * Since: 0.1.0
 **/
const gchar *
as_app_get_id (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->id;
}

/**
 * as_app_get_id_filename:
 * @app: a #AsApp instance.
 *
 * Returns a filename which represents the applications ID, e.g. "gimp.desktop"
 * becomes "gimp" and is used for cache directories.
 *
 * Returns: A utf8 filename
 *
 * Since: 0.3.0
 **/
const gchar *
as_app_get_id_filename (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->id_filename;
}

/**
 * as_app_get_categories:
 * @app: a #AsApp instance.
 *
 * Get the application categories.
 *
 * Returns: (element-type utf8) (transfer none): an array
 *
 * Since: 0.1.0
 **/
GPtrArray *
as_app_get_categories (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->categories;
}

/**
 * as_app_has_category:
 * @app: a #AsApp instance.
 * @category: a category string, e.g. "DesktopSettings"
 *
 * Searches the category list for a specific item.
 *
 * Returns: %TRUE if the application has got the specified category
 *
 * Since: 0.1.5
 */
gboolean
as_app_has_category (AsApp *app, const gchar *category)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	const gchar *tmp;
	guint i;

	for (i = 0; i < priv->categories->len; i++) {
		tmp = g_ptr_array_index (priv->categories, i);
		if (g_strcmp0 (tmp, category) == 0)
			return TRUE;
	}
	return FALSE;
}

/**
 * as_app_has_kudo:
 * @app: a #AsApp instance.
 * @kudo: a kudo string, e.g. "SearchProvider"
 *
 * Searches the kudo list for a specific item.
 *
 * Returns: %TRUE if the application has got the specified kudo
 *
 * Since: 0.2.2
 */
gboolean
as_app_has_kudo (AsApp *app, const gchar *kudo)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	const gchar *tmp;
	guint i;

	for (i = 0; i < priv->kudos->len; i++) {
		tmp = g_ptr_array_index (priv->kudos, i);
		if (g_strcmp0 (tmp, kudo) == 0)
			return TRUE;
	}
	return FALSE;
}

/**
 * as_app_has_kudo_kind:
 * @app: a #AsApp instance.
 * @kudo: a #AsKudoKind, e.g. %AS_KUDO_KIND_SEARCH_PROVIDER
 *
 * Searches the kudo list for a specific item.
 *
 * Returns: %TRUE if the application has got the specified kudo
 *
 * Since: 0.2.2
 */
gboolean
as_app_has_kudo_kind (AsApp *app, AsKudoKind kudo)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	const gchar *tmp;
	guint i;

	for (i = 0; i < priv->kudos->len; i++) {
		tmp = g_ptr_array_index (priv->kudos, i);
		if (as_kudo_kind_from_string (tmp) == kudo)
			return TRUE;
	}
	return FALSE;
}

/**
 * as_app_get_compulsory_for_desktops:
 * @app: a #AsApp instance.
 *
 * Returns the desktops where this application is compulsory.
 *
 * Returns: (element-type utf8) (transfer none): an array
 *
 * Since: 0.1.0
 **/
GPtrArray *
as_app_get_compulsory_for_desktops (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->compulsory_for_desktops;
}

/**
 * as_app_has_permission:
 * @app: a #AsApp instance.
 * @permission: a permission string, e.g. "Network"
 *
 * Searches the permission list for a specific item.
 *
 * Returns: %TRUE if the application has got the specified permission
 *
 * Since: 0.3.5
 */
gboolean
as_app_has_permission (AsApp *app, const gchar *permission)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	const gchar *tmp;
	guint i;

	for (i = 0; i < priv->permissions->len; i++) {
		tmp = g_ptr_array_index (priv->permissions, i);
		if (g_strcmp0 (tmp, permission) == 0)
			return TRUE;
	}
	return FALSE;
}

/**
 * as_app_get_keywords:
 * @app: a #AsApp instance.
 * @locale: the locale, or %NULL. e.g. "en_GB"
 *
 * Gets any keywords the application should match against.
 *
 * Returns: (element-type utf8) (transfer none): an array, or %NULL
 *
 * Since: 0.3.0
 **/
GPtrArray *
as_app_get_keywords (AsApp *app, const gchar *locale)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (locale == NULL)
		locale = "C";
	return g_hash_table_lookup (priv->keywords, locale);
}

/**
 * as_app_get_kudos:
 * @app: a #AsApp instance.
 *
 * Gets any kudos the application has obtained.
 *
 * Returns: (element-type utf8) (transfer none): an array
 *
 * Since: 0.2.2
 **/
GPtrArray *
as_app_get_kudos (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->kudos;
}

/**
 * as_app_get_permissions:
 * @app: a #AsApp instance.
 *
 * Gets any permissions the application has obtained.
 *
 * Returns: (element-type utf8) (transfer none): an array
 *
 * Since: 0.3.5
 **/
GPtrArray *
as_app_get_permissions (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->permissions;
}

/**
 * as_app_get_mimetypes:
 * @app: a #AsApp instance.
 *
 * Gets any mimetypes the application will register.
 *
 * Returns: (transfer none) (element-type utf8): an array
 *
 * Since: 0.2.0
 **/
GPtrArray *
as_app_get_mimetypes (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->mimetypes;
}

/**
 * as_app_get_releases:
 * @app: a #AsApp instance.
 *
 * Gets all the releases the application has had.
 *
 * Returns: (element-type AsRelease) (transfer none): an array
 *
 * Since: 0.1.0
 **/
GPtrArray *
as_app_get_releases (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->releases;
}

/**
 * as_app_get_release:
 * @app: a #AsApp instance.
 * @version: a version string
 *
 * Gets a specific release from the application.
 *
 * Returns: (transfer none): a release, or %NULL
 *
 * Since: 0.3.5
 **/
AsRelease *
as_app_get_release (AsApp *app, const gchar *version)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsRelease *release;
	guint i;

	for (i = 0; i < priv->releases->len; i++) {
		release = g_ptr_array_index (priv->releases, i);
		if (g_strcmp0 (as_release_get_version (release), version) == 0)
			return release;
	}
	return NULL;
}

/**
 * as_app_get_release_default:
 * @app: a #AsApp instance.
 *
 * Gets the default (newest) release from the application.
 *
 * Returns: (transfer none): a release, or %NULL
 *
 * Since: 0.3.5
 **/
AsRelease *
as_app_get_release_default (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsRelease *release_newest = NULL;
	AsRelease *release_tmp = NULL;
	guint i;

	for (i = 0; i < priv->releases->len; i++) {
		release_tmp = g_ptr_array_index (priv->releases, i);
		if (release_newest == NULL ||
		    as_release_vercmp (release_tmp, release_newest) < 1)
			release_newest = release_tmp;
	}
	return release_newest;
}

/**
 * as_app_get_provides:
 * @app: a #AsApp instance.
 *
 * Gets all the provides the application has.
 *
 * Returns: (element-type AsProvide) (transfer none): an array
 *
 * Since: 0.1.6
 **/
GPtrArray *
as_app_get_provides (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->provides;
}

/**
 * as_app_get_screenshots:
 * @app: a #AsApp instance.
 *
 * Gets any screenshots the application has defined.
 *
 * Returns: (element-type AsScreenshot) (transfer none): an array
 *
 * Since: 0.1.0
 **/
GPtrArray *
as_app_get_screenshots (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->screenshots;
}

/**
 * as_app_get_icons:
 * @app: a #AsApp instance.
 *
 * Gets any icons the application has defined.
 *
 * Returns: (element-type AsIcon) (transfer none): an array
 *
 * Since: 0.3.1
 **/
GPtrArray *
as_app_get_icons (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->icons;
}

/**
 * as_app_get_bundles:
 * @app: a #AsApp instance.
 *
 * Gets any bundles the application has defined.
 *
 * Returns: (element-type AsBundle) (transfer none): an array
 *
 * Since: 0.3.5
 **/
GPtrArray *
as_app_get_bundles (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->bundles;
}

/**
 * as_app_get_names:
 * @app: a #AsApp instance.
 *
 * Gets the names set for the application.
 *
 * Returns: (transfer none): hash table of names
 *
 * Since: 0.1.6
 **/
GHashTable *
as_app_get_names (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->names;
}

/**
 * as_app_get_comments:
 * @app: a #AsApp instance.
 *
 * Gets the comments set for the application.
 *
 * Returns: (transfer none): hash table of comments
 *
 * Since: 0.1.6
 **/
GHashTable *
as_app_get_comments (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->comments;
}

/**
 * as_app_get_developer_names:
 * @app: a #AsApp instance.
 *
 * Gets the developer_names set for the application.
 *
 * Returns: (transfer none): hash table of developer_names
 *
 * Since: 0.1.8
 **/
GHashTable *
as_app_get_developer_names (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->developer_names;
}

/**
 * as_app_get_metadata:
 * @app: a #AsApp instance.
 *
 * Gets the metadata set for the application.
 *
 * Returns: (transfer none): hash table of metadata
 *
 * Since: 0.1.6
 **/
GHashTable *
as_app_get_metadata (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->metadata;
}

/**
 * as_app_get_descriptions:
 * @app: a #AsApp instance.
 *
 * Gets the descriptions set for the application.
 *
 * Returns: (transfer none): hash table of descriptions
 *
 * Since: 0.1.6
 **/
GHashTable *
as_app_get_descriptions (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->descriptions;
}

/**
 * as_app_get_urls:
 * @app: a #AsApp instance.
 *
 * Gets the URLs set for the application.
 *
 * Returns: (transfer none): hash table of URLs
 *
 * Since: 0.1.0
 **/
GHashTable *
as_app_get_urls (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->urls;
}

/**
 * as_app_get_pkgnames:
 * @app: a #AsApp instance.
 *
 * Gets the package names (if any) for the application.
 *
 * Returns: (element-type utf8) (transfer none): an array
 *
 * Since: 0.1.0
 **/
GPtrArray *
as_app_get_pkgnames (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->pkgnames;
}

/**
 * as_app_get_architectures:
 * @app: a #AsApp instance.
 *
 * Gets the supported architectures for the application, or an empty list
 * if all architectures are supported.
 *
 * Returns: (element-type utf8) (transfer none): an array
 *
 * Since: 0.1.1
 **/
GPtrArray *
as_app_get_architectures (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->architectures;
}

/**
 * as_app_get_extends:
 * @app: a #AsApp instance.
 *
 * Gets the IDs that are extended from the addon.
 *
 * Returns: (element-type utf8) (transfer none): an array
 *
 * Since: 0.1.7
 **/
GPtrArray *
as_app_get_extends (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->extends;
}

/**
 * as_app_get_addons:
 * @app: a #AsApp instance.
 *
 * Gets all the addons the application has.
 *
 * Returns: (element-type AsApp) (transfer none): an array
 *
 * Since: 0.1.7
 **/
GPtrArray *
as_app_get_addons (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->addons;
}

/**
 * as_app_get_id_kind:
 * @app: a #AsApp instance.
 *
 * Gets the ID kind.
 *
 * Returns: enumerated value
 *
 * Since: 0.1.0
 **/
AsIdKind
as_app_get_id_kind (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->id_kind;
}

/**
 * as_app_get_name_size: (skip)
 * @app: a #AsApp instance.
 *
 * Gets the number of names.
 *
 * Returns: integer
 *
 * Since: 0.1.4
 **/
guint
as_app_get_name_size (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return g_hash_table_size (priv->names);
}

/**
 * as_app_get_comment_size: (skip)
 * @app: a #AsApp instance.
 *
 * Gets the number of comments.
 *
 * Returns: integer
 *
 * Since: 0.1.4
 **/
guint
as_app_get_comment_size (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return g_hash_table_size (priv->comments);
}

/**
 * as_app_get_description_size: (skip)
 * @app: a #AsApp instance.
 *
 * Gets the number of descriptions.
 *
 * Returns: integer
 *
 * Since: 0.1.4
 **/
guint
as_app_get_description_size (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return g_hash_table_size (priv->descriptions);
}

/**
 * as_app_get_source_kind:
 * @app: a #AsApp instance.
 *
 * Gets the source kind, i.e. where the AsApp came from.
 *
 * Returns: enumerated value
 *
 * Since: 0.1.4
 **/
AsAppSourceKind
as_app_get_source_kind (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->source_kind;
}

/**
 * as_app_get_state:
 * @app: a #AsApp instance.
 *
 * Gets the application state.
 *
 * Returns: enumerated value
 *
 * Since: 0.2.2
 **/
AsAppState
as_app_get_state (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->state;
}

/**
 * as_app_get_trust_flags:
 * @app: a #AsApp instance.
 *
 * Gets the trust flags, i.e. how trusted the incoming data is.
 *
 * Returns: bitfield
 *
 * Since: 0.2.2
 **/
AsAppTrustFlags
as_app_get_trust_flags (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->trust_flags;
}

/**
 * as_app_get_problems: (skip)
 * @app: a #AsApp instance.
 *
 * Gets the bitfield of problems.
 *
 * Returns: problems encountered during parsing the application
 *
 * Since: 0.1.4
 **/
AsAppProblems
as_app_get_problems (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->problems;
}

/**
 * as_app_get_pkgname_default:
 * @app: a #AsApp instance.
 *
 * Gets the default package name.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.2.0
 **/
const gchar *
as_app_get_pkgname_default (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (priv->pkgnames->len < 1)
		return NULL;
	return g_ptr_array_index (priv->pkgnames, 0);
}

/**
 * as_app_get_source_pkgname:
 * @app: a #AsApp instance.
 *
 * Gets the source package name that produced the binary package.
 * Only source packages producing more than one binary package will have this
 * entry set.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.2.4
 **/
const gchar *
as_app_get_source_pkgname (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->source_pkgname;
}

/**
 * as_app_get_icon_path:
 * @app: a #AsApp instance.
 *
 * Gets the application icon path.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.1.0
 **/
const gchar *
as_app_get_icon_path (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->icon_path;
}

/**
 * as_app_get_name:
 * @app: a #AsApp instance.
 * @locale: the locale, or %NULL. e.g. "en_GB"
 *
 * Gets the application name for a specific locale.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.1.0
 **/
const gchar *
as_app_get_name (AsApp *app, const gchar *locale)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return as_hash_lookup_by_locale (priv->names, locale);
}

/**
 * as_app_get_comment:
 * @app: a #AsApp instance.
 * @locale: the locale, or %NULL. e.g. "en_GB"
 *
 * Gets the application summary for a specific locale.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.1.0
 **/
const gchar *
as_app_get_comment (AsApp *app, const gchar *locale)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return as_hash_lookup_by_locale (priv->comments, locale);
}

/**
 * as_app_get_developer_name:
 * @app: a #AsApp instance.
 * @locale: the locale, or %NULL. e.g. "en_GB"
 *
 * Gets the application developer name for a specific locale.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.1.8
 **/
const gchar *
as_app_get_developer_name (AsApp *app, const gchar *locale)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return as_hash_lookup_by_locale (priv->developer_names, locale);
}

/**
 * as_app_get_description:
 * @app: a #AsApp instance.
 * @locale: the locale, or %NULL. e.g. "en_GB"
 *
 * Gets the application description markup for a specific locale.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.1.0
 **/
const gchar *
as_app_get_description (AsApp *app, const gchar *locale)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return as_hash_lookup_by_locale (priv->descriptions, locale);
}

/**
 * as_app_get_language:
 * @app: a #AsApp instance.
 * @locale: the locale, or %NULL. e.g. "en_GB"
 *
 * Gets the language coverage for the specific language.
 *
 * Returns: a percentage value where 0 is unspecified, or -1 for not found
 *
 * Since: 0.1.0
 **/
gint
as_app_get_language (AsApp *app, const gchar *locale)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	gboolean ret;
	gpointer value = NULL;

	if (locale == NULL)
		locale = "C";
	ret = g_hash_table_lookup_extended (priv->languages,
					    locale, NULL, &value);
	if (!ret)
		return -1;
	return GPOINTER_TO_INT (value);
}

/**
 * as_app_get_priority:
 * @app: a #AsApp instance.
 *
 * Gets the application priority. Larger values trump smaller values.
 *
 * Returns: priority value
 *
 * Since: 0.1.0
 **/
gint
as_app_get_priority (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->priority;
}

/**
 * as_app_get_languages:
 * @app: a #AsApp instance.
 *
 * Get a list of all languages.
 *
 * Returns: (transfer container) (element-type utf8): list of language values
 *
 * Since: 0.1.0
 **/
GList *
as_app_get_languages (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return g_hash_table_get_keys (priv->languages);
}

/**
 * as_app_get_url_item:
 * @app: a #AsApp instance.
 * @url_kind: the URL kind, e.g. %AS_URL_KIND_HOMEPAGE.
 *
 * Gets a URL.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.1.0
 **/
const gchar *
as_app_get_url_item (AsApp *app, AsUrlKind url_kind)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return g_hash_table_lookup (priv->urls,
				    as_url_kind_to_string (url_kind));
}

/**
 * as_app_get_metadata_item:
 * @app: a #AsApp instance.
 * @key: the metadata key.
 *
 * Gets some metadata item.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.1.0
 **/
const gchar *
as_app_get_metadata_item (AsApp *app, const gchar *key)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return g_hash_table_lookup (priv->metadata, key);
}

/**
 * as_app_get_project_group:
 * @app: a #AsApp instance.
 *
 * Gets an application project group.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.1.0
 **/
const gchar *
as_app_get_project_group (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->project_group;
}

/**
 * as_app_get_project_license:
 * @app: a #AsApp instance.
 *
 * Gets the application project license.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.1.0
 **/
const gchar *
as_app_get_project_license (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->project_license;
}

/**
 * as_app_get_metadata_license:
 * @app: a #AsApp instance.
 *
 * Gets the application project license.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.1.4
 **/
const gchar *
as_app_get_metadata_license (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->metadata_license;
}

/**
 * as_app_get_update_contact:
 * @app: a #AsApp instance.
 *
 * Gets the application upstream update contact email.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.1.4
 **/
const gchar *
as_app_get_update_contact (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->update_contact;
}

/**
 * as_app_get_origin:
 * @app: a #AsApp instance.
 *
 * Gets the application origin.
 *
 * Returns: the origin string, or %NULL if unset
 *
 * Since: 0.3.2
 **/
const gchar *
as_app_get_origin (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->origin;
}

/**
 * as_app_get_source_file:
 * @app: a #AsApp instance.
 *
 * Gets the source filename the instance was populated from.
 *
 * NOTE: this is not set for %AS_APP_SOURCE_KIND_APPSTREAM entries.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.2.2
 **/
const gchar *
as_app_get_source_file (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->source_file;
}

/**
 * as_app_validate_utf8:
 **/
static gboolean
as_app_validate_utf8 (const gchar *text)
{
	gboolean is_whitespace = TRUE;
	guint i;

	/* nothing */
	if (text == NULL)
		return TRUE;
	if (text[0] == '\0')
		return FALSE;

	/* is just whitespace */
	for (i = 0; text[i] != '\0' && is_whitespace; i++)
		is_whitespace = g_ascii_isspace (text[i]);
	if (is_whitespace)
		return FALSE;

	/* standard UTF-8 checks */
	if (!g_utf8_validate (text, -1, NULL))
		return FALSE;

	/* additional check for xmllint */
	for (i = 0; text[i] != '\0'; i++) {
		if (text[i] == 0x1f)
			return FALSE;
	}
	return TRUE;
}

/******************************************************************************/

/**
 * as_app_set_id:
 * @app: a #AsApp instance.
 * @id: the new _full_ application ID, e.g. "org.gnome.Software.desktop".
 *
 * Sets a new application ID. Any invalid characters will be automatically replaced.
 *
 * Since: 0.1.0
 **/
void
as_app_set_id (AsApp *app, const gchar *id)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	gchar *tmp;

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (id)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}

	g_free (priv->id);
	g_free (priv->id_filename);

	priv->id = g_strdup (id);
	priv->id_filename = g_strdup (priv->id);
	g_strdelimit (priv->id_filename, "&<>", '-');
	tmp = g_strrstr_len (priv->id_filename, -1, ".");
	if (tmp != NULL)
		*tmp = '\0';
}

/**
 * as_app_set_source_kind:
 * @app: a #AsApp instance.
 * @source_kind: the #AsAppSourceKind.
 *
 * Sets the source kind.
 *
 * Since: 0.1.4
 **/
void
as_app_set_source_kind (AsApp *app, AsAppSourceKind source_kind)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->source_kind = source_kind;
}

/**
 * as_app_set_state:
 * @app: a #AsApp instance.
 * @state: the #AsAppState.
 *
 * Sets the application state.
 *
 * Since: 0.2.2
 **/
void
as_app_set_state (AsApp *app, AsAppState state)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->state = state;
}

/**
 * as_app_set_trust_flags:
 * @app: a #AsApp instance.
 * @trust_flags: the #AsAppSourceKind.
 *
 * Sets the check flags, where %AS_APP_TRUST_FLAG_COMPLETE is completely
 * trusted input.
 *
 * Since: 0.2.2
 **/
void
as_app_set_trust_flags (AsApp *app, AsAppTrustFlags trust_flags)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->trust_flags = trust_flags;
}

/**
 * as_app_set_id_kind:
 * @app: a #AsApp instance.
 * @id_kind: the #AsIdKind.
 *
 * Sets the application kind.
 *
 * Since: 0.1.0
 **/
void
as_app_set_id_kind (AsApp *app, AsIdKind id_kind)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->id_kind = id_kind;
}

/**
 * as_app_set_project_group:
 * @app: a #AsApp instance.
 * @project_group: the project group, e.g. "GNOME".
 *
 * Set any project affiliation.
 *
 * Since: 0.1.0
 **/
void
as_app_set_project_group (AsApp *app, const gchar *project_group)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (project_group)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}

	g_free (priv->project_group);
	priv->project_group = g_strdup (project_group);
}

/**
 * as_app_set_project_license:
 * @app: a #AsApp instance.
 * @project_license: the project license string.
 *
 * Set the project license.
 *
 * Since: 0.1.0
 **/
void
as_app_set_project_license (AsApp *app, const gchar *project_license)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (project_license)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}

	g_free (priv->project_license);
	priv->project_license = g_strdup (project_license);
}

/**
 * as_app_set_metadata_license:
 * @app: a #AsApp instance.
 * @metadata_license: the project license string.
 *
 * Set the project license.
 *
 * Since: 0.1.4
 **/
void
as_app_set_metadata_license (AsApp *app, const gchar *metadata_license)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_auto(GStrv) tokens = NULL;

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (metadata_license)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}

	/* automatically replace deprecated license names */
	g_free (priv->metadata_license);
	tokens = as_utils_spdx_license_tokenize (metadata_license);
	priv->metadata_license = as_utils_spdx_license_detokenize (tokens);
}

/**
 * as_app_set_source_pkgname:
 * @app: a #AsApp instance.
 * @source_pkgname: the project license string.
 *
 * Set the project license.
 *
 * Since: 0.2.4
 **/
void
as_app_set_source_pkgname (AsApp *app,
			     const gchar *source_pkgname)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (source_pkgname)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}
	g_free (priv->source_pkgname);
	priv->source_pkgname = g_strdup (source_pkgname);
}

/**
 * as_app_set_source_file:
 * @app: a #AsApp instance.
 * @source_file: the filename.
 *
 * Set the file that the instance was sourced from.
 *
 * Since: 0.2.2
 **/
void
as_app_set_source_file (AsApp *app, const gchar *source_file)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_free (priv->source_file);
	priv->source_file = g_strdup (source_file);
}

/**
 * as_app_set_update_contact:
 * @app: a #AsApp instance.
 * @update_contact: the project license string.
 *
 * Set the project license.
 *
 * Since: 0.1.4
 **/
void
as_app_set_update_contact (AsApp *app, const gchar *update_contact)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	gboolean done_replacement = TRUE;
	gchar *tmp;
	gsize len;
	guint i;
	struct {
		const gchar	*search;
		const gchar	 replace;
	} replacements[] =  {
		{ "(@)",	'@' },
		{ " _at_ ",	'@' },
		{ "_at_",	'@' },
		{ "(at)",	'@' },
		{ " AT ",	'@' },
		{ "_dot_",	'.' },
		{ " DOT ",	'.' },
		{ NULL,		'\0' } };

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (update_contact)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}

	/* copy as-is */
	g_free (priv->update_contact);
	priv->update_contact = g_strdup (update_contact);
	if (priv->update_contact == NULL)
		return;

	/* keep going until we have no more matches */
	len = strlen (priv->update_contact);
	while (done_replacement) {
		done_replacement = FALSE;
		for (i = 0; replacements[i].search != NULL; i++) {
			tmp = g_strstr_len (priv->update_contact, -1,
					    replacements[i].search);
			if (tmp != NULL) {
				*tmp = replacements[i].replace;
				g_strlcpy (tmp + 1,
					   tmp + strlen (replacements[i].search),
					   len);
				done_replacement = TRUE;
			}
		}
	}
}

/**
 * as_app_set_origin:
 * @app: a #AsApp instance.
 * @origin: the origin, e.g. "fedora-21"
 *
 * Sets the application origin.
 *
 * Since: 0.3.2
 **/
void
as_app_set_origin (AsApp *app, const gchar *origin)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_free (priv->origin);
	priv->origin = g_strdup (origin);
}

/**
 * as_app_set_icon_path:
 * @app: a #AsApp instance.
 * @icon_path: the local path.
 *
 * Sets the icon path, where local icons would be found.
 *
 * Since: 0.1.0
 **/
void
as_app_set_icon_path (AsApp *app, const gchar *icon_path)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (icon_path)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}

	g_free (priv->icon_path);
	priv->icon_path = g_strdup (icon_path);
}

/**
 * as_app_set_name:
 * @app: a #AsApp instance.
 * @locale: the locale, or %NULL. e.g. "en_GB"
 * @name: the application name.
 *
 * Sets the application name for a specific locale.
 *
 * Since: 0.1.0
 **/
void
as_app_set_name (AsApp *app,
		 const gchar *locale,
		 const gchar *name)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	gchar *tmp_locale;

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (name)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}

	/* get fixed locale */
	tmp_locale = as_node_fix_locale (locale);
	if (tmp_locale == NULL)
		return;
	g_hash_table_insert (priv->names,
			     tmp_locale,
			     g_strdup (name));
}

/**
 * as_app_set_comment:
 * @app: a #AsApp instance.
 * @locale: the locale, or %NULL. e.g. "en_GB"
 * @comment: the application summary.
 *
 * Sets the application summary for a specific locale.
 *
 * Since: 0.1.0
 **/
void
as_app_set_comment (AsApp *app,
		    const gchar *locale,
		    const gchar *comment)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	gchar *tmp_locale;

	g_return_if_fail (comment != NULL);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (comment)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}

	/* get fixed locale */
	tmp_locale = as_node_fix_locale (locale);
	if (tmp_locale == NULL)
		return;
	g_hash_table_insert (priv->comments,
			     tmp_locale,
			     g_strdup (comment));
}

/**
 * as_app_set_developer_name:
 * @app: a #AsApp instance.
 * @locale: the locale, or %NULL. e.g. "en_GB"
 * @developer_name: the application developer name.
 *
 * Sets the application developer name for a specific locale.
 *
 * Since: 0.1.0
 **/
void
as_app_set_developer_name (AsApp *app,
			   const gchar *locale,
			   const gchar *developer_name)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	gchar *tmp_locale;

	g_return_if_fail (developer_name != NULL);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (developer_name)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}

	/* get fixed locale */
	tmp_locale = as_node_fix_locale (locale);
	if (tmp_locale == NULL)
		return;
	g_hash_table_insert (priv->developer_names,
			     tmp_locale,
			     g_strdup (developer_name));
}

/**
 * as_app_set_description:
 * @app: a #AsApp instance.
 * @locale: the locale, or %NULL. e.g. "en_GB"
 * @description: the application description.
 *
 * Sets the application descrption markup for a specific locale.
 *
 * Since: 0.1.0
 **/
void
as_app_set_description (AsApp *app,
			const gchar *locale,
			const gchar *description)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	gchar *tmp_locale;

	g_return_if_fail (description != NULL);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (description)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}

	/* get fixed locale */
	tmp_locale = as_node_fix_locale (locale);
	if (tmp_locale == NULL)
		return;
	g_hash_table_insert (priv->descriptions,
			     tmp_locale,
			     g_strdup (description));
}

/**
 * as_app_set_priority:
 * @app: a #AsApp instance.
 * @priority: the priority.
 *
 * Sets the application priority, where 0 is default and positive numbers
 * are better than negative numbers.
 *
 * Since: 0.1.0
 **/
void
as_app_set_priority (AsApp *app, gint priority)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->priority = priority;
}

/**
 * as_app_add_category:
 * @app: a #AsApp instance.
 * @category: the category.
 *
 * Adds a menu category to the application.
 *
 * Since: 0.1.0
 **/
void
as_app_add_category (AsApp *app, const gchar *category)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (category)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0 &&
	    as_ptr_array_find_string (priv->categories, category)) {
		return;
	}

	/* simple substitution */
	if (g_strcmp0 (category, "Feed") == 0)
		category = "News";

	g_ptr_array_add (priv->categories, g_strdup (category));
}


/**
 * as_app_add_compulsory_for_desktop:
 * @app: a #AsApp instance.
 * @compulsory_for_desktop: the desktop string, e.g. "GNOME".
 *
 * Adds a desktop that requires this application to be installed.
 *
 * Since: 0.1.0
 **/
void
as_app_add_compulsory_for_desktop (AsApp *app, const gchar *compulsory_for_desktop)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (compulsory_for_desktop)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0 &&
	    as_ptr_array_find_string (priv->compulsory_for_desktops,
				      compulsory_for_desktop)) {
		return;
	}

	g_ptr_array_add (priv->compulsory_for_desktops,
			 g_strdup (compulsory_for_desktop));
}

/**
 * as_app_add_keyword:
 * @app: a #AsApp instance.
 * @locale: the locale, or %NULL. e.g. "en_GB"
 * @keyword: the keyword.
 *
 * Add a keyword the application should match against.
 *
 * Since: 0.3.0
 **/
void
as_app_add_keyword (AsApp *app,
		    const gchar *locale,
		    const gchar *keyword)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	GPtrArray *tmp;
	g_autofree gchar *tmp_locale = NULL;

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (keyword)) {
		return;
	}

	/* get fixed locale */
	tmp_locale = as_node_fix_locale (locale);
	if (tmp_locale == NULL)
		return;

	/* create an array if required */
	tmp = g_hash_table_lookup (priv->keywords, tmp_locale);
	if (tmp == NULL) {
		tmp = g_ptr_array_new_with_free_func (g_free);
		g_hash_table_insert (priv->keywords, g_strdup (tmp_locale), tmp);
	} else if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0) {
		if (as_ptr_array_find_string (tmp, keyword))
			return;
	}
	g_ptr_array_add (tmp, g_strdup (keyword));
}

/**
 * as_app_add_kudo:
 * @app: a #AsApp instance.
 * @kudo: the kudo.
 *
 * Add a kudo the application has obtained.
 *
 * Since: 0.2.2
 **/
void
as_app_add_kudo (AsApp *app, const gchar *kudo)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (kudo)) {
		return;
	}
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0 &&
	    as_ptr_array_find_string (priv->kudos, kudo)) {
		return;
	}
	g_ptr_array_add (priv->kudos, g_strdup (kudo));
}

/**
 * as_app_add_permission:
 * @app: a #AsApp instance.
 * @permission: the permission.
 *
 * Add a permission the application has obtained.
 *
 * Since: 0.3.5
 **/
void
as_app_add_permission (AsApp *app, const gchar *permission)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (permission)) {
		return;
	}
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0 &&
	    as_ptr_array_find_string (priv->permissions, permission)) {
		return;
	}
	g_ptr_array_add (priv->permissions, g_strdup (permission));
}

/**
 * as_app_add_kudo_kind:
 * @app: a #AsApp instance.
 * @kudo_kind: the #AsKudoKind.
 *
 * Add a kudo the application has obtained.
 *
 * Since: 0.2.2
 **/
void
as_app_add_kudo_kind (AsApp *app, AsKudoKind kudo_kind)
{
	as_app_add_kudo (app, as_kudo_kind_to_string (kudo_kind));
}

/**
 * as_app_add_mimetype:
 * @app: a #AsApp instance.
 * @mimetype: the mimetype.
 *
 * Adds a mimetype the application can process.
 *
 * Since: 0.1.0
 **/
void
as_app_add_mimetype (AsApp *app, const gchar *mimetype)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (mimetype)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0 &&
	    as_ptr_array_find_string (priv->mimetypes, mimetype)) {
		return;
	}

	g_ptr_array_add (priv->mimetypes, g_strdup (mimetype));
}

/**
 * as_app_subsume_release:
 **/
static void
as_app_subsume_release (AsRelease *release, AsRelease *donor)
{
	AsChecksum *csum;
	AsChecksum *csum_tmp;
	GPtrArray *locations;
	GPtrArray *checksums;
	const gchar *tmp;
	guint i;

	/* this is high quality metadata */
	tmp = as_release_get_description (donor, NULL);
	if (tmp != NULL)
		as_release_set_description (release, NULL, tmp);

	/* overwrite the timestamp if the metadata is high quality,
	 * or if no timestamp has already been set */
	if (tmp != NULL || as_release_get_timestamp (release) == 0)
		as_release_set_timestamp (release, as_release_get_timestamp (donor));

	/* overwrite the version */
	tmp = as_release_get_version (donor);
	if (tmp != NULL && as_release_get_version (release) == NULL)
		as_release_set_version (release, tmp);

	/* copy all locations */
	locations = as_release_get_locations (donor);
	for (i = 0; i < locations->len; i++) {
		tmp = g_ptr_array_index (locations, i);
		as_release_add_location (release, tmp);
	}

	/* copy checksums if set */
	checksums = as_release_get_checksums (donor);
	for (i = 0; i < checksums->len; i++) {
		csum = g_ptr_array_index (checksums, i);
		tmp = as_checksum_get_filename (csum);
		csum_tmp = as_release_get_checksum_by_fn (release, tmp);
		if (csum_tmp != NULL)
			continue;
		as_release_add_checksum (release, csum);
	}
}

/**
 * as_app_add_release:
 * @app: a #AsApp instance.
 * @release: a #AsRelease instance.
 *
 * Adds a release to an application.
 *
 * Since: 0.1.0
 **/
void
as_app_add_release (AsApp *app, AsRelease *release)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsRelease *release_old;

	/* if already exists them update */
	release_old = as_app_get_release (app, as_release_get_version (release));
	if (release_old == NULL)
		release_old = as_app_get_release (app, NULL);
	if (release_old == release)
		return;
	if (release_old != NULL) {
		as_app_subsume_release (release_old, release);
		return;
	}

	g_ptr_array_add (priv->releases, g_object_ref (release));
}

/**
 * as_app_add_provide:
 * @app: a #AsApp instance.
 * @provide: a #AsProvide instance.
 *
 * Adds a provide to an application.
 *
 * Since: 0.1.6
 **/
void
as_app_add_provide (AsApp *app, AsProvide *provide)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsProvide *tmp;
	guint i;

	/* check for duplicates */
	if (priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) {
		for (i = 0; i < priv->provides->len; i++) {
			tmp = g_ptr_array_index (priv->provides, i);
			if (as_provide_get_kind (tmp) == as_provide_get_kind (provide) &&
			    g_strcmp0 (as_provide_get_value (tmp),
				       as_provide_get_value (provide)) == 0)
				return;
		}
	}

	g_ptr_array_add (priv->provides, g_object_ref (provide));
}

/**
 * as_app_sort_screenshots:
 **/
static gint
as_app_sort_screenshots (gconstpointer a, gconstpointer b)
{
	AsScreenshot *ss1 = *((AsScreenshot **) a);
	AsScreenshot *ss2 = *((AsScreenshot **) b);
	if (as_screenshot_get_priority (ss1) < as_screenshot_get_priority (ss2))
		return 1;
	if (as_screenshot_get_priority (ss1) > as_screenshot_get_priority (ss2))
		return -1;
	return g_strcmp0 (as_screenshot_get_caption (ss1, NULL),
			  as_screenshot_get_caption (ss2, NULL));
}

/**
 * as_app_add_screenshot:
 * @app: a #AsApp instance.
 * @screenshot: a #AsScreenshot instance.
 *
 * Adds a screenshot to an application.
 *
 * Since: 0.1.0
 **/
void
as_app_add_screenshot (AsApp *app, AsScreenshot *screenshot)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsScreenshot *ss;
	guint i;

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0) {
		for (i = 0; i < priv->screenshots->len; i++) {
			ss = g_ptr_array_index (priv->screenshots, i);
			if (as_screenshot_equal (ss, screenshot))
				return;
		}
	}

	/* add then resort */
	g_ptr_array_add (priv->screenshots, g_object_ref (screenshot));
	g_ptr_array_sort (priv->screenshots, as_app_sort_screenshots);

	/* make only the first screenshot default */
	for (i = 0; i < priv->screenshots->len; i++) {
		ss = g_ptr_array_index (priv->screenshots, i);
		as_screenshot_set_kind (ss, i == 0 ? AS_SCREENSHOT_KIND_DEFAULT :
						     AS_SCREENSHOT_KIND_NORMAL);
	}
}

/**
 * as_app_check_icon_duplicate:
 **/
static gboolean
as_app_check_icon_duplicate (AsIcon *icon1, AsIcon *icon2)
{
	if (as_icon_get_width (icon1) != as_icon_get_width (icon2))
		return FALSE;
	if (as_icon_get_height (icon1) != as_icon_get_height (icon2))
		return FALSE;
	if (g_strcmp0 (as_icon_get_name (icon1),
		       as_icon_get_name (icon2)) != 0)
		return FALSE;
	return TRUE;
}

/**
 * as_app_check_bundle_duplicate:
 **/
static gboolean
as_app_check_bundle_duplicate (AsBundle *bundle1, AsBundle *bundle2)
{
	if (as_bundle_get_kind (bundle1) != as_bundle_get_kind (bundle2))
		return FALSE;
	if (g_strcmp0 (as_bundle_get_id (bundle1),
		       as_bundle_get_id (bundle2)) != 0)
		return FALSE;
	return TRUE;
}

/**
 * as_app_add_icon:
 * @app: a #AsApp instance.
 * @icon: a #AsIcon instance.
 *
 * Adds a icon to an application.
 *
 * Since: 0.3.1
 **/
void
as_app_add_icon (AsApp *app, AsIcon *icon)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0) {
		AsIcon *ic_tmp;
		guint i;
		for (i = 0; i < priv->icons->len; i++) {
			ic_tmp = g_ptr_array_index (priv->icons, i);
			if (as_app_check_icon_duplicate (icon, ic_tmp))
				return;
		}
	}

	/* assume that desktop stock icons are available in HiDPI sizes */
	if (as_icon_get_kind (icon) == AS_ICON_KIND_STOCK) {
		switch (priv->id_kind) {
		case AS_ID_KIND_DESKTOP:
			as_app_add_kudo_kind (app, AS_KUDO_KIND_HI_DPI_ICON);
			break;
		default:
			break;
		}
	}
	g_ptr_array_add (priv->icons, g_object_ref (icon));
}

/**
 * as_app_add_bundle:
 * @app: a #AsApp instance.
 * @bundle: a #AsBundle instance.
 *
 * Adds a bundle to an application.
 *
 * Since: 0.3.5
 **/
void
as_app_add_bundle (AsApp *app, AsBundle *bundle)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0) {
		AsBundle *bu_tmp;
		guint i;
		for (i = 0; i < priv->bundles->len; i++) {
			bu_tmp = g_ptr_array_index (priv->bundles, i);
			if (as_app_check_bundle_duplicate (bundle, bu_tmp))
				return;
		}
	}
	g_ptr_array_add (priv->bundles, g_object_ref (bundle));
}

/**
 * as_app_add_pkgname:
 * @app: a #AsApp instance.
 * @pkgname: the package name.
 *
 * Adds a package name to an application.
 *
 * Since: 0.1.0
 **/
void
as_app_add_pkgname (AsApp *app, const gchar *pkgname)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (pkgname)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0 &&
	    as_ptr_array_find_string (priv->pkgnames, pkgname)) {
		return;
	}

	g_ptr_array_add (priv->pkgnames, g_strdup (pkgname));
}

/**
 * as_app_add_arch:
 * @app: a #AsApp instance.
 * @arch: the package name.
 *
 * Adds a package name to an application.
 *
 * Since: 0.1.1
 **/
void
as_app_add_arch (AsApp *app, const gchar *arch)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (arch)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0 &&
	    as_ptr_array_find_string (priv->architectures, arch)) {
		return;
	}

	g_ptr_array_add (priv->architectures, g_strdup (arch));
}

/**
 * as_app_add_language:
 * @app: a #AsApp instance.
 * @percentage: the percentage completion of the translation, or 0 for unknown
 * @locale: the locale, or %NULL. e.g. "en_GB"
 *
 * Adds a language to the application.
 *
 * Since: 0.1.0
 **/
void
as_app_add_language (AsApp *app,
		     gint percentage,
		     const gchar *locale)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (locale)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}

	if (locale == NULL)
		locale = "C";
	g_hash_table_insert (priv->languages,
			     g_strdup (locale),
			     GINT_TO_POINTER (percentage));
}

/**
 * as_app_add_url:
 * @app: a #AsApp instance.
 * @url_kind: the URL kind, e.g. %AS_URL_KIND_HOMEPAGE
 * @url: the full URL.
 *
 * Adds some URL data to the application.
 *
 * Since: 0.1.0
 **/
void
as_app_add_url (AsApp *app,
		AsUrlKind url_kind,
		const gchar *url)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (url)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}

	g_hash_table_insert (priv->urls,
			     g_strdup (as_url_kind_to_string (url_kind)),
			     g_strdup (url));
}

/**
 * as_app_add_metadata:
 * @app: a #AsApp instance.
 * @key: the metadata key.
 * @value: the value to store.
 *
 * Adds a metadata entry to the application.
 *
 * Since: 0.1.0
 **/
void
as_app_add_metadata (AsApp *app,
		     const gchar *key,
		     const gchar *value)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_return_if_fail (key != NULL);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (value)) {
		return;
	}

	if (value == NULL)
		value = "";
	g_hash_table_insert (priv->metadata,
			     g_strdup (key),
			     g_strdup (value));
}

/**
 * as_app_remove_metadata:
 * @app: a #AsApp instance.
 * @key: the metadata key.
 *
 * Removes a metadata item from the application.
 *
 * Since: 0.1.0
 **/
void
as_app_remove_metadata (AsApp *app, const gchar *key)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_hash_table_remove (priv->metadata, key);
}

/**
 * as_app_add_extends:
 * @app: a #AsApp instance.
 * @extends: the full ID, e.g. "eclipse.desktop".
 *
 * Adds a parent ID to the application.
 *
 * Since: 0.1.7
 **/
void
as_app_add_extends (AsApp *app, const gchar *extends)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (extends)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0 &&
	    as_ptr_array_find_string (priv->extends, extends)) {
		return;
	}

	/* we can never extend ourself */
	if (g_strcmp0 (priv->id, extends) == 0)
		return;

	g_ptr_array_add (priv->extends, g_strdup (extends));
}

/**
 * as_app_add_addon:
 * @app: a #AsApp instance.
 * @addon: a #AsApp instance.
 *
 * Adds a addon to an application.
 *
 * Since: 0.1.7
 **/
void
as_app_add_addon (AsApp *app, AsApp *addon)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_ptr_array_add (priv->addons, g_object_ref (addon));
}

/******************************************************************************/


/**
 * as_app_subsume_dict:
 **/
static void
as_app_subsume_dict (GHashTable *dest, GHashTable *src, gboolean overwrite)
{
	GList *l;
	const gchar *tmp;
	const gchar *key;
	const gchar *value;
	g_autoptr(GList) keys = NULL;

	keys = g_hash_table_get_keys (src);
	for (l = keys; l != NULL; l = l->next) {
		key = l->data;
		if (!overwrite) {
			tmp = g_hash_table_lookup (dest, key);
			if (tmp != NULL)
				continue;
		}
		value = g_hash_table_lookup (src, key);
		g_hash_table_insert (dest, g_strdup (key), g_strdup (value));
	}
}

/**
 * as_app_subsume_keywords:
 **/
static void
as_app_subsume_keywords (AsApp *app, AsApp *donor, gboolean overwrite)
{
	AsAppPrivate *priv = GET_PRIVATE (donor);
	GList *l;
	GPtrArray *array;
	const gchar *key;
	const gchar *tmp;
	guint i;
	g_autoptr(GList) keys = NULL;

	/* get all locales in the keywords dict */
	keys = g_hash_table_get_keys (priv->keywords);
	for (l = keys; l != NULL; l = l->next) {
		key = l->data;
		if (!overwrite) {
			array = as_app_get_keywords (app, key);
			if (array != NULL)
				continue;
		}

		/* get the array of keywords for the specific locale */
		array = g_hash_table_lookup (priv->keywords, key);
		if (array == NULL)
			continue;
		for (i = 0; i < array->len; i++) {
			tmp = g_ptr_array_index (array, i);
			as_app_add_keyword (app, key, tmp);
		}
	}
}

/**
 * as_app_subsume_icon:
 **/
static void
as_app_subsume_icon (AsApp *app, AsIcon *icon)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsIcon *ic_tmp;
	guint i;

	/* don't add a rubbish icon */
	if (as_icon_get_kind (icon) == AS_ICON_KIND_UNKNOWN)
		return;

	/* does application already have this icon in this size */
	for (i = 0; i < priv->icons->len; i++) {
		ic_tmp = AS_ICON (g_ptr_array_index (priv->icons, i));
		if (as_icon_get_height (ic_tmp) != as_icon_get_height (icon))
			continue;
		if (as_icon_get_width (ic_tmp) != as_icon_get_width (icon))
			continue;
		if (g_strcmp0 (as_icon_get_name (ic_tmp), as_icon_get_name (icon)) != 0)
			continue;
		return;
	}

	as_app_add_icon (app, icon);
}

/**
 * as_app_subsume_private:
 **/
static void
as_app_subsume_private (AsApp *app, AsApp *donor, AsAppSubsumeFlags flags)
{
	AsAppPrivate *priv = GET_PRIVATE (donor);
	AsAppPrivate *papp = GET_PRIVATE (app);
	AsBundle *bundle;
	AsScreenshot *ss;
	AsProvide *pr;
	const gchar *tmp;
	const gchar *key;
	gboolean overwrite;
	guint i;
	gint percentage;
	GList *l;
	g_autoptr(GList) keys = NULL;

	/* stop us shooting ourselves in the foot */
	papp->trust_flags |= AS_APP_TRUST_FLAG_CHECK_DUPLICATES;

	overwrite = (flags & AS_APP_SUBSUME_FLAG_NO_OVERWRITE) == 0;

	/* id-kind */
	if (papp->id_kind == AS_ID_KIND_UNKNOWN)
		as_app_set_id_kind (app, priv->id_kind);

	/* AppData or AppStream can overwrite the id-kind of desktop files */
	if ((priv->source_kind == AS_APP_SOURCE_KIND_APPDATA ||
	     priv->source_kind == AS_APP_SOURCE_KIND_APPSTREAM) &&
	    papp->source_kind == AS_APP_SOURCE_KIND_DESKTOP)
		as_app_set_id_kind (app, priv->id_kind);

	/* state */
	if (papp->state == AS_APP_STATE_UNKNOWN)
		as_app_set_state (app, priv->state);

	/* pkgnames */
	for (i = 0; i < priv->pkgnames->len; i++) {
		tmp = g_ptr_array_index (priv->pkgnames, i);
		as_app_add_pkgname (app, tmp);
	}

	/* bundles */
	for (i = 0; i < priv->bundles->len; i++) {
		bundle = g_ptr_array_index (priv->bundles, i);
		as_app_add_bundle (app, bundle);
	}

	/* releases */
	for (i = 0; i < priv->releases->len; i++) {
		AsRelease *rel= g_ptr_array_index (priv->releases, i);
		as_app_add_release (app, rel);
	}

	/* kudos */
	for (i = 0; i < priv->kudos->len; i++) {
		tmp = g_ptr_array_index (priv->kudos, i);
		as_app_add_kudo (app, tmp);
	}

	/* categories */
	for (i = 0; i < priv->categories->len; i++) {
		tmp = g_ptr_array_index (priv->categories, i);
		as_app_add_category (app, tmp);
	}

	/* permissions */
	for (i = 0; i < priv->permissions->len; i++) {
		tmp = g_ptr_array_index (priv->permissions, i);
		as_app_add_permission (app, tmp);
	}

	/* extends */
	for (i = 0; i < priv->extends->len; i++) {
		tmp = g_ptr_array_index (priv->extends, i);
		as_app_add_extends (app, tmp);
	}

	/* compulsory_for_desktops */
	for (i = 0; i < priv->compulsory_for_desktops->len; i++) {
		tmp = g_ptr_array_index (priv->compulsory_for_desktops, i);
		as_app_add_compulsory_for_desktop (app, tmp);
	}

	/* screenshots */
	for (i = 0; i < priv->screenshots->len; i++) {
		ss = g_ptr_array_index (priv->screenshots, i);
		as_app_add_screenshot (app, ss);
	}

	/* provides */
	for (i = 0; i < priv->provides->len; i++) {
		pr = g_ptr_array_index (priv->provides, i);
		as_app_add_provide (app, pr);
	}

	/* icons */
	for (i = 0; i < priv->icons->len; i++) {
		AsIcon *ic = g_ptr_array_index (priv->icons, i);
		as_app_subsume_icon (app, ic);
	}

	/* mimetypes */
	for (i = 0; i < priv->mimetypes->len; i++) {
		tmp = g_ptr_array_index (priv->mimetypes, i);
		as_app_add_mimetype (app, tmp);
	}

	/* do not subsume all properties */
	if ((flags & AS_APP_SUBSUME_FLAG_PARTIAL) > 0)
		return;

	/* vetos */
	for (i = 0; i < priv->vetos->len; i++) {
		tmp = g_ptr_array_index (priv->vetos, i);
		as_app_add_veto (app, "%s", tmp);
	}

	/* languages */
	keys = g_hash_table_get_keys (priv->languages);
	for (l = keys; l != NULL; l = l->next) {
		key = l->data;
		if (flags & AS_APP_SUBSUME_FLAG_NO_OVERWRITE) {
			percentage = as_app_get_language (app, key);
			if (percentage >= 0)
				continue;
		}
		percentage = GPOINTER_TO_INT (g_hash_table_lookup (priv->languages, key));
		as_app_add_language (app, percentage, key);
	}

	/* dictionaries */
	as_app_subsume_dict (papp->names, priv->names, overwrite);
	as_app_subsume_dict (papp->comments, priv->comments, overwrite);
	as_app_subsume_dict (papp->developer_names, priv->developer_names, overwrite);
	as_app_subsume_dict (papp->descriptions, priv->descriptions, overwrite);
	as_app_subsume_dict (papp->metadata, priv->metadata, overwrite);
	as_app_subsume_dict (papp->urls, priv->urls, overwrite);
	as_app_subsume_keywords (app, donor, overwrite);

	/* source */
	if (priv->source_file != NULL)
		as_app_set_source_file (app, priv->source_file);

	/* source_pkgname */
	if (priv->source_pkgname != NULL)
		as_app_set_source_pkgname (app, priv->source_pkgname);

	/* origin */
	if (priv->origin != NULL)
		as_app_set_origin (app, priv->origin);

	/* licenses */
	if (priv->project_license != NULL)
		as_app_set_project_license (app, priv->project_license);
	if (priv->metadata_license != NULL)
		as_app_set_metadata_license (app, priv->metadata_license);

	/* project_group */
	if (priv->project_group != NULL)
		as_app_set_project_group (app, priv->project_group);
}

/**
 * as_app_subsume_full:
 * @app: a #AsApp instance.
 * @donor: the donor.
 * @flags: any optional flags, e.g. %AS_APP_SUBSUME_FLAG_NO_OVERWRITE
 *
 * Copies information from the donor to the application object.
 *
 * Since: 0.1.4
 **/
void
as_app_subsume_full (AsApp *app, AsApp *donor, AsAppSubsumeFlags flags)
{
	g_assert (app != donor);

	/* two way sync implies no overwriting */
	if ((flags & AS_APP_SUBSUME_FLAG_BOTH_WAYS) > 0)
		flags |= AS_APP_SUBSUME_FLAG_NO_OVERWRITE;

	/* one way sync */
	as_app_subsume_private (app, donor, flags);

	/* and back again */
	if ((flags & AS_APP_SUBSUME_FLAG_BOTH_WAYS) > 0)
		as_app_subsume_private (donor, app, flags);
}

/**
 * as_app_subsume:
 * @app: a #AsApp instance.
 * @donor: the donor.
 *
 * Copies information from the donor to the application object.
 *
 * Since: 0.1.0
 **/
void
as_app_subsume (AsApp *app, AsApp *donor)
{
	as_app_subsume_full (app, donor, AS_APP_SUBSUME_FLAG_NONE);
}

/**
 * gs_app_node_language_sort_cb:
 **/
static gint
gs_app_node_language_sort_cb (gconstpointer a, gconstpointer b)
{
	return g_strcmp0 ((const gchar *) a, (const gchar *) b);
}

/**
 * as_app_node_insert_languages:
 **/
static void
as_app_node_insert_languages (AsApp *app, GNode *parent)
{
	GNode *node_tmp;
	GList *l;
	const gchar *locale;
	gchar tmp[4];
	gint percentage;
	g_autoptr(GList) langs = NULL;

	node_tmp = as_node_insert (parent, "languages", NULL, 0, NULL);
	langs = as_app_get_languages (app);
	langs = g_list_sort (langs, gs_app_node_language_sort_cb);
	for (l = langs; l != NULL; l = l->next) {
		locale = l->data;
		percentage = as_app_get_language (app, locale);
		if (percentage == 0) {
			as_node_insert (node_tmp, "lang", locale, 0, NULL);
		} else {
			g_snprintf (tmp, sizeof (tmp), "%i", percentage);
			as_node_insert (node_tmp, "lang", locale, 0,
					"percentage", tmp,
					NULL);
		}
	}
}

/**
 * as_app_ptr_array_sort_cb:
 **/
static gint
as_app_ptr_array_sort_cb (gconstpointer a, gconstpointer b)
{
	return g_strcmp0 (*((const gchar **) a), *((const gchar **) b));
}

/**
 * as_app_releases_sort_cb:
 **/
static gint
as_app_releases_sort_cb (gconstpointer a, gconstpointer b)
{
	AsRelease **rel1 = (AsRelease **) a;
	AsRelease **rel2 = (AsRelease **) b;
	return as_release_vercmp (*rel1, *rel2);
}

/**
 * as_app_icons_sort_cb:
 **/
static gint
as_app_icons_sort_cb (gconstpointer a, gconstpointer b)
{
	AsIcon **ic1 = (AsIcon **) a;
	AsIcon **ic2 = (AsIcon **) b;
	return g_strcmp0 (as_icon_get_name (*ic1), as_icon_get_name (*ic2));
}

/**
 * as_app_list_sort_cb:
 **/
static gint
as_app_list_sort_cb (gconstpointer a, gconstpointer b)
{
	return g_strcmp0 ((const gchar *) a, (const gchar *) b);
}

/**
 * as_app_node_insert_keywords:
 **/
static void
as_app_node_insert_keywords (AsApp *app, GNode *parent, AsNodeContext *ctx)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	GList *keys;
	GList *l;
	GNode *node_tmp;
	GPtrArray *keywords;
	const gchar *lang;
	const gchar *tmp;
	guint i;
	g_autoptr(GHashTable) already_in_c = NULL;

	/* don't add localized keywords that already exist in C, e.g.
	 * there's no point adding "c++" in 14 different languages */
	already_in_c = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, NULL);
	keywords = g_hash_table_lookup (priv->keywords, "C");
	if (keywords != NULL) {
		for (i = 0; i < keywords->len; i++) {
			tmp = g_ptr_array_index (keywords, i);
			g_hash_table_insert (already_in_c,
					     g_strdup (tmp),
					     GINT_TO_POINTER (1));
		}
	}

	keys = g_hash_table_get_keys (priv->keywords);
	keys = g_list_sort (keys, as_app_list_sort_cb);
	for (l = keys; l != NULL; l = l->next) {
		lang = l->data;
		keywords = g_hash_table_lookup (priv->keywords, lang);
		g_ptr_array_sort (keywords, as_app_ptr_array_sort_cb);
		for (i = 0; i < keywords->len; i++) {
			tmp = g_ptr_array_index (keywords, i);
			if (tmp == NULL)
				continue;
			if (g_strcmp0 (lang, "C") != 0 &&
			    g_hash_table_lookup (already_in_c, tmp) != NULL)
				continue;
			node_tmp = as_node_insert (parent,
						   "keyword", tmp,
						   0, NULL);
			if (g_strcmp0 (lang, "C") != 0) {
				as_node_add_attribute (node_tmp,
						       "xml:lang",
						       lang);
			}
		}
	}
	g_list_free (keys);
}

/**
 * as_app_node_insert: (skip)
 * @app: a #AsApp instance.
 * @parent: the parent #GNode to use..
 * @ctx: the #AsNodeContext
 *
 * Inserts the application into the DOM tree.
 *
 * Returns: (transfer none): A populated #GNode, or %NULL
 *
 * Since: 0.1.0
 **/
GNode *
as_app_node_insert (AsApp *app, GNode *parent, AsNodeContext *ctx)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsRelease *rel;
	AsScreenshot *ss;
	GNode *node_app;
	GNode *node_tmp;
	const gchar *tmp;
	guint i;

	/* <component> or <application> */
	node_app = as_node_insert (parent, "component", NULL, 0, NULL);
	if (priv->id_kind != AS_ID_KIND_UNKNOWN) {
		as_node_add_attribute (node_app,
				       "type",
				       as_id_kind_to_string (priv->id_kind));
	}

	/* <id> */
	as_node_insert (node_app, "id", priv->id, 0, NULL);

	/* <priority> */
	if (priv->priority != 0)
		as_node_add_attribute_as_int (node_app, "priority", priv->priority);

	/* <pkgname> */
	g_ptr_array_sort (priv->pkgnames, as_app_ptr_array_sort_cb);
	for (i = 0; i < priv->pkgnames->len; i++) {
		tmp = g_ptr_array_index (priv->pkgnames, i);
		as_node_insert (node_app, "pkgname", tmp, 0, NULL);
	}

	/* <source_pkgname> */
	if (priv->source_pkgname != NULL) {
		as_node_insert (node_app, "source_pkgname",
				priv->source_pkgname, 0, NULL);
	}

	/* <bundle> */
	for (i = 0; i < priv->bundles->len; i++) {
		AsBundle *bu = g_ptr_array_index (priv->bundles, i);
		as_bundle_node_insert (bu, node_app, ctx);
	}

	/* <name> */
	as_node_insert_localized (node_app, "name",
				  priv->names,
				  AS_NODE_INSERT_FLAG_DEDUPE_LANG);

	/* <summary> */
	as_node_insert_localized (node_app, "summary",
				  priv->comments,
				  AS_NODE_INSERT_FLAG_DEDUPE_LANG);

	/* <developer_name> */
	as_node_insert_localized (node_app, "developer_name",
				  priv->developer_names,
				  AS_NODE_INSERT_FLAG_DEDUPE_LANG);

	/* <description> */
	as_node_insert_localized (node_app, "description",
				  priv->descriptions,
				  AS_NODE_INSERT_FLAG_PRE_ESCAPED |
				  AS_NODE_INSERT_FLAG_DEDUPE_LANG);

	/* <icon> */
	g_ptr_array_sort (priv->icons, as_app_icons_sort_cb);
	for (i = 0; i < priv->icons->len; i++) {
		AsIcon *ic = g_ptr_array_index (priv->icons, i);
		as_icon_node_insert (ic, node_app, ctx);
	}

	/* <categories> */
	if (priv->categories->len > 0) {
		g_ptr_array_sort (priv->categories, as_app_ptr_array_sort_cb);
		node_tmp = as_node_insert (node_app, "categories", NULL, 0, NULL);
		for (i = 0; i < priv->categories->len; i++) {
			tmp = g_ptr_array_index (priv->categories, i);
			as_node_insert (node_tmp, "category", tmp, 0, NULL);
		}
	}

	/* <architectures> */
	if (priv->architectures->len > 0) {
		g_ptr_array_sort (priv->architectures, as_app_ptr_array_sort_cb);
		node_tmp = as_node_insert (node_app, "architectures", NULL, 0, NULL);
		for (i = 0; i < priv->architectures->len; i++) {
			tmp = g_ptr_array_index (priv->architectures, i);
			as_node_insert (node_tmp, "arch", tmp, 0, NULL);
		}
	}

	/* <keywords> */
	if (g_hash_table_size (priv->keywords) > 0) {
		node_tmp = as_node_insert (node_app, "keywords", NULL, 0, NULL);
		as_app_node_insert_keywords (app, node_tmp, ctx);
	}

	/* <kudos> */
	if (priv->kudos->len > 0) {
		g_ptr_array_sort (priv->kudos, as_app_ptr_array_sort_cb);
		node_tmp = as_node_insert (node_app, "kudos", NULL, 0, NULL);
		for (i = 0; i < priv->kudos->len; i++) {
			tmp = g_ptr_array_index (priv->kudos, i);
			as_node_insert (node_tmp, "kudo", tmp, 0, NULL);
		}
	}

	/* <permissions> */
	if (priv->permissions->len > 0) {
		g_ptr_array_sort (priv->permissions, as_app_ptr_array_sort_cb);
		node_tmp = as_node_insert (node_app, "permissions", NULL, 0, NULL);
		for (i = 0; i < priv->permissions->len; i++) {
			tmp = g_ptr_array_index (priv->permissions, i);
			as_node_insert (node_tmp, "permission", tmp, 0, NULL);
		}
	}

	/* <vetos> */
	if (priv->vetos->len > 0) {
		g_ptr_array_sort (priv->vetos, as_app_ptr_array_sort_cb);
		node_tmp = as_node_insert (node_app, "vetos", NULL, 0, NULL);
		for (i = 0; i < priv->vetos->len; i++) {
			tmp = g_ptr_array_index (priv->vetos, i);
			as_node_insert (node_tmp, "veto", tmp, 0, NULL);
		}
	}

	/* <mimetypes> */
	if (priv->mimetypes->len > 0) {
		g_ptr_array_sort (priv->mimetypes, as_app_ptr_array_sort_cb);
		node_tmp = as_node_insert (node_app, "mimetypes", NULL, 0, NULL);
		for (i = 0; i < priv->mimetypes->len; i++) {
			tmp = g_ptr_array_index (priv->mimetypes, i);
			as_node_insert (node_tmp, "mimetype", tmp, 0, NULL);
		}
	}

	/* <metadata_license> */
	if (as_node_context_get_output (ctx) == AS_APP_SOURCE_KIND_APPDATA ||
	    as_node_context_get_output (ctx) == AS_APP_SOURCE_KIND_METAINFO) {
		if (priv->metadata_license != NULL) {
			as_node_insert (node_app, "metadata_license",
					priv->metadata_license, 0, NULL);
		}
	}

	/* <project_license> */
	if (priv->project_license != NULL) {
		as_node_insert (node_app, "project_license",
				priv->project_license, 0, NULL);
	}

	/* <url> */
	as_node_insert_hash (node_app, "url", "type", priv->urls, 0);

	/* <project_group> */
	if (priv->project_group != NULL) {
		as_node_insert (node_app, "project_group",
				priv->project_group, 0, NULL);
	}

	/* <compulsory_for_desktop> */
	if (priv->compulsory_for_desktops != NULL) {
		g_ptr_array_sort (priv->compulsory_for_desktops,
				  as_app_ptr_array_sort_cb);
		for (i = 0; i < priv->compulsory_for_desktops->len; i++) {
			tmp = g_ptr_array_index (priv->compulsory_for_desktops, i);
			as_node_insert (node_app, "compulsory_for_desktop",
					tmp, 0, NULL);
		}
	}

	/* <extends> */
	if (priv->extends->len > 0) {
		g_ptr_array_sort (priv->extends, as_app_ptr_array_sort_cb);
		for (i = 0; i < priv->extends->len; i++) {
			tmp = g_ptr_array_index (priv->extends, i);
			as_node_insert (node_app, "extends", tmp, 0, NULL);
		}
	}

	/* <screenshots> */
	if (priv->screenshots->len > 0) {
		node_tmp = as_node_insert (node_app, "screenshots", NULL, 0, NULL);
		for (i = 0; i < priv->screenshots->len; i++) {
			ss = g_ptr_array_index (priv->screenshots, i);
			as_screenshot_node_insert (ss, node_tmp, ctx);
		}
	}

	/* <releases> */
	if (priv->releases->len > 0) {
		g_ptr_array_sort (priv->releases, as_app_releases_sort_cb);
		node_tmp = as_node_insert (node_app, "releases", NULL, 0, NULL);
		for (i = 0; i < priv->releases->len && i < 3; i++) {
			rel = g_ptr_array_index (priv->releases, i);
			as_release_node_insert (rel, node_tmp, ctx);
		}
	}

	/* <provides> */
	if (priv->provides->len > 0) {
		AsProvide *provide;
		node_tmp = as_node_insert (node_app, "provides", NULL, 0, NULL);
		for (i = 0; i < priv->provides->len; i++) {
			provide = g_ptr_array_index (priv->provides, i);
			as_provide_node_insert (provide, node_tmp, ctx);
		}
	}

	/* <languages> */
	if (g_hash_table_size (priv->languages) > 0)
		as_app_node_insert_languages (app, node_app);

	/* <update_contact> */
	if (as_node_context_get_output (ctx) == AS_APP_SOURCE_KIND_APPDATA ||
	    as_node_context_get_output (ctx) == AS_APP_SOURCE_KIND_METAINFO ||
	    as_node_context_get_output_trusted (ctx)) {
		if (priv->update_contact != NULL) {
			as_node_insert (node_app, "update_contact",
					priv->update_contact, 0, NULL);
		}
	}

	/* <metadata> */
	if (g_hash_table_size (priv->metadata) > 0) {
		node_tmp = as_node_insert (node_app, "metadata", NULL, 0, NULL);
		as_node_insert_hash (node_tmp, "value", "key", priv->metadata, FALSE);
	}

	return node_app;
}

/**
 * as_app_node_parse_child:
 **/
static gboolean
as_app_node_parse_child (AsApp *app, GNode *n, AsAppParseFlags flags,
			 AsNodeContext *ctx, GError **error)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	GNode *c;
	const gchar *tmp;
	gchar *taken;

	switch (as_node_get_tag (n)) {

	/* <id> */
	case AS_TAG_ID:
		if (as_node_get_attribute (n, "xml:lang") != NULL) {
			priv->problems |= AS_APP_PROBLEM_TRANSLATED_ID;
			break;
		}
		tmp = as_node_get_attribute (n, "type");
		if (tmp != NULL)
			as_app_set_id_kind (app, as_id_kind_from_string (tmp));
		as_app_set_id (app, as_node_get_data (n));
		break;

	/* <priority> */
	case AS_TAG_PRIORITY:
		as_app_set_priority (app, g_ascii_strtoll (as_node_get_data (n),
							   NULL, 10));
		break;

	/* <pkgname> */
	case AS_TAG_PKGNAME:
		g_ptr_array_add (priv->pkgnames, as_node_take_data (n));
		break;

	/* <bundle> */
	case AS_TAG_BUNDLE:
	{
		g_autoptr(AsBundle) ic = NULL;
		ic = as_bundle_new ();
		if (!as_bundle_node_parse (ic, n, ctx, error))
			return FALSE;
		as_app_add_bundle (app, ic);
		break;
	}

	/* <name> */
	case AS_TAG_NAME:
		taken = as_node_fix_locale (as_node_get_attribute (n, "xml:lang"));
		if (taken == NULL)
			break;
		g_hash_table_insert (priv->names,
				     taken,
				     as_node_take_data (n));
		break;

	/* <summary> */
	case AS_TAG_SUMMARY:
		taken = as_node_fix_locale (as_node_get_attribute (n, "xml:lang"));
		if (taken == NULL)
			break;
		g_hash_table_insert (priv->comments,
				     taken,
				     as_node_take_data (n));
		break;

	/* <developer_name> */
	case AS_TAG_DEVELOPER_NAME:
		taken = as_node_fix_locale (as_node_get_attribute (n, "xml:lang"));
		if (taken == NULL)
			break;
		g_hash_table_insert (priv->developer_names,
				     taken,
				     as_node_take_data (n));
		break;

	/* <description> */
	case AS_TAG_DESCRIPTION:

		/* unwrap appdata inline */
		if (priv->source_kind == AS_APP_SOURCE_KIND_APPDATA) {
			GError *error_local = NULL;
			g_autoptr(GHashTable) unwrapped = NULL;
			unwrapped = as_node_get_localized_unwrap (n, &error_local);
			if (unwrapped == NULL) {
				if (g_error_matches (error_local,
						     AS_NODE_ERROR,
						     AS_NODE_ERROR_INVALID_MARKUP)) {
					g_autoptr(GString) debug = NULL;
					debug = as_node_to_xml (n, AS_NODE_TO_XML_FLAG_NONE);
					g_warning ("ignoring description '%s' from %s: %s",
						   debug->str,
						   as_app_get_source_file (app),
						   error_local->message);
					g_error_free (error_local);
					break;
				}
				g_propagate_error (error, error_local);
				return FALSE;
			}
			as_app_subsume_dict (priv->descriptions, unwrapped, FALSE);
			break;
		}

		if (n->children == NULL) {
			/* pre-formatted */
			priv->problems |= AS_APP_PROBLEM_PREFORMATTED_DESCRIPTION;
			as_app_set_description (app,
						as_node_get_attribute (n, "xml:lang"),
						as_node_get_data (n));
		} else {
			g_autoptr(GString) xml = NULL;
			xml = as_node_to_xml (n->children,
					      AS_NODE_TO_XML_FLAG_INCLUDE_SIBLINGS);
			as_app_set_description (app,
						as_node_get_attribute (n, "xml:lang"),
						xml->str);
		}
		break;

	/* <icon> */
	case AS_TAG_ICON:
	{
		g_autoptr(AsIcon) ic = NULL;
		ic = as_icon_new ();
		as_icon_set_prefix (ic, priv->icon_path);
		if (!as_icon_node_parse (ic, n, ctx, error))
			return FALSE;
		as_app_add_icon (app, ic);
		break;
	}

	/* <categories> */
	case AS_TAG_CATEGORIES:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_ptr_array_set_size (priv->categories, 0);
		for (c = n->children; c != NULL; c = c->next) {
			if (as_node_get_tag (c) != AS_TAG_CATEGORY)
				continue;
			taken = as_node_take_data (c);
			if (taken == NULL)
				continue;
			g_ptr_array_add (priv->categories, taken);
		}
		break;

	/* <architectures> */
	case AS_TAG_ARCHITECTURES:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_ptr_array_set_size (priv->architectures, 0);
		for (c = n->children; c != NULL; c = c->next) {
			if (as_node_get_tag (c) != AS_TAG_ARCH)
				continue;
			taken = as_node_take_data (c);
			if (taken == NULL)
				continue;
			g_ptr_array_add (priv->architectures, taken);
		}
		break;

	/* <keywords> */
	case AS_TAG_KEYWORDS:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_hash_table_remove_all (priv->keywords);
		for (c = n->children; c != NULL; c = c->next) {
			if (as_node_get_tag (c) != AS_TAG_KEYWORD)
				continue;
			tmp = as_node_get_data (c);
			if (tmp == NULL)
				continue;
			taken = as_node_fix_locale (as_node_get_attribute (c, "xml:lang"));
			if (taken == NULL)
				continue;
			as_app_add_keyword (app, taken, tmp);
			g_free (taken);
		}
		break;

	/* <kudos> */
	case AS_TAG_KUDOS:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_ptr_array_set_size (priv->kudos, 0);
		for (c = n->children; c != NULL; c = c->next) {
			if (as_node_get_tag (c) != AS_TAG_KUDO)
				continue;
			taken = as_node_take_data (c);
			if (taken == NULL)
				continue;
			g_ptr_array_add (priv->kudos, taken);
		}
		break;

	/* <permissions> */
	case AS_TAG_PERMISSIONS:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_ptr_array_set_size (priv->permissions, 0);
		for (c = n->children; c != NULL; c = c->next) {
			if (as_node_get_tag (c) != AS_TAG_PERMISSION)
				continue;
			taken = as_node_take_data (c);
			if (taken == NULL)
				continue;
			g_ptr_array_add (priv->permissions, taken);
		}
		break;

	/* <vetos> */
	case AS_TAG_VETOS:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_ptr_array_set_size (priv->vetos, 0);
		for (c = n->children; c != NULL; c = c->next) {
			if (as_node_get_tag (c) != AS_TAG_VETO)
				continue;
			taken = as_node_take_data (c);
			if (taken == NULL)
				continue;
			g_ptr_array_add (priv->vetos, taken);
		}
		break;

	/* <mimetypes> */
	case AS_TAG_MIMETYPES:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_ptr_array_set_size (priv->mimetypes, 0);
		for (c = n->children; c != NULL; c = c->next) {
			if (as_node_get_tag (c) != AS_TAG_MIMETYPE)
				continue;
			taken = as_node_take_data (c);
			if (taken == NULL)
				continue;
			g_ptr_array_add (priv->mimetypes, taken);
		}
		break;

	/* <project_license> */
	case AS_TAG_PROJECT_LICENSE:
		if (as_node_get_attribute (n, "xml:lang") != NULL) {
			priv->problems |= AS_APP_PROBLEM_TRANSLATED_LICENSE;
			break;
		}
		g_free (priv->project_license);
		priv->project_license = as_node_take_data (n);
		break;

	/* <project_license> */
	case AS_TAG_METADATA_LICENSE:
		if (as_node_get_attribute (n, "xml:lang") != NULL) {
			priv->problems |= AS_APP_PROBLEM_TRANSLATED_LICENSE;
			break;
		}
		as_app_set_metadata_license (app, as_node_get_data (n));
		break;

	/* <source_pkgname> */
	case AS_TAG_SOURCE_PKGNAME:
		as_app_set_source_pkgname (app, as_node_get_data (n));
		break;

	/* <update_contact> */
	case AS_TAG_UPDATE_CONTACT:

		/* this is the old name */
		if (g_strcmp0 (as_node_get_name (n), "updatecontact") == 0)
			priv->problems |= AS_APP_PROBLEM_UPDATECONTACT_FALLBACK;

		as_app_set_update_contact (app, as_node_get_data (n));
		break;

	/* <url> */
	case AS_TAG_URL:
		tmp = as_node_get_attribute (n, "type");
		as_app_add_url (app,
				as_url_kind_from_string (tmp),
				as_node_get_data (n));
		break;

	/* <project_group> */
	case AS_TAG_PROJECT_GROUP:
		if (as_node_get_attribute (n, "xml:lang") != NULL) {
			priv->problems |= AS_APP_PROBLEM_TRANSLATED_PROJECT_GROUP;
			break;
		}
		g_free (priv->project_group);
		priv->project_group = as_node_take_data (n);
		break;

	/* <compulsory_for_desktop> */
	case AS_TAG_COMPULSORY_FOR_DESKTOP:
		g_ptr_array_add (priv->compulsory_for_desktops,
				 as_node_take_data (n));
		break;

	/* <extends> */
	case AS_TAG_EXTENDS:
		g_ptr_array_add (priv->extends, as_node_take_data (n));
		break;

	/* <screenshots> */
	case AS_TAG_SCREENSHOTS:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_ptr_array_set_size (priv->screenshots, 0);
		for (c = n->children; c != NULL; c = c->next) {
			g_autoptr(AsScreenshot) ss = NULL;
			if (as_node_get_tag (c) != AS_TAG_SCREENSHOT)
				continue;
			/* we don't yet support localised screenshots */
			if (as_node_get_attribute (c, "xml:lang") != NULL)
				continue;
			ss = as_screenshot_new ();
			if (!as_screenshot_node_parse (ss, c, ctx, error))
				return FALSE;
			as_app_add_screenshot (app, ss);
		}
		break;

	/* <releases> */
	case AS_TAG_RELEASES:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_ptr_array_set_size (priv->releases, 0);
		for (c = n->children; c != NULL; c = c->next) {
			g_autoptr(AsRelease) r = NULL;
			if (as_node_get_tag (c) != AS_TAG_RELEASE)
				continue;
			r = as_release_new ();
			if (!as_release_node_parse (r, c, ctx, error))
				return FALSE;
			as_app_add_release (app, r);
		}
		break;

	/* <provides> */
	case AS_TAG_PROVIDES:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_ptr_array_set_size (priv->provides, 0);
		for (c = n->children; c != NULL; c = c->next) {
			g_autoptr(AsProvide) p = NULL;
			p = as_provide_new ();
			if (!as_provide_node_parse (p, c, ctx, error))
				return FALSE;
			as_app_add_provide (app, p);
		}
		break;

	/* <languages> */
	case AS_TAG_LANGUAGES:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_hash_table_remove_all (priv->languages);
		for (c = n->children; c != NULL; c = c->next) {
			guint percent;
			if (as_node_get_tag (c) != AS_TAG_LANG)
				continue;
			percent = as_node_get_attribute_as_int (c, "percentage");
			if (percent == G_MAXINT)
				percent = 0;
			as_app_add_language (app, percent,
					     as_node_get_data (c));
		}
		break;

	/* <metadata> */
	case AS_TAG_METADATA:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_hash_table_remove_all (priv->metadata);
		for (c = n->children; c != NULL; c = c->next) {
			gchar *key;
			if (as_node_get_tag (c) != AS_TAG_VALUE)
				continue;
			key = as_node_take_attribute (c, "key");
			taken = as_node_take_data (c);
			if (taken == NULL)
				taken = g_strdup ("");
			g_hash_table_insert (priv->metadata, key, taken);
		}
		break;
	default:
		break;
	}
	return TRUE;
}

/**
 * as_app_check_for_hidpi_icons:
 **/
static void
as_app_check_for_hidpi_icons (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsIcon *icon_tmp;
	g_autofree gchar *fn_size = NULL;
	g_autoptr(AsIcon) icon_hidpi = NULL;

	/* does the file exist */
	icon_tmp = as_app_get_icon_default (app);
	fn_size = g_build_filename (priv->icon_path,
				    "128x128",
				    as_icon_get_name (icon_tmp),
				    NULL);
	if (!g_file_test (fn_size, G_FILE_TEST_EXISTS))
		return;

	/* create the HiDPI version */
	icon_hidpi = as_icon_new ();
	as_icon_set_prefix (icon_hidpi, priv->icon_path);
	as_icon_set_name (icon_hidpi, as_icon_get_name (icon_tmp));
	as_icon_set_width (icon_hidpi, 128);
	as_icon_set_height (icon_hidpi, 128);
	as_app_add_icon (app, icon_hidpi);
}

/**
 * as_app_node_parse_full:
 **/
static gboolean
as_app_node_parse_full (AsApp *app, GNode *node, AsAppParseFlags flags,
			AsNodeContext *ctx, GError **error)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	GNode *n;
	const gchar *tmp;
	guint prio;

	/* new style */
	if (g_strcmp0 (as_node_get_name (node), "component") == 0) {
		tmp = as_node_get_attribute (node, "type");
		if (tmp != NULL)
			as_app_set_id_kind (app, as_id_kind_from_string (tmp));
		prio = as_node_get_attribute_as_int (node, "priority");
		if (prio != G_MAXINT && prio != 0)
			as_app_set_priority (app, prio);
	}

	/* parse each node */
	if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA)) {
		g_ptr_array_set_size (priv->compulsory_for_desktops, 0);
		g_ptr_array_set_size (priv->pkgnames, 0);
		g_ptr_array_set_size (priv->architectures, 0);
		g_ptr_array_set_size (priv->extends, 0);
		g_ptr_array_set_size (priv->icons, 0);
		g_ptr_array_set_size (priv->bundles, 0);
		g_hash_table_remove_all (priv->keywords);
	}
	for (n = node->children; n != NULL; n = n->next) {
		if (!as_app_node_parse_child (app, n, flags, ctx, error))
			return FALSE;
	}

	/* if only one icon is listed, look for HiDPI versions too */
	if (as_app_get_icons(app)->len == 1)
		as_app_check_for_hidpi_icons (app);

	return TRUE;
}

/**
 * as_app_node_parse:
 * @app: a #AsApp instance.
 * @node: a #GNode.
 * @ctx: a #AsNodeContext.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DOM node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.0
 **/
gboolean
as_app_node_parse (AsApp *app, GNode *node, AsNodeContext *ctx, GError **error)
{
	return as_app_node_parse_full (app, node, AS_APP_PARSE_FLAG_NONE, ctx, error);
}

/**
 * as_app_node_parse_dep11_icons:
 **/
static gboolean
as_app_node_parse_dep11_icons (AsApp *app, GNode *node,
			       AsNodeContext *ctx, GError **error)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	const gchar *sizes[] = { "128x128", "64x64", "", NULL };
	guint i;
	guint size;
	g_autoptr(AsIcon) ic_tmp = NULL;

	/* YAML files only specify one icon for various sizes */
	ic_tmp = as_icon_new ();
	if (!as_icon_node_parse_dep11 (ic_tmp, node, ctx, error))
		return FALSE;

	/* find each size */
	for (i = 0; sizes[i] != NULL; i++) {
		g_autofree gchar *path = NULL;
		g_autofree gchar *size_name = NULL;
		g_autoptr(AsIcon) ic = NULL;

		size_name = g_build_filename (sizes[i],
					      as_icon_get_name (ic_tmp),
					      NULL);
		path = g_build_filename (priv->icon_path,
					 size_name,
					 NULL);
		if (!g_file_test (path, G_FILE_TEST_EXISTS))
			continue;

		/* only the first try is a HiDPI icon, assume 64px otherwise */
		size = (i == 0) ? 128 : 64;
		ic = as_icon_new ();
		as_icon_set_kind (ic, AS_ICON_KIND_CACHED);
		as_icon_set_prefix (ic, priv->icon_path);
		as_icon_set_name (ic, size_name);
		as_icon_set_width (ic, size);
		as_icon_set_height (ic, size);
		as_app_add_icon (app, ic);
	}
	return TRUE;
}

/**
 * as_app_node_parse_dep11:
 * @app: a #AsApp instance.
 * @node: a #GNode.
 * @ctx: a #AsNodeContext.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DEP-11 node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.3.0
 **/
gboolean
as_app_node_parse_dep11 (AsApp *app, GNode *node,
			 AsNodeContext *ctx, GError **error)
{
	GNode *c;
	GNode *c2;
	GNode *n;
	const gchar *tmp;

	for (n = node->children; n != NULL; n = n->next) {
		tmp = as_yaml_node_get_key (n);
		if (g_strcmp0 (tmp, "ID") == 0) {
			as_app_set_id (app, as_yaml_node_get_value (n));
			continue;
		}
		if (g_strcmp0 (tmp, "Type") == 0) {
			if (g_strcmp0 (as_yaml_node_get_value (n), "desktop-app") == 0) {
				as_app_set_id_kind (app, AS_ID_KIND_DESKTOP);
				continue;
			}
			continue;
		}
		if (g_strcmp0 (tmp, "Package") == 0) {
			as_app_add_pkgname (app, as_yaml_node_get_value (n));
			continue;
		}
		if (g_strcmp0 (tmp, "Name") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				as_app_set_name (app,
						 as_yaml_node_get_key (c),
						 as_yaml_node_get_value (c));
			}
			continue;
		}
		if (g_strcmp0 (tmp, "Summary") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				as_app_set_comment (app,
						    as_yaml_node_get_key (c),
						    as_yaml_node_get_value (c));
			}
			continue;
		}
		if (g_strcmp0 (tmp, "Description") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				as_app_set_description (app,
							as_yaml_node_get_key (c),
							as_yaml_node_get_value (c));
			}
			continue;
		}
		if (g_strcmp0 (tmp, "Keywords") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				for (c2 = c->children; c2 != NULL; c2 = c2->next) {
					if (as_yaml_node_get_key (c2) == NULL)
						continue;
					as_app_add_keyword (app,
							   as_yaml_node_get_key (c),
							   as_yaml_node_get_key (c2));
				}
			}
			continue;
		}
		if (g_strcmp0 (tmp, "Categories") == 0) {
			for (c = n->children; c != NULL; c = c->next)
				as_app_add_category (app, as_yaml_node_get_key (c));
			continue;
		}
		if (g_strcmp0 (tmp, "Icon") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				if (!as_app_node_parse_dep11_icons (app, c, ctx, error))
					return FALSE;
			}
			continue;
		}
		if (g_strcmp0 (tmp, "Bundle") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				g_autoptr(AsBundle) bu = NULL;
				bu = as_bundle_new ();
				if (!as_bundle_node_parse_dep11 (bu, c, ctx, error))
					return FALSE;
				as_app_add_bundle (app, bu);
			}
			continue;
		}
		if (g_strcmp0 (tmp, "Url") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				if (g_strcmp0 (as_yaml_node_get_key (c), "homepage") == 0) {
					as_app_add_url (app,
							AS_URL_KIND_HOMEPAGE,
							as_yaml_node_get_value (c));
					continue;
				}
			}
			continue;
		}
		if (g_strcmp0 (tmp, "Provides") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				if (g_strcmp0 (as_yaml_node_get_key (c), "mimetypes") == 0) {
					for (c2 = c->children; c2 != NULL; c2 = c2->next) {
						as_app_add_mimetype (app,
								     as_yaml_node_get_key (c2));
					}
					continue;
				} else {
					g_autoptr(AsProvide) pr = NULL;
					pr = as_provide_new ();
					if (!as_provide_node_parse_dep11 (pr, c, ctx, error))
						return FALSE;
					as_app_add_provide (app, pr);
				}
			}
			continue;
		}
		if (g_strcmp0 (tmp, "Screenshots") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				g_autoptr(AsScreenshot) ss = NULL;
				ss = as_screenshot_new ();
				if (!as_screenshot_node_parse_dep11 (ss, c, ctx, error))
					return FALSE;
				as_app_add_screenshot (app, ss);
			}
			continue;
		}
	}
	return TRUE;
}

/**
 * as_app_value_tokenize:
 **/
static gchar **
as_app_value_tokenize (const gchar *value)
{
	gchar **values;
	guint i;
	g_autofree gchar *delim = NULL;
	g_auto(GStrv) tmp = NULL;

	delim = g_strdup (value);
	g_strdelimit (delim, "/,.;:", ' ');
	tmp = g_strsplit (delim, " ", -1);
	values = g_new0 (gchar *, g_strv_length (tmp) + 1);
	for (i = 0; tmp[i] != NULL; i++)
		values[i] = g_utf8_strdown (tmp[i], -1);
	return values;
}

/**
 * as_app_remove_invalid_tokens:
 **/
static void
as_app_remove_invalid_tokens (gchar **tokens)
{
	guint i;
	guint idx = 0;
	guint len;

	if (tokens == NULL)
		return;

	/* remove any tokens that are invalid and maintain the order */
	len = g_strv_length (tokens);
	for (i = 0; i < len; i++) {
		if (!as_utils_search_token_valid (tokens[i])) {
			g_free (tokens[i]);
			tokens[i] = NULL;
			continue;
		}
		tokens[idx++] = tokens[i];
	}

	/* unused token space */
	for (i = idx; i < len; i++)
		tokens[i] = NULL;
}

/**
 * as_app_add_tokens:
 **/
static void
as_app_add_tokens (AsApp *app,
		   const gchar *value,
		   const gchar *locale,
		   guint score)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsAppTokenItem *token_item;

	/* sanity check */
	if (value == NULL) {
		g_critical ("trying to add NULL search token to %s",
			    as_app_get_id (app));
		return;
	}

	token_item = g_slice_new0 (AsAppTokenItem);
#if GLIB_CHECK_VERSION(2,39,1)
	if (g_strstr_len (value, -1, "+") == NULL &&
	    g_strstr_len (value, -1, "-") == NULL) {
		token_item->values_utf8 = g_str_tokenize_and_fold (value,
								   locale,
								   &token_item->values_ascii);
	}
#endif
	if (token_item->values_utf8 == NULL)
		token_item->values_utf8 = as_app_value_tokenize (value);

	/* remove any tokens that are invalid */
	as_app_remove_invalid_tokens (token_item->values_utf8);
	as_app_remove_invalid_tokens (token_item->values_ascii);

	token_item->score = score;
	g_ptr_array_add (priv->token_cache, token_item);
}

/**
 * as_app_create_token_cache_target:
 **/
static void
as_app_create_token_cache_target (AsApp *app, AsApp *donor)
{
	AsAppPrivate *priv = GET_PRIVATE (donor);
	GPtrArray *array;
	const gchar * const *locales;
	const gchar *tmp;
	guint i;
	guint j;

	/* add all the data we have */
	if (priv->id_filename != NULL)
		as_app_add_tokens (app, priv->id_filename, "C", 100);
	locales = g_get_language_names ();
	for (i = 0; locales[i] != NULL; i++) {
		if (g_str_has_suffix (locales[i], ".UTF-8"))
			continue;
		tmp = as_app_get_name (app, locales[i]);
		if (tmp != NULL)
			as_app_add_tokens (app, tmp, locales[i], 80);
		tmp = as_app_get_comment (app, locales[i]);
		if (tmp != NULL)
			as_app_add_tokens (app, tmp, locales[i], 60);
		tmp = as_app_get_description (app, locales[i]);
		if (tmp != NULL)
			as_app_add_tokens (app, tmp, locales[i], 20);
		array = as_app_get_keywords (app, locales[i]);
		if (array != NULL) {
			for (j = 0; j < array->len; j++) {
				tmp = g_ptr_array_index (array, j);
				as_app_add_tokens (app, tmp, locales[i], 90);
			}
		}
	}
	for (i = 0; i < priv->mimetypes->len; i++) {
		tmp = g_ptr_array_index (priv->mimetypes, i);
		as_app_add_tokens (app, tmp, "C", 1);
	}
	for (i = 0; i < priv->pkgnames->len; i++) {
		tmp = g_ptr_array_index (priv->pkgnames, i);
		as_app_add_tokens (app, tmp, "C", 1);
	}
}

/**
 * as_app_create_token_cache:
 **/
static void
as_app_create_token_cache (AsApp *app)
{
	AsApp *donor;
	AsAppPrivate *priv = GET_PRIVATE (app);
	guint i;

	as_app_create_token_cache_target (app, app);
	for (i = 0; i < priv->addons->len; i++) {
		donor = g_ptr_array_index (priv->addons, i);
		as_app_create_token_cache_target (app, donor);
	}
}

/**
 * as_app_search_matches:
 * @app: a #AsApp instance.
 * @search: the search term.
 *
 * Searches application data for a specific keyword.
 *
 * Returns: a match scrore, where 0 is no match and 100 is the best match.
 *
 * Since: 0.1.0
 **/
guint
as_app_search_matches (AsApp *app, const gchar *search)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsAppTokenItem *item;
	guint i, j;

	/* nothing to do */
	if (search == NULL)
		return 0;

	/* ensure the token cache is created */
	if (g_once_init_enter (&priv->token_cache_valid)) {
		as_app_create_token_cache (app);
		g_once_init_leave (&priv->token_cache_valid, TRUE);
	}

	/* find the search term */
	for (i = 0; i < priv->token_cache->len; i++) {
		item = g_ptr_array_index (priv->token_cache, i);

		/* prefer UTF-8 matches */
		if (item->values_utf8 != NULL) {
			for (j = 0; item->values_utf8[j] != NULL; j++) {
				if (g_str_has_prefix (item->values_utf8[j], search))
					return item->score;
			}
		}

		/* fall back to ASCII matches */
		if (item->values_ascii != NULL) {
			for (j = 0; item->values_ascii[j] != NULL; j++) {
				if (g_str_has_prefix (item->values_ascii[j], search))
					return item->score / 2;
			}
		}
	}
	return 0;
}

/**
 * as_app_get_search_tokens:
 * @app: a #AsApp instance.
 *
 * Returns all the search tokens for the application. These are unsorted.
 *
 * Returns: (transfer full): The string search tokens
 *
 * Since: 0.3.4
 */
GPtrArray *
as_app_get_search_tokens (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsAppTokenItem *item;
	GPtrArray *array;
	guint i, j;

	/* ensure the token cache is created */
	if (g_once_init_enter (&priv->token_cache_valid)) {
		as_app_create_token_cache (app);
		g_once_init_leave (&priv->token_cache_valid, TRUE);
	}

	/* return all the toek cache */
	array = g_ptr_array_new_with_free_func (g_free);
	for (i = 0; i < priv->token_cache->len; i++) {
		item = g_ptr_array_index (priv->token_cache, i);
		if (item->values_utf8 != NULL) {
			for (j = 0; item->values_utf8[j] != NULL; j++)
				g_ptr_array_add (array, g_strdup (item->values_utf8[j]));
		}
		if (item->values_ascii != NULL) {
			for (j = 0; item->values_ascii[j] != NULL; j++)
				g_ptr_array_add (array, g_strdup (item->values_ascii[j]));
		}
	}
	return array;
}

/**
 * as_app_search_matches_all:
 * @app: a #AsApp instance.
 * @search: the search terms.
 *
 * Searches application data for all the specific keywords.
 *
 * Returns: a match scrore, where 0 is no match and larger numbers are better
 * matches.
 *
 * It's probably a good idea to use as_utils_search_tokenize() to populate
 * search as very short or common keywords will return a lot of matches.
 *
 * Since: 0.1.3
 */
guint
as_app_search_matches_all (AsApp *app, gchar **search)
{
	guint i;
	guint matches_sum = 0;
	guint tmp;

	/* do *all* search keywords match */
	for (i = 0; search[i] != NULL; i++) {
		tmp = as_app_search_matches (app, search[i]);
		if (tmp == 0)
			return 0;
		matches_sum += tmp;
	}
	return matches_sum;
}

/**
 * as_app_parse_appdata_unintltoolize_cb:
 **/
static gboolean
as_app_parse_appdata_unintltoolize_cb (GNode *node, gpointer data)
{
	AsAppPrivate *priv = GET_PRIVATE (AS_APP (data));
	const gchar *name;

	name = as_node_get_name (node);
	if (g_strcmp0 (name, "_name") == 0) {
		as_node_set_name (node, "name");
		priv->problems |= AS_APP_PROBLEM_INTLTOOL_NAME;
		return FALSE;
	}
	if (g_strcmp0 (name, "_summary") == 0) {
		as_node_set_name (node, "summary");
		priv->problems |= AS_APP_PROBLEM_INTLTOOL_SUMMARY;
		return FALSE;
	}
	if (g_strcmp0 (name, "_caption") == 0) {
		as_node_set_name (node, "caption");
		return FALSE;
	}
	if (g_strcmp0 (name, "_p") == 0) {
		as_node_set_name (node, "p");
		priv->problems |= AS_APP_PROBLEM_INTLTOOL_DESCRIPTION;
		return FALSE;
	}
	if (g_strcmp0 (name, "_li") == 0) {
		as_node_set_name (node, "li");
		priv->problems |= AS_APP_PROBLEM_INTLTOOL_DESCRIPTION;
		return FALSE;
	}
	if (g_strcmp0 (name, "_ul") == 0) {
		as_node_set_name (node, "ul");
		priv->problems |= AS_APP_PROBLEM_INTLTOOL_DESCRIPTION;
		return FALSE;
	}
	if (g_strcmp0 (name, "_ol") == 0) {
		as_node_set_name (node, "ol");
		priv->problems |= AS_APP_PROBLEM_INTLTOOL_DESCRIPTION;
		return FALSE;
	}
	return FALSE;
}

/**
 * as_app_parse_appdata_file:
 **/
static gboolean
as_app_parse_appdata_file (AsApp *app,
			   const gchar *filename,
			   AsAppParseFlags flags,
			   GError **error)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsNodeFromXmlFlags from_xml_flags = AS_NODE_FROM_XML_FLAG_NONE;
	GNode *l;
	GNode *node;
	gboolean seen_application = FALSE;
	gchar *tmp;
	gsize len;
	g_autoptr(GError) error_local = NULL;
	g_autofree AsNodeContext *ctx = NULL;
	g_autofree gchar *data = NULL;
	g_autoptr(AsNode) root = NULL;

	/* open file */
	if (!g_file_get_contents (filename, &data, &len, &error_local)) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_INVALID_TYPE,
			     "%s could not be read: %s",
			     filename, error_local->message);
		return FALSE;
	}

	/* validate */
	tmp = g_strstr_len (data, len, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
	if (tmp == NULL)
		tmp = g_strstr_len (data, len, "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
	if (tmp == NULL)
		tmp = g_strstr_len (data, len, "<?xml version='1.0' encoding='utf-8'?>");
	if (tmp == NULL)
		priv->problems |= AS_APP_PROBLEM_NO_XML_HEADER;

	/* check for copyright */
	tmp = g_strstr_len (data, len, "<!-- Copyright");
	if (tmp == NULL)
		priv->problems |= AS_APP_PROBLEM_NO_COPYRIGHT_INFO;

	/* parse */
	if (flags & AS_APP_PARSE_FLAG_KEEP_COMMENTS)
		from_xml_flags |= AS_NODE_FROM_XML_FLAG_KEEP_COMMENTS;
	root = as_node_from_xml (data, from_xml_flags, &error_local);
	if (root == NULL) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_INVALID_TYPE,
			     "failed to parse %s: %s",
			     filename, error_local->message);
		return FALSE;
	}

	/* make the <_summary> tags into <summary> */
	if (flags & AS_APP_PARSE_FLAG_CONVERT_TRANSLATABLE) {
		g_node_traverse (root,
				 G_IN_ORDER,
				 G_TRAVERSE_ALL,
				 10,
				 as_app_parse_appdata_unintltoolize_cb,
				 app);
	}

	node = as_node_find (root, "application");
	if (node == NULL)
		node = as_node_find (root, "component");
	if (node == NULL) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_INVALID_TYPE,
			     "%s has an unrecognised contents",
			     filename);
		return FALSE;
	}
	for (l = node->children; l != NULL; l = l->next) {
		if (g_strcmp0 (as_node_get_name (l), "licence") == 0 ||
		    g_strcmp0 (as_node_get_name (l), "license") == 0) {
			as_node_set_name (l, "metadata_license");
			priv->problems |= AS_APP_PROBLEM_DEPRECATED_LICENCE;
			continue;
		}
		if (as_node_get_tag (l) == AS_TAG_COMPONENT) {
			if (seen_application)
				priv->problems |= AS_APP_PROBLEM_MULTIPLE_ENTRIES;
			seen_application = TRUE;
		}
	}
	ctx = as_node_context_new ();
	as_node_context_set_source_kind (ctx, AS_APP_SOURCE_KIND_APPDATA);
	if (!as_app_node_parse_full (app, node, flags, ctx, error))
		return FALSE;
	return TRUE;
}

/**
 * as_app_parse_file:
 * @app: a #AsApp instance.
 * @filename: file to load.
 * @flags: #AsAppParseFlags, e.g. %AS_APP_PARSE_FLAG_USE_HEURISTICS
 * @error: A #GError or %NULL.
 *
 * Parses a desktop or AppData file and populates the application state.
 *
 * Applications that are not suitable for the store will have vetos added.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.2
 **/
gboolean
as_app_parse_file (AsApp *app,
		   const gchar *filename,
		   AsAppParseFlags flags,
		   GError **error)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	GPtrArray *vetos;

	/* autodetect */
	if (priv->source_kind == AS_APP_SOURCE_KIND_UNKNOWN) {
		priv->source_kind = as_app_guess_source_kind (filename);
		if (priv->source_kind == AS_APP_SOURCE_KIND_UNKNOWN) {
			g_set_error (error,
				     AS_APP_ERROR,
				     AS_APP_ERROR_INVALID_TYPE,
				     "%s has an unrecognised extension",
				     filename);
			return FALSE;
		}
	}

	/* convert <_p> into <p> for easy validation */
	if (g_str_has_suffix (filename, ".appdata.xml.in") ||
	    g_str_has_suffix (filename, ".metainfo.xml.in"))
		flags |= AS_APP_PARSE_FLAG_CONVERT_TRANSLATABLE;

	/* all untrusted */
	as_app_set_trust_flags (AS_APP (app),
				AS_APP_TRUST_FLAG_CHECK_DUPLICATES |
				AS_APP_TRUST_FLAG_CHECK_VALID_UTF8);

	/* set the source location */
	as_app_set_source_file (app, filename);

	/* parse */
	switch (priv->source_kind) {
	case AS_APP_SOURCE_KIND_DESKTOP:
		if (!as_app_parse_desktop_file (app, filename, flags, error))
			return FALSE;
		break;
	case AS_APP_SOURCE_KIND_APPDATA:
	case AS_APP_SOURCE_KIND_METAINFO:
		if (!as_app_parse_appdata_file (app, filename, flags, error))
			return FALSE;
		break;
	case AS_APP_SOURCE_KIND_INF:
		if (!as_app_parse_inf_file (app, filename, flags, error))
			return FALSE;
		break;
	default:
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_INVALID_TYPE,
			     "%s has an unhandled type",
			     filename);
		return FALSE;
		break;
	}

	/* vetos are errors by default */
	vetos = as_app_get_vetos (app);
	if ((flags & AS_APP_PARSE_FLAG_ALLOW_VETO) == 0 && vetos->len > 0) {
		const gchar *tmp = g_ptr_array_index (vetos, 0);
		g_set_error_literal (error,
				     AS_APP_ERROR,
				     AS_APP_ERROR_INVALID_TYPE,
				     tmp);
		return FALSE;
	}

	return TRUE;
}

/**
 * as_app_to_file:
 * @app: a #AsApp instance.
 * @file: a #GFile
 * @cancellable: A #GCancellable, or %NULL
 * @error: A #GError or %NULL
 *
 * Exports a DOM tree to an XML file.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.2.0
 **/
gboolean
as_app_to_file (AsApp *app,
		GFile *file,
		GCancellable *cancellable,
		GError **error)
{
	g_autofree AsNodeContext *ctx = NULL;
	g_autoptr(AsNode) root = NULL;
	g_autoptr(GString) xml = NULL;

	root = as_node_new ();
	ctx = as_node_context_new ();
	as_node_context_set_version (ctx, 1.0);
	as_node_context_set_output (ctx, AS_APP_SOURCE_KIND_APPDATA);
	as_app_node_insert (app, root, ctx);
	xml = as_node_to_xml (root,
			      AS_NODE_TO_XML_FLAG_ADD_HEADER |
			      AS_NODE_TO_XML_FLAG_FORMAT_INDENT |
			      AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE);
	return g_file_replace_contents (file,
					xml->str,
					xml->len,
					NULL,
					FALSE,
					G_FILE_CREATE_NONE,
					NULL,
					cancellable,
					error);
}

/**
 * as_app_get_vetos:
 * @app: A #AsApp
 *
 * Gets the list of vetos.
 *
 * Returns: (transfer none) (element-type utf8): A list of vetos
 *
 * Since: 0.2.5
 **/
GPtrArray *
as_app_get_vetos (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->vetos;
}

/**
 * as_app_get_icon_default:
 * @app: A #AsApp
 *
 * Finds the default icon.
 *
 * Returns: (transfer none): a #AsIcon, or %NULL
 *
 * Since: 0.3.1
 **/
AsIcon *
as_app_get_icon_default (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsIcon *icon;
	guint i;
	guint j;
	AsIconKind kinds[] = {
		AS_ICON_KIND_STOCK,
		AS_ICON_KIND_LOCAL,
		AS_ICON_KIND_CACHED,
		AS_ICON_KIND_EMBEDDED,
		AS_ICON_KIND_REMOTE,
		AS_ICON_KIND_UNKNOWN };

	/* nothing */
	if (priv->icons->len == 0)
		return NULL;

	/* optimise common case */
	if (priv->icons->len == 1) {
		icon = g_ptr_array_index (priv->icons, 0);
		return icon;
	}

	/* search for icons in the preferred order */
	for (j = 0; kinds[j] != AS_ICON_KIND_UNKNOWN; j++) {
		for (i = 0; i < priv->icons->len; i++) {
			icon = g_ptr_array_index (priv->icons, i);
			if (as_icon_get_kind (icon) == kinds[j])
				return icon;
		}
	}

	/* we can't decide, just return the first added */
	icon = g_ptr_array_index (priv->icons, 0);
	return icon;
}

/**
 * as_app_get_bundle_default:
 * @app: A #AsApp
 *
 * Finds the default bundle.
 *
 * Returns: (transfer none): a #AsBundle, or %NULL
 *
 * Since: 0.3.5
 **/
AsBundle *
as_app_get_bundle_default (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsBundle *bundle;

	if (priv->bundles->len == 0)
		return NULL;
	bundle = g_ptr_array_index (priv->bundles, 0);
	return bundle;
}

/**
 * as_app_get_icon_for_size:
 * @app: A #AsApp
 * @width: Size in pixels
 * @height: Size in pixels
 *
 * Finds an icon of a specific size.
 *
 * Returns: (transfer none): a #AsIcon, or %NULL
 *
 * Since: 0.3.1
 **/
AsIcon *
as_app_get_icon_for_size (AsApp *app, guint width, guint height)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	guint i;

	for (i = 0; i < priv->icons->len; i++) {
		AsIcon *ic = g_ptr_array_index (priv->icons, i);
		if (as_icon_get_width (ic) == width &&
		    as_icon_get_height (ic) == height)
			return ic;
	}
	return NULL;
}

/**
 * as_app_convert_icons:
 * @app: A #AsApp.
 * @kind: the AsIconKind, e.g. %AS_ICON_KIND_EMBEDDED.
 * @error: A #GError or %NULL
 *
 * Converts all the icons in the application to a specific kind.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.3.1
 **/
gboolean
as_app_convert_icons (AsApp *app, AsIconKind kind, GError **error)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsIcon *icon;
	guint i;

	/* convert icons */
	for (i = 0; i < priv->icons->len; i++) {
		icon = g_ptr_array_index (priv->icons, i);
		if (!as_icon_convert_to_kind (icon, kind, error))
			return FALSE;
	}
	return TRUE;
}

/**
 * as_app_add_veto:
 * @app: A #AsApp
 * @fmt: format string
 * @...: varargs
 *
 * Adds a reason to not include the application in the metadata.
 *
 * Since: 0.2.5
 **/
void
as_app_add_veto (AsApp *app, const gchar *fmt, ...)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	gchar *tmp;
	va_list args;
	va_start (args, fmt);
	tmp = g_strdup_vprintf (fmt, args);
	va_end (args);
	g_ptr_array_add (priv->vetos, tmp);
}

/**
 * as_app_remove_veto:
 * @app: A #AsApp
 * @description: veto string
 *
 * Removes a reason to not include the application in the metadata.
 *
 * Since: 0.4.1
 **/
void
as_app_remove_veto (AsApp *app, const gchar *description)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	const gchar *tmp;
	guint i;

	for (i = 0; i < priv->vetos->len; i++) {
		tmp = g_ptr_array_index (priv->vetos, i);
		if (g_strcmp0 (tmp, description) == 0) {
			g_ptr_array_remove (priv->vetos, (gpointer) tmp);
			break;
		}
	}
}

/**
 * as_app_new:
 *
 * Creates a new #AsApp.
 *
 * Returns: (transfer full): a #AsApp
 *
 * Since: 0.1.0
 **/
AsApp *
as_app_new (void)
{
	AsApp *app;
	app = g_object_new (AS_TYPE_APP, NULL);
	return AS_APP (app);
}
