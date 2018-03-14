/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2018 Richard Hughes <richard@hughsie.com>
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
#include <fnmatch.h>

#include "as-app-private.h"
#include "as-bundle-private.h"
#include "as-content-rating-private.h"
#include "as-agreement-private.h"
#include "as-enums.h"
#include "as-icon-private.h"
#include "as-node-private.h"
#include "as-launchable-private.h"
#include "as-provide-private.h"
#include "as-release-private.h"
#include "as-ref-string.h"
#include "as-require-private.h"
#include "as-review-private.h"
#include "as-screenshot-private.h"
#include "as-stemmer.h"
#include "as-tag.h"
#include "as-translation-private.h"
#include "as-suggest-private.h"
#include "as-utils-private.h"
#include "as-yaml.h"

typedef struct
{
	AsAppProblems	 problems;
	AsIconKind	 icon_kind;
	AsAppKind	 kind;
	AsStemmer	*stemmer;
	GHashTable	*comments;			/* of AsRefString:AsRefString */
	GHashTable	*developer_names;		/* of AsRefString:AsRefString */
	GHashTable	*descriptions;			/* of AsRefString:AsRefString */
	GHashTable	*keywords;			/* of AsRefString:GPtrArray */
	GHashTable	*languages;			/* of AsRefString:AsRefString */
	GHashTable	*metadata;			/* of AsRefString:AsRefString */
	GHashTable	*names;				/* of AsRefString:AsRefString */
	GHashTable	*urls;				/* of AsRefString:AsRefString */
	GPtrArray	*addons;			/* of AsApp */
	GPtrArray	*categories;			/* of AsRefString */
	GPtrArray	*compulsory_for_desktops;	/* of AsRefString */
	GPtrArray	*extends;			/* of AsRefString */
	GPtrArray	*kudos;				/* of AsRefString */
	GPtrArray	*permissions;			/* of AsRefString */
	GPtrArray	*mimetypes;			/* of AsRefString */
	GPtrArray	*pkgnames;			/* of AsRefString */
	GPtrArray	*architectures;			/* of AsRefString */
	GPtrArray	*formats;			/* of AsFormat */
	GPtrArray	*releases;			/* of AsRelease */
	GPtrArray	*provides;			/* of AsProvide */
	GPtrArray	*launchables;			/* of AsLaunchable */
	GPtrArray	*screenshots;			/* of AsScreenshot */
	GPtrArray	*reviews;			/* of AsReview */
	GPtrArray	*content_ratings;		/* of AsContentRating */
	GPtrArray	*agreements;			/* of AsAgreement */
	GPtrArray	*icons;				/* of AsIcon */
	GPtrArray	*bundles;			/* of AsBundle */
	GPtrArray	*translations;			/* of AsTranslation */
	GPtrArray	*suggests;			/* of AsSuggest */
	GPtrArray	*requires;			/* of AsRequire */
	GPtrArray	*vetos;				/* of AsRefString */
	AsAppScope	 scope;
	AsAppMergeKind	 merge_kind;
	AsAppState	 state;
	guint32		 trust_flags;
	AsAppQuirk	 quirk;
	guint16		 search_match;
	AsRefString	*icon_path;
	AsRefString	*id_filename;
	AsRefString	*id;
	AsRefString	*origin;
	AsRefString	*project_group;
	AsRefString	*project_license;
	AsRefString	*metadata_license;
	AsRefString	*source_pkgname;
	AsRefString	*update_contact;
	gchar		*unique_id;
	gboolean	 unique_id_valid;
	AsRefString	*branch;
	gint		 priority;
	gsize		 token_cache_valid;
	GHashTable	*token_cache;			/* of AsRefString:AsAppTokenType* */
	GHashTable	*search_blacklist;		/* of AsRefString:1 */
} AsAppPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsApp, as_app, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (as_app_get_instance_private (o))

typedef guint16	AsAppTokenType;	/* big enough for both bitshifts */

/**
 * as_app_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.1.2
 **/
G_DEFINE_QUARK (as-app-error-quark, as_app_error)

/**
 * as_app_kind_to_string:
 * @kind: the #AsAppKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 *
 * Since: 0.5.10
 **/
const gchar *
as_app_kind_to_string (AsAppKind kind)
{
	if (kind == AS_APP_KIND_DESKTOP)
		return "desktop";
	if (kind == AS_APP_KIND_CODEC)
		return "codec";
	if (kind == AS_APP_KIND_FONT)
		return "font";
	if (kind == AS_APP_KIND_INPUT_METHOD)
		return "inputmethod";
	if (kind == AS_APP_KIND_WEB_APP)
		return "webapp";
	if (kind == AS_APP_KIND_SOURCE)
		return "source";
	if (kind == AS_APP_KIND_ADDON)
		return "addon";
	if (kind == AS_APP_KIND_FIRMWARE)
		return "firmware";
	if (kind == AS_APP_KIND_RUNTIME)
		return "runtime";
	if (kind == AS_APP_KIND_GENERIC)
		return "generic";
	if (kind == AS_APP_KIND_OS_UPDATE)
		return "os-update";
	if (kind == AS_APP_KIND_OS_UPGRADE)
		return "os-upgrade";
	if (kind == AS_APP_KIND_SHELL_EXTENSION)
		return "shell-extension";
	if (kind == AS_APP_KIND_LOCALIZATION)
		return "localization";
	if (kind == AS_APP_KIND_CONSOLE)
		return "console-application";
	if (kind == AS_APP_KIND_DRIVER)
		return "driver";
	return "unknown";
}

/**
 * as_app_kind_from_string:
 * @kind: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #AsAppKind or %AS_APP_KIND_UNKNOWN for unknown
 *
 * Since: 0.5.10
 **/
AsAppKind
as_app_kind_from_string (const gchar *kind)
{
	if (g_strcmp0 (kind, "desktop-application") == 0)
		return AS_APP_KIND_DESKTOP;
	if (g_strcmp0 (kind, "codec") == 0)
		return AS_APP_KIND_CODEC;
	if (g_strcmp0 (kind, "font") == 0)
		return AS_APP_KIND_FONT;
	if (g_strcmp0 (kind, "inputmethod") == 0)
		return AS_APP_KIND_INPUT_METHOD;
	if (g_strcmp0 (kind, "web-application") == 0)
		return AS_APP_KIND_WEB_APP;
	if (g_strcmp0 (kind, "source") == 0)
		return AS_APP_KIND_SOURCE;
	if (g_strcmp0 (kind, "addon") == 0)
		return AS_APP_KIND_ADDON;
	if (g_strcmp0 (kind, "firmware") == 0)
		return AS_APP_KIND_FIRMWARE;
	if (g_strcmp0 (kind, "runtime") == 0)
		return AS_APP_KIND_RUNTIME;
	if (g_strcmp0 (kind, "generic") == 0)
		return AS_APP_KIND_GENERIC;
	if (g_strcmp0 (kind, "os-update") == 0)
		return AS_APP_KIND_OS_UPDATE;
	if (g_strcmp0 (kind, "os-upgrade") == 0)
		return AS_APP_KIND_OS_UPGRADE;
	if (g_strcmp0 (kind, "shell-extension") == 0)
		return AS_APP_KIND_SHELL_EXTENSION;
	if (g_strcmp0 (kind, "localization") == 0)
		return AS_APP_KIND_LOCALIZATION;
	if (g_strcmp0 (kind, "console-application") == 0)
		return AS_APP_KIND_CONSOLE;
	if (g_strcmp0 (kind, "driver") == 0)
		return AS_APP_KIND_DRIVER;

	/* legacy */
	if (g_strcmp0 (kind, "desktop") == 0)
		return AS_APP_KIND_DESKTOP;
	if (g_strcmp0 (kind, "desktop-app") == 0)
		return AS_APP_KIND_DESKTOP;
	if (g_strcmp0 (kind, "webapp") == 0)
		return AS_APP_KIND_WEB_APP;

	return AS_APP_KIND_UNKNOWN;
}

/**
 * as_app_source_kind_from_string:
 * @source_kind: a source kind string
 *
 * Converts the text representation to an enumerated value.
 *
 * Return value: A #AsFormatKind, e.g. %AS_FORMAT_KIND_APPSTREAM.
 *
 * Since: 0.2.2
 **/
AsFormatKind
as_app_source_kind_from_string (const gchar *source_kind)
{
	return as_format_kind_from_string (source_kind);
}

/**
 * as_app_source_kind_to_string:
 * @source_kind: the #AsFormatKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @source_kind, or %NULL for unknown
 *
 * Since: 0.2.2
 **/
const gchar *
as_app_source_kind_to_string (AsFormatKind source_kind)
{
	return as_format_kind_to_string (source_kind);
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
	if (state == AS_APP_STATE_PURCHASABLE)
		return "purchasable";
	if (state == AS_APP_STATE_PURCHASING)
		return "purchasing";
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
 * as_app_scope_from_string:
 * @scope: a source kind string
 *
 * Converts the text representation to an enumerated value.
 *
 * Return value: A #AsAppScope, e.g. %AS_APP_SCOPE_SYSTEM.
 *
 * Since: 0.6.1
 **/
AsAppScope
as_app_scope_from_string (const gchar *scope)
{
	if (g_strcmp0 (scope, "user") == 0)
		return AS_APP_SCOPE_USER;
	if (g_strcmp0 (scope, "system") == 0)
		return AS_APP_SCOPE_SYSTEM;
	return AS_APP_SCOPE_UNKNOWN;
}

/**
 * as_app_scope_to_string:
 * @scope: the #AsAppScope, e.g. %AS_APP_SCOPE_SYSTEM
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @scope, or %NULL for unknown
 *
 * Since: 0.6.1
 **/
const gchar *
as_app_scope_to_string (AsAppScope scope)
{
	if (scope == AS_APP_SCOPE_USER)
		return "user";
	if (scope == AS_APP_SCOPE_SYSTEM)
		return "system";
	return NULL;
}

/**
 * as_app_merge_kind_from_string:
 * @merge_kind: a source kind string
 *
 * Converts the text representation to an enumerated value.
 *
 * Return value: A #AsAppMergeKind, e.g. %AS_APP_MERGE_KIND_REPLACE.
 *
 * Since: 0.6.1
 **/
AsAppMergeKind
as_app_merge_kind_from_string (const gchar *merge_kind)
{
	if (g_strcmp0 (merge_kind, "none") == 0)
		return AS_APP_MERGE_KIND_NONE;
	if (g_strcmp0 (merge_kind, "replace") == 0)
		return AS_APP_MERGE_KIND_REPLACE;
	if (g_strcmp0 (merge_kind, "append") == 0)
		return AS_APP_MERGE_KIND_APPEND;
	return AS_APP_MERGE_KIND_NONE;
}

/**
 * as_app_merge_kind_to_string:
 * @merge_kind: the #AsAppMergeKind, e.g. %AS_APP_MERGE_KIND_REPLACE
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @merge_kind, or %NULL for unknown
 *
 * Since: 0.6.1
 **/
const gchar *
as_app_merge_kind_to_string (AsAppMergeKind merge_kind)
{
	if (merge_kind == AS_APP_MERGE_KIND_NONE)
		return "none";
	if (merge_kind == AS_APP_MERGE_KIND_REPLACE)
		return "replace";
	if (merge_kind == AS_APP_MERGE_KIND_APPEND)
		return "append";
	return NULL;
}

/**
 * as_app_guess_source_kind:
 * @filename: a file name
 *
 * Guesses the source kind based from the filename.
 *
 * Return value: A #AsFormatKind, e.g. %AS_FORMAT_KIND_APPSTREAM.
 *
 * Since: 0.1.8
 **/
AsFormatKind
as_app_guess_source_kind (const gchar *filename)
{
	return as_format_guess_kind (filename);
}

static void
as_app_finalize (GObject *object)
{
	AsApp *app = AS_APP (object);
	AsAppPrivate *priv = GET_PRIVATE (app);

	if (priv->stemmer != NULL)
		g_object_unref (priv->stemmer);
	if (priv->search_blacklist != NULL)
		g_hash_table_unref (priv->search_blacklist);

	if (priv->icon_path != NULL)
		as_ref_string_unref (priv->icon_path);
	if (priv->id_filename != NULL)
		as_ref_string_unref (priv->id_filename);
	if (priv->id != NULL)
		as_ref_string_unref (priv->id);
	if (priv->project_group != NULL)
		as_ref_string_unref (priv->project_group);
	if (priv->project_license != NULL)
		as_ref_string_unref (priv->project_license);
	if (priv->metadata_license != NULL)
		as_ref_string_unref (priv->metadata_license);
	if (priv->origin != NULL)
		as_ref_string_unref (priv->origin);
	if (priv->source_pkgname != NULL)
		as_ref_string_unref (priv->source_pkgname);
	if (priv->update_contact != NULL)
		as_ref_string_unref (priv->update_contact);
	g_free (priv->unique_id);
	if (priv->branch != NULL)
		as_ref_string_unref (priv->branch);
	g_hash_table_unref (priv->comments);
	g_hash_table_unref (priv->developer_names);
	g_hash_table_unref (priv->descriptions);
	g_hash_table_unref (priv->keywords);
	g_hash_table_unref (priv->languages);
	g_hash_table_unref (priv->metadata);
	g_hash_table_unref (priv->names);
	g_hash_table_unref (priv->urls);
	g_hash_table_unref (priv->token_cache);
	g_ptr_array_unref (priv->addons);
	g_ptr_array_unref (priv->categories);
	g_ptr_array_unref (priv->compulsory_for_desktops);
	g_ptr_array_unref (priv->content_ratings);
	g_ptr_array_unref (priv->agreements);
	g_ptr_array_unref (priv->extends);
	g_ptr_array_unref (priv->kudos);
	g_ptr_array_unref (priv->permissions);
	g_ptr_array_unref (priv->formats);
	g_ptr_array_unref (priv->mimetypes);
	g_ptr_array_unref (priv->pkgnames);
	g_ptr_array_unref (priv->architectures);
	g_ptr_array_unref (priv->releases);
	g_ptr_array_unref (priv->provides);
	g_ptr_array_unref (priv->launchables);
	g_ptr_array_unref (priv->screenshots);
	g_ptr_array_unref (priv->reviews);
	g_ptr_array_unref (priv->icons);
	g_ptr_array_unref (priv->bundles);
	g_ptr_array_unref (priv->translations);
	g_ptr_array_unref (priv->suggests);
	g_ptr_array_unref (priv->requires);
	g_ptr_array_unref (priv->vetos);

	G_OBJECT_CLASS (as_app_parent_class)->finalize (object);
}

static void
as_app_init (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->categories = g_ptr_array_new_with_free_func ((GDestroyNotify) as_ref_string_unref);
	priv->compulsory_for_desktops = g_ptr_array_new_with_free_func ((GDestroyNotify) as_ref_string_unref);
	priv->content_ratings = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->agreements = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->extends = g_ptr_array_new_with_free_func ((GDestroyNotify) as_ref_string_unref);
	priv->keywords = g_hash_table_new_full (g_str_hash, g_str_equal,
						(GDestroyNotify) as_ref_string_unref,
						(GDestroyNotify) g_ptr_array_unref);
	priv->kudos = g_ptr_array_new_with_free_func ((GDestroyNotify) as_ref_string_unref);
	priv->permissions = g_ptr_array_new_with_free_func ((GDestroyNotify) as_ref_string_unref);
	priv->formats = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->mimetypes = g_ptr_array_new_with_free_func ((GDestroyNotify) as_ref_string_unref);
	priv->pkgnames = g_ptr_array_new_with_free_func ((GDestroyNotify) as_ref_string_unref);
	priv->architectures = g_ptr_array_new_with_free_func ((GDestroyNotify) as_ref_string_unref);
	priv->addons = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->releases = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->provides = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->launchables = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->screenshots = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->reviews = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->icons = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->bundles = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->translations = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->suggests = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->requires = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->vetos = g_ptr_array_new_with_free_func ((GDestroyNotify) as_ref_string_unref);

	priv->comments = g_hash_table_new_full (g_str_hash, g_str_equal,
						(GDestroyNotify) as_ref_string_unref,
						(GDestroyNotify) as_ref_string_unref);
	priv->developer_names = g_hash_table_new_full (g_str_hash, g_str_equal,
						       (GDestroyNotify) as_ref_string_unref,
						       (GDestroyNotify) as_ref_string_unref);
	priv->descriptions = g_hash_table_new_full (g_str_hash, g_str_equal,
						    (GDestroyNotify) as_ref_string_unref,
						    (GDestroyNotify) as_ref_string_unref);
	priv->languages = g_hash_table_new_full (g_str_hash, g_str_equal,
						 (GDestroyNotify) as_ref_string_unref, NULL);
	priv->metadata = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						(GDestroyNotify) as_ref_string_unref,
						(GDestroyNotify) as_ref_string_unref);
	priv->names = g_hash_table_new_full (g_str_hash, g_str_equal,
					     (GDestroyNotify) as_ref_string_unref,
					     (GDestroyNotify) as_ref_string_unref);
	priv->urls = g_hash_table_new_full (g_str_hash, g_str_equal,
					    (GDestroyNotify) as_ref_string_unref,
					    (GDestroyNotify) as_ref_string_unref);
	priv->token_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
						   (GDestroyNotify) as_ref_string_unref,
						   g_free);
	priv->search_match = AS_APP_SEARCH_MATCH_LAST;
}

