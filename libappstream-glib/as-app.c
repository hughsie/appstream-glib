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
#include "as-node.h"
#include "as-tag.h"
#include "as-utils.h"

typedef struct _AsAppPrivate	AsAppPrivate;
struct _AsAppPrivate
{
	AsAppIconKind	 icon_kind;
	AsAppIdKind	 id_kind;
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
	gchar		*icon_path;
	gchar		*id;
	gchar		*id_full;
	gchar		*project_group;
	gchar		*project_license;
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
	g_ptr_array_unref (priv->compulsory_for_desktop);
	g_ptr_array_unref (priv->keywords);
	g_ptr_array_unref (priv->mimetypes);
	g_ptr_array_unref (priv->pkgnames);
	g_ptr_array_unref (priv->releases);
	g_ptr_array_unref (priv->screenshots);
	g_ptr_array_unref (priv->token_cache);

	G_OBJECT_CLASS (as_app_parent_class)->finalize (object);
}

/**
 * as_app_token_item_free:
 */
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
	priv->compulsory_for_desktop = g_ptr_array_new_with_free_func (g_free);
	priv->keywords = g_ptr_array_new_with_free_func (g_free);
	priv->mimetypes = g_ptr_array_new_with_free_func (g_free);
	priv->pkgnames = g_ptr_array_new_with_free_func (g_free);
	priv->releases = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->screenshots = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->token_cache = g_ptr_array_new_with_free_func ((GDestroyNotify) as_app_token_item_free);

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
as_app_id_kind_to_string (AsAppIdKind id_kind)
{
	if (id_kind == AS_APP_ID_KIND_DESKTOP)
		return "desktop";
	if (id_kind == AS_APP_ID_KIND_CODEC)
		return "codec";
	if (id_kind == AS_APP_ID_KIND_FONT)
		return "font";
	if (id_kind == AS_APP_ID_KIND_INPUT_METHOD)
		return "inputmethod";
	if (id_kind == AS_APP_ID_KIND_WEB_APP)
		return "webapp";
	if (id_kind == AS_APP_ID_KIND_SOURCE)
		return "source";
	return "unknown";
}

/**
 * as_app_id_kind_from_string:
 **/
AsAppIdKind
as_app_id_kind_from_string (const gchar *id_kind)
{
	if (g_strcmp0 (id_kind, "desktop") == 0)
		return AS_APP_ID_KIND_DESKTOP;
	if (g_strcmp0 (id_kind, "codec") == 0)
		return AS_APP_ID_KIND_CODEC;
	if (g_strcmp0 (id_kind, "font") == 0)
		return AS_APP_ID_KIND_FONT;
	if (g_strcmp0 (id_kind, "inputmethod") == 0)
		return AS_APP_ID_KIND_INPUT_METHOD;
	if (g_strcmp0 (id_kind, "webapp") == 0)
		return AS_APP_ID_KIND_WEB_APP;
	if (g_strcmp0 (id_kind, "source") == 0)
		return AS_APP_ID_KIND_SOURCE;
	return AS_APP_ID_KIND_UNKNOWN;
}

/**
 * as_app_icon_kind_to_string:
 **/
const gchar *
as_app_icon_kind_to_string (AsAppIconKind icon_kind)
{
	if (icon_kind == AS_APP_ICON_KIND_CACHED)
		return "cached";
	if (icon_kind == AS_APP_ICON_KIND_STOCK)
		return "stock";
	if (icon_kind == AS_APP_ICON_KIND_REMOTE)
		return "remote";
	return "unknown";
}

/**
 * as_app_icon_kind_from_string:
 **/
AsAppIconKind
as_app_icon_kind_from_string (const gchar *icon_kind)
{
	if (g_strcmp0 (icon_kind, "cached") == 0)
		return AS_APP_ICON_KIND_CACHED;
	if (g_strcmp0 (icon_kind, "stock") == 0)
		return AS_APP_ICON_KIND_STOCK;
	if (g_strcmp0 (icon_kind, "remote") == 0)
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
	return priv->id_kind;
}

/**
 * as_app_get_icon_kind:
 **/
AsAppIconKind
as_app_get_icon_kind (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->icon_kind;
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
 * as_app_get_icon_path:
 **/
const gchar *
as_app_get_icon_path (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->icon_path;
}

/**
 * as_app_get_name:
 **/
const gchar *
as_app_get_name (AsApp *app, const gchar *locale)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (locale == NULL)
		locale = "C";
	return g_hash_table_lookup (priv->names, locale);
}

