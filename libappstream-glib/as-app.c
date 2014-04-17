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

#include <fnmatch.h>

#include "as-app-private.h"
#include "as-enums.h"
#include "as-node-private.h"
#include "as-release-private.h"
#include "as-screenshot-private.h"
#include "as-tag.h"
#include "as-utils-private.h"

typedef struct _AsAppPrivate	AsAppPrivate;
struct _AsAppPrivate
{
	AsIconKind	 icon_kind;
	AsIdKind	 id_kind;
	GHashTable	*comments;			/* of locale:string */
	GHashTable	*descriptions;			/* of locale:string */
	GHashTable	*languages;			/* of locale:string */
	GHashTable	*metadata;			/* of key:value */
	GHashTable	*names;				/* of locale:string */
	GHashTable	*urls;				/* of key:string */
	GPtrArray	*categories;			/* of string */
	GPtrArray	*compulsory_for_desktops;	/* of string */
	GPtrArray	*keywords;			/* of string */
	GPtrArray	*mimetypes;			/* of string */
	GPtrArray	*pkgnames;			/* of string */
	GPtrArray	*architectures;			/* of string */
	GPtrArray	*releases;			/* of AsRelease */
	GPtrArray	*screenshots;			/* of AsScreenshot */
	gchar		*icon;
	gchar		*icon_path;
	gchar		*id;
	gchar		*id_full;
	gchar		*project_group;
	gchar		*project_license;
	gint		 priority;
	gsize		 token_cache_valid;
	GPtrArray	*token_cache;			/* of AsAppTokenItem */
};

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
 * as_app_finalize:
 **/