static void
as_app_class_init (AsAppClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = as_app_finalize;
}

/******************************************************************************/

static gboolean
as_app_equal_int (guint v1, guint v2)
{
	if (v1 == 0 || v2 == 0)
		return TRUE;
	return v1 == v2;
}

static gboolean
as_app_equal_str (const gchar *v1, const gchar *v2)
{
	if (v1 == NULL || v2 == NULL)
		return TRUE;
	return g_strcmp0 (v1, v2) == 0;
}

static gboolean
as_app_equal_array_str (GPtrArray *v1, GPtrArray *v2)
{
	if (v1->len == 0 || v2->len == 0)
		return TRUE;
	return g_strcmp0 (g_ptr_array_index (v1, 0),
			  g_ptr_array_index (v2, 0)) == 0;
}

AsBundleKind
as_app_get_bundle_kind (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* prefer bundle */
	if (priv->bundles->len > 0) {
		AsBundle *bundle = g_ptr_array_index (priv->bundles, 0);
		if (as_bundle_get_kind (bundle) != AS_BUNDLE_KIND_UNKNOWN)
			return as_bundle_get_kind (bundle);
	}

	/* fallback to packages */
	if (priv->pkgnames->len > 0)
		return AS_BUNDLE_KIND_PACKAGE;

	/* nothing */
	return AS_BUNDLE_KIND_UNKNOWN;
}

/**
 * as_app_equal:
 * @app1: a #AsApp instance.
 * @app2: another #AsApp instance.
 *
 * Compare one application with another for equality using the following
 * properties:
 *
 *  1. scope, e.g. `system` or `user`
 *  2. bundle kind, e.g. `package` or `flatpak`
 *  3. origin, e.g. `fedora` or `gnome-apps-nightly`
 *  4. kind, e.g. `app` or `runtime`
 *  5. AppStream ID, e.g. `gimp.desktop`
 *  6. branch, e.g. `stable` or `master`
 *
 * Returns: %TRUE if the applications are equal
 *
 * Since: 0.6.1
 **/
gboolean
as_app_equal (AsApp *app1, AsApp *app2)
{
	AsAppPrivate *priv1 = GET_PRIVATE (app1);
	AsAppPrivate *priv2 = GET_PRIVATE (app2);

	g_return_val_if_fail (AS_IS_APP (app1), FALSE);
	g_return_val_if_fail (AS_IS_APP (app2), FALSE);

	if (app1 == app2)
		return TRUE;
	if (!as_app_equal_int (priv1->scope, priv2->scope))
		return FALSE;
	if (!as_app_equal_int (priv1->kind, priv2->kind))
		return FALSE;
	if (!as_app_equal_str (priv1->id_filename, priv2->id_filename))
		return FALSE;
	if (!as_app_equal_str (priv1->origin, priv2->origin))
		return FALSE;
	if (!as_app_equal_str (priv1->branch, priv2->branch))
		return FALSE;
	if (!as_app_equal_array_str (priv1->architectures,
				     priv2->architectures))
		return FALSE;
	if (!as_app_equal_int (as_app_get_bundle_kind (app1),
			       as_app_get_bundle_kind (app2)))
		return FALSE;
	return TRUE;
}

/**
 * as_app_get_unique_id:
 * @app: a #AsApp instance.
 *
 * Gets the unique ID value to represent the component.
 *
 * Returns: the unique ID, e.g. `system/package/fedora/desktop/gimp.desktop/master`
 *
 * Since: 0.6.1
 **/
const gchar *
as_app_get_unique_id (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (priv->unique_id == NULL || !priv->unique_id_valid) {
		g_free (priv->unique_id);
		if (as_app_has_quirk (app, AS_APP_QUIRK_MATCH_ANY_PREFIX)) {
			priv->unique_id = as_utils_unique_id_build (AS_APP_SCOPE_UNKNOWN,
								    AS_BUNDLE_KIND_UNKNOWN,
								    NULL,
								    priv->kind,
								    as_app_get_id_no_prefix (app),
								    NULL);
		} else {
			priv->unique_id = as_utils_unique_id_build (priv->scope,
								    as_app_get_bundle_kind (app),
								    priv->origin,
								    priv->kind,
								    as_app_get_id_no_prefix (app),
								    priv->branch);
		}
		priv->unique_id_valid = TRUE;
	}
	return priv->unique_id;
}

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
 * as_app_get_id_no_prefix:
 * @app: a #AsApp instance.
 *
 * Gets the full ID value, stripping any prefix.
 *
 * Returns: the ID, e.g. "org.gnome.Software.desktop"
 *
 * Since: 0.5.12
 **/
const gchar *
as_app_get_id_no_prefix (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	gchar *tmp;
	if (priv->id == NULL)
		return NULL;
	tmp = g_strrstr (priv->id, ":");
	if (tmp != NULL)
		return tmp + 1;
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
 * as_app_has_compulsory_for_desktop:
 * @app: a #AsApp instance.
 * @desktop: a desktop string, e.g. "GNOME"
 *
 * Searches the compulsory for desktop list for a specific item.
 *
 * Returns: %TRUE if the application is compulsory for a specific desktop
 *
 * Since: 0.5.12
 */
gboolean
as_app_has_compulsory_for_desktop (AsApp *app, const gchar *desktop)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	const gchar *tmp;
	guint i;

	for (i = 0; i < priv->compulsory_for_desktops->len; i++) {
		tmp = g_ptr_array_index (priv->compulsory_for_desktops, i);
		if (g_strcmp0 (tmp, desktop) == 0)
			return TRUE;
	}
	return FALSE;
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
 * as_app_get_format_default:
 * @app: a #AsApp instance.
 *
 * Returns the default format.
 *
 * Returns: (transfer none): A #AsFormat, or %NULL if not found
 *
 * Since: 0.6.9
 */
AsFormat *
as_app_get_format_default (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (priv->formats->len > 0) {
		AsFormat *format = g_ptr_array_index (priv->formats, 0);
		return format;
	}
	return NULL;
}

/**
 * as_app_get_format_by_filename:
 * @app: a #AsApp instance.
 * @filename: a filename, e.g. "/home/hughsie/dave.desktop"
 *
 * Searches the list of formats for a specific filename.
 *
 * Returns: (transfer none): A #AsFormat, or %NULL if not found
 *
 * Since: 0.6.9
 */
AsFormat *
as_app_get_format_by_filename (AsApp *app, const gchar *filename)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	for (guint i = 0; i < priv->formats->len; i++) {
		AsFormat *format = g_ptr_array_index (priv->formats, i);
		if (g_strcmp0 (as_format_get_filename (format), filename) == 0)
			return format;
	}
	return NULL;
}

/**
 * as_app_get_format_by_kind:
 * @app: a #AsApp instance.
 * @kind: a #AsFormatKind, e.g. %AS_FORMAT_KIND_APPDATA
 *
 * Searches the list of formats for a specific format kind.
 *
 * Returns: (transfer none): A #AsFormat, or %NULL if not found
 *
 * Since: 0.6.9
 */
AsFormat *
as_app_get_format_by_kind (AsApp *app, AsFormatKind kind)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	for (guint i = 0; i < priv->formats->len; i++) {
		AsFormat *format = g_ptr_array_index (priv->formats, i);
		if (as_format_get_kind (format) == kind)
			return format;
	}
	return NULL;
}

/**
 * as_app_get_keywords:
 * @app: a #AsApp instance.
 * @locale: (nullable): the locale. e.g. "en_GB"
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
 * as_app_get_formats:
 * @app: a #AsApp instance.
 *
 * Gets any formats that make up the application.
 *
 * Returns: (element-type utf8) (transfer none): an array
 *
 * Since: 0.6.9
 **/
GPtrArray *
as_app_get_formats (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->formats;
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
 * as_app_get_release_by_version:
 * @app: a #AsApp instance.
 * @version: a release version number, e.g. "1.2.3"
 *
 * Gets a specific release from the application.
 *
 * Returns: (transfer none): a release, or %NULL
 *
 * Since: 0.7.3
 **/
AsRelease *
as_app_get_release_by_version (AsApp *app, const gchar *version)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	for (guint i = 0; i < priv->releases->len; i++) {
		AsRelease *release_tmp = g_ptr_array_index (priv->releases, i);
		if (g_strcmp0 (version, as_release_get_version (release_tmp)) == 0)
			return release_tmp;
	}
	return NULL;
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
 * as_app_get_launchables:
 * @app: a #AsApp instance.
 *
 * Gets all the launchables the application has.
 *
 * Returns: (element-type AsLaunchable) (transfer none): an array
 *
 * Since: 0.6.13
 **/
GPtrArray *
as_app_get_launchables (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->launchables;
}

/**
 * as_app_get_launchable_by_kind:
 * @app: a #AsApp instance.
 * @kind: a #AsLaunchableKind, e.g. %AS_FORMAT_KIND_APPDATA
 *
 * Searches the list of launchables for a specific launchable kind.
 *
 * Returns: (transfer none): A #AsLaunchable, or %NULL if not found
 *
 * Since: 0.6.13
 */
AsLaunchable *
as_app_get_launchable_by_kind (AsApp *app, AsLaunchableKind kind)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	for (guint i = 0; i < priv->launchables->len; i++) {
		AsLaunchable *launchable = g_ptr_array_index (priv->launchables, i);
		if (as_launchable_get_kind (launchable) == kind)
			return launchable;
	}
	return NULL;
}

/**
 * as_app_get_launchable_default:
 * @app: a #AsApp instance.
 *
 * Returns the default launchable.
 *
 * Returns: (transfer none): A #AsLaunchable, or %NULL if not found
 *
 * Since: 0.6.13
 */
AsLaunchable *
as_app_get_launchable_default (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (priv->launchables->len > 0) {
		AsLaunchable *launchable = g_ptr_array_index (priv->launchables, 0);
		return launchable;
	}
	return NULL;
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
 * as_app_get_screenshot_default:
 * @app: a #AsApp instance.
 *
 * Gets the default screenshot for the component.
 *
 * Returns: (transfer none): a screenshot or %NULL
 *
 * Since: 0.7.3
 **/
AsScreenshot *
as_app_get_screenshot_default (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (priv->screenshots->len == 0)
		return NULL;
	return AS_SCREENSHOT (g_ptr_array_index (priv->screenshots, 0));
}

/**
 * as_app_get_reviews:
 * @app: a #AsApp instance.
 *
 * Gets any reviews the application has defined.
 *
 * Returns: (element-type AsScreenshot) (transfer none): an array
 *
 * Since: 0.6.1
 **/
GPtrArray *
as_app_get_reviews (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->reviews;
}

/**
 * as_app_get_content_ratings:
 * @app: a #AsApp instance.
 *
 * Gets any content_ratings the application has defined.
 *
 * Returns: (element-type AsContentRating) (transfer none): an array
 *
 * Since: 0.5.12
 **/
GPtrArray *
as_app_get_content_ratings (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->content_ratings;
}

/**
 * as_app_get_content_rating:
 * @app: a #AsApp instance.
 * @kind: a ratings kind, e.g. "oars-1.0"
 *
 * Gets a content ratings the application has defined of a specific type.
 *
 * Returns: (transfer none): a #AsContentRating or NULL for not found
 *
 * Since: 0.5.12
 **/
AsContentRating *
as_app_get_content_rating (AsApp *app, const gchar *kind)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	guint i;

	for (i = 0; i < priv->content_ratings->len; i++) {
		AsContentRating *content_rating;
		content_rating = g_ptr_array_index (priv->content_ratings, i);
		if (g_strcmp0 (as_content_rating_get_kind (content_rating), kind) == 0)
			return content_rating;
	}
	return NULL;
}

