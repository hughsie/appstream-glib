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

#include "config.h"

#include "as-app.h"

typedef struct _AsAppPrivate	AsAppPrivate;
struct _AsAppPrivate
{
	AsAppIconKind	 icon_type;
	AsAppIdKind	 kind;
	GHashTable	*comments;			/* of locale:string */
	GHashTable	*descriptions;			/* of locale:string */
	GHashTable	*languages;			/* of locale:string */
	GHashTable	*metadata;			/* of key:value */
	GHashTable	*names;				/* of locale:string */
	GHashTable	*urls;				/* of key:string */
	GPtrArray	*categories;			/* of string */
	GPtrArray	*compulsory_for_desktop;	/* of string */
	GPtrArray	*keywords;			/* of string */
	GPtrArray	*mimetypes;			/* of string */
	GPtrArray	*pkgnames;			/* of string */
	GPtrArray	*releases;			/* of AsRelease */
	GPtrArray	*screenshots;			/* of AsScreenshot */
	gchar		*icon;
	gchar		*id;
	gchar		*id_full;
	gchar		*project_group;
	gchar		*project_license;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsApp, as_app, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_app_get_instance_private (o))

/**
 * as_app_finalize:
 **/
static void
as_app_finalize (GObject *object)
{
	AsApp *app = AS_APP (object);
	AsAppPrivate *priv = GET_PRIVATE (app);

	g_free (priv->icon);
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
	g_ptr_array_unref (priv->compulsory_for_desktop);
	g_ptr_array_unref (priv->keywords);
	g_ptr_array_unref (priv->mimetypes);
	g_ptr_array_unref (priv->pkgnames);
	g_ptr_array_unref (priv->releases);
	g_ptr_array_unref (priv->screenshots);

	G_OBJECT_CLASS (as_app_parent_class)->finalize (object);
}

/**
 * as_app_init:
 **/
static void
as_app_init (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->categories = g_ptr_array_new_with_free_func (g_free);
	priv->compulsory_for_desktop = g_ptr_array_new_with_free_func (g_free);
	priv->keywords = g_ptr_array_new_with_free_func (g_free);
	priv->mimetypes = g_ptr_array_new_with_free_func (g_free);
	priv->pkgnames = g_ptr_array_new_with_free_func (g_free);
	priv->releases = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->screenshots = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	priv->comments = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->descriptions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->languages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
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

/**
 * as_app_id_kind_to_string:
 **/
const gchar *
as_app_id_kind_to_string (AsAppIdKind kind)
{
	if (kind == AS_APP_ID_KIND_DESKTOP)
		return "desktop";
	if (kind == AS_APP_ID_KIND_CODEC)
		return "codec";
	if (kind == AS_APP_ID_KIND_FONT)
		return "font";
	if (kind == AS_APP_ID_KIND_INPUT_METHOD)
		return "inputmethod";
	if (kind == AS_APP_ID_KIND_WEB_APP)
		return "webapp";
	if (kind == AS_APP_ID_KIND_SOURCE)
		return "source";
	return "unknown";
}

/**
 * as_app_id_kind_from_string:
 **/
AsAppIdKind
as_app_id_kind_from_string (const gchar *kind)
{
	if (g_strcmp0 (kind, "desktop") == 0)
		return AS_APP_ID_KIND_DESKTOP;
	if (g_strcmp0 (kind, "codec") == 0)
		return AS_APP_ID_KIND_CODEC;
	if (g_strcmp0 (kind, "font") == 0)
		return AS_APP_ID_KIND_FONT;
	if (g_strcmp0 (kind, "inputmethod") == 0)
		return AS_APP_ID_KIND_INPUT_METHOD;
	if (g_strcmp0 (kind, "webapp") == 0)
		return AS_APP_ID_KIND_WEB_APP;
	if (g_strcmp0 (kind, "source") == 0)
		return AS_APP_ID_KIND_SOURCE;
	return AS_APP_ID_KIND_UNKNOWN;
}


/**
 * as_app_icon_kind_to_string:
 **/
const gchar *
as_app_icon_kind_to_string (AsAppIconKind icon_type)
{
	if (icon_type == AS_APP_ICON_KIND_CACHED)
		return "cached";
	if (icon_type == AS_APP_ICON_KIND_STOCK)
		return "stock";
	if (icon_type == AS_APP_ICON_KIND_REMOTE)
		return "remote";
	return "unknown";
}

/**
 * as_app_icon_kind_from_string:
 **/
AsAppIconKind
as_app_icon_kind_from_string (const gchar *icon_type)
{
	if (g_strcmp0 (icon_type, "cached") == 0)
		return AS_APP_ICON_KIND_CACHED;
	if (g_strcmp0 (icon_type, "stock") == 0)
		return AS_APP_ICON_KIND_STOCK;
	if (g_strcmp0 (icon_type, "remote") == 0)
		return AS_APP_ICON_KIND_REMOTE;
	return AS_APP_ICON_KIND_UNKNOWN;
}

/******************************************************************************/


/**
 * as_app_get_id_full:
 **/
const gchar *
as_app_get_id_full (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->id_full;
}

/**
 * as_app_get_id:
 **/
const gchar *
as_app_get_id (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->id;
}

/**
 * as_app_get_categories:
 **/
GPtrArray *
as_app_get_categories (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->categories;
}

/**
 * as_app_get_keywords:
 **/
GPtrArray *
as_app_get_keywords (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->keywords;
}

/**
 * as_app_get_releases:
 **/
GPtrArray *
as_app_get_releases (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->releases;
}

/**
 * as_app_get_screenshots:
 **/
GPtrArray *
as_app_get_screenshots (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->screenshots;
}

/**
 * as_app_get_urls:
 */
GHashTable *
as_app_get_urls (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->urls;
}

/**
 * as_app_get_pkgnames:
 **/
GPtrArray *
as_app_get_pkgnames (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->pkgnames;
}

/**
 * as_app_get_id_kind:
 **/
AsAppIdKind
as_app_get_id_kind (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->kind;
}

/**
 * as_app_get_icon:
 **/
const gchar *
as_app_get_icon (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->icon;
}

/**
 * as_app_get_name:
 **/
const gchar *
as_app_get_name (AsApp *app, const gchar *locale)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return g_hash_table_lookup (priv->names, locale);
}

