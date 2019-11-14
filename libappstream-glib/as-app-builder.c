/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2019 Kalev Lember <klember@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:as-app-builder
 * @short_description: Scan the filesystem for installed languages
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * This object will parse a gettext catalog directory and calculate the
 * language stats for an application.
 *
 * See also: #AsApp
 */

#include "config.h"

#include <string.h>

#include "as-app-builder.h"

typedef struct {
	gchar		*locale;
	guint		 nstrings;
	guint		 percentage;
} AsAppBuilderEntry;

typedef struct {
	guint		 max_nstrings;
	GList		*data;
	GPtrArray	*translations;		/* no ref */
} AsAppBuilderContext;

static AsAppBuilderEntry *
as_app_builder_entry_new (void)
{
	AsAppBuilderEntry *entry;
	entry = g_slice_new0 (AsAppBuilderEntry);
	return entry;
}

static void
as_app_builder_entry_free (AsAppBuilderEntry *entry)
{
	g_free (entry->locale);
	g_slice_free (AsAppBuilderEntry, entry);
}

static AsAppBuilderContext *
as_app_builder_ctx_new (void)
{
	AsAppBuilderContext *ctx;
	ctx = g_new0 (AsAppBuilderContext, 1);
	return ctx;
}

static void
as_app_builder_ctx_free (AsAppBuilderContext *ctx)
{
	g_list_free_full (ctx->data, (GDestroyNotify) as_app_builder_entry_free);
	g_free (ctx);
}

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
} AsAppBuilderGettextHeader;

static void
as_app_builder_add_entry (AsAppBuilderContext *ctx, AsAppBuilderEntry *entry)
{
	if (entry->nstrings > ctx->max_nstrings)
		ctx->max_nstrings = entry->nstrings;
	ctx->data = g_list_prepend (ctx->data, entry);
}

static gboolean
as_app_builder_parse_file_gettext (AsAppBuilderContext *ctx,
				   const gchar *locale,
				   const gchar *filename,
				   GError **error)
{
	AsAppBuilderEntry *entry;
	AsAppBuilderGettextHeader h;
	g_autofree gchar *data = NULL;
	gboolean swapped;

	/* read data */
	if (!g_file_get_contents (filename, &data, NULL, error))
		return FALSE;

	/* we only strictly need the header */
	memcpy (&h, data, sizeof (AsAppBuilderGettextHeader));
	if (h.magic == 0x950412de)
		swapped = FALSE;
	else if (h.magic == 0xde120495)
		swapped = TRUE;
	else {
		g_set_error_literal (error,
				     AS_APP_ERROR,
				     AS_APP_ERROR_FAILED,
				     "file is invalid");
		return FALSE;
	}
	entry = as_app_builder_entry_new ();
	entry->locale = g_strdup (locale);
	if (swapped)
		entry->nstrings = GUINT32_SWAP_LE_BE (h.nstrings);
	else
		entry->nstrings = h.nstrings;
	as_app_builder_add_entry (ctx, entry);
	return TRUE;
}

static gboolean
as_app_builder_search_locale_gettext (AsAppBuilderContext *ctx,
				      const gchar *locale,
				      const gchar *messages_path,
				      AsAppBuilderFlags flags,
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

	/* do a first pass at this, trying to find the preferred .mo */
	mo_paths = g_ptr_array_new_with_free_func (g_free);
	while ((filename = g_dir_read_name (dir)) != NULL) {
		g_autofree gchar *path = NULL;
		path = g_build_filename (messages_path, filename, NULL);
		if (!g_file_test (path, G_FILE_TEST_EXISTS))
			continue;
		for (i = 0; i < ctx->translations->len; i++) {
			AsTranslation *t = g_ptr_array_index (ctx->translations, i);
			g_autofree gchar *fn = NULL;
			if (as_translation_get_kind (t) != AS_TRANSLATION_KIND_GETTEXT &&
			    as_translation_get_kind (t) != AS_TRANSLATION_KIND_UNKNOWN)
				continue;
			fn = g_strdup_printf ("%s.mo", as_translation_get_id (t));
			if (g_strcmp0 (filename, fn) == 0) {
				if (!as_app_builder_parse_file_gettext (ctx,
									locale,
									path,
									error))
					return FALSE;
				found_anything = TRUE;
			}
		}
		g_ptr_array_add (mo_paths, g_strdup (path));
	}

	/* we got data from one or more of the translations */
	if (found_anything == TRUE)
		return TRUE;

	/* fall back to parsing *everything*, which might give us more
	 * language results than is actually true */
	if (flags & AS_APP_BUILDER_FLAG_USE_FALLBACKS) {
		for (i = 0; i < mo_paths->len; i++) {
			filename = g_ptr_array_index (mo_paths, i);
			if (!as_app_builder_parse_file_gettext (ctx,
								locale,
								filename,
								error))
				return FALSE;
		}
	}

	return TRUE;
}