static void
as_app_finalize (GObject *object)
{
	AsApp *app = AS_APP (object);
	AsAppPrivate *priv = GET_PRIVATE (app);

	g_free (priv->icon);
	g_free (priv->icon_path);
	g_free (priv->id);
	g_free (priv->id_full);
	g_free (priv->project_group);
	g_free (priv->project_license);
	g_hash_table_unref (priv->comments);
	g_hash_table_unref (priv->descriptions);
	g_hash_table_unref (priv->languages);
	g_hash_table_unref (priv->metadata);
	g_hash_table_unref (priv->names);
	g_hash_table_unref (priv->urls);
	g_ptr_array_unref (priv->categories);
	g_ptr_array_unref (priv->compulsory_for_desktops);
	g_ptr_array_unref (priv->keywords);
	g_ptr_array_unref (priv->mimetypes);
	g_ptr_array_unref (priv->pkgnames);
	g_ptr_array_unref (priv->architectures);
	g_ptr_array_unref (priv->releases);
	g_ptr_array_unref (priv->screenshots);
	g_ptr_array_unref (priv->token_cache);

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
	priv->keywords = g_ptr_array_new_with_free_func (g_free);
	priv->mimetypes = g_ptr_array_new_with_free_func (g_free);
	priv->pkgnames = g_ptr_array_new_with_free_func (g_free);
	priv->architectures = g_ptr_array_new_with_free_func (g_free);
	priv->releases = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->screenshots = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->token_cache = g_ptr_array_new_with_free_func ((GDestroyNotify) as_app_token_item_free);

	priv->comments = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
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
 * as_app_get_id_full:
 * @app: a #AsApp instance.
 *
 * Gets the full ID value.
 *
 * Returns: the ID, e.g. "org.gnome.Software.desktop"
 *
 * Since: 0.1.0
 **/
const gchar *
as_app_get_id_full (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->id_full;
}

/**
 * as_app_get_id:
 * @app: a #AsApp instance.
 *
 * Returns the short version of the ID.
 *
 * Returns: the short ID, e.g. "org.gnome.Software"
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
 * as_app_get_keywords:
 * @app: a #AsApp instance.
 *
 * Gets any keywords the application should match against.
 *
 * Returns: (element-type utf8) (transfer none): an array
 *
 * Since: 0.1.0
 **/
GPtrArray *
as_app_get_keywords (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->keywords;
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
 * as_app_get_urls:
 * @app: a #AsApp instance.
 *
 * Gets the URLs set for the application.
 *
 * Returns: (transfer none): URLs
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
 * as_app_get_icon_kind:
 * @app: a #AsApp instance.
 *
 * Gets the icon kind.
 *
 * Returns: enumerated value
 *
 * Since: 0.1.0
 **/
AsIconKind
as_app_get_icon_kind (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->icon_kind;
}

/**
 * as_app_get_icon:
 * @app: a #AsApp instance.
 *
 * Gets the application icon. Use as_app_get_icon_path() if you need the create
 * a full filename.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.1.0
 **/
const gchar *
as_app_get_icon (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->icon;
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
 * Returns: a percentage value
 *
 * Since: 0.1.0
 **/
gint
as_app_get_language (AsApp *app, const gchar *locale)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (locale == NULL)
		locale = "C";
	return GPOINTER_TO_INT (g_hash_table_lookup (priv->languages, locale));
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

/******************************************************************************/

/**
 * as_app_set_id_full:
 * @app: a #AsApp instance.
 * @id_full: the new _full_ application ID, e.g. "org.gnome.Software.desktop".
 * @id_full_len: the size of @id_full, or -1 if %NULL-terminated.
 *
 * Sets a new application ID. Any invalid characters will be automatically replaced.
 *
 * Since: 0.1.0
 **/
void
as_app_set_id_full (AsApp *app, const gchar *id_full, gssize id_full_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	gchar *tmp;

	g_free (priv->id_full);
	g_free (priv->id);

	priv->id_full = as_strndup (id_full, id_full_len);
	g_strdelimit (priv->id_full, "&<>", '-');
	priv->id = g_strdup (priv->id_full);
	tmp = g_strrstr_len (priv->id, -1, ".");
	if (tmp != NULL)
		*tmp = '\0';
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
 * @project_group_len: the size of @project_group, or -1 if %NULL-terminated.
 *
 * Set any project affiliation.
 *
 * Since: 0.1.0
 **/
void
as_app_set_project_group (AsApp *app,
			  const gchar *project_group,
			  gssize project_group_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_free (priv->project_group);
	priv->project_group = as_strndup (project_group, project_group_len);
}

/**
 * as_app_set_project_license:
 * @app: a #AsApp instance.
 * @project_license: the project license string.
 * @project_license_len: the size of @project_license, or -1 if %NULL-terminated.
 *
 * Set the project license.
 *
 * Since: 0.1.0
 **/
void
as_app_set_project_license (AsApp *app,
			    const gchar *project_license,
			    gssize project_license_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_free (priv->project_license);
	priv->project_license = as_strndup (project_license, project_license_len);
}

/**
 * as_app_set_icon:
 * @app: a #AsApp instance.
 * @icon: the icon filename or URL.
 * @icon_len: the size of @icon, or -1 if %NULL-terminated.
 *
 * Set the application icon.
 *
 * Since: 0.1.0
 **/
void
as_app_set_icon (AsApp *app, const gchar *icon, gssize icon_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_free (priv->icon);
	priv->icon = as_strndup (icon, icon_len);
}

/**
 * as_app_set_icon_path:
 * @app: a #AsApp instance.
 * @icon_path: the local path.
 * @icon_path_len: the size of @icon_path, or -1 if %NULL-terminated.
 *
 * Sets the icon path, where local icons would be found.
 *
 * Since: 0.1.0
 **/
void
as_app_set_icon_path (AsApp *app, const gchar *icon_path, gssize icon_path_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_free (priv->icon_path);
	priv->icon_path = as_strndup (icon_path, icon_path_len);
}

/**
 * as_app_set_icon_kind:
 * @app: a #AsApp instance.
 * @icon_kind: the #AsIconKind.
 *
 * Sets the icon kind.
 *
 * Since: 0.1.0
 **/
void
as_app_set_icon_kind (AsApp *app, AsIconKind icon_kind)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->icon_kind = icon_kind;
}

/**
 * as_app_set_name:
 * @app: a #AsApp instance.
 * @locale: the locale, or %NULL. e.g. "en_GB"
 * @name: the application name.
 * @name_len: the size of @name, or -1 if %NULL-terminated.
 *
 * Sets the application name for a specific locale.
 *
 * Since: 0.1.0
 **/
void
as_app_set_name (AsApp *app,
		 const gchar *locale,
		 const gchar *name,
		 gssize name_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (locale == NULL)
		locale = "C";
	g_hash_table_insert (priv->names,
			     g_strdup (locale),
			     as_strndup (name, name_len));
}

/**
 * as_app_set_comment:
 * @app: a #AsApp instance.
 * @locale: the locale, or %NULL. e.g. "en_GB"
 * @comment: the application summary.
 * @comment_len: the size of @comment, or -1 if %NULL-terminated.
 *
 * Sets the application summary for a specific locale.
 *
 * Since: 0.1.0
 **/
void
as_app_set_comment (AsApp *app,
		    const gchar *locale,
		    const gchar *comment,
		    gssize comment_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_return_if_fail (comment != NULL);
	if (locale == NULL)
		locale = "C";
	g_hash_table_insert (priv->comments,
			     g_strdup (locale),
			     as_strndup (comment, comment_len));
}

/**
 * as_app_set_description:
 * @app: a #AsApp instance.
 * @locale: the locale, or %NULL. e.g. "en_GB"
 * @description: the application description.
 * @description_len: the size of @description, or -1 if %NULL-terminated.
 *
 * Sets the application descrption markup for a specific locale.
 *
 * Since: 0.1.0
 **/
void
as_app_set_description (AsApp *app,
			const gchar *locale,
			const gchar *description,
			gssize description_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_return_if_fail (description != NULL);
	if (locale == NULL)
		locale = "C";
	g_hash_table_insert (priv->descriptions,
			     g_strdup (locale),
			     as_strndup (description, description_len));
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
 * as_app_array_find_string:
 **/
static gboolean
as_app_array_find_string (GPtrArray *array, const gchar *value)
{
	const gchar *tmp;
	guint i;

	for (i = 0; i < array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		if (g_strcmp0 (tmp, value) == 0)
			return TRUE;
	}
	return FALSE;
}

/**
 * as_app_add_category:
 * @app: a #AsApp instance.
 * @category: the category.
 * @category_len: the size of @category, or -1 if %NULL-terminated.
 *
 * Adds a menu category to the application.
 *
 * Since: 0.1.0
 **/
void
as_app_add_category (AsApp *app, const gchar *category, gssize category_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* simple substitution */
	if (g_strcmp0 (category, "Feed") == 0)
		category = "News";

	if (as_app_array_find_string (priv->categories, category))
		return;
	g_ptr_array_add (priv->categories, as_strndup (category, category_len));
}


/**
 * as_app_add_compulsory_for_desktop:
 * @app: a #AsApp instance.
 * @compulsory_for_desktop: the desktop string, e.g. "GNOME".
 * @compulsory_for_desktop_len: the size of @compulsory_for_desktop, or -1 if %NULL-terminated.
 *
 * Adds a desktop that requires this application to be installed.
 *
 * Since: 0.1.0
 **/
void
as_app_add_compulsory_for_desktop (AsApp *app,
				   const gchar *compulsory_for_desktop,
				   gssize compulsory_for_desktop_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (as_app_array_find_string (priv->compulsory_for_desktops,
				      compulsory_for_desktop))
		return;
	g_ptr_array_add (priv->compulsory_for_desktops,
			 as_strndup (compulsory_for_desktop,
				     compulsory_for_desktop_len));
}

/**
 * as_app_add_keyword:
 * @app: a #AsApp instance.
 * @keyword: the keyword.
 * @keyword_len: the size of @keyword, or -1 if %NULL-terminated.
 *
 * Add a keyword the application should match against.
 *
 * Since: 0.1.0
 **/
void
as_app_add_keyword (AsApp *app, const gchar *keyword, gssize keyword_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (as_app_array_find_string (priv->keywords, keyword))
		return;
	g_ptr_array_add (priv->keywords, as_strndup (keyword, keyword_len));
}

/**
 * as_app_add_mimetype:
 * @app: a #AsApp instance.
 * @mimetype: the mimetype.
 * @mimetype_len: the size of @mimetype, or -1 if %NULL-terminated.
 *
 * Adds a mimetype the application can process.
 *
 * Since: 0.1.0
 **/
void
as_app_add_mimetype (AsApp *app, const gchar *mimetype, gssize mimetype_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (as_app_array_find_string (priv->mimetypes, mimetype))
		return;
	g_ptr_array_add (priv->mimetypes, as_strndup (mimetype, mimetype_len));
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
	g_ptr_array_add (priv->releases, g_object_ref (release));
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
	g_ptr_array_add (priv->screenshots, g_object_ref (screenshot));
}

/**
 * as_app_add_pkgname:
 * @app: a #AsApp instance.
 * @pkgname: the package name.
 * @pkgname_len: the size of @pkgname, or -1 if %NULL-terminated.
 *
 * Adds a package name to an application.
 *
 * Since: 0.1.0
 **/
void
as_app_add_pkgname (AsApp *app, const gchar *pkgname, gssize pkgname_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (as_app_array_find_string (priv->pkgnames, pkgname))
		return;
	g_ptr_array_add (priv->pkgnames, as_strndup (pkgname, pkgname_len));
}

/**
 * as_app_add_arch:
 * @app: a #AsApp instance.
 * @arch: the package name.
 * @arch_len: the size of @arch, or -1 if %NULL-terminated.
 *
 * Adds a package name to an application.
 *
 * Since: 0.1.1
 **/
void
as_app_add_arch (AsApp *app, const gchar *arch, gssize arch_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (as_app_array_find_string (priv->architectures, arch))
		return;
	g_ptr_array_add (priv->architectures, as_strndup (arch, arch_len));
}

/**
 * as_app_add_language:
 * @app: a #AsApp instance.
 * @percentage: the percentage completion of the translation.
 * @locale: the locale, or %NULL. e.g. "en_GB"
 * @locale_len: the size of @locale, or -1 if %NULL-terminated.
 *
 * Adds a language to the application.
 *
 * Since: 0.1.0
 **/
void
as_app_add_language (AsApp *app,
		     gint percentage,
		     const gchar *locale,
		     gssize locale_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (locale == NULL)
		locale = "C";
	g_hash_table_insert (priv->languages,
			     as_strndup (locale, locale_len),
			     GINT_TO_POINTER (percentage));
}

/**
 * as_app_add_url:
 * @app: a #AsApp instance.
 * @url_kind: the URL kind, e.g. %AS_URL_KIND_HOMEPAGE
 * @url: the full URL.
 * @url_len: the size of @url, or -1 if %NULL-terminated.
 *
 * Adds some URL data to the application.
 *
 * Since: 0.1.0
 **/
void
as_app_add_url (AsApp *app,
		AsUrlKind url_kind,
		const gchar *url,
		gssize url_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_hash_table_insert (priv->urls,
			     g_strdup (as_url_kind_to_string (url_kind)),
			     as_strndup (url, url_len));
}

/**
 * as_app_add_metadata:
 * @app: a #AsApp instance.
 * @key: the metadata key.
 * @value: the value to store.
 * @value_len: the size of @value, or -1 if %NULL-terminated.
 *
 * Adds a metadata entry to the application.
 *
 * Since: 0.1.0
 **/
void
as_app_add_metadata (AsApp *app,
		     const gchar *key,
		     const gchar *value,
		     gssize value_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_return_if_fail (key != NULL);
	if (value == NULL)
		value = "";
	g_hash_table_insert (priv->metadata,
			     g_strdup (key),
			     as_strndup (value, value_len));
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

/******************************************************************************/

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
	AsAppPrivate *priv = GET_PRIVATE (donor);
	AsScreenshot *ss;
	const gchar *tmp;
	guint i;
	gint percentage;
	GList *l;
	GList *langs;

	g_assert (app != donor);

	/* pkgnames */
	for (i = 0; i < priv->pkgnames->len; i++) {
		tmp = g_ptr_array_index (priv->pkgnames, i);
		as_app_add_pkgname (app, tmp, -1);
	}

	/* screenshots */
	for (i = 0; i < priv->screenshots->len; i++) {
		ss = g_ptr_array_index (priv->screenshots, i);
		as_app_add_screenshot (app, ss);
	}

	/* languages */
	langs = g_hash_table_get_keys (priv->languages);
	for (l = langs; l != NULL; l = l->next) {
		tmp = l->data;
		percentage = GPOINTER_TO_INT (g_hash_table_lookup (priv->languages, tmp));
		as_app_add_language (app, percentage, tmp, -1);
	}
	g_list_free (langs);

	/* icon */
	if (priv->icon != NULL)
		as_app_set_icon (app, priv->icon, -1);
}

/**
 * as_app_node_insert_languages:
 **/
static void
as_app_node_insert_languages (AsApp *app, GNode *parent)
{
	GNode *node_tmp;
	GList *langs;
	GList *l;
	const gchar *locale;
	gchar tmp[4];
	gint percentage;

	node_tmp = as_node_insert (parent, "languages", NULL, 0, NULL);
	langs = as_app_get_languages (app);
	for (l = langs; l != NULL; l = l->next) {
		locale = l->data;
		percentage = as_app_get_language (app, locale);
		if (percentage < 0) {
			as_node_insert (node_tmp, "lang", locale, 0, NULL);
		} else {
			g_snprintf (tmp, sizeof (tmp), "%i", percentage);
			as_node_insert (node_tmp, "lang", locale, 0,
					"percentage", tmp,
					NULL);
		}
	}
	g_list_free (langs);
}

/**
 * as_app_node_insert: (skip)
 * @app: a #AsApp instance.
 * @parent: the parent #GNode to use..
 * @api_version: the AppStream API version
 *
 * Inserts the application into the DOM tree.
 *
 * Returns: (transfer full): A populated #GNode
 *
 * Since: 0.1.0
 **/
GNode *
as_app_node_insert (AsApp *app, GNode *parent, gdouble api_version)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsRelease *rel;
	AsScreenshot *ss;
	GNode *node_app;
	GNode *node_tmp;
	const gchar *tmp;
	guint i;

	/* <component> or <application> */
	if (api_version >= 0.6) {
		node_app = as_node_insert (parent, "component", NULL, 0, NULL);
		if (priv->id_kind != AS_ID_KIND_UNKNOWN) {
			as_node_add_attribute (node_app,
					       "type",
					       as_id_kind_to_string (priv->id_kind),
					       -1);
		}
	} else {
		node_app = as_node_insert (parent, "application", NULL, 0, NULL);
	}

	/* <id> */
	node_tmp = as_node_insert (node_app, "id", priv->id_full, 0, NULL);
	if (api_version < 0.6 && priv->id_kind != AS_ID_KIND_UNKNOWN) {
		as_node_add_attribute (node_tmp,
				       "type",
				       as_id_kind_to_string (priv->id_kind),
				       -1);
	}

	/* <priority> */
	if (priv->priority != 0) {
		gchar prio[6];
		g_snprintf (prio, sizeof (prio), "%i", priv->priority);
		as_node_insert (node_app, "priority", prio, 0, NULL);
	}

	/* <pkgname> */
	for (i = 0; i < priv->pkgnames->len; i++) {
		tmp = g_ptr_array_index (priv->pkgnames, i);
		as_node_insert (node_app, "pkgname", tmp, 0, NULL);
	}

	/* <name> */
	as_node_insert_localized (node_app, "name",
				  priv->names,
				  AS_NODE_INSERT_FLAG_DEDUPE_LANG);

	/* <summary> */
	as_node_insert_localized (node_app, "summary",
				  priv->comments,
				  AS_NODE_INSERT_FLAG_DEDUPE_LANG);

	/* <description> */
	if (api_version < 0.6) {
		as_node_insert_localized (node_app, "description",
					  priv->descriptions,
					  AS_NODE_INSERT_FLAG_NO_MARKUP |
					  AS_NODE_INSERT_FLAG_DEDUPE_LANG);
	} else {
		as_node_insert_localized (node_app, "description",
					  priv->descriptions,
					  AS_NODE_INSERT_FLAG_PRE_ESCAPED |
					  AS_NODE_INSERT_FLAG_DEDUPE_LANG);
	}

	/* <icon> */
	if (priv->icon != NULL) {
		as_node_insert (node_app, "icon", priv->icon, 0,
				"type", as_icon_kind_to_string (priv->icon_kind),
				NULL);
	}

	/* <categories> */
	if (api_version >= 0.5) {
		if (priv->categories->len > 0) {
			node_tmp = as_node_insert (node_app, "categories", NULL, 0, NULL);
			for (i = 0; i < priv->categories->len; i++) {
				tmp = g_ptr_array_index (priv->categories, i);
				as_node_insert (node_tmp, "category", tmp, 0, NULL);
			}
		}
	} else {
		if (priv->categories->len > 0) {
			node_tmp = as_node_insert (node_app, "appcategories", NULL, 0, NULL);
			for (i = 0; i < priv->categories->len; i++) {
				tmp = g_ptr_array_index (priv->categories, i);
				as_node_insert (node_tmp, "appcategory", tmp, 0, NULL);
			}
		}
	}

	/* <architectures> */
	if (priv->architectures->len > 0 && api_version >= 0.6) {
		node_tmp = as_node_insert (node_app, "architectures", NULL, 0, NULL);
		for (i = 0; i < priv->architectures->len; i++) {
			tmp = g_ptr_array_index (priv->architectures, i);
			as_node_insert (node_tmp, "arch", tmp, 0, NULL);
		}
	}

	/* <keywords> */
	if (priv->keywords->len > 0) {
		node_tmp = as_node_insert (node_app, "keywords", NULL, 0, NULL);
		for (i = 0; i < priv->keywords->len; i++) {
			tmp = g_ptr_array_index (priv->keywords, i);
			as_node_insert (node_tmp, "keyword", tmp, 0, NULL);
		}
	}

	/* <mimetypes> */
	if (priv->mimetypes->len > 0) {
		node_tmp = as_node_insert (node_app, "mimetypes", NULL, 0, NULL);
		for (i = 0; i < priv->mimetypes->len; i++) {
			tmp = g_ptr_array_index (priv->mimetypes, i);
			as_node_insert (node_tmp, "mimetype", tmp, 0, NULL);
		}
	}

	/* <project_license> or <licence> */
	if (priv->project_license != NULL) {
		if (api_version >= 0.4) {
			as_node_insert (node_app, "project_license",
					priv->project_license, 0, NULL);
		} else {
			as_node_insert (node_app, "licence",
					priv->project_license, 0, NULL);
		}
	}

	/* <url> */
	as_node_insert_hash (node_app, "url", "type", priv->urls, 0);

	/* <project_group> */
	if (priv->project_group != NULL && api_version >= 0.4) {
		as_node_insert (node_app, "project_group",
				priv->project_group, 0, NULL);
	}

	/* <compulsory_for_desktop> */
	if (priv->compulsory_for_desktops != NULL && api_version >= 0.4) {
		for (i = 0; i < priv->compulsory_for_desktops->len; i++) {
			tmp = g_ptr_array_index (priv->compulsory_for_desktops, i);
			as_node_insert (node_app, "compulsory_for_desktop",
					tmp, 0, NULL);
		}
	}

	/* <screenshots> */
	if (priv->screenshots->len > 0 && api_version >= 0.4) {
		node_tmp = as_node_insert (node_app, "screenshots", NULL, 0, NULL);
		for (i = 0; i < priv->screenshots->len; i++) {
			ss = g_ptr_array_index (priv->screenshots, i);
			as_screenshot_node_insert (ss, node_tmp, api_version);
		}
	}

	/* <releases> */
	if (priv->releases->len > 0 && api_version >= 0.6) {
		node_tmp = as_node_insert (node_app, "releases", NULL, 0, NULL);
		for (i = 0; i < priv->releases->len && i < 3; i++) {
			rel = g_ptr_array_index (priv->releases, i);
			as_release_node_insert (rel, node_tmp, api_version);
		}
	}

	/* <languages> */
	if (g_hash_table_size (priv->languages) > 0 && api_version >= 0.4)
		as_app_node_insert_languages (app, node_app);

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
as_app_node_parse_child (AsApp *app, GNode *n, GError **error)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsRelease *r;
	AsScreenshot *ss;
	GNode *c;
	GString *xml;
	const gchar *tmp;
	gboolean ret = TRUE;
	gchar *taken;
	guint percent;

	switch (as_node_get_tag (n)) {

	/* <id> */
	case AS_TAG_ID:
		tmp = as_node_get_attribute (n, "type");
		if (tmp != NULL)
			as_app_set_id_kind (app, as_id_kind_from_string (tmp));
		as_app_set_id_full (app, as_node_get_data (n), -1);
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

	/* <name> */
	case AS_TAG_NAME:
		taken = as_node_take_attribute (n, "xml:lang");
		if (taken == NULL)
			taken = g_strdup ("C");
		g_hash_table_insert (priv->names,
				     taken,
				     as_node_take_data (n));
		break;

	/* <summary> */
	case AS_TAG_SUMMARY:
		taken = as_node_take_attribute (n, "xml:lang");
		if (taken == NULL)
			taken = g_strdup ("C");
		g_hash_table_insert (priv->comments,
				     taken,
				     as_node_take_data (n));
		break;

	/* <description> */
	case AS_TAG_DESCRIPTION:
		if (n->children == NULL) {
			/* pre-formatted */
			as_app_set_description (app,
						as_node_get_attribute (n, "xml:lang"),
						as_node_get_data (n),
						-1);
		} else {
			xml = as_node_to_xml (n->children, AS_NODE_TO_XML_FLAG_NONE);
			as_app_set_description (app,
						as_node_get_attribute (n, "xml:lang"),
						xml->str, xml->len);
			g_string_free (xml, TRUE);
		}
		break;

	/* <icon> */
	case AS_TAG_ICON:
		tmp = as_node_get_attribute (n, "type");
		as_app_set_icon_kind (app, as_icon_kind_from_string (tmp));
		g_free (priv->icon);
		priv->icon = as_node_take_data (n);
		break;

	/* <categories> */
	case AS_TAG_CATEGORIES:
		g_ptr_array_set_size (priv->categories, 0);
		for (c = n->children; c != NULL; c = c->next) {
			if (as_node_get_tag (c) != AS_TAG_CATEGORY)
				continue;
			g_ptr_array_add (priv->categories, as_node_take_data (c));
		}
		break;

	/* <architectures> */
	case AS_TAG_ARCHITECTURES:
		g_ptr_array_set_size (priv->architectures, 0);
		for (c = n->children; c != NULL; c = c->next) {
			if (as_node_get_tag (c) != AS_TAG_ARCH)
				continue;
			g_ptr_array_add (priv->architectures, as_node_take_data (c));
		}
		break;

	/* <keywords> */
	case AS_TAG_KEYWORDS:
		g_ptr_array_set_size (priv->keywords, 0);
		for (c = n->children; c != NULL; c = c->next) {
			if (as_node_get_tag (c) != AS_TAG_KEYWORD)
				continue;
			g_ptr_array_add (priv->keywords, as_node_take_data (c));
		}
		break;

	/* <mimetypes> */
	case AS_TAG_MIMETYPES:
		g_ptr_array_set_size (priv->mimetypes, 0);
		for (c = n->children; c != NULL; c = c->next) {
			if (as_node_get_tag (c) != AS_TAG_MIMETYPE)
				continue;
			g_ptr_array_add (priv->mimetypes, as_node_take_data (c));
		}
		break;

	/* <project_license> */
	case AS_TAG_PROJECT_LICENSE:
		g_free (priv->project_license);
		priv->project_license = as_node_take_data (n);
		break;

	/* <url> */
	case AS_TAG_URL:
		tmp = as_node_get_attribute (n, "type");
		as_app_add_url (app,
				as_url_kind_from_string (tmp),
				as_node_get_data (n), -1);
		break;

	/* <project_group> */
	case AS_TAG_PROJECT_GROUP:
		g_free (priv->project_group);
		priv->project_group = as_node_take_data (n);
		break;

	/* <compulsory_for_desktop> */
	case AS_TAG_COMPULSORY_FOR_DESKTOP:
		g_ptr_array_add (priv->compulsory_for_desktops,
				 as_node_take_data (n));
		break;

	/* <screenshots> */
	case AS_TAG_SCREENSHOTS:
		g_ptr_array_set_size (priv->screenshots, 0);
		for (c = n->children; c != NULL; c = c->next) {
			if (as_node_get_tag (c) != AS_TAG_SCREENSHOT)
				continue;
			ss = as_screenshot_new ();
			ret = as_screenshot_node_parse (ss, c, error);
			if (!ret) {
				g_object_unref (ss);
				goto out;
			}
			as_app_add_screenshot (app, ss);
			g_object_unref (ss);
		}
		break;

	/* <releases> */
	case AS_TAG_RELEASES:
		g_ptr_array_set_size (priv->releases, 0);
		for (c = n->children; c != NULL; c = c->next) {
			if (as_node_get_tag (c) != AS_TAG_RELEASE)
				continue;
			r = as_release_new ();
			ret = as_release_node_parse (r, c, error);
			if (!ret) {
				g_object_unref (r);
				goto out;
			}
			as_app_add_release (app, r);
			g_object_unref (r);
		}
		break;

	/* <languages> */
	case AS_TAG_LANGUAGES:
		g_hash_table_remove_all (priv->languages);
		for (c = n->children; c != NULL; c = c->next) {
			if (as_node_get_tag (c) != AS_TAG_LANG)
				continue;
			percent = as_node_get_attribute_as_int (c, "percentage");
			as_app_add_language (app, percent,
					     as_node_get_data (c), -1);
		}
		break;

	/* <metadata> */
	case AS_TAG_METADATA:
		g_hash_table_remove_all (priv->metadata);
		for (c = n->children; c != NULL; c = c->next) {
			gchar *key;
			if (as_node_get_tag (c) != AS_TAG_VALUE)
				continue;
			key = as_node_take_attribute (c, "key");
			taken = as_node_take_data (c);
			if (taken == NULL)
				taken = g_strdup ("");
			g_hash_table_insert (priv->metadata,
					     key,
					     taken);
		}
		break;
	default:
		break;
	}
out:
	return ret;
}

/**
 * as_app_node_parse:
 * @app: a #AsApp instance.
 * @node: a #GNode.
 * @error: A #GError or %NULL.
 *
 * Populates the object from a DOM node.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.0
 **/
gboolean
as_app_node_parse (AsApp *app, GNode *node, GError **error)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	GNode *n;
	const gchar *tmp;
	gboolean ret = TRUE;

	/* new style */
	if (g_strcmp0 (as_node_get_name (node), "component") == 0) {
		tmp = as_node_get_attribute (node, "type");
		if (tmp != NULL)
			as_app_set_id_kind (app, as_id_kind_from_string (tmp));
	}

	/* parse each node */
	g_ptr_array_set_size (priv->compulsory_for_desktops, 0);
	g_ptr_array_set_size (priv->pkgnames, 0);
	g_ptr_array_set_size (priv->architectures, 0);
	for (n = node->children; n != NULL; n = n->next) {
		ret = as_app_node_parse_child (app, n, error);
		if (!ret)
			goto out;
	}
out:
	return ret;
}

#if !GLIB_CHECK_VERSION(2,39,1)
/**
 * as_app_value_tokenize:
 **/
static gchar **
as_app_value_tokenize (const gchar *value)
{
	gchar *delim;
	gchar **tmp;
	gchar **values;
	guint i;
	delim = g_strdup (value);
	g_strdelimit (delim, "/,.;:", ' ');
	tmp = g_strsplit (delim, " ", -1);
	values = g_new0 (gchar *, g_strv_length (tmp) + 1);
	for (i = 0; tmp[i] != NULL; i++) {
		values[i] = g_utf8_strdown (tmp[i], -1);
	}
	g_strfreev (tmp);
	g_free (delim);
	return values;
}
#endif

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
			    as_app_get_id_full (app));
		return;
	}

	token_item = g_slice_new0 (AsAppTokenItem);