/**
 * as_app_get_comment:
 **/
const gchar *
as_app_get_comment (AsApp *app, const gchar *locale)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return g_hash_table_lookup (priv->comments, locale);
}

/**
 * as_app_get_language:
 **/
const gchar *
as_app_get_language (AsApp *app, const gchar *locale)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return g_hash_table_lookup (priv->languages, locale);
}

/**
 * as_app_get_languages:
 **/
GList *
as_app_get_languages (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return g_hash_table_get_keys (priv->languages);
}

/**
 * as_app_get_url_item:
 **/
const gchar *
as_app_get_url_item (AsApp *app, const gchar *type)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return g_hash_table_lookup (priv->urls, type);
}

/**
 * as_app_get_metadata_item:
 **/
const gchar *
as_app_get_metadata_item (AsApp *app, const gchar *key)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return g_hash_table_lookup (priv->metadata, key);
}

/**
 * as_app_get_project_group:
 **/
const gchar *
as_app_get_project_group (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->project_group;
}

/******************************************************************************/

/**
 * as_app_set_id_full:
 **/
void
as_app_set_id_full (AsApp *app, const gchar *id_full)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	gchar *tmp;

	priv->id_full = g_strdup (id_full);
	g_strdelimit (priv->id_full, "&<>", '-');
	priv->id = g_strdup (priv->id_full);
	tmp = g_strrstr_len (priv->id, -1, ".");
	if (tmp != NULL)
		*tmp = '\0';
}

/**
 * as_app_set_id_kind:
 **/
void
as_app_set_id_kind (AsApp *app, AsAppIdKind kind)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->kind = kind;
}

/**
 * as_app_set_project_group:
 **/
void
as_app_set_project_group (AsApp *app, const gchar *project_group)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_free (priv->project_group);
	priv->project_group = g_strdup (project_group);
}

/**
 * as_app_set_project_license:
 **/
void
as_app_set_project_license (AsApp *app, const gchar *project_license)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_free (priv->project_license);
	priv->project_license = g_strdup (project_license);
}

/**
 * as_app_set_icon:
 **/
void
as_app_set_icon (AsApp *app, const gchar *icon)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_free (priv->icon);
	priv->icon = g_strdup (icon);
}

/**
 * as_app_set_icon_kind:
 **/
void
as_app_set_icon_kind (AsApp *app, AsAppIconKind icon_type)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->icon_type = icon_type;
}

/**
 * as_app_set_name:
 **/
void
as_app_set_name (AsApp *app, const gchar *locale, const gchar *name)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (locale == NULL)
		locale = "C";
	g_hash_table_insert (priv->names, g_strdup (locale), g_strdup (name));
}