/**
 * as_app_get_comment:
 **/
const gchar *
as_app_get_comment (AsApp *app, const gchar *locale)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (locale == NULL)
		locale = "C";
	return g_hash_table_lookup (priv->comments, locale);
}

/**
 * as_app_get_description:
 **/
const gchar *
as_app_get_description (AsApp *app, const gchar *locale)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (locale == NULL)
		locale = "C";
	return g_hash_table_lookup (priv->descriptions, locale);
}

/**
 * as_app_get_language:
 **/
const gchar *
as_app_get_language (AsApp *app, const gchar *locale)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (locale == NULL)
		locale = "C";
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

/**
 * as_app_get_project_license:
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
 **/
void
as_app_set_id_full (AsApp *app, const gchar *id_full, gssize id_full_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	gchar *tmp;

	priv->id_full = as_strndup (id_full, id_full_len);
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
as_app_set_id_kind (AsApp *app, AsAppIdKind id_kind)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->id_kind = id_kind;
}

/**
 * as_app_set_project_group:
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
 **/
void
as_app_set_icon_kind (AsApp *app, AsAppIconKind icon_kind)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->icon_kind = icon_kind;
}

/**
 * as_app_set_name:
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
 **/
void
as_app_add_compulsory_for_desktop (AsApp *app,
				   const gchar *compulsory_for_desktop,
				   gssize compulsory_for_desktop_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (as_app_array_find_string (priv->compulsory_for_desktop,
				      compulsory_for_desktop))
		return;
	g_ptr_array_add (priv->compulsory_for_desktop,
			 as_strndup (compulsory_for_desktop,
				     compulsory_for_desktop_len));
}

/**
 * as_app_add_keyword:
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
as_app_add_pkgname (AsApp *app, const gchar *pkgname, gssize pkgname_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (as_app_array_find_string (priv->pkgnames, pkgname))
		return;
	g_ptr_array_add (priv->pkgnames, as_strndup (pkgname, pkgname_len));
}

/**
 * as_app_add_language:
 **/
void
as_app_add_language (AsApp *app,
		     const gchar *locale,
		     const gchar *value,
		     gssize value_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (locale == NULL)
		locale = "C";
	g_hash_table_insert (priv->languages,
			     g_strdup (locale),
			     as_strndup (value, value_len));
}

/**
 * as_app_add_url:
 **/
void
as_app_add_url (AsApp *app, const gchar *type, const gchar *url, gssize url_len)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_hash_table_insert (priv->urls,
			     g_strdup (type),
			     as_strndup (url, url_len));
}

/**
 * as_app_add_metadata:
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
		value = g_hash_table_lookup (priv->languages, tmp);
		as_app_add_language (app, tmp, value, -1);
	}
	g_list_free (langs);

	/* icon */
	if (priv->icon != NULL)
		as_app_set_icon (app, priv->icon, -1);
}

/**
 * as_app_node_insert:
 **/