typedef enum {
	AS_APP_TRANSLATION_QM_TAG_END		= 0x1,
	/* SourceText16 */
	AS_APP_TRANSLATION_QM_TAG_TRANSLATION	= 0x3,
	/* Context16 */
	AS_APP_TRANSLATION_QM_TAG_OBSOLETE1	= 0x5,
	AS_APP_TRANSLATION_QM_TAG_SOURCE_TEXT	= 0x6,
	AS_APP_TRANSLATION_QM_TAG_CONTEXT	= 0x7,
	AS_APP_TRANSLATION_QM_TAG_COMMENT	= 0x8,
	/* Obsolete2 */
	AS_APP_TRANSLATION_QM_TAG_LAST
} AsAppBuilderQmTag;

typedef enum {
	AS_APP_TRANSLATION_QM_SECTION_CONTEXTS	= 0x2f,
	AS_APP_TRANSLATION_QM_SECTION_HASHES	= 0x42,
	AS_APP_TRANSLATION_QM_SECTION_MESSAGES	= 0x69,
	AS_APP_TRANSLATION_QM_SECTION_NUMERUS	= 0x88,
	AS_APP_TRANSLATION_QM_SECTION_DEPS	= 0x96,
	AS_APP_TRANSLATION_QM_SECTION_LAST
} AsAppBuilderQmSection;

static guint8
_read_uint8 (const guint8 *data, guint32 *offset)
{
	guint8 tmp;
	tmp = data[*offset];
	(*offset) += 1;
	return tmp;
}

static guint32
_read_uint32 (const guint8 *data, guint32 *offset)
{
	guint32 tmp = 0;
	memcpy (&tmp, data + *offset, 4);
	(*offset) += 4;
	return GUINT32_FROM_BE (tmp);
}

static void
as_app_builder_parse_data_qt (AsAppBuilderContext *ctx,
			      const gchar *locale,
			      const guint8 *data,
			      guint32 len)
{
	AsAppBuilderEntry *entry;
	guint32 m = 0;
	guint nstrings = 0;

	/* read data */
	while (m < len) {
		guint32 tag_len;
		AsAppBuilderQmTag tag = _read_uint8(data, &m);
		switch (tag) {
		case AS_APP_TRANSLATION_QM_TAG_END:
			break;
		case AS_APP_TRANSLATION_QM_TAG_OBSOLETE1:
			m += 4;
			break;
		case AS_APP_TRANSLATION_QM_TAG_TRANSLATION:
		case AS_APP_TRANSLATION_QM_TAG_SOURCE_TEXT:
		case AS_APP_TRANSLATION_QM_TAG_CONTEXT:
		case AS_APP_TRANSLATION_QM_TAG_COMMENT:
			tag_len = _read_uint32 (data, &m);
			if (tag_len < 0xffffffff)
				m += tag_len;
			if (tag == AS_APP_TRANSLATION_QM_TAG_TRANSLATION)
				nstrings++;
			break;
		default:
			m = G_MAXUINT32;
			break;
		}
	}

	/* add new entry */
	entry = as_app_builder_entry_new ();
	entry->locale = g_strdup (locale);
	entry->nstrings = nstrings;
	as_app_builder_add_entry (ctx, entry);
}