/**
 * as_app_get_agreements:
 * @app: a #AsApp instance.
 *
 * Gets any agreements the application has defined.
 *
 * Returns: (element-type AsAgreement) (transfer none): an array
 *
 * Since: 0.7.8
 **/
GPtrArray *
as_app_get_agreements (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->agreements;
}

/**
 * as_app_get_agreement_by_kind:
 * @app: a #AsApp instance.
 * @kind: an agreement kind, e.g. %AS_AGREEMENT_KIND_EULA
 *
 * Gets a agreement the application has defined of a specific type.
 *
 * Returns: (transfer none): a #AsAgreement or NULL for not found
 *
 * Since: 0.7.8
 **/
AsAgreement *
as_app_get_agreement_by_kind (AsApp *app, AsAgreementKind kind)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	guint i;

	for (i = 0; i < priv->agreements->len; i++) {
		AsAgreement *agreement;
		agreement = g_ptr_array_index (priv->agreements, i);
		if (as_agreement_get_kind (agreement) == kind)
			return agreement;
	}
	return NULL;
}

/**
 * as_app_get_agreement_default:
 * @app: a #AsApp instance.
 *
 * Gets a privacy policys the application has defined of a specific type.
 *
 * Returns: (transfer none): a #AsAgreement or NULL for not found
 *
 * Since: 0.7.8
 **/
AsAgreement *
as_app_get_agreement_default (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (priv->agreements->len < 1)
		return NULL;
	return g_ptr_array_index (priv->agreements, 0);
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
 * as_app_get_translations:
 * @app: a #AsApp instance.
 *
 * Gets any translations the application has defined.
 *
 * Returns: (element-type AsTranslation) (transfer none): an array
 *
 * Since: 0.5.8
 **/
GPtrArray *
as_app_get_translations (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->translations;
}

/**
 * as_app_get_suggests:
 * @app: a #AsApp instance.
 *
 * Gets any suggests the application has defined.
 *
 * Returns: (element-type AsSuggest) (transfer none): an array
 *
 * Since: 0.6.1
 **/
GPtrArray *
as_app_get_suggests (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->suggests;
}

/**
 * as_app_get_requires:
 * @app: a #AsApp instance.
 *
 * Gets any requires the application has defined. A rquirement could be that
 * a firmware version has to be below a defined version or that another
 * application is required to be installed.
 *
 * Returns: (element-type AsRequire) (transfer none): an array
 *
 * Since: 0.6.7
 **/
GPtrArray *
as_app_get_requires (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->requires;
}

/**
 * as_app_get_require_by_value:
 * @app: a #AsApp instance.
 * @kind: a #AsRequireKind, e.g. %AS_REQUIRE_KIND_FIRMWARE
 * @value: a string, or NULL, e.g. `bootloader`
 *
 * Gets a specific requirement for the application.
 *
 * Returns: (transfer none): A #AsRequire, or %NULL for not found
 *
 * Since: 0.6.7
 **/
AsRequire *
as_app_get_require_by_value (AsApp *app, AsRequireKind kind, const gchar *value)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	for (guint i = 0; i < priv->requires->len; i++) {
		AsRequire *req = g_ptr_array_index (priv->requires, i);
		if (as_require_get_kind (req) == kind &&
		    g_strcmp0 (as_require_get_value (req), value) == 0)
			return req;
	}
	return NULL;
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
 * Returns: (transfer none) (element-type utf8 utf8): hash table of metadata
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

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
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
	return priv->kind;
}
G_GNUC_END_IGNORE_DEPRECATIONS

/**
 * as_app_get_kind:
 * @app: a #AsApp instance.
 *
 * Gets the ID kind.
 *
 * Returns: enumerated value
 *
 * Since: 0.5.10
 **/
AsAppKind
as_app_get_kind (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->kind;
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
AsFormatKind
as_app_get_source_kind (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (priv->formats->len > 0) {
		AsFormat *format = g_ptr_array_index (priv->formats, 0);
		return as_format_get_kind (format);
	}
	return AS_FORMAT_KIND_UNKNOWN;
}

/**
 * as_app_get_scope:
 * @app: a #AsApp instance.
 *
 * Gets the scope of the application.
 *
 * Returns: enumerated value
 *
 * Since: 0.6.1
 **/
AsAppScope
as_app_get_scope (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->scope;
}

/**
 * as_app_get_merge_kind:
 * @app: a #AsApp instance.
 *
 * Gets the merge_kind of the application.
 *
 * Returns: enumerated value
 *
 * Since: 0.6.1
 **/
AsAppMergeKind
as_app_get_merge_kind (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->merge_kind;
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
guint32
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
 * @locale: (nullable): the locale. e.g. "en_GB"
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
 * @locale: (nullable): the locale. e.g. "en_GB"
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
 * @locale: (nullable): the locale. e.g. "en_GB"
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
 * @locale: (nullable): the locale. e.g. "en_GB"
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
 * @locale: (nullable): the locale. e.g. "en_GB"
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
	gpointer value = NULL;
	g_auto(GStrv) lang = NULL;

	/* fallback */
	if (locale == NULL)
		locale = "C";

	/* look up full locale */
	if (g_hash_table_lookup_extended (priv->languages,
					  locale, NULL, &value)) {
		return GPOINTER_TO_INT (value);
	}

	/* look up just country code */
	lang = g_strsplit (locale, "_", 2);
	if (g_strv_length (lang) == 2 &&
	    g_hash_table_lookup_extended (priv->languages,
					  lang[0], NULL, &value)) {
		return GPOINTER_TO_INT (value);
	}

	/* failed */
	return -1;
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
 * Gets the default source filename the instance was populated from.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.2.2
 **/
const gchar *
as_app_get_source_file (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (priv->formats->len > 0) {
		AsFormat *format = g_ptr_array_index (priv->formats, 0);
		return as_format_get_filename (format);
	}
	return NULL;
}

/**
 * as_app_get_branch:
 * @app: a #AsApp instance.
 *
 * Gets the branch for the application.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.6.1
 **/
const gchar *
as_app_get_branch (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->branch;
}

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
	guint i;
	const gchar *suffixes[] = {
		".desktop",
		".addon",
		".firmware",
		".shell-extension",
		NULL };

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (id)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}

	/* save full ID */
	as_ref_string_assign_safe (&priv->id, id);

	/* save filename */
	if (priv->id_filename != NULL)
		as_ref_string_unref (priv->id_filename);
	priv->id_filename = as_ref_string_new_copy (as_app_get_id_no_prefix (app));
	g_strdelimit (priv->id_filename, "&<>", '-');
	for (i = 0; suffixes[i] != NULL; i++) {
		tmp = g_strrstr_len (priv->id_filename, -1, suffixes[i]);
		if (tmp != NULL)
			*tmp = '\0';
	}

	/* no longer valid */
	priv->unique_id_valid = FALSE;
}

/**
 * as_app_set_source_kind:
 * @app: a #AsApp instance.
 * @source_kind: the #AsFormatKind.
 *
 * Sets the source kind.
 *
 * Since: 0.1.4
 **/
void
as_app_set_source_kind (AsApp *app, AsFormatKind source_kind)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_autoptr(AsFormat) format = NULL;

	/* already exists */
	if (priv->formats->len > 0) {
		AsFormat *format_tmp = g_ptr_array_index (priv->formats, 0);
		as_format_set_kind (format_tmp, source_kind);
		return;
	}

	/* create something */
	format = as_format_new ();
	as_format_set_kind (format, source_kind);
	as_app_add_format (app, format);
}

/**
 * as_app_set_scope:
 * @app: a #AsApp instance.
 * @scope: the #AsAppScope.
 *
 * Sets the scope of the application.
 *
 * Since: 0.6.1
 **/
void
as_app_set_scope (AsApp *app, AsAppScope scope)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->scope = scope;

	/* no longer valid */
	priv->unique_id_valid = FALSE;
}

/**
 * as_app_set_merge_kind:
 * @app: a #AsApp instance.
 * @merge_kind: the #AsAppMergeKind.
 *
 * Sets the merge kind of the application.
 *
 * Since: 0.6.1
 **/
void
as_app_set_merge_kind (AsApp *app, AsAppMergeKind merge_kind)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->merge_kind = merge_kind;
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
 * @trust_flags: the #AsAppTrustFlags.
 *
 * Sets the check flags, where %AS_APP_TRUST_FLAG_COMPLETE is completely
 * trusted input.
 *
 * Since: 0.2.2
 **/
void
as_app_set_trust_flags (AsApp *app, guint32 trust_flags)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->trust_flags = trust_flags;
}

/**
 * as_app_has_quirk:
 * @app: a #AsApp instance.
 * @quirk: the #AsAppQuirk, e.g. %AS_APP_QUIRK_PROVENANCE
 *
 * Queries to see if an application has a specific attribute.
 *
 * Returns: %TRUE if the application has the attribute
 *
 * Since: 0.5.10
 **/
gboolean
as_app_has_quirk (AsApp *app, AsAppQuirk quirk)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return (priv->quirk & quirk) > 0;
}

/**
 * as_app_add_quirk:
 * @app: a #AsApp instance.
 * @quirk: the #AsAppQuirk, e.g. %AS_APP_QUIRK_PROVENANCE
 *
 * Adds a specific attribute to an application.
 *
 * Since: 0.5.10
 **/
void
as_app_add_quirk (AsApp *app, AsAppQuirk quirk)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->quirk |= quirk;
}

/**
 * as_app_set_kind:
 * @app: a #AsApp instance.
 * @kind: the #AsAppKind.
 *
 * Sets the application kind.
 *
 * Since: 0.5.10
 **/
void
as_app_set_kind (AsApp *app, AsAppKind kind)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->kind = kind;

	/* no longer valid */
	priv->unique_id_valid = FALSE;
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
/**
 * as_app_set_id_kind:
 * @app: a #AsApp instance.
 * @id_kind: the #AsAppKind.
 *
 * Sets the application kind.
 *
 * Since: 0.1.0
 **/
void
as_app_set_id_kind (AsApp *app, AsIdKind id_kind)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->kind = id_kind;
}
G_GNUC_END_IGNORE_DEPRECATIONS

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

	/* check value */
	if (priv->trust_flags != AS_APP_TRUST_FLAG_COMPLETE) {
		if (g_strcmp0 (project_group, "") == 0) {
			priv->problems |= AS_APP_PROBLEM_INVALID_PROJECT_GROUP;
			return;
		}
	}

	as_ref_string_assign_safe (&priv->project_group, project_group);
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

	as_ref_string_assign_safe (&priv->project_license, project_license);
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
	g_autofree gchar *tmp = NULL;
	g_auto(GStrv) tokens = NULL;

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (metadata_license)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}

	/* automatically replace deprecated license names */
	tokens = as_utils_spdx_license_tokenize (metadata_license);
	tmp = as_utils_spdx_license_detokenize (tokens);
	as_ref_string_assign_safe (&priv->metadata_license, tmp);
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
	as_ref_string_assign_safe (&priv->source_pkgname, source_pkgname);
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
	g_autoptr(AsFormat) format = as_format_new ();
	as_format_set_filename (format, source_file);
	as_app_add_format (app, format);
}

/**
 * as_app_set_branch:
 * @app: a #AsApp instance.
 * @branch: the branch, e.g. "master" or "3-16".
 *
 * Set the branch that the instance was sourced from.
 *
 * Since: 0.6.1
 **/
void
as_app_set_branch (AsApp *app, const gchar *branch)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	as_ref_string_assign_safe (&priv->branch, branch);

	/* no longer valid */
	priv->unique_id_valid = FALSE;
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
	as_ref_string_assign_safe (&priv->update_contact, update_contact);
	if (priv->update_contact == NULL)
		return;

	/* keep going until we have no more matches */
	len = (guint) strlen (priv->update_contact);
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
	as_ref_string_assign_safe (&priv->origin, origin);
	priv->unique_id_valid = FALSE;
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

	as_ref_string_assign_safe (&priv->icon_path, icon_path);
}

/**
 * as_app_set_name:
 * @app: a #AsApp instance.
 * @locale: (nullable): the locale. e.g. "en_GB"
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
	g_autoptr(AsRefString) locale_fixed = NULL;

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (name)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}

	/* get fixed locale */
	locale_fixed = as_node_fix_locale (locale);
	if (locale_fixed == NULL)
		return;
	g_hash_table_insert (priv->names,
			     as_ref_string_ref (locale_fixed),
			     as_ref_string_new (name));
}

/**
 * as_app_set_comment:
 * @app: a #AsApp instance.
 * @locale: (nullable): the locale. e.g. "en_GB"
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
	g_autoptr(AsRefString) locale_fixed = NULL;

	g_return_if_fail (comment != NULL);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (comment)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}

	/* get fixed locale */
	locale_fixed = as_node_fix_locale (locale);
	if (locale_fixed == NULL)
		return;
	g_hash_table_insert (priv->comments,
			     as_ref_string_ref (locale_fixed),
			     as_ref_string_new (comment));
}

/**
 * as_app_set_developer_name:
 * @app: a #AsApp instance.
 * @locale: (nullable): the locale. e.g. "en_GB"
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
	g_autoptr(AsRefString) locale_fixed = NULL;

	g_return_if_fail (developer_name != NULL);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (developer_name)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}

	/* get fixed locale */
	locale_fixed = as_node_fix_locale (locale);
	if (locale_fixed == NULL)
		return;
	g_hash_table_insert (priv->developer_names,
			     as_ref_string_ref (locale_fixed),
			     as_ref_string_new (developer_name));
}

/**
 * as_app_set_description:
 * @app: a #AsApp instance.
 * @locale: (nullable): the locale. e.g. "en_GB"
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
	g_autoptr(AsRefString) locale_fixed = NULL;

	g_return_if_fail (description != NULL);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (description)) {
		priv->problems |= AS_APP_PROBLEM_NOT_VALID_UTF8;
		return;
	}

	/* get fixed locale */
	locale_fixed = as_node_fix_locale (locale);
	if (locale_fixed == NULL)
		return;
	g_hash_table_insert (priv->descriptions,
			     as_ref_string_ref (locale_fixed),
			     as_ref_string_new (description));
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

	g_return_if_fail (category != NULL);

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

	g_ptr_array_add (priv->categories, as_ref_string_new (category));
}