#if GLIB_CHECK_VERSION(2,39,1)
	token_item->values_utf8 = g_str_tokenize_and_fold (value,
							   locale,
							   &token_item->values_ascii);
#else
	token_item->values_utf8 = as_app_value_tokenize (value);
#endif
	token_item->score = score;
	g_ptr_array_add (priv->token_cache, token_item);
}

/**
 * as_app_create_token_cache:
 **/
static void
as_app_create_token_cache (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	const gchar * const *locales;
	const gchar *tmp;
	guint i;

	/* add all the data we have */
	if (priv->id != NULL)
		as_app_add_tokens (app, priv->id, "C", 100);
	locales = g_get_language_names ();
	for (i = 0; locales[i] != NULL; i++) {
		tmp = as_app_get_name (app, locales[i]);
		if (tmp != NULL)
			as_app_add_tokens (app, tmp, locales[i], 80);
		tmp = as_app_get_comment (app, locales[i]);
		if (tmp != NULL)
			as_app_add_tokens (app, tmp, locales[i], 60);
		tmp = as_app_get_description (app, locales[i]);
		if (tmp != NULL)
			as_app_add_tokens (app, tmp, locales[i], 20);
	}
	for (i = 0; i < priv->keywords->len; i++) {
		tmp = g_ptr_array_index (priv->keywords, i);
		as_app_add_tokens (app, tmp, "C", 40);
	}
	for (i = 0; i < priv->mimetypes->len; i++) {
		tmp = g_ptr_array_index (priv->mimetypes, i);
		as_app_add_tokens (app, tmp, "C", 1);
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
 * as_app_search_matches_all:
 * @app: a #AsApp instance.
 * @search: the search terms.
 *
 * Searches application data for all the specific keywords.
 *
 * Returns: a match scrore, where 0 is no match and larger numbers are better
 * matches.
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
 * as_app_desktop_key_get_locale:
 */
static gchar *
as_app_desktop_key_get_locale (const gchar *key)
{
	gchar *tmp1;
	gchar *tmp2;
	gchar *locale = NULL;

	tmp1 = g_strstr_len (key, -1, "[");
	if (tmp1 == NULL)
		goto out;
	tmp2 = g_strstr_len (tmp1, -1, "]");
	if (tmp1 == NULL)
		goto out;
	locale = g_strdup (tmp1 + 1);
	locale[tmp2 - tmp1 - 1] = '\0';
out:
	return locale;
}

/**
 * as_app_infer_file_key:
 **/
static gboolean
as_app_infer_file_key (AsApp *app,
		       GKeyFile *kf,
		       const gchar *key,
		       GError **error)
{
	gboolean ret = TRUE;
	gchar *tmp = NULL;

	if (g_strcmp0 (key, "X-GNOME-UsesNotifications") == 0) {
		as_app_add_metadata (app, "X-Kudo-UsesNotifications", "", -1);
		goto out;
	}

	if (g_strcmp0 (key, "X-GNOME-Bugzilla-Product") == 0) {
		as_app_set_project_group (app, "GNOME", -1);
		goto out;
	}

	if (g_strcmp0 (key, "X-MATE-Bugzilla-Product") == 0) {
		as_app_set_project_group (app, "MATE", -1);
		goto out;
	}

	if (g_strcmp0 (key, "X-KDE-StartupNotify") == 0) {
		as_app_set_project_group (app, "KDE", -1);
		goto out;
	}

	if (g_strcmp0 (key, "X-DocPath") == 0) {
		tmp = g_key_file_get_string (kf,
					     G_KEY_FILE_DESKTOP_GROUP,
					     key,
					     NULL);
		if (g_str_has_prefix (tmp, "http://userbase.kde.org/"))
			as_app_set_project_group (app, "KDE", -1);
		goto out;
	}

	/* Exec */
	if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_EXEC) == 0) {
		tmp = g_key_file_get_string (kf,
					     G_KEY_FILE_DESKTOP_GROUP,
					     key,
					     NULL);
		if (g_str_has_prefix (tmp, "xfce4-"))
			as_app_set_project_group (app, "XFCE", -1);
		goto out;
	}
out:
	g_free (tmp);
	return ret;
}