static gboolean
as_app_builder_parse_file_qt (AsAppBuilderContext *ctx,
			      const gchar *locale,
			      const gchar *filename,
			      GError **error)
{
	gsize len;
	guint32 m = 0;
	g_autofree guint8 *data = NULL;
	const guint8 qm_magic[] = {
		0x3c, 0xb8, 0x64, 0x18, 0xca, 0xef, 0x9c, 0x95,
		0xcd, 0x21, 0x1c, 0xbf, 0x60, 0xa1, 0xbd, 0xdd
	};

	/* load file */
	if (!g_file_get_contents (filename, (gchar **) &data, &len, error))
		return FALSE;

	/* check header */
	if (len < sizeof(qm_magic) ||
	    memcmp (data, qm_magic, sizeof(qm_magic)) != 0) {
		g_set_error_literal (error,
				     AS_APP_ERROR,
				     AS_APP_ERROR_FAILED,
				     "file is invalid");
		return FALSE;
	}
	m += (guint32) sizeof(qm_magic);

	/* parse each section */
	while (m < len) {
		AsAppBuilderQmSection section = _read_uint8(data, &m);
		guint32 section_len = _read_uint32 (data, &m);
		if (section_len > len - m) {
			g_set_error_literal (error,
					     AS_APP_ERROR,
					     AS_APP_ERROR_FAILED,
					     "file is invalid, section too large");
			return FALSE;
		}
		if (section == AS_APP_TRANSLATION_QM_SECTION_MESSAGES) {
			as_app_builder_parse_data_qt (ctx,
						      locale,
						      data + m,
						      section_len);
		}
		m += section_len;
	}

	return TRUE;
}

static gboolean
as_app_builder_search_translations_qt (AsAppBuilderContext *ctx,
				       const gchar *prefix,
				       AsAppBuilderFlags flags,
				       GError **error)
{
	guint i;

	/* search for each translation ID */
	for (i = 0; i < ctx->translations->len; i++) {
		AsTranslation *t;
		const gchar *dirname;
		const gchar *filename;
		const gchar *install_dir;
		g_autofree gchar *path = NULL;
		g_autoptr(GDir) dir = NULL;

		/* FIXME: this path probably has to be specified as an attribute
		 * in the <translations> tag from the AppData file */
		t = g_ptr_array_index (ctx->translations, i);
		if (as_translation_get_kind (t) != AS_TRANSLATION_KIND_QT &&
		    as_translation_get_kind (t) != AS_TRANSLATION_KIND_UNKNOWN)
			continue;
		if (as_translation_get_id (t) == NULL)
			continue;
		install_dir = as_translation_get_id (t);
		path = g_build_filename (prefix,
					 "share",
					 install_dir,
					 "translations",
					 NULL);
		if (!g_file_test (path, G_FILE_TEST_EXISTS))
			return TRUE;
		dir = g_dir_open (path, 0, error);
		if (dir == NULL)
			return FALSE;

		/* look for ${prefix}/share/${install_dir}/translations/${id}_${locale}.qm */
		while ((filename = g_dir_read_name (dir)) != NULL) {
			g_autofree gchar *fn = NULL;
			g_autofree gchar *locale = NULL;
			if (!g_str_has_prefix (filename, as_translation_get_id (t)))
				continue;
			if (!g_str_has_suffix (filename, ".qm"))
				continue;
			fn = g_build_filename (path, filename, NULL);
			if (!g_file_test (fn, G_FILE_TEST_IS_REGULAR))
				continue;
			locale = g_strdup (filename + strlen (as_translation_get_id (t)) + 1);
			g_strdelimit (locale, ".", '\0');
			if (!as_app_builder_parse_file_qt (ctx, locale, fn, error))
				return FALSE;
		}

		g_dir_rewind (dir);

		/* look for ${prefix}/share/${install_dir}/translations/${id}/${locale}.qm */
		while ((dirname = g_dir_read_name (dir)) != NULL) {
			g_autofree gchar *path_subdir = NULL;
			g_autoptr(GDir) subdir = NULL;

			if (!g_str_equal (dirname, as_translation_get_id (t)))
				continue;
			path_subdir = g_build_filename (path, dirname, NULL);
			if (!g_file_test (path_subdir, G_FILE_TEST_IS_DIR))
				continue;
			subdir = g_dir_open (path_subdir, 0, error);
			if (subdir == NULL)
				return FALSE;

			while ((filename = g_dir_read_name (subdir)) != NULL) {
				g_autofree gchar *fn = NULL;
				g_autofree gchar *locale = NULL;
				if (!g_str_has_suffix (filename, ".qm"))
					continue;
				fn = g_build_filename (path_subdir, filename, NULL);
				if (!g_file_test (fn, G_FILE_TEST_IS_REGULAR))
					continue;
				locale = g_strdup (filename);
				g_strdelimit (locale, ".", '\0');
				if (!as_app_builder_parse_file_qt (ctx, locale, fn, error))
					return FALSE;
			}
		}
	}

	return TRUE;
}