/**
 * as_app_remove_category:
 * @app: a #AsApp instance.
 * @category: the category.
 *
 * Removed a menu category from the application.
 *
 * Since: 0.6.13
 **/
void
as_app_remove_category (AsApp *app, const gchar *category)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	for (guint i = 0; i < priv->categories->len; i++) {
		const gchar *tmp = g_ptr_array_index (priv->categories, i);
		if (g_strcmp0 (tmp, category) == 0) {
			g_ptr_array_remove (priv->categories, (gpointer) tmp);
			break;
		}
	}
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

	g_return_if_fail (compulsory_for_desktop != NULL);

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
			 as_ref_string_new (compulsory_for_desktop));
}

/**
 * as_app_add_keyword:
 * @app: a #AsApp instance.
 * @locale: (nullable): the locale. e.g. "en_GB"
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
	g_autoptr(AsRefString) locale_fixed = NULL;

	g_return_if_fail (keyword != NULL);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (keyword)) {
		return;
	}

	/* get fixed locale */
	locale_fixed = as_node_fix_locale (locale);
	if (locale_fixed == NULL)
		return;

	/* create an array if required */
	tmp = g_hash_table_lookup (priv->keywords, locale_fixed);
	if (tmp == NULL) {
		tmp = g_ptr_array_new_with_free_func ((GDestroyNotify) as_ref_string_unref);
		g_hash_table_insert (priv->keywords, as_ref_string_ref (locale_fixed), tmp);
	} else if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0) {
		if (as_ptr_array_find_string (tmp, keyword))
			return;
	}
	g_ptr_array_add (tmp, as_ref_string_new (keyword));

	/* cache already populated */
	if (priv->token_cache_valid) {
		g_warning ("%s has token cache, invaliding as %s was added",
			   as_app_get_unique_id (app), keyword);
		g_hash_table_remove_all (priv->token_cache);
		priv->token_cache_valid = FALSE;
	}
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

	g_return_if_fail (kudo != NULL);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (kudo)) {
		return;
	}
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0 &&
	    as_ptr_array_find_string (priv->kudos, kudo)) {
		return;
	}
	g_ptr_array_add (priv->kudos, as_ref_string_new (kudo));
}

/**
 * as_app_remove_kudo:
 * @app: a #AsApp instance.
 * @kudo: the kudo.
 *
 * Remove a kudo the application has obtained.
 *
 * Since: 0.6.13
 **/
void
as_app_remove_kudo (AsApp *app, const gchar *kudo)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	for (guint i = 0; i < priv->kudos->len; i++) {
		const gchar *tmp = g_ptr_array_index (priv->kudos, i);
		if (g_strcmp0 (tmp, kudo) == 0) {
			g_ptr_array_remove (priv->kudos, (gpointer) tmp);
			break;
		}
	}
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

	g_return_if_fail (permission != NULL);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_VALID_UTF8) > 0 &&
	    !as_app_validate_utf8 (permission)) {
		return;
	}
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0 &&
	    as_ptr_array_find_string (priv->permissions, permission)) {
		return;
	}
	g_ptr_array_add (priv->permissions, as_ref_string_new (permission));
}

static void
as_app_recalculate_state (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	gboolean is_installed = FALSE;
	gboolean is_available = FALSE;
	for (guint i = 0; i < priv->formats->len; i++) {
		AsFormat *format = g_ptr_array_index (priv->formats, i);
		switch (as_format_get_kind (format)) {
		case AS_FORMAT_KIND_APPDATA:
		case AS_FORMAT_KIND_DESKTOP:
		case AS_FORMAT_KIND_METAINFO:
			is_installed = TRUE;
			break;
		case AS_FORMAT_KIND_APPSTREAM:
			is_available = TRUE;
			break;
		default:
			break;
		}
	}
	if (is_installed) {
		as_app_set_state (app, AS_APP_STATE_INSTALLED);
		return;
	}
	if (is_available) {
		as_app_set_state (app, AS_APP_STATE_AVAILABLE);
		return;
	}
	as_app_set_state (app, AS_APP_STATE_UNKNOWN);
}

/**
 * as_app_add_format:
 * @app: a #AsApp instance.
 * @format: the #AsFormat.
 *
 * Add a format the application has been built from.
 *
 * Since: 0.6.9
 **/
void
as_app_add_format (AsApp *app, AsFormat *format)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_return_if_fail (AS_IS_APP (app));
	g_return_if_fail (AS_IS_FORMAT (format));

	/* check for duplicates */
	for (guint i = 0; i < priv->formats->len; i++) {
		AsFormat *fmt = g_ptr_array_index (priv->formats, i);
		if (as_format_equal (fmt, format))
			return;
	}

	/* add */
	g_ptr_array_add (priv->formats, g_object_ref (format));
	as_app_recalculate_state (app);
}

/**
 * as_app_remove_format:
 * @app: a #AsApp instance.
 * @format: the #AsFormat.
 *
 * Removes a format the application has been built from.
 *
 * Since: 0.6.9
 **/
void
as_app_remove_format (AsApp *app, AsFormat *format)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_return_if_fail (AS_IS_APP (app));
	g_return_if_fail (AS_IS_FORMAT (format));
	g_ptr_array_remove (priv->formats, format);
	as_app_recalculate_state (app);
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

	g_return_if_fail (mimetype != NULL);

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

	g_ptr_array_add (priv->mimetypes, as_ref_string_new (mimetype));
}

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

	/* only installed is useful */
	if (as_release_get_state (donor) == AS_RELEASE_STATE_INSTALLED)
		as_release_set_state (release, AS_RELEASE_STATE_INSTALLED);

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
		priv->problems |= AS_APP_PROBLEM_DUPLICATE_RELEASE;
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
 * as_app_add_launchable:
 * @app: a #AsApp instance.
 * @launchable: a #AsLaunchable instance.
 *
 * Adds a launchable to an application.
 *
 * Since: 0.6.13
 **/
void
as_app_add_launchable (AsApp *app, AsLaunchable *launchable)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* check for duplicates */
	if (priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) {
		for (guint i = 0; i < priv->launchables->len; i++) {
			AsLaunchable *lau = g_ptr_array_index (priv->launchables, i);
			if (as_launchable_get_kind (lau) == as_launchable_get_kind (launchable) &&
			    g_strcmp0 (as_launchable_get_value (lau),
				       as_launchable_get_value (launchable)) == 0)
				return;
		}
	}

	g_ptr_array_add (priv->launchables, g_object_ref (launchable));
}

static gint
as_app_sort_screenshots (gconstpointer a, gconstpointer b)
{
	AsScreenshot *ss1 = *((AsScreenshot **) a);
	AsScreenshot *ss2 = *((AsScreenshot **) b);
	if (as_screenshot_get_kind (ss1) < as_screenshot_get_kind (ss2))
		return 1;
	if (as_screenshot_get_kind (ss1) > as_screenshot_get_kind (ss2))
		return -1;
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
			if (as_screenshot_equal (ss, screenshot)) {
				priv->problems |= AS_APP_PROBLEM_DUPLICATE_SCREENSHOT;
				return;
			}
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
 * as_app_add_review:
 * @app: a #AsApp instance.
 * @review: a #AsReview instance.
 *
 * Adds a review to an application.
 *
 * Since: 0.6.1
 **/
void
as_app_add_review (AsApp *app, AsReview *review)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsReview *review_tmp;
	guint i;

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0) {
		for (i = 0; i < priv->reviews->len; i++) {
			review_tmp = g_ptr_array_index (priv->reviews, i);
			if (as_review_equal (review_tmp, review))
				return;
		}
	}
	g_ptr_array_add (priv->reviews, g_object_ref (review));
}

/**
 * as_app_add_content_rating:
 * @app: a #AsApp instance.
 * @content_rating: a #AsContentRating instance.
 *
 * Adds a content_rating to an application.
 *
 * Since: 0.5.12
 **/
void
as_app_add_content_rating (AsApp *app, AsContentRating *content_rating)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0) {
		for (guint i = 0; i < priv->content_ratings->len; i++) {
			AsContentRating *cr_tmp = g_ptr_array_index (priv->content_ratings, i);
			if (g_strcmp0 (as_content_rating_get_kind (cr_tmp),
				       as_content_rating_get_kind (content_rating)) == 0) {
				priv->problems |= AS_APP_PROBLEM_DUPLICATE_CONTENT_RATING;
				return;
			}
		}
	}
	g_ptr_array_add (priv->content_ratings, g_object_ref (content_rating));
}

/**
 * as_app_add_agreement:
 * @app: a #AsApp instance.
 * @agreement: a #AsAgreement instance.
 *
 * Adds a agreement to an application.
 *
 * Since: 0.7.8
 **/
void
as_app_add_agreement (AsApp *app, AsAgreement *agreement)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0) {
		for (guint i = 0; i < priv->agreements->len; i++) {
			AsAgreement *cr_tmp = g_ptr_array_index (priv->agreements, i);
			if (as_agreement_get_kind (cr_tmp) == as_agreement_get_kind (agreement)) {
				priv->problems |= AS_APP_PROBLEM_DUPLICATE_AGREEMENT;
				return;
			}
		}
	}
	g_ptr_array_add (priv->agreements, g_object_ref (agreement));
}

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

static gboolean
as_app_check_translation_duplicate (AsTranslation *translation1, AsTranslation *translation2)
{
	if (as_translation_get_kind (translation1) != as_translation_get_kind (translation2))
		return FALSE;
	if (g_strcmp0 (as_translation_get_id (translation1),
		       as_translation_get_id (translation2)) != 0)
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
		switch (priv->kind) {
		case AS_APP_KIND_DESKTOP:
			as_app_add_kudo_kind (app, AS_KUDO_KIND_HI_DPI_ICON);
			break;
		default:
			break;
		}
	}
	g_ptr_array_add (priv->icons, g_object_ref (icon));
}

static void
as_app_parse_flatpak_id (AsApp *app, const gchar *bundle_id)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_auto(GStrv) split = NULL;

	/* not set */
	if (bundle_id == NULL)
		return;

	/* split into type/id/arch/branch */
	split = g_strsplit (bundle_id, "/", -1);
	if (g_strv_length (split) != 4) {
		g_warning ("invalid flatpak bundle ID: %s", bundle_id);
		return;
	}

	/* only set if not already set */
	if (priv->architectures->len == 0)
		as_app_add_arch (app, split[2]);
	if (priv->branch == NULL)
		as_app_set_branch (app, split[3]);
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

	/* set the architecture and branch */
	switch (as_bundle_get_kind (bundle)) {
	case AS_BUNDLE_KIND_FLATPAK:
		as_app_parse_flatpak_id (app, as_bundle_get_id (bundle));
		break;
	default:
		break;
	}

	g_ptr_array_add (priv->bundles, g_object_ref (bundle));

	/* no longer valid */
	priv->unique_id_valid = FALSE;
}

/**
 * as_app_add_translation:
 * @app: a #AsApp instance.
 * @translation: a #AsTranslation instance.
 *
 * Adds a translation to an application.
 *
 * Since: 0.5.8
 **/
void
as_app_add_translation (AsApp *app, AsTranslation *translation)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0) {
		AsTranslation *bu_tmp;
		guint i;
		for (i = 0; i < priv->translations->len; i++) {
			bu_tmp = g_ptr_array_index (priv->translations, i);
			if (as_app_check_translation_duplicate (translation, bu_tmp))
				return;
		}
	}
	g_ptr_array_add (priv->translations, g_object_ref (translation));
}

/**
 * as_app_add_suggest:
 * @app: a #AsApp instance.
 * @suggest: a #AsSuggest instance.
 *
 * Adds a suggest to an application.
 *
 * Since: 0.6.1
 **/
void
as_app_add_suggest (AsApp *app, AsSuggest *suggest)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_ptr_array_add (priv->suggests, g_object_ref (suggest));
}

/**
 * as_app_add_require:
 * @app: a #AsApp instance.
 * @require: a #AsRequire instance.
 *
 * Adds a require to an application.
 *
 * Since: 0.6.7
 **/
void
as_app_add_require (AsApp *app, AsRequire *require)
{
	AsAppPrivate *priv = GET_PRIVATE (app);

	/* handle untrusted */
	if ((priv->trust_flags & AS_APP_TRUST_FLAG_CHECK_DUPLICATES) > 0) {
		for (guint i = 0; i < priv->requires->len; i++) {
			AsRequire *req_tmp = g_ptr_array_index (priv->requires, i);
			if (as_require_equal (require, req_tmp))
				return;
		}
	}
	g_ptr_array_add (priv->requires, g_object_ref (require));
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

	g_return_if_fail (pkgname != NULL);

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

	g_ptr_array_add (priv->pkgnames, as_ref_string_new (pkgname));

	/* no longer valid */
	priv->unique_id_valid = FALSE;
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

	g_return_if_fail (arch != NULL);

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

	g_ptr_array_add (priv->architectures, as_ref_string_new (arch));
}