/**
 * as_app_parse_file_key:
 **/
static gboolean
as_app_parse_file_key (AsApp *app,
		       GKeyFile *kf,
		       const gchar *key,
		       GError **error)
{

	gboolean ret = TRUE;
	gchar **list = NULL;
	gchar *dot = NULL;
	gchar *locale = NULL;
	gchar *tmp = NULL;
	guint i;

	/* NoDisplay */
	if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY) == 0) {
		as_app_add_metadata (app, "NoDisplay", "", -1);
		goto out;
	}

	/* Type */
	if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_TYPE) == 0) {
		tmp = g_key_file_get_string (kf,
					     G_KEY_FILE_DESKTOP_GROUP,
					     key,
					     NULL);
		if (g_strcmp0 (tmp, G_KEY_FILE_DESKTOP_TYPE_APPLICATION) != 0) {
			g_set_error_literal (error,
					     AS_APP_ERROR,
					     AS_APP_ERROR_INVALID_TYPE,
					     "not an application");
			ret = FALSE;
			goto out;
		}
		goto out;
	}

	/* Icon */
	if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_ICON) == 0) {
		tmp = g_key_file_get_string (kf,
					     G_KEY_FILE_DESKTOP_GROUP,
					     key,
					     NULL);
		if (tmp != NULL && tmp[0] != '\0') {
			as_app_set_icon (app, tmp, -1);
			dot = g_strstr_len (tmp, -1, ".");
			if (dot != NULL)
				*dot = '\0';
			if (as_utils_is_stock_icon_name (tmp)) {
				as_app_set_icon (app, tmp, -1);
				as_app_set_icon_kind (app, AS_ICON_KIND_STOCK);
			}
		}
		goto out;
	}

	/* Categories */
	if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_CATEGORIES) == 0) {
		list = g_key_file_get_string_list (kf,
						   G_KEY_FILE_DESKTOP_GROUP,
						   key,
						   NULL, NULL);
		for (i = 0; list[i] != NULL; i++) {

			/* check categories that if present would blacklist
			 * the application */
			if (fnmatch ("X-*-Settings-Panel", list[i], 0) == 0 ||
			    fnmatch ("X-*-SettingsDialog", list[i], 0) == 0) {
				ret = FALSE;
				g_set_error (error,
					     AS_APP_ERROR,
					     AS_APP_ERROR_INVALID_TYPE,
					     "category %s is blacklisted",
					     list[i]);
				goto out;
			}

			/* ignore some useless keys */
			if (g_strcmp0 (list[i], "GTK") == 0)
				continue;
			if (g_strcmp0 (list[i], "Qt") == 0)
				continue;
			if (g_strcmp0 (list[i], "KDE") == 0)
				continue;
			if (g_strcmp0 (list[i], "GNOME") == 0)
				continue;
			if (g_str_has_prefix (list[i], "X-"))
				continue;
			as_app_add_category (app, list[i], -1);
		}
		goto out;
	}

	if (g_strcmp0 (key, "Keywords") == 0) {
		list = g_key_file_get_string_list (kf,
						   G_KEY_FILE_DESKTOP_GROUP,
						   key,
						   NULL, NULL);
		for (i = 0; list[i] != NULL; i++)
			as_app_add_keyword (app, list[i], -1);
		goto out;
	}

	if (g_strcmp0 (key, "MimeType") == 0) {
		list = g_key_file_get_string_list (kf,
						   G_KEY_FILE_DESKTOP_GROUP,
						   key,
						   NULL, NULL);
		for (i = 0; list[i] != NULL; i++)
			as_app_add_mimetype (app, list[i], -1);
		goto out;
	}

	if (g_strcmp0 (key, "X-AppInstall-Package") == 0) {
		tmp = g_key_file_get_string (kf,
					     G_KEY_FILE_DESKTOP_GROUP,
					     key,
					     NULL);
		if (tmp != NULL && tmp[0] != '\0')
			as_app_add_pkgname (app, tmp, -1);
		goto out;
	}

	/* OnlyShowIn */
	if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN) == 0) {
		/* if an app has only one entry, it's that desktop */
		list = g_key_file_get_string_list (kf,
						   G_KEY_FILE_DESKTOP_GROUP,
						   key,
						   NULL, NULL);
		if (g_strv_length (list) == 1)
			as_app_set_project_group (app, list[0], -1);
		goto out;
	}

	/* Name */
	if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_NAME) == 0) {
		tmp = g_key_file_get_string (kf,
					     G_KEY_FILE_DESKTOP_GROUP,
					     key,
					     NULL);
		if (tmp != NULL && tmp[0] != '\0')
			as_app_set_name (app, "C", tmp, -1);
		goto out;
	}

	/* Name[] */
	if (g_str_has_prefix (key, G_KEY_FILE_DESKTOP_KEY_NAME)) {
		locale = as_app_desktop_key_get_locale (key);
		tmp = g_key_file_get_locale_string (kf,
						    G_KEY_FILE_DESKTOP_GROUP,
						    G_KEY_FILE_DESKTOP_KEY_NAME,
						    locale,
						    NULL);
		if (tmp != NULL && tmp[0] != '\0')
			as_app_set_name (app, locale, tmp, -1);
		goto out;
	}

	/* Comment */
	if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_COMMENT) == 0) {
		tmp = g_key_file_get_string (kf,
					     G_KEY_FILE_DESKTOP_GROUP,
					     key,
					     NULL);
		if (tmp != NULL && tmp[0] != '\0')
			as_app_set_comment (app, "C", tmp, -1);
		goto out;
	}

	/* Comment[] */
	if (g_str_has_prefix (key, G_KEY_FILE_DESKTOP_KEY_COMMENT)) {
		locale = as_app_desktop_key_get_locale (key);
		tmp = g_key_file_get_locale_string (kf,
						    G_KEY_FILE_DESKTOP_GROUP,
						    G_KEY_FILE_DESKTOP_KEY_COMMENT,
						    locale,
						    NULL);
		if (tmp != NULL && tmp[0] != '\0')
			as_app_set_comment (app, locale, tmp, -1);
		goto out;
	}
