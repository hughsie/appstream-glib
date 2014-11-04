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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <config.h>
#include <fnmatch.h>

#include <asb-plugin.h>

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
} AsbGettextHeader;

typedef struct {
	gchar		*locale;
	guint		 nstrings;
	guint		 percentage;
} AsbGettextEntry;

typedef struct {
	guint		 max_nstrings;
	GList		*data;
	gchar		*prefered_mo_filename;
} AsbGettextContext;

/**
 * asb_plugin_get_name:
 */
const gchar *
asb_plugin_get_name (void)
{
	return "gettext";
}

/**
 * asb_plugin_add_globs:
 */
void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/share/locale/*/LC_MESSAGES/*.mo");
}

/**
 * asb_gettext_entry_new:
 **/
static AsbGettextEntry *
asb_gettext_entry_new (void)
{
	AsbGettextEntry *entry;
	entry = g_slice_new0 (AsbGettextEntry);
	return entry;
}

/**
 * asb_gettext_entry_free:
 **/
static void
asb_gettext_entry_free (AsbGettextEntry *entry)
{
	g_free (entry->locale);
	g_slice_free (AsbGettextEntry, entry);
}

/**
 * asb_gettext_ctx_new:
 **/
static AsbGettextContext *
asb_gettext_ctx_new (void)
{
	AsbGettextContext *ctx;
	ctx = g_new0 (AsbGettextContext, 1);
	return ctx;
}

/**
 * asb_gettext_ctx_free:
 **/
static void
asb_gettext_ctx_free (AsbGettextContext *ctx)
{
	g_list_free_full (ctx->data, (GDestroyNotify) asb_gettext_entry_free);
	g_free (ctx->prefered_mo_filename);
	g_free (ctx);
}

/**
 * asb_gettext_parse_file:
 **/
static gboolean
asb_gettext_parse_file (AsbGettextContext *ctx,
			const gchar *locale,
			const gchar *filename,
			GError **error)
{
	AsbGettextEntry *entry;
	AsbGettextHeader *h;
	_cleanup_free_ gchar *data = NULL;

	/* read data, although we only strictly need the header */
	if (!g_file_get_contents (filename, &data, NULL, error))
		return FALSE;

	h = (AsbGettextHeader *) data;
	entry = asb_gettext_entry_new ();
	entry->locale = g_strdup (locale);
	entry->nstrings = h->nstrings;
	if (entry->nstrings > ctx->max_nstrings)
		ctx->max_nstrings = entry->nstrings;
	ctx->data = g_list_prepend (ctx->data, entry);
	return TRUE;
}

/**
 * asb_gettext_ctx_search_locale:
 **/
static gboolean
asb_gettext_ctx_search_locale (AsbGettextContext *ctx,
			       const gchar *locale,
			       const gchar *messages_path,
			       GError **error)
{
	const gchar *filename;
	guint i;
	_cleanup_dir_close_ GDir *dir = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *mo_paths = NULL;

	dir = g_dir_open (messages_path, 0, error);
	if (dir == NULL)
		return FALSE;

	/* do a first pass at this, trying to find the prefered .mo */
	mo_paths = g_ptr_array_new_with_free_func (g_free);
	while ((filename = g_dir_read_name (dir)) != NULL) {
		_cleanup_free_ gchar *path = NULL;
		path = g_build_filename (messages_path, filename, NULL);
		if (!g_file_test (path, G_FILE_TEST_EXISTS))
			continue;
		if (g_strcmp0 (filename, ctx->prefered_mo_filename) == 0) {
			if (!asb_gettext_parse_file (ctx, locale, path, error))
				return FALSE;
			return TRUE;
		}
		g_ptr_array_add (mo_paths, g_strdup (path));
	}

	/* fall back to parsing *everything*, which might give us more
	 * language results than is actually true */
	for (i = 0; i < mo_paths->len; i++) {
		filename = g_ptr_array_index (mo_paths, i);
		if (!asb_gettext_parse_file (ctx, locale, filename, error))
			return FALSE;
	}

	return TRUE;
}

static gint
asb_gettext_entry_sort_cb (gconstpointer a, gconstpointer b)
{
	return g_strcmp0 (((AsbGettextEntry *) a)->locale, ((AsbGettextEntry *) b)->locale);
}

/**
 * asb_gettext_ctx_search_path:
 **/
static gboolean
asb_gettext_ctx_search_path (AsbGettextContext *ctx,
			     const gchar *prefix,
			     GError **error)
{
	const gchar *filename;
	AsbGettextEntry *e;
	GList *l;
	_cleanup_dir_close_ GDir *dir = NULL;
	_cleanup_free_ gchar *root = NULL;

	/* search for .mo files in the prefix */
	root = g_build_filename (prefix, "/usr/share/locale", NULL);
	if (!g_file_test (root, G_FILE_TEST_EXISTS))
		return TRUE;
	dir = g_dir_open (root, 0, error);
	if (dir == NULL)
		return FALSE;
	while ((filename = g_dir_read_name (dir)) != NULL) {
		_cleanup_free_ gchar *path = NULL;
		path = g_build_filename (root, filename, "LC_MESSAGES", NULL);
		if (g_file_test (path, G_FILE_TEST_EXISTS)) {
			if (!asb_gettext_ctx_search_locale (ctx, filename, path, error))
				return FALSE;
		}
	}

	/* calculate percentages */
	for (l = ctx->data; l != NULL; l = l->next) {
		e = l->data;
		e->percentage = MIN (e->nstrings * 100 / ctx->max_nstrings, 100);
	}

	/* sort */
	ctx->data = g_list_sort (ctx->data, asb_gettext_entry_sort_cb);
	return TRUE;
}

/**
 * asb_plugin_process_app:
 */
gboolean
asb_plugin_process_app (AsbPlugin *plugin,
			AsbPackage *pkg,
			AsbApp *app,
			const gchar *tmpdir,
			GError **error)
{
	AsbGettextContext *ctx = NULL;
	AsbGettextEntry *e;
	gboolean ret;
	GList *l;

	/* search */
	ctx = asb_gettext_ctx_new ();
	ctx->prefered_mo_filename = g_strdup_printf ("%s.mo", asb_package_get_name (pkg));
	ret = asb_gettext_ctx_search_path (ctx, tmpdir, error);
	if (!ret)
		goto out;

	/* print results */
	for (l = ctx->data; l != NULL; l = l->next) {
		e = l->data;
		if (e->percentage < 25)
			continue;
		as_app_add_language (AS_APP (app), e->percentage, e->locale, -1);
	}
out:
	asb_gettext_ctx_free (ctx);
	return ret;
}