/**
 * as_app_set_comment:
 **/
void
as_app_set_comment (AsApp *app, const gchar *locale, const gchar *comment)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_return_if_fail (comment != NULL);
	if (locale == NULL)
		locale = "C";
	g_hash_table_insert (priv->comments, g_strdup (locale), g_strdup (comment));
}

/**
 * as_app_set_description:
 **/
void
as_app_set_description (AsApp *app, const gchar *locale, const gchar *description)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_return_if_fail (description != NULL);
	if (locale == NULL)
		locale = "C";
	g_hash_table_insert (priv->descriptions,
			     g_strdup (locale),
			     g_strdup (description));
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
 **/
void
as_app_add_category (AsApp *app, const gchar *category)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* simple substitution */
	if (g_strcmp0 (category, "Feed") == 0)
		category = "News";

	if (as_app_array_find_string (priv->categories, category))
		return;
	g_ptr_array_add (priv->categories, g_strdup (category));
}


/**
 * as_app_add_compulsory_for_desktop:
 **/
void
as_app_add_compulsory_for_desktop (AsApp *app, const gchar *compulsory_for_desktop)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (as_app_array_find_string (priv->compulsory_for_desktop,
				      compulsory_for_desktop))
		return;
	g_ptr_array_add (priv->compulsory_for_desktop,
			 g_strdup (compulsory_for_desktop));
}

/**
 * as_app_add_keyword:
 **/
void
as_app_add_keyword (AsApp *app, const gchar *keyword)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (as_app_array_find_string (priv->keywords, keyword))
		return;
	g_ptr_array_add (priv->keywords, g_strdup (keyword));
}

/**
 * as_app_add_mimetype:
 **/
void
as_app_add_mimetype (AsApp *app, const gchar *mimetype)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (as_app_array_find_string (priv->mimetypes, mimetype))
		return;
	g_ptr_array_add (priv->mimetypes, g_strdup (mimetype));
}

/**
 * as_app_add_release:
 **/
void
as_app_add_release (AsApp *app, AsRelease *release)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_ptr_array_add (priv->releases, g_object_ref (release));
}

/**
 * as_app_add_screenshot:
 **/
void
as_app_add_screenshot (AsApp *app, AsScreenshot *screenshot)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_ptr_array_add (priv->screenshots, g_object_ref (screenshot));
}

/**
 * as_app_add_pkgname:
 **/
void
as_app_add_pkgname (AsApp *app, const gchar *pkgname)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (as_app_array_find_string (priv->pkgnames, pkgname))
		return;
	g_ptr_array_add (priv->pkgnames, g_strdup (pkgname));
}

/**
 * as_app_add_language:
 **/
void
as_app_add_language (AsApp *app, const gchar *locale, const gchar *value)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (locale == NULL)
		locale = "C";
	g_hash_table_insert (priv->languages,
			     g_strdup (locale),
			     g_strdup (value));
}

/**
 * as_app_add_url:
 **/
void
as_app_add_url (AsApp *app, const gchar *type, const gchar *url)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_hash_table_insert (priv->urls, g_strdup (type), g_strdup (url));
}

/**
 * as_app_add_metadata:
 **/
void
as_app_add_metadata (AsApp *app, const gchar *key, const gchar *value)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_hash_table_insert (priv->metadata, g_strdup (key), g_strdup (value));
}

/**
 * as_app_remove_metadata:
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
 **/
void
as_app_subsume (AsApp *app, AsApp *donor)
{
	AsAppPrivate *priv = GET_PRIVATE (donor);
	AsScreenshot *ss;
	const gchar *tmp;
	const gchar *value;
	guint i;
	GList *l;
	GList *langs;

	g_assert (app != donor);

	/* pkgnames */
	for (i = 0; i < priv->pkgnames->len; i++) {
		tmp = g_ptr_array_index (priv->pkgnames, i);
		as_app_add_pkgname (app, tmp);
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
		value = g_hash_table_lookup (priv->languages, tmp);
		as_app_add_language (app, tmp, value);
	}
	g_list_free (langs);

	/* icon */
	if (priv->icon != NULL)
		as_app_set_icon (app, priv->icon);
}

/**
 * as_app_new:
 **/
AsApp *
as_app_new (void)
{
	AsApp *app;
	app = g_object_new (AS_TYPE_APP, NULL);
	return AS_APP (app);
}
