/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
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
 * SECTION:as-gettext
 * @short_description: a hashed array gettext of applications
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * This object will parse a gettext catalog directory and calculate the
 * language stats for an application.
 *
 * See also: #AsApp
 */

#include "config.h"

#include <fnmatch.h>

#include "as-app-gettext.h"

typedef struct {
} AsGettextPrivate;

typedef struct {
	guint32		 magic;
	guint32		 revision;
	guint32		 nstrings;
	guint32		 orig_tab_offset;
	guint32		 trans_tab_offset;
	guint32		 hash_tab_size;
	guint32		 hash_tab_offset;
	guint32		 n_sysdep_segments;
	guint32		 sysdep_segments_offset;
	guint32		 n_sysdep_strings;
	guint32		 orig_sysdep_tab_offset;
	guint32		 trans_sysdep_tab_offset;
} AsGettextHeader;

typedef struct {
	gchar		*locale;
	guint		 nstrings;
	guint		 percentage;
} AsGettextEntry;

typedef struct {
	guint		 max_nstrings;
	GList		*data;
	gchar		**intl_domains;
	guint		 min_percentage;
} AsGettextContext;

/**
 * as_gettext_entry_new:
 **/
static AsGettextEntry *
as_gettext_entry_new (void)
{
	AsGettextEntry *entry;
	entry = g_slice_new0 (AsGettextEntry);
	return entry;
}

/**
 * as_gettext_entry_free:
 **/
static void
as_gettext_entry_free (AsGettextEntry *entry)
{
	g_free (entry->locale);
	g_slice_free (AsGettextEntry, entry);
}

/**
 * as_gettext_ctx_new:
 **/
static AsGettextContext *
as_gettext_ctx_new (void)
{
	AsGettextContext *ctx;
	ctx = g_new0 (AsGettextContext, 1);
	return ctx;
}

/**
 * as_gettext_ctx_free:
 **/
static void
as_gettext_ctx_free (AsGettextContext *ctx)
{
	g_list_free_full (ctx->data, (GDestroyNotify) as_gettext_entry_free);
	g_strfreev (ctx->intl_domains);
	g_free (ctx);
}

/**
 * as_gettext_parse_file:
 **/
static gboolean
as_gettext_parse_file (AsGettextContext *ctx,
			const gchar *locale,
			const gchar *filename,
			GError **error)
{
	AsGettextEntry *entry;
	AsGettextHeader *h;
	g_autofree gchar *data = NULL;
	gboolean swapped;

	/* read data, although we only strictly need the header */
	if (!g_file_get_contents (filename, &data, NULL, error))
		return FALSE;

	h = (AsGettextHeader *) data;
	if (h->magic == 0x950412de)
		swapped = FALSE;
	else if (h->magic == 0xde120495)
		swapped = TRUE;
	else
		return FALSE;
	entry = as_gettext_entry_new ();
	entry->locale = g_strdup (locale);
	if (swapped)
		entry->nstrings = GUINT32_SWAP_LE_BE (h->nstrings);
	else
		entry->nstrings = h->nstrings;
	if (entry->nstrings > ctx->max_nstrings)
		ctx->max_nstrings = entry->nstrings;
	ctx->data = g_list_prepend (ctx->data, entry);
	return TRUE;
}

/**
 * as_gettext_ctx_search_locale:
 **/