static gboolean
as_app_builder_search_translations_gettext (AsAppBuilderContext *ctx,
					    const gchar *prefix,
					    AsAppBuilderFlags flags,
					    GError **error)
{
	const gchar *locale;
	g_autofree gchar *path = NULL;
	g_autoptr(GDir) dir = NULL;

	path = g_build_filename (prefix, "share", "locale", NULL);
	if (!g_file_test (path, G_FILE_TEST_EXISTS))
		return TRUE;
	dir = g_dir_open (path, 0, error);
	if (dir == NULL)
		return FALSE;
	while ((locale = g_dir_read_name (dir)) != NULL) {
		g_autofree gchar *fn = NULL;
		fn = g_build_filename (path, locale, "LC_MESSAGES", NULL);
		if (!g_file_test (fn, G_FILE_TEST_EXISTS))
			continue;
		if (!as_app_builder_search_locale_gettext (ctx, locale, fn, flags, error))
			return FALSE;
	}
	return TRUE;
}

static gchar *
as_app_builder_get_locale_from_pak_fn (const gchar *basename)
{
	gchar *locale;
	gchar *str;

	/* FIXME: convert using a lookup table where required */
	locale = g_strdup (basename);
	str = g_strrstr (locale, ".");
	if (str != NULL)
		*str = '\0';
	g_strdelimit (locale, "-", '_');
	return locale;
}

static gboolean
as_app_builder_parse_file_pak (AsAppBuilderContext *ctx,
			       const gchar *locale,
			       const gchar *filename,
			       GError **error)
{
	AsAppBuilderEntry *entry;
	gsize len = 0;
	guint32 nr_resources;
	guint32 version_number;
	guint8 encoding;
	g_autofree gchar *data = NULL;

	if (!g_file_get_contents (filename, &data, &len, error))
		return FALSE;
	if (len < 9) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_FAILED,
			     "file invalid, %" G_GSIZE_FORMAT "b in size",
			     len);
		return FALSE;
	}

	/* get 4 byte version number */
	memcpy (&version_number, data + 0, 4);
	if (version_number != 4) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_FAILED,
			     "version number invalid, "
			     "got %" G_GUINT32_FORMAT " expected 4",
			     version_number);
		return FALSE;
	}

	/* get 4 byte number of resources */
	memcpy (&nr_resources, data + 4, 4);
	if (nr_resources == 0) {
		g_set_error_literal (error,
				     AS_APP_ERROR,
				     AS_APP_ERROR_FAILED,
				     "no resources found");
		return FALSE;
	}

	/* get single byte of encoding */
	encoding = (guint8) data[8];
	if (encoding != 0 && encoding != 1) {
		g_set_error (error,
			     AS_APP_ERROR,
			     AS_APP_ERROR_FAILED,
			     "PAK encoding invalid, got %u expected 0 or 1",
			     encoding);
		return FALSE;
	}

	/* create entry without reading resources */
	entry = as_app_builder_entry_new ();
	entry->locale = g_strdup (locale);
	entry->nstrings = nr_resources;
	as_app_builder_add_entry (ctx, entry);
	return TRUE;
}