/**
 * as_app_add_language:
 * @app: a #AsApp instance.
 * @percentage: the percentage completion of the translation, or 0 for unknown
 * @locale: (nullable): the locale. e.g. "en_GB"
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
			     as_ref_string_new (locale),
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
	if (url == NULL) {
		g_hash_table_remove (priv->urls, as_url_kind_to_string (url_kind));
	} else {
		g_hash_table_insert (priv->urls,
				     as_ref_string_new (as_url_kind_to_string (url_kind)),
				     as_ref_string_new (url));
	}
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
			     as_ref_string_new (key),
			     as_ref_string_new (value));
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

	g_ptr_array_add (priv->extends, as_ref_string_new (extends));
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


static void
as_app_subsume_dict (GHashTable *dest, GHashTable *src, guint64 flags)
{
	GList *l;
	const gchar *tmp;
	const gchar *key;
	const gchar *value;
	g_autoptr(GList) keys = NULL;

	keys = g_hash_table_get_keys (src);
	if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 && keys != NULL)
		g_hash_table_remove_all (dest);
	for (l = keys; l != NULL; l = l->next) {
		key = l->data;
		if (flags & AS_APP_SUBSUME_FLAG_NO_OVERWRITE) {
			tmp = g_hash_table_lookup (dest, key);
			if (tmp != NULL)
				continue;
		}
		value = g_hash_table_lookup (src, key);
		g_hash_table_insert (dest, as_ref_string_new (key), as_ref_string_new (value));
	}
}

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

static void
as_app_subsume_private (AsApp *app, AsApp *donor, guint64 flags)
{
	AsAppPrivate *priv = GET_PRIVATE (donor);
	AsAppPrivate *papp = GET_PRIVATE (app);
	const gchar *tmp;
	const gchar *key;
	guint i;

	/* stop us shooting ourselves in the foot */
	papp->trust_flags |= AS_APP_TRUST_FLAG_CHECK_DUPLICATES;

	/* id-kind */
	if (flags & AS_APP_SUBSUME_FLAG_KIND) {
		if (papp->kind == AS_APP_KIND_UNKNOWN)
			as_app_set_kind (app, priv->kind);
	}

	/* AppData or AppStream can overwrite the id-kind of desktop files */
	if (flags & AS_APP_SUBSUME_FLAG_SOURCE_KIND) {
		if ((as_app_get_format_by_kind (donor, AS_FORMAT_KIND_APPDATA) != NULL ||
		     as_app_get_format_by_kind (donor, AS_FORMAT_KIND_APPSTREAM) != NULL) &&
		    as_app_get_format_by_kind (app, AS_FORMAT_KIND_DESKTOP) != NULL)
			as_app_set_kind (app, priv->kind);
	}

	/* state */
	if (flags & AS_APP_SUBSUME_FLAG_STATE) {
		if (papp->state == AS_APP_STATE_UNKNOWN)
			as_app_set_state (app, priv->state);
	}

	/* pkgnames */
	if (flags & AS_APP_SUBSUME_FLAG_BUNDLES) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->pkgnames->len > 0)
			g_ptr_array_set_size (papp->pkgnames, 0);
		for (i = 0; i < priv->pkgnames->len; i++) {
			tmp = g_ptr_array_index (priv->pkgnames, i);
			as_app_add_pkgname (app, tmp);
		}
	}

	/* bundles */
	if (flags & AS_APP_SUBSUME_FLAG_BUNDLES) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->bundles->len > 0)
			g_ptr_array_set_size (papp->bundles, 0);
		for (i = 0; i < priv->bundles->len; i++) {
			AsBundle *bundle = g_ptr_array_index (priv->bundles, i);
			as_app_add_bundle (app, bundle);
		}
	}

	/* translations */
	if (flags & AS_APP_SUBSUME_FLAG_TRANSLATIONS) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->translations->len > 0)
			g_ptr_array_set_size (papp->translations, 0);
		for (i = 0; i < priv->translations->len; i++) {
			AsTranslation *tr = g_ptr_array_index (priv->translations, i);
			as_app_add_translation (app, tr);
		}
	}

	/* suggests */
	if (flags & AS_APP_SUBSUME_FLAG_SUGGESTS) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->suggests->len > 0)
			g_ptr_array_set_size (papp->suggests, 0);
		for (i = 0; i < priv->suggests->len; i++) {
			AsSuggest *suggest = g_ptr_array_index (priv->suggests, i);
			as_app_add_suggest (app, suggest);
		}
	}

	/* requires */
	if (flags & AS_APP_SUBSUME_FLAG_SUGGESTS) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->requires->len > 0)
			g_ptr_array_set_size (papp->requires, 0);
		for (i = 0; i < priv->requires->len; i++) {
			AsRequire *require = g_ptr_array_index (priv->requires, i);
			as_app_add_require (app, require);
		}
	}

	/* releases */
	if (flags & AS_APP_SUBSUME_FLAG_RELEASES) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->releases->len > 0)
			g_ptr_array_set_size (papp->releases, 0);
		for (i = 0; i < priv->releases->len; i++) {
			AsRelease *rel= g_ptr_array_index (priv->releases, i);
			as_app_add_release (app, rel);
		}
	}

	/* kudos */
	if (flags & AS_APP_SUBSUME_FLAG_KUDOS) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->kudos->len > 0)
			g_ptr_array_set_size (papp->kudos, 0);
		for (i = 0; i < priv->kudos->len; i++) {
			tmp = g_ptr_array_index (priv->kudos, i);
			as_app_add_kudo (app, tmp);
		}
	}

	/* categories */
	if (flags & AS_APP_SUBSUME_FLAG_CATEGORIES) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->categories->len > 0)
			g_ptr_array_set_size (papp->categories, 0);
		for (i = 0; i < priv->categories->len; i++) {
			tmp = g_ptr_array_index (priv->categories, i);
			as_app_add_category (app, tmp);
		}
	}

	/* permissions */
	if (flags & AS_APP_SUBSUME_FLAG_PERMISSIONS) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->permissions->len > 0)
			g_ptr_array_set_size (papp->permissions, 0);
		for (i = 0; i < priv->permissions->len; i++) {
			tmp = g_ptr_array_index (priv->permissions, i);
			as_app_add_permission (app, tmp);
		}
	}

	/* extends */
	if (flags & AS_APP_SUBSUME_FLAG_EXTENDS) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->extends->len > 0)
			g_ptr_array_set_size (papp->extends, 0);
		for (i = 0; i < priv->extends->len; i++) {
			tmp = g_ptr_array_index (priv->extends, i);
			as_app_add_extends (app, tmp);
		}
	}

	/* compulsory_for_desktops */
	if (flags & AS_APP_SUBSUME_FLAG_COMPULSORY) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->compulsory_for_desktops->len > 0)
			g_ptr_array_set_size (papp->compulsory_for_desktops, 0);
		for (i = 0; i < priv->compulsory_for_desktops->len; i++) {
			tmp = g_ptr_array_index (priv->compulsory_for_desktops, i);
			as_app_add_compulsory_for_desktop (app, tmp);
		}
	}

	/* screenshots */
	if (flags & AS_APP_SUBSUME_FLAG_SCREENSHOTS) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->screenshots->len > 0)
			g_ptr_array_set_size (papp->screenshots, 0);
		for (i = 0; i < priv->screenshots->len; i++) {
			AsScreenshot *ss = g_ptr_array_index (priv->screenshots, i);
			as_app_add_screenshot (app, ss);
		}
	}

	/* reviews */
	if (flags & AS_APP_SUBSUME_FLAG_REVIEWS) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->reviews->len > 0)
			g_ptr_array_set_size (papp->reviews, 0);
		for (i = 0; i < priv->reviews->len; i++) {
			AsReview *review = g_ptr_array_index (priv->reviews, i);
			as_app_add_review (app, review);
		}
	}

	/* content_ratings */
	if (flags & AS_APP_SUBSUME_FLAG_CONTENT_RATINGS) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->content_ratings->len > 0)
			g_ptr_array_set_size (papp->content_ratings, 0);
		for (i = 0; i < priv->content_ratings->len; i++) {
			AsContentRating *content_rating;
			content_rating = g_ptr_array_index (priv->content_ratings, i);
			as_app_add_content_rating (app, content_rating);
		}
	}

	/* agreements */
	if (flags & AS_APP_SUBSUME_FLAG_AGREEMENTS) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->agreements->len > 0)
			g_ptr_array_set_size (papp->agreements, 0);
		for (i = 0; i < priv->agreements->len; i++) {
			AsAgreement *agreement;
			agreement = g_ptr_array_index (priv->agreements, i);
			as_app_add_agreement (app, agreement);
		}
	}

	/* provides */
	if (flags & AS_APP_SUBSUME_FLAG_PROVIDES) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->provides->len > 0)
			g_ptr_array_set_size (papp->provides, 0);
		for (i = 0; i < priv->provides->len; i++) {
			AsProvide *pr = g_ptr_array_index (priv->provides, i);
			as_app_add_provide (app, pr);
		}
	}

	/* launchables */
	if (flags & AS_APP_SUBSUME_FLAG_LAUNCHABLES) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->launchables->len > 0)
			g_ptr_array_set_size (papp->launchables, 0);
		for (i = 0; i < priv->launchables->len; i++) {
			AsLaunchable *lau = g_ptr_array_index (priv->launchables, i);
			as_app_add_launchable (app, lau);
		}
	}

	/* icons */
	if (flags & AS_APP_SUBSUME_FLAG_ICONS) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->icons->len > 0)
			g_ptr_array_set_size (papp->icons, 0);
		for (i = 0; i < priv->icons->len; i++) {
			AsIcon *ic = g_ptr_array_index (priv->icons, i);
			as_app_subsume_icon (app, ic);
		}
	}

	/* mimetypes */
	if (flags & AS_APP_SUBSUME_FLAG_MIMETYPES) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->mimetypes->len > 0)
			g_ptr_array_set_size (papp->mimetypes, 0);
		for (i = 0; i < priv->mimetypes->len; i++) {
			tmp = g_ptr_array_index (priv->mimetypes, i);
			as_app_add_mimetype (app, tmp);
		}
	}

	/* vetos */
	if (flags & AS_APP_SUBSUME_FLAG_VETOS) {
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 &&
		    priv->vetos->len > 0)
			g_ptr_array_set_size (papp->vetos, 0);
		for (i = 0; i < priv->vetos->len; i++) {
			tmp = g_ptr_array_index (priv->vetos, i);
			as_app_add_veto (app, "%s", tmp);
		}
	}

	/* languages */
	if (flags & AS_APP_SUBSUME_FLAG_LANGUAGES) {
		GList *l;
		g_autoptr(GList) keys = g_hash_table_get_keys (priv->languages);
		if ((flags & AS_APP_SUBSUME_FLAG_REPLACE) > 0 && keys != NULL)
			g_hash_table_remove_all (papp->languages);
		for (l = keys; l != NULL; l = l->next) {
			gint percentage;
			key = l->data;
			if (flags & AS_APP_SUBSUME_FLAG_NO_OVERWRITE) {
				percentage = as_app_get_language (app, key);
				if (percentage >= 0)
					continue;
			}
			percentage = GPOINTER_TO_INT (g_hash_table_lookup (priv->languages, key));
			as_app_add_language (app, percentage, key);
		}
	}

	/* dictionaries */
	if (flags & AS_APP_SUBSUME_FLAG_NAME)
		as_app_subsume_dict (papp->names, priv->names, flags);
	if (flags & AS_APP_SUBSUME_FLAG_COMMENT)
		as_app_subsume_dict (papp->comments, priv->comments, flags);
	if (flags & AS_APP_SUBSUME_FLAG_DEVELOPER_NAME)
		as_app_subsume_dict (papp->developer_names, priv->developer_names, flags);
	if (flags & AS_APP_SUBSUME_FLAG_DESCRIPTION)
		as_app_subsume_dict (papp->descriptions, priv->descriptions, flags);
	if (flags & AS_APP_SUBSUME_FLAG_METADATA)
		as_app_subsume_dict (papp->metadata, priv->metadata, flags);
	if (flags & AS_APP_SUBSUME_FLAG_URL)
		as_app_subsume_dict (papp->urls, priv->urls, flags);
	if (flags & AS_APP_SUBSUME_FLAG_KEYWORDS)
		as_app_subsume_keywords (app, donor, flags);

	/* branch */
	if (flags & AS_APP_SUBSUME_FLAG_BRANCH) {
		if (priv->branch != NULL)
			as_app_set_branch (app, priv->branch);
	}

	/* formats */
	if (flags & AS_APP_SUBSUME_FLAG_FORMATS) {
		for (i = 0; i < priv->formats->len; i++) {
			AsFormat *fmt = g_ptr_array_index (priv->formats, i);
			as_app_add_format (app, fmt);
		}
	}

	/* source_pkgname */
	if (flags & AS_APP_SUBSUME_FLAG_BUNDLES) {
		if (priv->source_pkgname != NULL)
			as_app_set_source_pkgname (app, priv->source_pkgname);
	}

	/* origin */
	if (flags & AS_APP_SUBSUME_FLAG_ORIGIN) {
		if (priv->origin != NULL)
			as_app_set_origin (app, priv->origin);
	}

	/* licenses */
	if (flags & AS_APP_SUBSUME_FLAG_PROJECT_LICENSE) {
		if (priv->project_license != NULL)
			as_app_set_project_license (app, priv->project_license);
	}
	if (flags & AS_APP_SUBSUME_FLAG_METADATA_LICENSE) {
		if (priv->metadata_license != NULL)
			as_app_set_metadata_license (app, priv->metadata_license);
	}

	/* project_group */
	if (flags & AS_APP_SUBSUME_FLAG_PROJECT_GROUP) {
		if (priv->project_group != NULL)
			as_app_set_project_group (app, priv->project_group);
	}
}

/**
 * as_app_subsume_full:
 * @app: a #AsApp instance.
 * @donor: the donor.
 * @flags: any optional #AsAppSubsumeFlags, e.g. %AS_APP_SUBSUME_FLAG_NO_OVERWRITE
 *
 * Copies information from the donor to the application object.
 *
 * Since: 0.1.4
 **/
void
as_app_subsume_full (AsApp *app, AsApp *donor, guint64 flags)
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
	as_app_subsume_full (app, donor, AS_APP_SUBSUME_FLAG_DEDUPE);
}

static gint
gs_app_node_language_sort_cb (gconstpointer a, gconstpointer b)
{
	return g_strcmp0 ((const gchar *) a, (const gchar *) b);
}

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

static gint
as_app_ptr_array_sort_cb (gconstpointer a, gconstpointer b)
{
	return g_strcmp0 (*((const gchar **) a), *((const gchar **) b));
}

static gint
as_app_releases_sort_cb (gconstpointer a, gconstpointer b)
{
	AsRelease **rel1 = (AsRelease **) a;
	AsRelease **rel2 = (AsRelease **) b;
	return as_release_vercmp (*rel1, *rel2);
}

static gint
as_app_provides_sort_cb (gconstpointer a, gconstpointer b)
{
	AsProvide *prov1 = *((AsProvide **) a);
	AsProvide *prov2 = *((AsProvide **) b);

	if (as_provide_get_kind (prov1) < as_provide_get_kind (prov2))
		return -1;
	if (as_provide_get_kind (prov1) > as_provide_get_kind (prov2))
		return 1;
	return g_strcmp0 (as_provide_get_value (prov1),
			  as_provide_get_value (prov2));
}

static gint
as_app_launchables_sort_cb (gconstpointer a, gconstpointer b)
{
	AsLaunchable *lau1 = *((AsLaunchable **) a);
	AsLaunchable *lau2 = *((AsLaunchable **) b);

	if (as_launchable_get_kind (lau1) < as_launchable_get_kind (lau2))
		return -1;
	if (as_launchable_get_kind (lau1) > as_launchable_get_kind (lau2))
		return 1;
	return g_strcmp0 (as_launchable_get_value (lau1),
			  as_launchable_get_value (lau2));
}