out:
	g_free (locale);
	g_free (tmp);
	g_strfreev (list);
	return ret;
}

/**
 * as_app_parse_file:
 * @app: a #AsApp instance.
 * @desktop_file: desktop file to load.
 * @flags: #AsAppParseFlags, e.g. %AS_APP_PARSE_FLAG_USE_HEURISTICS
 * @error: A #GError or %NULL.
 *
 * Parses a desktop file and populates the application state.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.2
 **/
gboolean
as_app_parse_file (AsApp *app,
		   const gchar *desktop_file,
		   AsAppParseFlags flags,
		   GError **error)
{
	GKeyFile *kf;
	gboolean ret;
	gchar *app_id = NULL;
	gchar **keys = NULL;
	gchar *tmp;
	guint i;

	/* load file */
	kf = g_key_file_new ();
	ret = g_key_file_load_from_file (kf, desktop_file,
					 G_KEY_FILE_KEEP_TRANSLATIONS,
					 error);
	if (!ret)
		goto out;

	/* create app */
	app_id = g_path_get_basename (desktop_file);
	as_app_set_id_kind (app, AS_ID_KIND_DESKTOP);

	/* Ubuntu helpfully put the package name in the desktop file name */
	tmp = g_strstr_len (app_id, -1, ":");
	if (tmp != NULL)
		as_app_set_id_full (app, tmp + 1, -1);
	else
		as_app_set_id_full (app, app_id, -1);

	/* look at all the keys */
	keys = g_key_file_get_keys (kf, G_KEY_FILE_DESKTOP_GROUP, NULL, error);
	if (keys == NULL) {
		ret = FALSE;
		goto out;
	}
	for (i = 0; keys[i] != NULL; i++) {
		ret = as_app_parse_file_key (app, kf, keys[i], error);
		if (!ret)
			goto out;
		if ((flags & AS_APP_PARSE_FLAG_USE_HEURISTICS) > 0) {
			ret = as_app_infer_file_key (app, kf, keys[i], error);
			if (!ret)
				goto out;
		}
	}

	/* all applications require icons */
	if (as_app_get_icon (app) == NULL) {
		ret = FALSE;
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_INVALID_TYPE,
			     "Application %s has no icon",
			     desktop_file);
		goto out;
	}
out:
	g_free (app_id);
	g_key_file_unref (kf);
	g_strfreev (keys);
	return ret;
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