static gboolean
as_app_builder_search_translations_pak (AsAppBuilderContext *ctx,
					const gchar *prefix,
					AsAppBuilderFlags flags,
					GError **error)
{
	guint i;

	/* search for each translation ID */
	for (i = 0; i < ctx->translations->len; i++) {
		const gchar *tmp;
		g_autoptr(GDir) dir = NULL;
		AsTranslation *t = g_ptr_array_index (ctx->translations, i);
		const gchar *libdirs[] = { "lib64", "lib", NULL };

		/* required */
		if (as_translation_get_id (t) == NULL)
			continue;
		for (guint j = 0; libdirs[j] != NULL; j++) {
			g_autofree gchar *path = NULL;
			path = g_build_filename (prefix,
						 libdirs[j],
						 as_translation_get_id (t),
						 "locales",
						 NULL);
			if (!g_file_test (path, G_FILE_TEST_EXISTS))
				continue;
			dir = g_dir_open (path, 0, error);
			if (dir == NULL)
				return FALSE;

			/* parse file for sanity */
			while ((tmp = g_dir_read_name (dir)) != 0) {
				g_autofree gchar *locale = NULL;
				g_autofree gchar *fn = g_build_filename (path, tmp, NULL);
				locale = as_app_builder_get_locale_from_pak_fn (tmp);
				if (!as_app_builder_parse_file_pak (ctx, locale, fn, error))
					return FALSE;
			}
		}
	}
	return TRUE;
}

static gchar *
as_app_builder_get_locale_from_xpi_fn (const gchar *basename)
{
	gchar *locale;
	gchar *str;

	/* remove prefix */
	if (g_str_has_prefix (basename, "langpack-"))
		basename += 9;

	/* remove suffix */
	locale = g_strdup (basename);
	str = g_strrstr (locale, "@");
	if (str != NULL)
		*str = '\0';
	g_strdelimit (locale, "-", '_');
	return locale;
}

static gboolean
as_app_builder_parse_file_xpi (AsAppBuilderContext *ctx,
			       const gchar *locale,
			       const gchar *filename,
			       GError **error)
{
	AsAppBuilderEntry *entry = as_app_builder_entry_new ();
	entry->locale = g_strdup (locale);
	entry->nstrings = 100;	/* FIXME: parse info */
	as_app_builder_add_entry (ctx, entry);
	return TRUE;
}

static gboolean
as_app_builder_search_translations_xpi (AsAppBuilderContext *ctx,
					const gchar *prefix,
					AsAppBuilderFlags flags,
					GError **error)
{
	const gchar *tmp;
	g_autoptr(GDir) dir = NULL;
	const gchar *libdirs[] = { "lib64", "lib", NULL };

	/* list files */
	for (guint j = 0; libdirs[j] != NULL; j++) {
		g_autofree gchar *path = NULL;
		path = g_build_filename (prefix,
					 libdirs[j],
					 "firefox",
					 "langpacks",
					 NULL);
		if (!g_file_test (path, G_FILE_TEST_EXISTS))
			continue;
		dir = g_dir_open (path, 0, error);
		if (dir == NULL)
			return FALSE;

		/* parse file for sanity */
		while ((tmp = g_dir_read_name (dir)) != 0) {
			g_autofree gchar *locale = NULL;
			g_autofree gchar *fn = g_build_filename (path, tmp, NULL);
			if (g_file_test (fn, G_FILE_TEST_IS_SYMLINK))
				continue;
			locale = as_app_builder_get_locale_from_xpi_fn (tmp);
			if (!as_app_builder_parse_file_xpi (ctx, locale, fn, error))
				return FALSE;
		}
	}

	return TRUE;
}