static gint
as_app_icons_sort_cb (gconstpointer a, gconstpointer b)
{
	AsIcon **ic1 = (AsIcon **) a;
	AsIcon **ic2 = (AsIcon **) b;
	return g_strcmp0 (as_icon_get_name (*ic1), as_icon_get_name (*ic2));
}

static gint
as_app_list_sort_cb (gconstpointer a, gconstpointer b)
{
	return g_strcmp0 ((const gchar *) a, (const gchar *) b);
}

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
					      (GDestroyNotify) as_ref_string_unref, NULL);
	keywords = g_hash_table_lookup (priv->keywords, "C");
	if (keywords != NULL) {
		for (i = 0; i < keywords->len; i++) {
			tmp = g_ptr_array_index (keywords, i);
			g_hash_table_insert (already_in_c,
					     as_ref_string_new (tmp),
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
	if (priv->kind != AS_APP_KIND_UNKNOWN) {
		as_node_add_attribute (node_app,
				       "type",
				       as_app_kind_to_string (priv->kind));
	}

	/* merge type */
	if (priv->merge_kind != AS_APP_MERGE_KIND_UNKNOWN &&
	    priv->merge_kind != AS_APP_MERGE_KIND_NONE) {
		as_node_add_attribute (node_app,
				       "merge",
				       as_app_merge_kind_to_string (priv->merge_kind));
	}

	/* <id> */
	if (priv->id != NULL)
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

	/* <translation> */
	for (i = 0; i < priv->translations->len; i++) {
		AsTranslation *bu = g_ptr_array_index (priv->translations, i);
		as_translation_node_insert (bu, node_app, ctx);
	}

	/* <suggests> */
	for (i = 0; i < priv->suggests->len; i++) {
		AsSuggest *suggest = g_ptr_array_index (priv->suggests, i);
		as_suggest_node_insert (suggest, node_app, ctx);
	}

	/* <requires> */
	if (priv->requires->len > 0) {
		node_tmp = as_node_insert (node_app, "requires", NULL, 0, NULL);
		for (i = 0; i < priv->requires->len; i++) {
			AsRequire *require = g_ptr_array_index (priv->requires, i);
			as_require_node_insert (require, node_tmp, ctx);
		}
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
	if (as_node_context_get_output (ctx) == AS_FORMAT_KIND_APPDATA ||
	    as_node_context_get_output (ctx) == AS_FORMAT_KIND_METAINFO) {
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

	/* <reviews> */
	if (priv->reviews->len > 0) {
		AsReview *review;
		node_tmp = as_node_insert (node_app, "reviews", NULL, 0, NULL);
		for (i = 0; i < priv->reviews->len; i++) {
			review = g_ptr_array_index (priv->reviews, i);
			as_review_node_insert (review, node_tmp, ctx);
		}
	}

	/* <content_ratings> */
	if (priv->content_ratings->len > 0) {
		for (i = 0; i < priv->content_ratings->len; i++) {
			AsContentRating *content_rating;
			content_rating = g_ptr_array_index (priv->content_ratings, i);
			as_content_rating_node_insert (content_rating, node_app, ctx);
		}
	}

	/* <agreements> */
	if (priv->agreements->len > 0) {
		for (i = 0; i < priv->agreements->len; i++) {
			AsAgreement *agreement;
			agreement = g_ptr_array_index (priv->agreements, i);
			as_agreement_node_insert (agreement, node_app, ctx);
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
		g_ptr_array_sort (priv->provides, as_app_provides_sort_cb);
		node_tmp = as_node_insert (node_app, "provides", NULL, 0, NULL);
		for (i = 0; i < priv->provides->len; i++) {
			provide = g_ptr_array_index (priv->provides, i);
			as_provide_node_insert (provide, node_tmp, ctx);
		}
	}

	/* <launchables> */
	if (priv->launchables->len > 0) {
		g_ptr_array_sort (priv->launchables, as_app_launchables_sort_cb);
		for (i = 0; i < priv->launchables->len; i++) {
			AsLaunchable *launchable = g_ptr_array_index (priv->launchables, i);
			as_launchable_node_insert (launchable, node_app, ctx);
		}
	}

	/* <languages> */
	if (g_hash_table_size (priv->languages) > 0)
		as_app_node_insert_languages (app, node_app);

	/* <update_contact> */
	if (as_node_context_get_output (ctx) == AS_FORMAT_KIND_APPDATA ||
	    as_node_context_get_output (ctx) == AS_FORMAT_KIND_METAINFO ||
	    as_node_context_get_output_trusted (ctx)) {
		if (priv->update_contact != NULL) {
			as_node_insert (node_app, "update_contact",
					priv->update_contact, 0, NULL);
		}
	}

	/* <custom> or <metadata> */
	if (g_hash_table_size (priv->metadata) > 0) {
		tmp = as_node_context_get_version (ctx) > 0.9 ? "custom" : "metadata";
		node_tmp = as_node_insert (node_app, tmp, NULL, 0, NULL);
		as_node_insert_hash (node_tmp, "value", "key", priv->metadata, FALSE);
	}

	return node_app;
}

static gboolean
as_app_node_parse_child (AsApp *app, GNode *n, guint32 flags,
			 AsNodeContext *ctx, GError **error)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsRefString *str;
	GNode *c;
	const gchar *tmp;
	g_autoptr(AsRefString) xml_lang = NULL;

	switch (as_node_get_tag (n)) {

	/* <id> */
	case AS_TAG_ID:
		if (as_node_get_attribute (n, "xml:lang") != NULL) {
			priv->problems |= AS_APP_PROBLEM_TRANSLATED_ID;
			break;
		}
		tmp = as_node_get_attribute (n, "type");
		if (tmp != NULL)
			as_app_set_kind (app, as_app_kind_from_string (tmp));
		as_app_set_id (app, as_node_get_data (n));
		break;

	/* <priority> */
	case AS_TAG_PRIORITY:
	{
		gint64 tmp64 = g_ascii_strtoll (as_node_get_data (n), NULL, 10);
		as_app_set_priority (app, (gint) tmp64);
		break;
	}

	/* <pkgname> */
	case AS_TAG_PKGNAME:
		str = as_node_get_data_as_refstr (n);
		if (str != NULL)
			g_ptr_array_add (priv->pkgnames, as_ref_string_ref (str));
		break;

	/* <bundle> */
	case AS_TAG_BUNDLE:
	{
		g_autoptr(AsBundle) bu = NULL;
		bu = as_bundle_new ();
		if (!as_bundle_node_parse (bu, n, ctx, error))
			return FALSE;
		as_app_add_bundle (app, bu);
		break;
	}

	/* <translation> */
	case AS_TAG_TRANSLATION:
	{
		g_autoptr(AsTranslation) ic = NULL;
		ic = as_translation_new ();
		if (!as_translation_node_parse (ic, n, ctx, error))
			return FALSE;
		as_app_add_translation (app, ic);
		break;
	}

	/* <suggests> */
	case AS_TAG_SUGGESTS:
	{
		g_autoptr(AsSuggest) ic = NULL;
		ic = as_suggest_new ();
		if (!as_suggest_node_parse (ic, n, ctx, error))
			return FALSE;
		as_app_add_suggest (app, ic);
		break;
	}

	/* <requires> */
	case AS_TAG_REQUIRES:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_ptr_array_set_size (priv->requires, 0);
		for (c = n->children; c != NULL; c = c->next) {
			g_autoptr(AsRequire) ic = NULL;
			ic = as_require_new ();
			if (!as_require_node_parse (ic, c, ctx, error))
				return FALSE;
			as_app_add_require (app, ic);
		}
		if (n->children == NULL)
			priv->problems |= AS_APP_PROBLEM_EXPECTED_CHILDREN;
		break;

	/* <name> */
	case AS_TAG_NAME:
		xml_lang = as_node_fix_locale (as_node_get_attribute (n, "xml:lang"));
		if (xml_lang == NULL)
			break;
		str = as_node_get_data_as_refstr (n);
		if (str != NULL) {
			g_hash_table_insert (priv->names,
					     as_ref_string_ref (xml_lang),
					     as_ref_string_ref (str));
		}
		break;

	/* <summary> */
	case AS_TAG_SUMMARY:
		xml_lang = as_node_fix_locale (as_node_get_attribute (n, "xml:lang"));
		if (xml_lang == NULL)
			break;
		str = as_node_get_data_as_refstr (n);
		if (str != NULL) {
			g_hash_table_insert (priv->comments,
					     as_ref_string_ref (xml_lang),
					     as_ref_string_ref (str));
		}
		break;

	/* <developer_name> */
	case AS_TAG_DEVELOPER_NAME:
		xml_lang = as_node_fix_locale (as_node_get_attribute (n, "xml:lang"));
		if (xml_lang == NULL)
			break;
		str = as_node_get_data_as_refstr (n);
		if (str != NULL) {
			g_hash_table_insert (priv->developer_names,
					     as_ref_string_ref (xml_lang),
					     as_ref_string_ref (str));
		}
		break;

	/* <description> */
	case AS_TAG_DESCRIPTION:
	{
		/* unwrap appdata inline */
		AsFormat *format = as_app_get_format_by_kind (app, AS_FORMAT_KIND_APPDATA);
		if (format != NULL) {
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
						   as_format_get_filename (format),
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
	}

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
			tmp = as_node_get_data_as_refstr (c);
			if (tmp == NULL)
				continue;
			as_app_add_category (app, tmp);
		}
		if (n->children == NULL)
			priv->problems |= AS_APP_PROBLEM_EXPECTED_CHILDREN;
		break;

	/* <architectures> */
	case AS_TAG_ARCHITECTURES:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_ptr_array_set_size (priv->architectures, 0);
		for (c = n->children; c != NULL; c = c->next) {
			if (as_node_get_tag (c) != AS_TAG_ARCH)
				continue;
			str = as_node_get_data_as_refstr (c);
			if (str == NULL)
				continue;
			g_ptr_array_add (priv->architectures,
					 as_ref_string_ref (str));
		}
		if (n->children == NULL)
			priv->problems |= AS_APP_PROBLEM_EXPECTED_CHILDREN;
		break;

	/* <keywords> */
	case AS_TAG_KEYWORDS:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_hash_table_remove_all (priv->keywords);
		for (c = n->children; c != NULL; c = c->next) {
			g_autoptr(AsRefString) xml_lang2 = NULL;
			if (as_node_get_tag (c) != AS_TAG_KEYWORD)
				continue;
			tmp = as_node_get_data (c);
			if (tmp == NULL)
				continue;
			xml_lang2 = as_node_fix_locale (as_node_get_attribute (c, "xml:lang"));
			if (xml_lang2 == NULL)
				continue;
			if (g_strstr_len (tmp, -1, ",") != NULL)
				priv->problems |= AS_APP_PROBLEM_INVALID_KEYWORDS;
			as_app_add_keyword (app, xml_lang2, tmp);
		}
		if (n->children == NULL)
			priv->problems |= AS_APP_PROBLEM_EXPECTED_CHILDREN;
		break;

	/* <kudos> */
	case AS_TAG_KUDOS:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_ptr_array_set_size (priv->kudos, 0);
		for (c = n->children; c != NULL; c = c->next) {
			if (as_node_get_tag (c) != AS_TAG_KUDO)
				continue;
			str = as_node_get_data_as_refstr (c);
			if (str == NULL)
				continue;
			g_ptr_array_add (priv->kudos, as_ref_string_ref (str));
		}
		if (n->children == NULL)
			priv->problems |= AS_APP_PROBLEM_EXPECTED_CHILDREN;
		break;

	/* <permissions> */
	case AS_TAG_PERMISSIONS:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_ptr_array_set_size (priv->permissions, 0);
		for (c = n->children; c != NULL; c = c->next) {
			if (as_node_get_tag (c) != AS_TAG_PERMISSION)
				continue;
			str = as_node_get_data_as_refstr (c);
			if (str == NULL)
				continue;
			g_ptr_array_add (priv->permissions, as_ref_string_ref (str));
		}
		if (n->children == NULL)
			priv->problems |= AS_APP_PROBLEM_EXPECTED_CHILDREN;
		break;

	/* <vetos> */
	case AS_TAG_VETOS:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_ptr_array_set_size (priv->vetos, 0);
		for (c = n->children; c != NULL; c = c->next) {
			if (as_node_get_tag (c) != AS_TAG_VETO)
				continue;
			str = as_node_get_data_as_refstr (c);
			if (str == NULL)
				continue;
			g_ptr_array_add (priv->vetos, as_ref_string_ref (str));
		}
		if (n->children == NULL)
			priv->problems |= AS_APP_PROBLEM_EXPECTED_CHILDREN;
		break;

	/* <mimetypes> */
	case AS_TAG_MIMETYPES:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_ptr_array_set_size (priv->mimetypes, 0);
		for (c = n->children; c != NULL; c = c->next) {
			if (as_node_get_tag (c) != AS_TAG_MIMETYPE)
				continue;
			str = as_node_get_data_as_refstr (c);
			if (str == NULL)
				continue;
			g_ptr_array_add (priv->mimetypes, as_ref_string_ref (str));
		}
		if (n->children == NULL)
			priv->problems |= AS_APP_PROBLEM_EXPECTED_CHILDREN;
		break;

	/* <project_license> */
	case AS_TAG_PROJECT_LICENSE:
		if (as_node_get_attribute (n, "xml:lang") != NULL) {
			priv->problems |= AS_APP_PROBLEM_TRANSLATED_LICENSE;
			break;
		}
		as_ref_string_assign (&priv->project_license, as_node_get_data_as_refstr (n));
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
		as_app_set_project_group (app, as_node_get_data (n));
		break;

	/* <compulsory_for_desktop> */
	case AS_TAG_COMPULSORY_FOR_DESKTOP:
		str = as_node_get_data_as_refstr (n);
		if (str == NULL)
			break;
		g_ptr_array_add (priv->compulsory_for_desktops,
				 as_ref_string_ref (str));
		break;

	/* <extends> */
	case AS_TAG_EXTENDS:
		str = as_node_get_data_as_refstr (n);
		if (str == NULL)
			break;
		g_ptr_array_add (priv->extends, as_ref_string_ref (str));
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
		if (n->children == NULL)
			priv->problems |= AS_APP_PROBLEM_EXPECTED_CHILDREN;
		break;

	/* <reviews> */
	case AS_TAG_REVIEWS:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_ptr_array_set_size (priv->reviews, 0);
		for (c = n->children; c != NULL; c = c->next) {
			g_autoptr(AsReview) review = NULL;
			if (as_node_get_tag (c) != AS_TAG_REVIEW)
				continue;
			review = as_review_new ();
			if (!as_review_node_parse (review, c, ctx, error))
				return FALSE;
			as_app_add_review (app, review);
		}
		break;

	/* <content_ratings> */
	case AS_TAG_CONTENT_RATING:
	{
		g_autoptr(AsContentRating) content_rating = NULL;
		content_rating = as_content_rating_new ();
		if (!as_content_rating_node_parse (content_rating, n, ctx, error))
			return FALSE;
		as_app_add_content_rating (app, content_rating);
		break;
	}

	/* <agreements> */
	case AS_TAG_AGREEMENT:
	{
		g_autoptr(AsAgreement) agreement = NULL;
		agreement = as_agreement_new ();
		if (!as_agreement_node_parse (agreement, n, ctx, error))
			return FALSE;
		as_app_add_agreement (app, agreement);
		break;
	}

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
		if (n->children == NULL)
			priv->problems |= AS_APP_PROBLEM_EXPECTED_CHILDREN;
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

	/* <launchables> */
	case AS_TAG_LAUNCHABLE:
	{
		g_autoptr(AsLaunchable) lau = NULL;
		lau = as_launchable_new ();
		if (!as_launchable_node_parse (lau, n, ctx, error))
			return FALSE;
		as_app_add_launchable (app, lau);
		break;
	}

	/* <languages> */
	case AS_TAG_LANGUAGES:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_hash_table_remove_all (priv->languages);
		for (c = n->children; c != NULL; c = c->next) {
			gint percent;
			if (as_node_get_tag (c) != AS_TAG_LANG)
				continue;
			percent = as_node_get_attribute_as_int (c, "percentage");
			if (percent == G_MAXINT)
				percent = 0;
			as_app_add_language (app, percent,
					     as_node_get_data (c));
		}
		if (n->children == NULL)
			priv->problems |= AS_APP_PROBLEM_EXPECTED_CHILDREN;
		break;

	/* <custom> or <metadata> */
	case AS_TAG_METADATA:
	case AS_TAG_CUSTOM:
		if (!(flags & AS_APP_PARSE_FLAG_APPEND_DATA))
			g_hash_table_remove_all (priv->metadata);
		for (c = n->children; c != NULL; c = c->next) {
			AsRefString *key;
			AsRefString *value;
			if (as_node_get_tag (c) != AS_TAG_VALUE)
				continue;
			key = as_node_get_attribute_as_refstr (c, "key");
			value = as_node_get_data_as_refstr (c);
			if (value == NULL) {
				g_hash_table_insert (priv->metadata,
						     as_ref_string_ref (key),
						     as_ref_string_new_static (""));
			} else {
				g_hash_table_insert (priv->metadata,
						     as_ref_string_ref (key),
						     as_ref_string_ref (value));
			}
		}
		if (n->children == NULL)
			priv->problems |= AS_APP_PROBLEM_EXPECTED_CHILDREN;
		break;
	default:
		priv->problems |= AS_APP_PROBLEM_INVALID_XML_TAG;
		break;
	}
	return TRUE;
}

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

static gboolean
as_app_node_parse_full (AsApp *app, GNode *node, guint32 flags,
			AsNodeContext *ctx, GError **error)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	GNode *n;
	const gchar *tmp;
	gint prio;

	/* new style */
	if (g_strcmp0 (as_node_get_name (node), "component") == 0) {
		tmp = as_node_get_attribute (node, "type");
		if (tmp == NULL)
			as_app_set_kind (app, AS_APP_KIND_GENERIC);
		else
			as_app_set_kind (app, as_app_kind_from_string (tmp));
		tmp = as_node_get_attribute (node, "merge");
		if (tmp != NULL)
			as_app_set_merge_kind (app, as_app_merge_kind_from_string (tmp));
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
		g_ptr_array_set_size (priv->translations, 0);
		g_ptr_array_set_size (priv->suggests, 0);
		g_ptr_array_set_size (priv->requires, 0);
		g_ptr_array_set_size (priv->content_ratings, 0);
		g_ptr_array_set_size (priv->agreements, 0);
		g_ptr_array_set_size (priv->launchables, 0);
		g_hash_table_remove_all (priv->keywords);
	}
	for (n = node->children; n != NULL; n = n->next) {
		if (!as_app_node_parse_child (app, n, flags, ctx, error))
			return FALSE;
	}

	/* if only one icon is listed, look for HiDPI versions too */
	if (as_app_get_icons(app)->len == 1)
		as_app_check_for_hidpi_icons (app);

	/* add the launchable if missing for desktop apps */
	if (priv->launchables->len == 0 &&
	    priv->kind == AS_APP_KIND_DESKTOP &&
	    priv->id != NULL) {
		AsLaunchable *lau = as_launchable_new ();
		as_launchable_set_kind (lau, AS_LAUNCHABLE_KIND_DESKTOP_ID);
		if (g_str_has_suffix (priv->id, ".desktop")) {
			as_launchable_set_value (lau, priv->id);
		} else {
			g_autofree gchar *id_tmp = NULL;
			id_tmp = g_strdup_printf ("%s.desktop", priv->id);
			as_launchable_set_value (lau, id_tmp);
		}
		g_ptr_array_add (priv->launchables, lau);
	}

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

static gboolean
as_app_node_parse_dep11_icons (AsApp *app, GNode *node,
			       AsNodeContext *ctx, GError **error)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	const gchar *sizes[] = { "128x128", "64x64", "", NULL };
	guint i;
	guint size;
	g_autoptr(AsIcon) ic_tmp = NULL;

	if (g_strcmp0 (as_yaml_node_get_key (node), "cached") == 0) {
		if (node->children == NULL) {
			/* legacy compatibility */
			ic_tmp = as_icon_new ();
			as_icon_set_kind (ic_tmp, AS_ICON_KIND_CACHED);
			as_icon_set_name (ic_tmp, as_yaml_node_get_value (node));
		} else {
			GNode *sn;
			/* we have a modern YAML file */
			for (sn = node->children; sn != NULL; sn = sn->next) {
				g_autoptr(AsIcon) icon = NULL;
				icon = as_icon_new ();
				as_icon_set_kind (icon, AS_ICON_KIND_CACHED);
				as_icon_set_prefix (icon, priv->icon_path);
				if (!as_icon_node_parse_dep11 (icon, sn, ctx, error))
					return FALSE;
				as_app_add_icon (app, icon);
			}
		}
	} else if (g_strcmp0 (as_yaml_node_get_key (node), "stock") == 0) {
		g_autoptr(AsIcon) icon = NULL;
		icon = as_icon_new ();
		as_icon_set_name (icon, as_yaml_node_get_value (node));
		as_icon_set_kind (icon, AS_ICON_KIND_STOCK);
		as_icon_set_prefix (icon, priv->icon_path);
		as_app_add_icon (app, icon);
	} else {
		GNode *sn;
		AsIconKind ikind;

		if (g_strcmp0 (as_yaml_node_get_key (node), "remote") == 0) {
			ikind = AS_ICON_KIND_REMOTE;
		} else if (g_strcmp0 (as_yaml_node_get_key (node), "local") == 0) {
			ikind = AS_ICON_KIND_REMOTE;
		} else {
			/* We have an unknown icon type, and just ignore that here */
			return TRUE;
		}

		for (sn = node->children; sn != NULL; sn = sn->next) {
			g_autoptr(AsIcon) icon = NULL;
			icon = as_icon_new ();
			as_icon_set_kind (icon, ikind);
			if (!as_icon_node_parse_dep11 (icon, sn, ctx, error))
				return FALSE;
			as_app_add_icon (app, icon);
		}
	}

	if (ic_tmp == NULL) {
		/* we have no icon which we need to probe sizes for */
		return TRUE;
	}

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
	const gchar *nonfatal_str = NULL;
	const gchar *tmp;

	for (n = node->children; n != NULL; n = n->next) {
		tmp = as_yaml_node_get_key (n);
		if (g_strcmp0 (tmp, "ID") == 0) {
			as_app_set_id (app, as_yaml_node_get_value (n));
			continue;
		}
		if (g_strcmp0 (tmp, "Type") == 0) {
			tmp = as_yaml_node_get_value (n);
			if (tmp == NULL)
				continue;
			as_app_set_kind (app, as_app_kind_from_string (tmp));
			continue;
		}
		if (g_strcmp0 (tmp, "Package") == 0) {
			as_app_add_pkgname (app, as_yaml_node_get_value (n));
			continue;
		}
		if (g_strcmp0 (tmp, "Name") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				if (as_yaml_node_get_key (c) == NULL)
					continue;
				as_app_set_name (app,
						 as_yaml_node_get_key (c),
						 as_yaml_node_get_value (c));
			}
			continue;
		}
		if (g_strcmp0 (tmp, "Summary") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				if (as_yaml_node_get_key (c) == NULL)
					continue;
				as_app_set_comment (app,
						    as_yaml_node_get_key (c),
						    as_yaml_node_get_value (c));
			}
			continue;
		}
		if (g_strcmp0 (tmp, "Description") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				if (as_yaml_node_get_key (c) == NULL)
					continue;
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
					if (as_yaml_node_get_key (c) == NULL)
						continue;
					as_app_add_keyword (app,
							   as_yaml_node_get_key (c),
							   as_yaml_node_get_key (c2));
				}
			}
			continue;
		}
		if (g_strcmp0 (tmp, "Categories") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				tmp = as_yaml_node_get_key (c);
				if (tmp == NULL) {
					nonfatal_str = "contained empty category";
					continue;
				}
				as_app_add_category (app, tmp);
			}
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
		if (g_strcmp0 (tmp, "Translation") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				g_autoptr(AsTranslation) bu = NULL;
				bu = as_translation_new ();
				if (!as_translation_node_parse_dep11 (bu, c, ctx, error))
					return FALSE;
				as_app_add_translation (app, bu);
			}
			continue;
		}
		if (g_strcmp0 (tmp, "Suggests") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				g_autoptr(AsSuggest) bu = NULL;
				bu = as_suggest_new ();
				if (!as_suggest_node_parse_dep11 (bu, c, ctx, error))
					return FALSE;
				as_app_add_suggest (app, bu);
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
		if (g_strcmp0 (tmp, "Reviews") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				g_autoptr(AsReview) review = NULL;
				review = as_review_new ();
				if (!as_review_node_parse_dep11 (review, c, ctx, error))
					return FALSE;
				as_app_add_review (app, review);
			}
			continue;
		}
		if (g_strcmp0 (tmp, "Extends") == 0) {
			for (c = n->children; c != NULL; c = c->next)
				as_app_add_extends (app, as_yaml_node_get_key (c));
			continue;
		}
		if (g_strcmp0 (tmp, "Releases") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				g_autoptr(AsRelease) rel = NULL;
				rel = as_release_new ();
				if (!as_release_node_parse_dep11 (rel, c, ctx, error))
					return FALSE;
				as_app_add_release (app, rel);
			}
			continue;
		}
		if (g_strcmp0 (tmp, "DeveloperName") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				as_app_set_developer_name (app,
							   as_yaml_node_get_key (c),
							   as_yaml_node_get_value (c));
			}
			continue;
		}
		if (g_strcmp0 (tmp, "ProjectLicense") == 0) {
			as_app_set_project_license (app, as_yaml_node_get_value (n));
			continue;
		}
		if (g_strcmp0 (tmp, "ProjectGroup") == 0) {
			as_app_set_project_group (app, as_yaml_node_get_value (n));
			continue;
		}
		if (g_strcmp0 (tmp, "CompulsoryForDesktops") == 0) {
			for (c = n->children; c != NULL; c = c->next) {
				tmp = as_yaml_node_get_key (c);
				if (tmp == NULL) {
					nonfatal_str = "contained empty desktop";
					continue;
				}
				as_app_add_compulsory_for_desktop (app, tmp);
			}
			continue;
		}
	}
	if (nonfatal_str != NULL) {
		g_debug ("nonfatal warning from %s: %s",
			 as_app_get_id (app), nonfatal_str);
	}
	return TRUE;
}