GNode *
as_app_node_insert (AsApp *app, GNode *parent)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsRelease *rel;
	AsScreenshot *ss;
	GNode *node_app;
	GNode *node_tmp;
	const gchar *tmp;
	guint i;

	node_app = as_node_insert (parent, "application", NULL, 0, NULL);

	/* <id> */
	as_node_insert (node_app, "id", priv->id_full, 0,
			"type", as_app_id_kind_to_string (priv->id_kind),
			NULL);

	/* <pkgname> */
	for (i = 0; i < priv->pkgnames->len; i++) {
		tmp = g_ptr_array_index (priv->pkgnames, i);
		as_node_insert (node_app, "pkgname", tmp, 0, NULL);
	}

	/* <name> */
	as_node_insert_localized (node_app, "name", priv->names, 0);

	/* <summary> */
	as_node_insert_localized (node_app, "summary", priv->comments, 0);

	/* <description> */
	as_node_insert_localized (node_app, "description", priv->descriptions,
				  AS_NODE_INSERT_FLAG_PRE_ESCAPED);

	/* <icon> */
	if (priv->icon != NULL) {
		as_node_insert (node_app, "icon", priv->icon, 0,
				"type", as_app_icon_kind_to_string (priv->icon_kind),
				NULL);
	}

	/* <categories> */
	if (priv->categories->len > 0) {
		node_tmp = as_node_insert (node_app, "appcategories", NULL, 0, NULL);
		for (i = 0; i < priv->categories->len; i++) {
			tmp = g_ptr_array_index (priv->categories, i);
			as_node_insert (node_tmp, "appcategory", tmp, 0, NULL);
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

	/* <project_license> */
	if (priv->project_license != NULL) {
		as_node_insert (node_app, "project_license",
				priv->project_license, 0, NULL);
	}

	/* <url> */
//		as_node_insert (node_app, "url", priv->homepage_url, 0,
//				"type", "homepage",
//				NULL);
	as_node_insert_hash (node_app, "url", "type", priv->urls, 0);

	/* <project_group> */
	if (priv->project_group != NULL) {
		as_node_insert (node_app, "project_group",
				priv->project_group, 0, NULL);
	}

	/* <compulsory_for_desktop> */
	if (priv->compulsory_for_desktop != NULL) {
		for (i = 0; i < priv->compulsory_for_desktop->len; i++) {
			tmp = g_ptr_array_index (priv->compulsory_for_desktop, i);
			as_node_insert (node_app, "compulsory_for_desktop",
					tmp, 0, NULL);
		}
	}

	/* <screenshots> */
	if (priv->screenshots->len > 0) {
		node_tmp = as_node_insert (node_app, "screenshots", NULL, 0, NULL);
		for (i = 0; i < priv->screenshots->len; i++) {
			ss = g_ptr_array_index (priv->screenshots, i);
			as_screenshot_node_insert (ss, node_tmp);
		}
	}

	/* <releases> */
	if (priv->releases->len > 0) {
		node_tmp = as_node_insert (node_app, "releases", NULL, 0, NULL);
		for (i = 0; i < priv->releases->len && i < 3; i++) {
			rel = g_ptr_array_index (priv->releases, i);
			as_release_node_insert (rel, node_tmp);
		}
	}

	/* <languages> */
	if (g_hash_table_size (priv->languages) > 0) {
		node_tmp = as_node_insert (node_app, "languages", NULL, 0, NULL);
		as_node_insert_hash (node_tmp, "lang", "percentage", priv->languages, TRUE);
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
as_app_node_parse_child (AsApp *app, GNode *n, GError **error)
{
	AsTag tag;
	GNode *c;
	GString *xml;
	const gchar *tmp;
	gboolean ret = TRUE;

	/* <id> */
	tag = as_tag_from_string (as_node_get_name (n));
	if (tag == AS_TAG_ID) {
		tmp = as_node_get_attribute (n, "type");
		as_app_set_id_kind (app, as_app_id_kind_from_string (tmp));
		as_app_set_id_full (app, as_node_get_data (n), -1);
		goto out;
	}

	/* <pkgname> */
	if (tag == AS_TAG_PKGNAME) {
		as_app_add_pkgname (app, as_node_get_data (n), -1);
		goto out;
	}

	/* <name> */
	if (tag == AS_TAG_NAME) {
		as_app_set_name (app,
				 as_node_get_attribute (n, "xml:lang"),
				 as_node_get_data (n), -1);
	}

	/* <summary> */
	if (tag == AS_TAG_SUMMARY) {
		as_app_set_comment (app,
				    as_node_get_attribute (n, "xml:lang"),
				    as_node_get_data (n), -1);
	}

	/* <description> */
	if (tag == AS_TAG_DESCRIPTION) {
		xml = as_node_to_xml (n->children, AS_NODE_TO_XML_FLAG_NONE);
		as_app_set_description (app,
					 as_node_get_attribute (n, "xml:lang"),
					 xml->str, xml->len);
		g_string_free (xml, TRUE);
	}

	/* <icon> */
	if (tag == AS_TAG_ICON) {
		tmp = as_node_get_attribute (n, "type");
		as_app_set_icon_kind (app, as_app_icon_kind_from_string (tmp));
		as_app_set_icon (app, as_node_get_data (n), -1);
		goto out;
	}

	/* <categories> */
	if (tag == AS_TAG_APPCATEGORIES) {
		for (c = n->children; c != NULL; c = c->next) {
			if (g_strcmp0 (as_node_get_name (c),
				       "appcategory") != 0)
				continue;
			as_app_add_category (app, as_node_get_data (c), -1);
		}
	}

	/* <keywords> */
	if (tag == AS_TAG_KEYWORDS) {
		for (c = n->children; c != NULL; c = c->next) {
			if (g_strcmp0 (as_node_get_name (c),
				       "keyword") != 0)
				continue;
			as_app_add_keyword (app, as_node_get_data (c), -1);
		}
	}

	/* <mimetypes> */
	if (tag == AS_TAG_MIMETYPES) {
		for (c = n->children; c != NULL; c = c->next) {
			if (g_strcmp0 (as_node_get_name (c),
				       "mimetype") != 0)
				continue;
			as_app_add_mimetype (app, as_node_get_data (c), -1);
		}
	}

	/* <project_license> */
	if (tag == AS_TAG_PROJECT_LICENSE) {
		as_app_set_project_license (app, as_node_get_data (n), -1);
		goto out;
	}

	/* <url> */
	if (tag == AS_TAG_URL) {
		tmp = as_node_get_attribute (n, "type");
		as_app_add_url (app, tmp, as_node_get_data (n), -1);
		goto out;
	}

	/* <project_group> */
	if (tag == AS_TAG_PROJECT_GROUP) {
		as_app_set_project_group (app, as_node_get_data (n), -1);
		goto out;
	}

	/* <compulsory_for_desktop> */
	if (tag == AS_TAG_COMPULSORY_FOR_DESKTOP) {
		as_app_add_compulsory_for_desktop (app, as_node_get_data (n), -1);
		goto out;
	}

	/* <screenshots> */
	if (tag == AS_TAG_SCREENSHOTS) {
		AsScreenshot *ss;
		for (c = n->children; c != NULL; c = c->next) {
			if (g_strcmp0 (as_node_get_name (c), "screenshot") != 0)
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
	}

	/* <releases> */
	if (tag == AS_TAG_RELEASES) {
		AsRelease *r;
		for (c = n->children; c != NULL; c = c->next) {
			if (g_strcmp0 (as_node_get_name (c), "release") != 0)
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
	}

	/* <languages> */
	if (tag == AS_TAG_LANGUAGES) {
		for (c = n->children; c != NULL; c = c->next) {
			if (g_strcmp0 (as_node_get_name (c), "lang") != 0)
				continue;
			tmp = as_node_get_attribute (c, "percentage");
			as_app_add_language (app, as_node_get_data (c), tmp, -1);
		}
	}

	/* <metadata> */
	if (tag == AS_TAG_METADATA) {
		for (c = n->children; c != NULL; c = c->next) {
			if (g_strcmp0 (as_node_get_name (c), "value") != 0)
				continue;
			tmp = as_node_get_attribute (c, "key");
			as_app_add_metadata (app, tmp, as_node_get_data (c), -1);
		}
	}
out:
	return ret;
}

/**
 * as_app_node_parse:
 **/
gboolean
as_app_node_parse (AsApp *app, GNode *node, GError **error)
{
	GNode *n;
	gboolean ret = TRUE;

	/* parse each node */
	for (n = node->children; n != NULL; n = n->next) {
		ret = as_app_node_parse_child (app, n, error);
		if (!ret)
			goto out;
	}
out:
	return ret;
}

/**
 * as_app_add_tokens:
 */
static void
as_app_add_tokens (AsApp *app,
		   const gchar *value,
		   const gchar *locale,
		   guint score)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsAppTokenItem *token_item;

	/* sanity check */
	if (value == NULL)
		return;

	token_item = g_slice_new (AsAppTokenItem);
	token_item->values_utf8 = g_str_tokenize_and_fold (value,
							   locale,
							   &token_item->values_ascii);
	token_item->score = score;
	g_ptr_array_add (priv->token_cache, token_item);
}

/**
 * as_app_create_token_cache:
 */
static void
as_app_create_token_cache (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	const gchar * const *locales;
	const gchar *tmp;
	guint i;

	/* add all the data we have */
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
 */
guint
as_app_search_matches (AsApp *app, const gchar *search)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsAppTokenItem *token_item;
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
		token_item = g_ptr_array_index (priv->token_cache, i);
		for (j = 0; token_item->values_utf8[j] != NULL; j++) {
			if (g_str_has_prefix (token_item->values_utf8[j], search))
				return token_item->score;
		}
		for (j = 0; token_item->values_ascii[j] != NULL; j++) {
			if (g_str_has_prefix (token_item->values_utf8[j], search))
				return token_item->score / 2;
		}
	}
	return 0;
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