static gboolean
as_gettext_ctx_search_locale (AsGettextContext *ctx,
			      const gchar *locale,
			      const gchar *messages_path,
			      GError **error)
{
	const gchar *filename;
	gboolean found_anything = FALSE;
	guint i;
	g_autoptr(GDir) dir = NULL;
	g_autoptr(GPtrArray) mo_paths = NULL;

	/* list files */
	dir = g_dir_open (messages_path, 0, error);
	if (dir == NULL)
		return FALSE;

	/* do a first pass at this, trying to find the prefered .mo */
	mo_paths = g_ptr_array_new_with_free_func (g_free);
	while ((filename = g_dir_read_name (dir)) != NULL) {
		g_autofree gchar *path = NULL;
		path = g_build_filename (messages_path, filename, NULL);
		if (!g_file_test (path, G_FILE_TEST_EXISTS))
			continue;
		for (i = 0; ctx->intl_domains != NULL &&
			    ctx->intl_domains[i] != NULL; i++) {
			g_autofree gchar *fn = NULL;
			fn = g_strdup_printf ("%s.mo", ctx->intl_domains[i]);
			if (g_strcmp0 (filename, fn) == 0) {
				if (!as_gettext_parse_file (ctx, locale, path, error))
					return FALSE;
				found_anything = TRUE;
			}
		}
		g_ptr_array_add (mo_paths, g_strdup (path));
	}

	/* we got data from one or more of the intl_domains */
	if (found_anything == TRUE)
		return TRUE;

	/* fall back to parsing *everything*, which might give us more
	 * language results than is actually true */
	for (i = 0; i < mo_paths->len; i++) {
		filename = g_ptr_array_index (mo_paths, i);
		if (!as_gettext_parse_file (ctx, locale, filename, error))
			return FALSE;
	}

	return TRUE;
}

/**
 * as_gettext_entry_sort_cb:
 **/
static gint
as_gettext_entry_sort_cb (gconstpointer a, gconstpointer b)
{
	return g_strcmp0 (((AsGettextEntry *) a)->locale,
			  ((AsGettextEntry *) b)->locale);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(AsGettextContext, as_gettext_ctx_free)

/**
 * as_app_gettext_search_path:
 * @app: an #AsApp
 * @path: a path to search, e.g. "/usr/share/locale"
 * @intl_domains: (allow-none): gettext domains, e.g. ["gnome-software"]
 * @min_percentage: minimum percentage to add language
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError or %NULL
 *
 * Searches a gettext catalog path for languages, and using a heuristic
 * adds <language> tags to the specified application.
 *
 * If @intl_domains is not set then all domains are matched, which may
 * include more languages than you intended to.
 *
 * @min_percentage sets the minimum percentage to add a language tag.
 * The usual value would be 25% and any language less complete than
 * this will not be added.
 *
 * The purpose of this functionality is to avoid blowing up the size
 * of the AppStream metadata with a lot of extra data detailing
 * languages with very few translated strings.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.5.6
 **/
gboolean
as_app_gettext_search_path (AsApp *app,
			    const gchar *path,
			    gchar **intl_domains,
			    guint min_percentage,
			    GCancellable *cancellable,
			    GError **error)
{
	AsGettextEntry *e;
	GList *l;
	const gchar *filename;
	g_autoptr(AsGettextContext) ctx = NULL;
	g_autoptr(GDir) dir = NULL;

	dir = g_dir_open (path, 0, error);
	if (dir == NULL)
		return FALSE;
	ctx = as_gettext_ctx_new ();
	ctx->min_percentage = min_percentage;
	ctx->intl_domains = g_strdupv (intl_domains);
	while ((filename = g_dir_read_name (dir)) != NULL) {
		g_autofree gchar *fn = NULL;
		fn = g_build_filename (path, filename, "LC_MESSAGES", NULL);
		if (g_file_test (fn, G_FILE_TEST_EXISTS)) {
			if (!as_gettext_ctx_search_locale (ctx, filename, fn, error))
				return FALSE;
		}
	}

	/* calculate percentages */
	for (l = ctx->data; l != NULL; l = l->next) {
		e = l->data;
		e->percentage = MIN (e->nstrings * 100 / ctx->max_nstrings, 100);
	}

	/* sort */
	ctx->data = g_list_sort (ctx->data, as_gettext_entry_sort_cb);

	/* add results */
	for (l = ctx->data; l != NULL; l = l->next) {
		e = l->data;
		if (e->percentage < ctx->min_percentage)
			continue;
		as_app_add_language (app, e->percentage, e->locale);
	}
	return TRUE;
}