static gchar **
as_app_value_tokenize (const gchar *value)
{
	g_autofree gchar *delim = NULL;
	delim = g_utf8_strdown (value, -1);
	g_strdelimit (delim, "/,.;:", ' ');
	return g_strsplit (delim, " ", -1);
}

static void
as_app_add_token_internal (AsApp *app,
			   const gchar *value,
			   guint16 match_flag)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsAppTokenType *match_pval;
	g_autoptr(AsRefString) value_stem = NULL;

	/* invalid */
	if (!as_utils_search_token_valid (value))
		return;

	/* get the most suitable value */
	if (priv->stemmer != NULL)
		value_stem = as_stemmer_process (priv->stemmer, value);
	if (value_stem == NULL)
		return;

	/* blacklisted */
	if (priv->search_blacklist != NULL &&
	    g_hash_table_lookup (priv->search_blacklist, value_stem) != NULL)
		return;

	/* does the token already exist */
	match_pval = g_hash_table_lookup (priv->token_cache, value_stem);
	if (match_pval != NULL) {
		*match_pval |= match_flag;
		return;
	}

	/* create and add */
	match_pval = g_new0 (AsAppTokenType, 1);
	*match_pval = match_flag;
	g_hash_table_insert (priv->token_cache,
			     as_ref_string_ref (value_stem),
			     match_pval);
}

static void
as_app_add_token (AsApp *app,
		  const gchar *value,
		  gboolean allow_split,
		  guint16 match_flag)
{
	/* add extra tokens for names like x-plane or half-life */
	if (allow_split && g_strstr_len (value, -1, "-") != NULL) {
		guint i;
		g_auto(GStrv) split = g_strsplit (value, "-", -1);
		for (i = 0; split[i] != NULL; i++)
			as_app_add_token_internal (app, split[i], match_flag);
	}

	/* add the whole token always, even when we split on hyphen */
	as_app_add_token_internal (app, value, match_flag);
}

static gboolean
as_app_needs_transliteration (const gchar *locale)
{
	if (g_strcmp0 (locale, "C") == 0)
		return FALSE;
	if (g_strcmp0 (locale, "en") == 0)
		return FALSE;
	if (g_str_has_prefix (locale, "en_"))
		return FALSE;
	return TRUE;
}