static gint
as_app_builder_entry_sort_cb (gconstpointer a, gconstpointer b)
{
	return g_strcmp0 (((AsAppBuilderEntry *) a)->locale,
			  ((AsAppBuilderEntry *) b)->locale);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(AsAppBuilderContext, as_app_builder_ctx_free)

/**
 * as_app_builder_search_translations:
 * @app: an #AsApp
 * @prefix: a prefix to search, e.g. "/usr"
 * @min_percentage: minimum percentage to add language
 * @flags: #AsAppBuilderFlags, e.g. %AS_APP_BUILDER_FLAG_USE_FALLBACKS
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError or %NULL
 *
 * Searches a prefix for languages, and using a heuristic adds <language>
 * tags to the specified application.
 *
 * If there are no #AsTranslation objects set on the #AsApp then all domains
 * are matched, which may include more languages than you intended to.
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
 * Since: 0.5.8
 **/
gboolean
as_app_builder_search_translations (AsApp *app,
				    const gchar *prefix,
				    guint min_percentage,
				    AsAppBuilderFlags flags,
				    GCancellable *cancellable,
				    GError **error)
{
	AsAppBuilderEntry *e;
	GList *l;
	g_autoptr(AsAppBuilderContext) ctx = NULL;

	ctx = as_app_builder_ctx_new ();
	ctx->translations = as_app_get_translations (app);

	/* search for QT .qm files */
	if (!as_app_builder_search_translations_qt (ctx, prefix, flags, error))
		return FALSE;

	/* search for gettext .mo files */
	if (!as_app_builder_search_translations_gettext (ctx, prefix, flags, error))
		return FALSE;

	/* search for Google .pak files */
	if (!as_app_builder_search_translations_pak (ctx, prefix, flags, error))
		return FALSE;

	/* search for Mozilla .xpi files */
	if (!as_app_builder_search_translations_xpi (ctx, prefix, flags, error))
		return FALSE;

	/* calculate percentages */
	for (l = ctx->data; l != NULL; l = l->next) {
		e = l->data;
		e->percentage = MIN (e->nstrings * 100 / ctx->max_nstrings, 100);
	}

	/* sort */
	ctx->data = g_list_sort (ctx->data, as_app_builder_entry_sort_cb);

	/* add results */
	for (l = ctx->data; l != NULL; l = l->next) {
		e = l->data;
		if (e->percentage < min_percentage)
			continue;
		as_app_add_language (app, (gint) e->percentage, e->locale);
	}
	return TRUE;
}

static gboolean
as_app_builder_search_path (AsApp *app,
			    const gchar *prefix,
			    const gchar *path,
			    AsAppBuilderFlags flags)
{
	const gchar *tmp;
	g_autofree gchar *fn_prefix = NULL;
	g_autoptr(GDir) dir = NULL;

	/* find dir */
	fn_prefix = g_build_filename (prefix, path, NULL);
	if (!g_file_test (fn_prefix, G_FILE_TEST_IS_DIR))
		return FALSE;
	dir = g_dir_open (fn_prefix, 0, NULL);
	if (dir == NULL)
		return FALSE;

	/* find any file with the app-id prefix */
	while ((tmp = g_dir_read_name (dir)) != NULL) {
		if (g_str_has_prefix (tmp, as_app_get_id (app)))
			return TRUE;
	}

	/* just anything */
	if (flags & AS_APP_BUILDER_FLAG_USE_FALLBACKS)
		return TRUE;

	return FALSE;
}

/**
 * as_app_builder_search_kudos:
 * @app: an #AsApp
 * @prefix: a prefix to search, e.g. "/usr"
 * @flags: #AsAppBuilderFlags, e.g. %AS_APP_BUILDER_FLAG_USE_FALLBACKS
 * @error: a #GError or %NULL
 *
 * Searches a prefix for auto-detected kudos.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.5.8
 **/
gboolean
as_app_builder_search_kudos (AsApp *app,
			     const gchar *prefix,
			     AsAppBuilderFlags flags,
			     GError **error)
{
	/* gnome-shell search provider */
	if (!as_app_has_kudo_kind (app, AS_KUDO_KIND_SEARCH_PROVIDER) &&
	    as_app_builder_search_path (app, prefix,
					"share/gnome-shell/search-providers",
					flags)) {
		g_debug ("auto-adding SearchProvider kudo");
		as_app_add_kudo_kind (AS_APP (app), AS_KUDO_KIND_SEARCH_PROVIDER);
	}

	/* hicolor icon */
	if (!as_app_has_kudo_kind (app, AS_KUDO_KIND_HIGH_CONTRAST) &&
	    as_app_builder_search_path (app, prefix,
					"share/icons/hicolor/symbolic/apps",
					flags)) {
		g_debug ("auto-adding HighContrast kudo");
		as_app_add_kudo_kind (AS_APP (app), AS_KUDO_KIND_HIGH_CONTRAST);
	}
	return TRUE;
}

static gboolean
as_app_builder_search_dbus_file (AsApp *app,
				 const gchar *filename,
				 AsProvideKind provide_kind,
				 GError **error)
{
	g_autofree gchar *name = NULL;
	g_autoptr(AsProvide) provide = NULL;
	g_autoptr(GKeyFile) kf = NULL;

	/* load file */
	kf = g_key_file_new ();
	if (!g_key_file_load_from_file (kf, filename, G_KEY_FILE_NONE, error))
		return FALSE;
	name = g_key_file_get_string (kf, "D-BUS Service", "Name", error);
	if (name == NULL)
		return FALSE;

	/* add provide */
	provide = as_provide_new ();
	as_provide_set_kind (provide, provide_kind);
	as_provide_set_value (provide, name);
	as_app_add_provide (AS_APP (app), provide);
	return TRUE;
}

static gboolean
as_app_builder_search_dbus (AsApp *app,
			    const gchar *prefix,
			    const gchar *path,
			    AsProvideKind provide_kind,
			    AsAppBuilderFlags flags,
			    GError **error)
{
	const gchar *tmp;
	g_autofree gchar *fn_prefix = NULL;
	g_autoptr(GDir) dir = NULL;

	/* find dir */
	fn_prefix = g_build_filename (prefix, path, NULL);
	if (!g_file_test (fn_prefix, G_FILE_TEST_IS_DIR))
		return TRUE;
	dir = g_dir_open (fn_prefix, 0, error);
	if (dir == NULL)
		return FALSE;

	/* find any file with the app-id prefix */
	while ((tmp = g_dir_read_name (dir)) != NULL) {
		g_autofree gchar *fn = NULL;
		if ((flags & AS_APP_BUILDER_FLAG_USE_FALLBACKS) == 0) {
			if (!g_str_has_prefix (tmp, as_app_get_id (app)))
				continue;
		}
		fn = g_build_filename (fn_prefix, tmp, NULL);
		if (!as_app_builder_search_dbus_file (app, fn, provide_kind, error))
			return FALSE;
	}
	return TRUE;
}

/**
 * as_app_builder_search_provides:
 * @app: an #AsApp
 * @prefix: a prefix to search, e.g. "/usr"
 * @flags: #AsAppBuilderFlags, e.g. %AS_APP_BUILDER_FLAG_USE_FALLBACKS
 * @error: a #GError or %NULL
 *
 * Searches a prefix for auto-detected provides.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.5.8
 **/
gboolean
as_app_builder_search_provides (AsApp *app,
				const gchar *prefix,
				AsAppBuilderFlags flags,
				GError **error)
{
	/* skip for addons */
	if (as_app_get_kind (AS_APP (app)) == AS_APP_KIND_ADDON)
		return TRUE;

	if (!as_app_builder_search_dbus (app, prefix,
					 "share/dbus-1/system-services",
					 AS_PROVIDE_KIND_DBUS_SYSTEM,
					 flags, error))
		return FALSE;
	if (!as_app_builder_search_dbus (app, prefix,
					 "share/dbus-1/services",
					 AS_PROVIDE_KIND_DBUS_SESSION,
					 flags, error))
		return FALSE;
	return TRUE;
}