static void
as_app_add_tokens (AsApp *app,
		   const gchar *value,
		   const gchar *locale,
		   gboolean allow_split,
		   guint16 match_flag)
{
	guint i;
	g_auto(GStrv) values_utf8 = NULL;
	g_auto(GStrv) values_ascii = NULL;

	/* sanity check */
	if (value == NULL) {
		g_critical ("trying to add NULL search token to %s",
			    as_app_get_id (app));
		return;
	}

#if GLIB_CHECK_VERSION(2,39,1)
	/* tokenize with UTF-8 fallbacks */
	if (as_app_needs_transliteration (locale) &&
	    g_strstr_len (value, -1, "+") == NULL &&
	    g_strstr_len (value, -1, "-") == NULL) {
		values_utf8 = g_str_tokenize_and_fold (value, locale, &values_ascii);
	}
#endif
	if (values_utf8 == NULL)
		values_utf8 = as_app_value_tokenize (value);

	/* add each token */
	for (i = 0; values_utf8 != NULL && values_utf8[i] != NULL; i++)
		as_app_add_token (app, values_utf8[i], allow_split, match_flag);
	for (i = 0; values_ascii != NULL && values_ascii[i] != NULL; i++)
		as_app_add_token (app, values_ascii[i], allow_split, match_flag);
}

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
	if (priv->search_match & AS_APP_SEARCH_MATCH_ID) {
		if (priv->id_filename != NULL) {
			as_app_add_token (app, priv->id_filename, FALSE,
					  AS_APP_SEARCH_MATCH_ID);
		}
	}
	locales = g_get_language_names ();
	for (i = 0; locales[i] != NULL; i++) {
		if (g_str_has_suffix (locales[i], ".UTF-8"))
			continue;
		if (priv->search_match & AS_APP_SEARCH_MATCH_NAME) {
			tmp = as_app_get_name (app, locales[i]);
			if (tmp != NULL) {
				as_app_add_tokens (app, tmp, locales[i], TRUE,
						   AS_APP_SEARCH_MATCH_NAME);
			}
		}
		if (priv->search_match & AS_APP_SEARCH_MATCH_COMMENT) {
			tmp = as_app_get_comment (app, locales[i]);
			if (tmp != NULL) {
				as_app_add_tokens (app, tmp, locales[i], TRUE,
						   AS_APP_SEARCH_MATCH_COMMENT);
			}
		}
		if (priv->search_match & AS_APP_SEARCH_MATCH_DESCRIPTION) {
			tmp = as_app_get_description (app, locales[i]);
			if (tmp != NULL) {
				as_app_add_tokens (app, tmp, locales[i], FALSE,
						   AS_APP_SEARCH_MATCH_DESCRIPTION);
			}
		}
		if (priv->search_match & AS_APP_SEARCH_MATCH_KEYWORD) {
			array = as_app_get_keywords (app, locales[i]);
			if (array != NULL) {
				for (j = 0; j < array->len; j++) {
					tmp = g_ptr_array_index (array, j);
					as_app_add_tokens (app, tmp, locales[i], FALSE,
							   AS_APP_SEARCH_MATCH_KEYWORD);
				}
			}
		}
	}
	if (priv->search_match & AS_APP_SEARCH_MATCH_MIMETYPE) {
		for (i = 0; i < priv->mimetypes->len; i++) {
			tmp = g_ptr_array_index (priv->mimetypes, i);
			as_app_add_token (app, tmp, FALSE, AS_APP_SEARCH_MATCH_MIMETYPE);
		}
	}
	if (priv->search_match & AS_APP_SEARCH_MATCH_PKGNAME) {
		for (i = 0; i < priv->pkgnames->len; i++) {
			tmp = g_ptr_array_index (priv->pkgnames, i);
			as_app_add_token (app, tmp, FALSE, AS_APP_SEARCH_MATCH_PKGNAME);
		}
	}
	if (priv->search_match & AS_APP_SEARCH_MATCH_ORIGIN) {
		if (priv->origin != NULL) {
			as_app_add_token (app, priv->origin, TRUE,
					  AS_APP_SEARCH_MATCH_ORIGIN);
		}
	}
}

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
	AsAppTokenType *match_pval;
	GList *l;
	guint16 result = 0;
	g_autoptr(GList) keys = NULL;
	g_autoptr(AsRefString) search_stem = NULL;

	/* ensure the token cache is created */
	if (g_once_init_enter (&priv->token_cache_valid)) {
		as_app_create_token_cache (app);
		g_once_init_leave (&priv->token_cache_valid, TRUE);
	}

	/* nothing to do */
	if (search == NULL)
		return 0;

	/* find the exact match (which is more awesome than a partial match) */
	if (priv->stemmer != NULL)
		search_stem = as_stemmer_process (priv->stemmer, search);
	if (search_stem == NULL)
		return 0;
	match_pval = g_hash_table_lookup (priv->token_cache, search_stem);
	if (match_pval != NULL)
		return (guint) *match_pval << 2;

	/* need to do partial match */
	keys = g_hash_table_get_keys (priv->token_cache);
	for (l = keys; l != NULL; l = l->next) {
		const gchar *key = l->data;
		if (g_str_has_prefix (key, search_stem)) {
			match_pval = g_hash_table_lookup (priv->token_cache, key);
			result |= *match_pval;
		}
	}
	return result;
}

/**
 * as_app_get_search_tokens:
 * @app: a #AsApp instance.
 *
 * Returns all the search tokens for the application. These are unsorted.
 *
 * Returns: (transfer container): The string search tokens
 *
 * Since: 0.3.4
 */
GPtrArray *
as_app_get_search_tokens (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	GList *l;
	GPtrArray *array;
	g_autoptr(GList) keys = NULL;

	/* ensure the token cache is created */
	if (g_once_init_enter (&priv->token_cache_valid)) {
		as_app_create_token_cache (app);
		g_once_init_leave (&priv->token_cache_valid, TRUE);
	}

	/* return all the token cache */
	keys = g_hash_table_get_keys (priv->token_cache);
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) as_ref_string_unref);
	for (l = keys; l != NULL; l = l->next)
		g_ptr_array_add (array, as_ref_string_ref (l->data));
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
		matches_sum |= tmp;
	}
	return matches_sum;
}

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

static void
as_app_parse_appdata_guess_project_group (AsApp *app)
{
	const gchar *tmp;
	guint i;
	struct {
		const gchar *project_group;
		const gchar *url_glob;
	} table[] = {
		{ "elementary",		"http*://elementary.io*" },
		{ "Enlightenment",	"http://*enlightenment.org*" },
		{ "GNOME",		"http*://*.gnome.org*" },
		{ "GNOME",		"http://gnome-*.sourceforge.net/" },
		{ "KDE",		"http://*kde-apps.org/*" },
		{ "KDE",		"http*://*.kde.org*" },
		{ "LXDE",		"http://lxde.org*" },
		{ "LXDE",		"http://lxde.sourceforge.net/*" },
		{ "LXDE",		"http://pcmanfm.sourceforge.net/*" },
		{ "MATE",		"http://*mate-desktop.org*" },
		{ "XFCE",		"http://*xfce.org*" },
		{ NULL, NULL }
	};

	/* match a URL glob and set the project group */
	tmp = as_app_get_url_item (app, AS_URL_KIND_HOMEPAGE);
	if (tmp == NULL)
		return;
	for (i = 0; table[i].project_group != NULL; i++) {
		if (fnmatch (table[i].url_glob, tmp, 0) == 0) {
			as_app_set_project_group (app, table[i].project_group);
			return;
		}
	}

	/* use summary to guess the project group */
	tmp = as_app_get_comment (AS_APP (app), NULL);
	if (tmp != NULL && g_strstr_len (tmp, -1, "for KDE") != NULL) {
		as_app_set_project_group (AS_APP (app), "KDE");
		return;
	}
}

static int
as_utils_fnmatch (const gchar *pattern, const gchar *text, gsize text_sz, gint flags)
{
	if (text_sz != -1 && text[text_sz-1] != '\0') {
		g_autofree gchar *text_with_nul = g_strndup (text, text_sz);
		return fnmatch (pattern, text_with_nul, flags);
	}
	return fnmatch (pattern, text, flags);
}

/**
 * as_app_parse_data:
 * @app: a #AsApp instance.
 * @data: data to parse.
 * @flags: #AsAppParseFlags, e.g. %AS_APP_PARSE_FLAG_USE_HEURISTICS
 * @error: A #GError or %NULL.
 *
 * Parses an AppData file and populates the application state.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.7.5
 **/
gboolean
as_app_parse_data (AsApp *app, GBytes *data, guint32 flags, GError **error)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	AsNodeFromXmlFlags from_xml_flags = AS_NODE_FROM_XML_FLAG_NONE;
	GNode *node;
	const gchar *data_raw;
	gboolean seen_application = FALSE;
	gsize len = 0;
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsNode) root = NULL;

	/* validate */
	data_raw = g_bytes_get_data (data, &len);
	if (g_strstr_len (data_raw, (gssize) len, "<?xml version=") == NULL)
		priv->problems |= AS_APP_PROBLEM_NO_XML_HEADER;

	/* check for copyright */
	if (as_utils_fnmatch ("*<!--*Copyright*-->*", data_raw, len, 0) != 0)
		priv->problems |= AS_APP_PROBLEM_NO_COPYRIGHT_INFO;

	/* parse */
	if (flags & AS_APP_PARSE_FLAG_KEEP_COMMENTS)
		from_xml_flags |= AS_NODE_FROM_XML_FLAG_KEEP_COMMENTS;
	root = as_node_from_bytes (data, from_xml_flags, error);
	if (root == NULL)
		return FALSE;

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
		g_set_error_literal (error,
				     AS_APP_ERROR,
				     AS_APP_ERROR_INVALID_TYPE,
				     "no <component> node");
		return FALSE;
	}
	for (GNode *l = node->children; l != NULL; l = l->next) {
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
	as_node_context_set_format_kind (ctx, AS_FORMAT_KIND_APPDATA);
	if (!as_app_node_parse_full (app, node, flags, ctx, error))
		return FALSE;

	/* use heuristics */
	if (flags & AS_APP_PARSE_FLAG_USE_HEURISTICS) {
		if (as_app_get_project_group (app) == NULL)
			as_app_parse_appdata_guess_project_group (app);
	}

	return TRUE;
}

static gboolean
as_app_parse_appdata_file (AsApp *app,
			   const gchar *filename,
			   guint32 flags,
			   GError **error)
{
	gsize len;
	g_autofree gchar *data_raw = NULL;
	g_autoptr(GBytes) data = NULL;
	g_autoptr(GError) error_local = NULL;

	/* open file */
	if (!g_file_get_contents (filename, &data_raw, &len, &error_local)) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_INVALID_TYPE,
			     "%s could not be read: %s",
			     filename, error_local->message);
		return FALSE;
	}
	data = g_bytes_new_take (g_steal_pointer (&data_raw), len);
	if (!as_app_parse_data (app, data, flags, &error_local)) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_INVALID_TYPE,
			     "failed to parse %s: %s",
			     filename, error_local->message);
		return FALSE;
	}
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
as_app_parse_file (AsApp *app, const gchar *filename, guint32 flags, GError **error)
{
	GPtrArray *vetos;
	g_autoptr(AsFormat) format = as_format_new ();

	/* autodetect */
	as_format_set_filename (format, filename);
	if (as_format_get_kind (format) == AS_FORMAT_KIND_UNKNOWN) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_INVALID_TYPE,
			     "%s has an unrecognised extension",
			     filename);
		return FALSE;
	}
	as_app_add_format (app, format);

	/* convert <_p> into <p> for easy validation */
	if (g_str_has_suffix (filename, ".appdata.xml.in") ||
	    g_str_has_suffix (filename, ".metainfo.xml.in"))
		flags |= AS_APP_PARSE_FLAG_CONVERT_TRANSLATABLE;

	/* all untrusted */
	as_app_set_trust_flags (AS_APP (app),
				AS_APP_TRUST_FLAG_CHECK_DUPLICATES |
				AS_APP_TRUST_FLAG_CHECK_VALID_UTF8);

	/* parse */
	switch (as_format_get_kind (format)) {
	case AS_FORMAT_KIND_DESKTOP:
		if (!as_app_parse_desktop_file (app, filename, flags, error))
			return FALSE;
		break;
	case AS_FORMAT_KIND_APPDATA:
	case AS_FORMAT_KIND_METAINFO:
		if (!as_app_parse_appdata_file (app, filename, flags, error))
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
 * @cancellable: (nullable): A #GCancellable
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
	g_autoptr(AsNodeContext) ctx = NULL;
	g_autoptr(AsNode) root = NULL;
	g_autoptr(GString) xml = NULL;

	root = as_node_new ();
	ctx = as_node_context_new ();
	as_node_context_set_version (ctx, 1.0);
	as_node_context_set_output (ctx, AS_FORMAT_KIND_APPDATA);
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
	g_autofree gchar *tmp = NULL;
	va_list args;
	va_start (args, fmt);
	tmp = g_strdup_vprintf (fmt, args);
	va_end (args);
	g_ptr_array_add (priv->vetos, as_ref_string_new (tmp));
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
 * as_app_set_stemmer: (skip)
 **/
void
as_app_set_stemmer (AsApp *app, AsStemmer *stemmer)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	g_set_object (&priv->stemmer, stemmer);
}

/**
 * as_app_set_search_blacklist: (skip)
 **/
void
as_app_set_search_blacklist (AsApp *app, GHashTable *search_blacklist)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	if (priv->search_blacklist != NULL)
		g_hash_table_unref (priv->search_blacklist);
	priv->search_blacklist = g_hash_table_ref (search_blacklist);
}

/**
 * as_app_set_search_match:
 * @app: a #AsApp instance.
 * @search_match: the #AsAppSearchMatch, e.g. %AS_APP_SEARCH_MATCH_PKGNAME
 *
 * Sets the token match fields. The bitfield given here is used to choose what
 * is included in the token cache.
 *
 * Since: 0.6.13
 **/
void
as_app_set_search_match (AsApp *app, guint16 search_match)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	priv->search_match = search_match;
}

/**
 * as_app_get_search_match:
 * @app: a #AsApp instance.
 *
 * Gets the token match fields. The bitfield given here is used to choose what
 * is included in the token cache.
 *
 * Returns: a #AsAppSearchMatch, e.g. %AS_APP_SEARCH_MATCH_PKGNAME
 *
 * Since: 0.6.13
 **/
guint16
as_app_get_search_match (AsApp *app)
{
	AsAppPrivate *priv = GET_PRIVATE (app);
	return priv->search_match;
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
