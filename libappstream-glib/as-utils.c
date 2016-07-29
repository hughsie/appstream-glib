/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2016 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2011 Paolo Bacchilega <paobac@src.gnome.org>
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
 * SECTION:as-utils
 * @short_description: Helper functions that are used inside libappstream-glib
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * These functions are used internally to libappstream-glib, and some may be
 * useful to user-applications.
 */

#include "config.h"

#include <fnmatch.h>
#include <string.h>
#include <archive_entry.h>
#include <archive.h>
#include <libsoup/soup.h>
#include <stdlib.h>
#include <uuid.h>

#include "as-app.h"
#include "as-enums.h"
#include "as-node.h"
#include "as-resources.h"
#include "as-store.h"
#include "as-utils.h"
#include "as-utils-private.h"

/**
 * as_utils_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.3.7
 **/
G_DEFINE_QUARK (as-utils-error-quark, as_utils_error)

/**
 * as_hash_lookup_by_locale:
 * @hash: a #GHashTable.
 * @locale: the locale, or %NULL to use the users default local.
 *
 * Gets the 'best' data entry in a hash table using the user-set list
 * of preferred languages.
 *
 * This is how methods like as_app_get_name(app,NULL) return the localized
 * data for the user.
 *
 * Returns: the string value, or %NULL if there was no data
 *
 * Since: 0.1.0
 **/
const gchar *
as_hash_lookup_by_locale (GHashTable *hash, const gchar *locale)
{
	const gchar *const *locales;
	const gchar *tmp = NULL;
	guint i;

	g_return_val_if_fail (hash != NULL, NULL);

	/* the user specified a locale */
	if (locale != NULL)
		return g_hash_table_lookup (hash, locale);

	/* use LANGUAGE, LC_ALL, LC_MESSAGES and LANG */
	locales = g_get_language_names ();
	for (i = 0; locales[i] != NULL; i++) {
		tmp = g_hash_table_lookup (hash, locales[i]);
		if (tmp != NULL)
			return tmp;
	}
	return NULL;
}

static gchar *
as_utils_locale_to_language (const gchar *locale)
{
	gchar *tmp;
	gchar *country_code;

	/* invalid */
	if (locale == NULL)
		return NULL;

	/* return the part before the _ (not always 2 chars!) */
	country_code = g_strdup (locale);
	tmp = g_strstr_len (country_code, -1, "_");
	if (tmp != NULL)
		*tmp = '\0';
	return country_code;
}

/**
 * as_utils_locale_is_compatible:
 * @locale1: a locale string, or %NULL
 * @locale2: a locale string, or %NULL
 *
 * Calculates if one locale is compatible with another.
 * When doing the calculation the locale and language code is taken into
 * account if possible.
 *
 * Returns: %TRUE if the locale is compatible.
 *
 * Since: 0.5.14
 **/
gboolean
as_utils_locale_is_compatible (const gchar *locale1, const gchar *locale2)
{
	g_autofree gchar *lang1 = as_utils_locale_to_language (locale1);
	g_autofree gchar *lang2 = as_utils_locale_to_language (locale2);

	/* we've specified "don't care" and locale unspecified */
	if (locale1 == NULL && locale2 == NULL)
		return TRUE;

	/* forward */
	if (locale1 == NULL && locale2 != NULL) {
		const gchar *const *locales = g_get_language_names ();
		return g_strv_contains (locales, locale2) ||
		       g_strv_contains (locales, lang2);
	}

	/* backwards */
	if (locale1 != NULL && locale2 == NULL) {
		const gchar *const *locales = g_get_language_names ();
		return g_strv_contains (locales, locale1) ||
		       g_strv_contains (locales, lang1);
	}

	/* both specified */
	if (g_strcmp0 (locale1, locale2) == 0)
		return TRUE;
	if (g_strcmp0 (locale1, lang2) == 0)
		return TRUE;
	if (g_strcmp0 (lang1, locale2) == 0)
		return TRUE;
	return FALSE;
}

/**
 * as_utils_is_stock_icon_name:
 * @name: an icon name
 *
 * Searches the known list of stock icons.
 *
 * Returns: %TRUE if the icon is a "stock icon name" and does not need to be
 *          included in the AppStream icon tarball
 *
 * Since: 0.1.3
 **/
gboolean
as_utils_is_stock_icon_name (const gchar *name)
{
	g_autoptr(GBytes) data = NULL;
	g_autofree gchar *key = NULL;
	gchar *tmp;

	/* load the readonly data section and look for the icon name */
	data = g_resource_lookup_data (as_get_resource (),
				       "/org/freedesktop/appstream-glib/as-stock-icons.txt",
				       G_RESOURCE_LOOKUP_FLAGS_NONE,
				       NULL);
	if (data == NULL)
		return FALSE;
	key = g_strdup_printf ("\n%s\n", name);
	tmp = g_strstr_len (key, -1, "-symbolic");
	if (tmp != NULL) {
		tmp[0] = '\n';
		tmp[1] = '\0';
	}
	return g_strstr_len (g_bytes_get_data (data, NULL), -1, key) != NULL;
}

/**
 * as_utils_is_spdx_license_id:
 * @license_id: a single SPDX license ID, e.g. "CC-BY-3.0"
 *
 * Searches the known list of SPDX license IDs.
 *
 * Returns: %TRUE if the icon is a valid "SPDX license ID"
 *
 * Since: 0.1.5
 **/
gboolean
as_utils_is_spdx_license_id (const gchar *license_id)
{
	g_autoptr(GBytes) data = NULL;
	g_autofree gchar *key = NULL;

	/* handle invalid */
	if (license_id == NULL || license_id[0] == '\0')
		return FALSE;

	/* this is used to map non-SPDX licence-ids to legitimate values */
	if (g_str_has_prefix (license_id, "LicenseRef-"))
		return TRUE;

	/* load the readonly data section and look for the icon name */
	data = g_resource_lookup_data (as_get_resource (),
				       "/org/freedesktop/appstream-glib/as-license-ids.txt",
				       G_RESOURCE_LOOKUP_FLAGS_NONE,
				       NULL);
	if (data == NULL)
		return FALSE;
	key = g_strdup_printf ("\n%s\n", license_id);
	return g_strstr_len (g_bytes_get_data (data, NULL), -1, key) != NULL;
}

/**
 * as_utils_is_blacklisted_id:
 * @desktop_id: a desktop ID, e.g. "gimp.desktop"
 *
 * Searches the known list of blacklisted desktop IDs.
 *
 * Returns: %TRUE if the desktop ID is blacklisted
 *
 * Since: 0.2.2
 **/
gboolean
as_utils_is_blacklisted_id (const gchar *desktop_id)
{
	return FALSE;
}

/**
 * as_utils_is_environment_id:
 * @environment_id: a desktop ID, e.g. "GNOME"
 *
 * Searches the known list of registered environment IDs.
 *
 * Returns: %TRUE if the environment ID is valid
 *
 * Since: 0.2.4
 **/
gboolean
as_utils_is_environment_id (const gchar *environment_id)
{
	g_autoptr(GBytes) data = NULL;
	g_autofree gchar *key = NULL;

	/* load the readonly data section and look for the icon name */
	data = g_resource_lookup_data (as_get_resource (),
				       "/org/freedesktop/appstream-glib/as-environment-ids.txt",
				       G_RESOURCE_LOOKUP_FLAGS_NONE,
				       NULL);
	if (data == NULL)
		return FALSE;
	key = g_strdup_printf ("\n%s\n", environment_id);
	return g_strstr_len (g_bytes_get_data (data, NULL), -1, key) != NULL;
}

/**
 * as_utils_is_category_id:
 * @category_id: a desktop ID, e.g. "AudioVideoEditing"
 *
 * Searches the known list of registered category IDs.
 *
 * Returns: %TRUE if the category ID is valid
 *
 * Since: 0.2.4
 **/
gboolean
as_utils_is_category_id (const gchar *category_id)
{
	g_autoptr(GBytes) data = NULL;
	g_autofree gchar *key = NULL;

	/* load the readonly data section and look for the icon name */
	data = g_resource_lookup_data (as_get_resource (),
				       "/org/freedesktop/appstream-glib/as-category-ids.txt",
				       G_RESOURCE_LOOKUP_FLAGS_NONE,
				       NULL);
	if (data == NULL)
		return FALSE;
	key = g_strdup_printf ("\n%s\n", category_id);
	return g_strstr_len (g_bytes_get_data (data, NULL), -1, key) != NULL;
}

typedef struct {
	gboolean	 last_token_literal;
	GPtrArray	*array;
	GString		*collect;
} AsUtilsSpdxHelper;

static gpointer
_g_ptr_array_last (GPtrArray *array)
{
	return g_ptr_array_index (array, array->len - 1);
}

static void
as_utils_spdx_license_tokenize_drop (AsUtilsSpdxHelper *helper)
{
	const gchar *tmp = helper->collect->str;
	guint i;
	g_autofree gchar *last_literal = NULL;
	struct {
		const gchar	*old;
		const gchar	*new;
	} licenses[] =  {
		{ "CC0",	"CC0-1.0" },
		{ "CC-BY",	"CC-BY-3.0" },
		{ "CC-BY-SA",	"CC-BY-SA-3.0" },
		{ "GFDL",	"GFDL-1.3" },
		{ "GPL-2",	"GPL-2.0" },
		{ "GPL-3",	"GPL-3.0" },
		{ "proprietary", "LicenseRef-proprietary" },
		{ NULL, NULL } };

	/* nothing from last time */
	if (helper->collect->len == 0)
		return;

	/* is license enum */
	if (as_utils_is_spdx_license_id (tmp)) {
		g_ptr_array_add (helper->array, g_strdup_printf ("@%s", tmp));
		helper->last_token_literal = FALSE;
		g_string_truncate (helper->collect, 0);
		return;
	}

	/* is old license enum */
	for (i = 0; licenses[i].old != NULL; i++) {
		if (g_strcmp0 (tmp, licenses[i].old) != 0)
			continue;
		g_ptr_array_add (helper->array,
				 g_strdup_printf ("@%s", licenses[i].new));
		helper->last_token_literal = FALSE;
		g_string_truncate (helper->collect, 0);
		return;
	}

	/* is conjunctive */
	if (g_strcmp0 (tmp, "and") == 0 || g_strcmp0 (tmp, "AND") == 0) {
		g_ptr_array_add (helper->array, g_strdup ("&"));
		helper->last_token_literal = FALSE;
		g_string_truncate (helper->collect, 0);
		return;
	}

	/* is disjunctive */
	if (g_strcmp0 (tmp, "or") == 0 || g_strcmp0 (tmp, "OR") == 0) {
		g_ptr_array_add (helper->array, g_strdup ("|"));
		helper->last_token_literal = FALSE;
		g_string_truncate (helper->collect, 0);
		return;
	}

	/* is literal */
	if (helper->last_token_literal) {
		last_literal = g_strdup (_g_ptr_array_last (helper->array));
		g_ptr_array_remove_index (helper->array, helper->array->len - 1);
		g_ptr_array_add (helper->array,
				 g_strdup_printf ("%s %s", last_literal, tmp));
	} else {
		g_ptr_array_add (helper->array, g_strdup (tmp));
		helper->last_token_literal = TRUE;
	}
	g_string_truncate (helper->collect, 0);
}

/**
 * as_utils_spdx_license_tokenize:
 * @license: a license string, e.g. "LGPLv2+ and (QPL or GPLv2) and MIT"
 *
 * Tokenizes the SPDX license string (or any simarly formatted string)
 * into parts. Any licence parts of the string e.g. "LGPL-2.0+" are prefexed
 * with "@", the conjunctive replaced with "&" and the disjunctive replaced
 * with "|". Brackets are added as indervidual tokens and other strings are
 * appended into single tokens where possible.
 *
 * Returns: (transfer full): array of strings, or %NULL for invalid
 *
 * Since: 0.1.5
 **/
gchar **
as_utils_spdx_license_tokenize (const gchar *license)
{
	guint i;
	AsUtilsSpdxHelper helper;

	/* handle invalid */
	if (license == NULL)
		return NULL;

	helper.last_token_literal = FALSE;
	helper.collect = g_string_new ("");
	helper.array = g_ptr_array_new_with_free_func (g_free);
	for (i = 0; license[i] != '\0'; i++) {

		/* handle brackets */
		if (license[i] == '(' || license[i] == ')') {
			as_utils_spdx_license_tokenize_drop (&helper);
			g_ptr_array_add (helper.array, g_strdup_printf ("%c", license[i]));
			helper.last_token_literal = FALSE;
			continue;
		}

		/* space, so dump queue */
		if (license[i] == ' ') {
			as_utils_spdx_license_tokenize_drop (&helper);
			continue;
		}
		g_string_append_c (helper.collect, license[i]);
	}

	/* dump anything remaining */
	as_utils_spdx_license_tokenize_drop (&helper);

	/* return GStrv */
	g_ptr_array_add (helper.array, NULL);
	g_string_free (helper.collect, TRUE);
	return (gchar **) g_ptr_array_free (helper.array, FALSE);
}

/**
 * as_utils_spdx_license_detokenize:
 * @license_tokens: license tokens, typically from as_utils_spdx_license_tokenize()
 *
 * De-tokenizes the SPDX licenses into a string.
 *
 * Returns: (transfer full): string, or %NULL for invalid
 *
 * Since: 0.2.5
 **/
gchar *
as_utils_spdx_license_detokenize (gchar **license_tokens)
{
	GString *tmp;
	guint i;

	/* handle invalid */
	if (license_tokens == NULL)
		return NULL;

	tmp = g_string_new ("");
	for (i = 0; license_tokens[i] != NULL; i++) {
		if (g_strcmp0 (license_tokens[i], "&") == 0) {
			g_string_append (tmp, " AND ");
			continue;
		}
		if (g_strcmp0 (license_tokens[i], "|") == 0) {
			g_string_append (tmp, " OR ");
			continue;
		}
		if (license_tokens[i][0] != '@') {
			g_string_append (tmp, license_tokens[i]);
			continue;
		}
		g_string_append (tmp, license_tokens[i] + 1);
	}
	return g_string_free (tmp, FALSE);
}

/**
 * as_utils_is_spdx_license:
 * @license: a SPDX license string, e.g. "CC-BY-3.0 and GFDL-1.3"
 *
 * Checks the licence string to check it being a valid licence.
 * NOTE: SPDX licences can't typically contain brackets.
 *
 * Returns: %TRUE if the icon is a valid "SPDX license"
 *
 * Since: 0.2.5
 **/
gboolean
as_utils_is_spdx_license (const gchar *license)
{
	guint i;
	g_auto(GStrv) tokens = NULL;

	/* handle nothing set */
	if (license == NULL || license[0] == '\0')
		return FALSE;

	/* no license information whatsoever */
	if (g_strcmp0 (license, "NONE") == 0)
		return TRUE;

	/* creator has intentionally provided no information */
	if (g_strcmp0 (license, "NOASSERTION") == 0)
		return TRUE;

	tokens = as_utils_spdx_license_tokenize (license);
	if (tokens == NULL)
		return FALSE;
	for (i = 0; tokens[i] != NULL; i++) {
		if (tokens[i][0] == '@') {
			if (as_utils_is_spdx_license_id (tokens[i] + 1))
				continue;
		}
		if (as_utils_is_spdx_license_id (tokens[i]))
			continue;
		if (g_strcmp0 (tokens[i], "&") == 0)
			continue;
		if (g_strcmp0 (tokens[i], "|") == 0)
			continue;
		return FALSE;
	}
	return TRUE;
}

/**
 * as_utils_license_to_spdx:
 * @license: a not-quite SPDX license string, e.g. "GPLv3+"
 *
 * Converts a non-SPDX license into an SPDX format string where possible.
 *
 * Returns: the best-effort SPDX license string
 *
 * Since: 0.5.5
 **/
gchar *
as_utils_license_to_spdx (const gchar *license)
{
	GString *str;
	guint i;
	guint j;
	guint license_len;
	struct {
		const gchar	*old;
		const gchar	*new;
	} convert[] =  {
		{ " with exceptions",		NULL },
		{ " with advertising",		NULL },
		{ " and ",			" AND " },
		{ " or ",			" OR " },
		{ "AGPLv3+",			"AGPL-3.0" },
		{ "AGPLv3",			"AGPL-3.0" },
		{ "Artistic 2.0",		"Artistic-2.0" },
		{ "Artistic clarified",		"Artistic-2.0" },
		{ "Artistic",			"Artistic-1.0" },
		{ "ASL 1.1",			"Apache-1.1" },
		{ "ASL 2.0",			"Apache-2.0" },
		{ "Boost",			"BSL-1.0" },
		{ "BSD",			"BSD-3-Clause" },
		{ "CC0",			"CC0-1.0" },
		{ "CC-BY-SA",			"CC-BY-SA-3.0" },
		{ "CC-BY",			"CC-BY-3.0" },
		{ "CDDL",			"CDDL-1.0" },
		{ "CeCILL-C",			"CECILL-C" },
		{ "CeCILL",			"CECILL-2.0" },
		{ "CPAL",			"CPAL-1.0" },
		{ "CPL",			"CPL-1.0" },
		{ "EPL",			"EPL-1.0" },
		{ "Free Art",			"ClArtistic" },
		{ "GFDL",			"GFDL-1.3" },
		{ "GPL+",			"GPL-1.0+" },
		{ "GPLv2+",			"GPL-2.0+" },
		{ "GPLv2",			"GPL-2.0" },
		{ "GPLv3+",			"GPL-3.0+" },
		{ "GPLv3",			"GPL-3.0" },
		{ "IBM",			"IPL-1.0" },
		{ "LGPL+",			"LGPL-2.1+" },
		{ "LGPLv2.1",			"LGPL-2.1" },
		{ "LGPLv2+",			"LGPL-2.1+" },
		{ "LGPLv2",			"LGPL-2.1" },
		{ "LGPLv3+",			"LGPL-3.0+" },
		{ "LGPLv3",			"LGPL-3.0" },
		{ "LPPL",			"LPPL-1.3c" },
		{ "MPLv1.0",			"MPL-1.0" },
		{ "MPLv1.1",			"MPL-1.1" },
		{ "MPLv2.0",			"MPL-2.0" },
		{ "Netscape",			"NPL-1.1" },
		{ "OFL",			"OFL-1.1" },
		{ "Python",			"Python-2.0" },
		{ "QPL",			"QPL-1.0" },
		{ "SPL",			"SPL-1.0" },
		{ "zlib",			"Zlib" },
		{ "ZPLv2.0",			"ZPL-2.0" },
		{ "Unlicense",			"CC0-1.0" },
		{ "Public Domain",		"LicenseRef-public-domain" },
		{ "Copyright only",		"LicenseRef-public-domain" },
		{ "Proprietary",		"LicenseRef-proprietary" },
		{ "Commercial",			"LicenseRef-proprietary" },
		{ NULL, NULL } };

	/* nothing set */
	if (license == NULL)
		return NULL;

	/* already in SPDX format */
	if (as_utils_is_spdx_license (license))
		return g_strdup (license);

	/* go through the string looking for case-insensitive matches */
	str = g_string_new ("");
	license_len = (guint) strlen (license);
	for (i = 0; i < license_len; i++) {
		gboolean found = FALSE;
		for (j = 0; convert[j].old != NULL; j++) {
			guint old_len = (guint) strlen (convert[j].old);
			if (g_ascii_strncasecmp (license + i,
						 convert[j].old,
						 old_len) != 0)
				continue;
			if (convert[j].new != NULL)
				g_string_append (str, convert[j].new);
			i += old_len - 1;
			found = TRUE;
		}
		if (!found)
			g_string_append_c (str, license[i]);
	}
	return g_string_free (str, FALSE);
}

static void
as_pixbuf_blur_private (GdkPixbuf *src, GdkPixbuf *dest, gint radius, guchar *div_kernel_size)
{
	gint width, height, src_rowstride, dest_rowstride, n_channels;
	guchar *p_src, *p_dest, *c1, *c2;
	gint x, y, i, i1, i2, width_minus_1, height_minus_1, radius_plus_1;
	gint r, g, b, a;
	guchar *p_dest_row, *p_dest_col;

	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	n_channels = gdk_pixbuf_get_n_channels (src);
	radius_plus_1 = radius + 1;

	/* horizontal blur */
	p_src = gdk_pixbuf_get_pixels (src);
	p_dest = gdk_pixbuf_get_pixels (dest);
	src_rowstride = gdk_pixbuf_get_rowstride (src);
	dest_rowstride = gdk_pixbuf_get_rowstride (dest);
	width_minus_1 = width - 1;
	for (y = 0; y < height; y++) {

		/* calc the initial sums of the kernel */
		r = g = b = a = 0;
		for (i = -radius; i <= radius; i++) {
			c1 = p_src + (CLAMP (i, 0, width_minus_1) * n_channels);
			r += c1[0];
			g += c1[1];
			b += c1[2];
		}

		p_dest_row = p_dest;
		for (x = 0; x < width; x++) {
			/* set as the mean of the kernel */
			p_dest_row[0] = div_kernel_size[r];
			p_dest_row[1] = div_kernel_size[g];
			p_dest_row[2] = div_kernel_size[b];
			p_dest_row += n_channels;

			/* the pixel to add to the kernel */
			i1 = x + radius_plus_1;
			if (i1 > width_minus_1)
				i1 = width_minus_1;
			c1 = p_src + (i1 * n_channels);

			/* the pixel to remove from the kernel */
			i2 = x - radius;
			if (i2 < 0)
				i2 = 0;
			c2 = p_src + (i2 * n_channels);

			/* calc the new sums of the kernel */
			r += c1[0] - c2[0];
			g += c1[1] - c2[1];
			b += c1[2] - c2[2];
		}

		p_src += src_rowstride;
		p_dest += dest_rowstride;
	}

	/* vertical blur */
	p_src = gdk_pixbuf_get_pixels (dest);
	p_dest = gdk_pixbuf_get_pixels (src);
	src_rowstride = gdk_pixbuf_get_rowstride (dest);
	dest_rowstride = gdk_pixbuf_get_rowstride (src);
	height_minus_1 = height - 1;
	for (x = 0; x < width; x++) {

		/* calc the initial sums of the kernel */
		r = g = b = a = 0;
		for (i = -radius; i <= radius; i++) {
			c1 = p_src + (CLAMP (i, 0, height_minus_1) * src_rowstride);
			r += c1[0];
			g += c1[1];
			b += c1[2];
		}

		p_dest_col = p_dest;
		for (y = 0; y < height; y++) {
			/* set as the mean of the kernel */

			p_dest_col[0] = div_kernel_size[r];
			p_dest_col[1] = div_kernel_size[g];
			p_dest_col[2] = div_kernel_size[b];
			p_dest_col += dest_rowstride;

			/* the pixel to add to the kernel */
			i1 = y + radius_plus_1;
			if (i1 > height_minus_1)
				i1 = height_minus_1;
			c1 = p_src + (i1 * src_rowstride);

			/* the pixel to remove from the kernel */
			i2 = y - radius;
			if (i2 < 0)
				i2 = 0;
			c2 = p_src + (i2 * src_rowstride);

			/* calc the new sums of the kernel */
			r += c1[0] - c2[0];
			g += c1[1] - c2[1];
			b += c1[2] - c2[2];
		}

		p_src += n_channels;
		p_dest += n_channels;
	}
}

/**
 * as_pixbuf_blur:
 * @src: the GdkPixbuf.
 * @radius: the pixel radius for the gaussian blur, typical values are 1..3
 * @iterations: Amount to blur the image, typical values are 1..5
 *
 * Blurs an image. Warning, this method is s..l..o..w... for large images.
 *
 * Since: 0.3.2
 **/
void
as_pixbuf_blur (GdkPixbuf *src, gint radius, gint iterations)
{
	gint kernel_size;
	gint i;
	g_autofree guchar *div_kernel_size = NULL;
	g_autoptr(GdkPixbuf) tmp = NULL;

	tmp = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src),
			      gdk_pixbuf_get_has_alpha (src),
			      gdk_pixbuf_get_bits_per_sample (src),
			      gdk_pixbuf_get_width (src),
			      gdk_pixbuf_get_height (src));
	kernel_size = 2 * radius + 1;
	div_kernel_size = g_new (guchar, 256 * kernel_size);
	for (i = 0; i < 256 * kernel_size; i++)
		div_kernel_size[i] = (guchar) (i / kernel_size);

	while (iterations-- > 0)
		as_pixbuf_blur_private (src, tmp, radius, div_kernel_size);
}

#define interpolate_value(original, reference, distance)		\
	(CLAMP (((distance) * (reference)) +				\
		((1.0 - (distance)) * (original)), 0, 255))

/**
 * as_pixbuf_sharpen: (skip)
 * @src: the GdkPixbuf.
 * @radius: the pixel radius for the unsharp mask, typical values are 1..3
 * @amount: Amount to sharpen the image, typical values are -0.1 to -0.9
 *
 * Sharpens an image. Warning, this method is s..l..o..w... for large images.
 *
 * Since: 0.2.2
 **/
void
as_pixbuf_sharpen (GdkPixbuf *src, gint radius, gdouble amount)
{
	gint width, height, rowstride, n_channels;
	gint x, y;
	guchar *p_blurred;
	guchar *p_blurred_row;
	guchar *p_src;
	guchar *p_src_row;
	g_autoptr(GdkPixbuf) blurred = NULL;

	blurred = gdk_pixbuf_copy (src);
	as_pixbuf_blur (blurred, radius, 3);

	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	rowstride = gdk_pixbuf_get_rowstride (src);
	n_channels = gdk_pixbuf_get_n_channels (src);

	p_src = gdk_pixbuf_get_pixels (src);
	p_blurred = gdk_pixbuf_get_pixels (blurred);

	for (y = 0; y < height; y++) {
		p_src_row = p_src;
		p_blurred_row = p_blurred;
		for (x = 0; x < width; x++) {
			p_src_row[0] = (guchar) interpolate_value (p_src_row[0],
							  p_blurred_row[0],
							  amount);
			p_src_row[1] = (guchar) interpolate_value (p_src_row[1],
							  p_blurred_row[1],
							  amount);
			p_src_row[2] = (guchar) interpolate_value (p_src_row[2],
							  p_blurred_row[2],
							  amount);
			p_src_row += n_channels;
			p_blurred_row += n_channels;
		}
		p_src += rowstride;
		p_blurred += rowstride;
	}
}

/**
 * as_utils_find_icon_filename_full:
 * @destdir: the destdir.
 * @search: the icon search name, e.g. "microphone.svg"
 * @flags: A #AsUtilsFindIconFlag bitfield
 * @error: A #GError or %NULL
 *
 * Finds an icon filename from a filesystem root.
 *
 * Returns: (transfer full): a newly allocated %NULL terminated string
 *
 * Since: 0.3.1
 **/
gchar *
as_utils_find_icon_filename_full (const gchar *destdir,
				  const gchar *search,
				  AsUtilsFindIconFlag flags,
				  GError **error)
{
	guint i;
	guint j;
	guint k;
	guint m;
	const gchar **sizes;
	const gchar *pixmap_dirs[] = { "pixmaps", "icons", NULL };
	const gchar *theme_dirs[] = { "hicolor", "oxygen", NULL };
	const gchar *supported_ext[] = { ".png",
					 ".gif",
					 ".svg",
					 ".xpm",
					 "",
					 NULL };
	const gchar *sizes_lo_dpi[] = { "64x64",
					"128x128",
					"96x96",
					"256x256",
					"512x512",
					"scalable",
					"48x48",
					"32x32",
					"24x24",
					"16x16",
					NULL };
	const gchar *sizes_hi_dpi[] = { "128x128",
					"256x256",
					"512x512",
					"scalable",
					NULL };
	const gchar *types[] = { "actions",
				 "animations",
				 "apps",
				 "categories",
				 "devices",
				 "emblems",
				 "emotes",
				 "filesystems",
				 "intl",
				 "mimetypes",
				 "places",
				 "status",
				 "stock",
				 NULL };
	g_autofree gchar *prefix = NULL;

	/* fallback */
	if (destdir == NULL)
		destdir = "";

	/* is this an absolute path */
	if (search[0] == '/') {
		g_autofree gchar *tmp = NULL;
		tmp = g_build_filename (destdir, search, NULL);
		if (!g_file_test (tmp, G_FILE_TEST_EXISTS)) {
			g_set_error (error,
				     AS_UTILS_ERROR,
				     AS_UTILS_ERROR_FAILED,
				     "specified icon '%s' does not exist",
				     search);
			return NULL;
		}
		return g_strdup (tmp);
	}

	/* all now found in the prefix */
	prefix = g_strdup_printf ("%s/usr", destdir);
	if (!g_file_test (prefix, G_FILE_TEST_EXISTS)) {
		g_free (prefix);
		prefix = g_strdup (destdir);
	}
	if (!g_file_test (prefix, G_FILE_TEST_EXISTS)) {
		g_set_error (error,
			     AS_UTILS_ERROR,
			     AS_UTILS_ERROR_FAILED,
			     "Failed to find icon in prefix %s", search);
		return NULL;
	}

	/* icon theme apps */
	sizes = flags & AS_UTILS_FIND_ICON_HI_DPI ? sizes_hi_dpi : sizes_lo_dpi;
	for (k = 0; theme_dirs[k] != NULL; k++) {
		for (i = 0; sizes[i] != NULL; i++) {
			for (m = 0; types[m] != NULL; m++) {
				for (j = 0; supported_ext[j] != NULL; j++) {
					g_autofree gchar *tmp = NULL;
					tmp = g_strdup_printf ("%s/share/icons/"
							       "%s/%s/%s/%s%s",
							       prefix,
							       theme_dirs[k],
							       sizes[i],
							       types[m],
							       search,
							       supported_ext[j]);
					if (g_file_test (tmp, G_FILE_TEST_EXISTS))
						return g_strdup (tmp);
				}
			}
		}
	}

	/* pixmap */
	for (i = 0; pixmap_dirs[i] != NULL; i++) {
		for (j = 0; supported_ext[j] != NULL; j++) {
			g_autofree gchar *tmp = NULL;
			tmp = g_strdup_printf ("%s/share/%s/%s%s",
					       prefix,
					       pixmap_dirs[i],
					       search,
					       supported_ext[j]);
			if (g_file_test (tmp, G_FILE_TEST_IS_REGULAR))
				return g_strdup (tmp);
		}
	}

	/* failed */
	g_set_error (error,
		     AS_UTILS_ERROR,
		     AS_UTILS_ERROR_FAILED,
		     "Failed to find icon %s", search);
	return NULL;
}

/**
 * as_utils_find_icon_filename:
 * @destdir: the destdir.
 * @search: the icon search name, e.g. "microphone.svg"
 * @error: A #GError or %NULL
 *
 * Finds an icon filename from a filesystem root.
 *
 * Returns: (transfer full): a newly allocated %NULL terminated string
 *
 * Since: 0.2.5
 **/
gchar *
as_utils_find_icon_filename (const gchar *destdir,
			     const gchar *search,
			     GError **error)
{
	return as_utils_find_icon_filename_full (destdir, search,
						 AS_UTILS_FIND_ICON_NONE,
						 error);
}

static const gchar *
as_utils_location_get_prefix (AsUtilsLocation location)
{
	if (location == AS_UTILS_LOCATION_SHARED)
		return "/usr/share";
	if (location == AS_UTILS_LOCATION_CACHE)
		return "/var/cache";
	if (location == AS_UTILS_LOCATION_USER)
		return "~/.local/share";
	return NULL;
}

static gboolean
as_utils_install_icon (AsUtilsLocation location,
		       const gchar *filename,
		       const gchar *origin,
		       const gchar *destdir,
		       GError **error)
{
	const gchar *pathname;
	const gchar *tmp;
	gboolean ret = TRUE;
	gsize len;
	int r;
	struct archive *arch = NULL;
	struct archive_entry *entry;
	g_autofree gchar *data = NULL;
	g_autofree gchar *dir = NULL;

	dir = g_strdup_printf ("%s%s/app-info/icons/%s",
			       destdir,
			       as_utils_location_get_prefix (location),
			       origin);

	/* load file at once to avoid seeking */
	ret = g_file_get_contents (filename, &data, &len, error);
	if (!ret)
		goto out;

	/* read anything */
	arch = archive_read_new ();
	archive_read_support_format_all (arch);
	archive_read_support_filter_all (arch);
	r = archive_read_open_memory (arch, data, len);
	if (r) {
		ret = FALSE;
		g_set_error (error,
			     AS_UTILS_ERROR,
			     AS_UTILS_ERROR_FAILED,
			     "Cannot open: %s",
			     archive_error_string (arch));
		goto out;
	}

	/* decompress each file */
	for (;;) {
		g_autofree gchar *buf = NULL;

		r = archive_read_next_header (arch, &entry);
		if (r == ARCHIVE_EOF)
			break;
		if (r != ARCHIVE_OK) {
			ret = FALSE;
			g_set_error (error,
				     AS_UTILS_ERROR,
				     AS_UTILS_ERROR_FAILED,
				     "Cannot read header: %s",
				     archive_error_string (arch));
			goto out;
		}

		/* no output file */
		pathname = archive_entry_pathname (entry);
		if (pathname == NULL)
			continue;

		/* update output path */
		buf = g_build_filename (dir, pathname, NULL);
		archive_entry_update_pathname_utf8 (entry, buf);

		/* update hardlinks */
		tmp = archive_entry_hardlink (entry);
		if (tmp != NULL) {
			g_autofree gchar *buf_link = NULL;
			buf_link = g_build_filename (dir, tmp, NULL);
			archive_entry_update_hardlink_utf8 (entry, buf_link);
		}

		/* update symlinks */
		tmp = archive_entry_symlink (entry);
		if (tmp != NULL) {
			g_autofree gchar *buf_link = NULL;
			buf_link = g_build_filename (dir, tmp, NULL);
			archive_entry_update_symlink_utf8 (entry, buf_link);
		}

		r = archive_read_extract (arch, entry, 0);
		if (r != ARCHIVE_OK) {
			ret = FALSE;
			g_set_error (error,
				     AS_UTILS_ERROR,
				     AS_UTILS_ERROR_FAILED,
				     "Cannot extract: %s",
				     archive_error_string (arch));
			goto out;
		}
	}
out:
	if (arch != NULL) {
		archive_read_close (arch);
		archive_read_free (arch);
	}
	return ret;
}

static gboolean
as_utils_install_xml (const gchar *filename,
		      const gchar *origin,
		      const gchar *dir,
		      const gchar *destdir,
		      GError **error)
{
	gchar *tmp;
	g_autofree gchar *basename = NULL;
	g_autofree gchar *path_dest = NULL;
	g_autofree gchar *path_parent = NULL;
	g_autoptr(GFile) file_dest = NULL;
	g_autoptr(GFile) file_src = NULL;

	/* create directory structure */
	path_parent = g_strdup_printf ("%s%s", destdir, dir);
	if (g_mkdir_with_parents (path_parent, 0777) != 0) {
		g_set_error (error,
			     AS_UTILS_ERROR,
			     AS_UTILS_ERROR_FAILED,
			     "Failed to create %s", path_parent);
		return FALSE;
	}

	/* calculate the new destination */
	file_src = g_file_new_for_path (filename);
	basename = g_path_get_basename (filename);
	if (origin != NULL) {
		g_autofree gchar *basename_new = NULL;
		tmp = g_strstr_len (basename, -1, ".");
		if (tmp == NULL) {
			g_set_error (error,
				     AS_UTILS_ERROR,
				     AS_UTILS_ERROR_FAILED,
				     "Name of XML file invalid %s",
				     basename);
			return FALSE;
		}
		basename_new = g_strdup_printf ("%s%s", origin, tmp);
		/* replace the fedora.xml.gz into %{origin}.xml.gz */
		path_dest = g_build_filename (path_parent, basename_new, NULL);
	} else {
		path_dest = g_build_filename (path_parent, basename, NULL);
	}

	/* actually copy file */
	file_dest = g_file_new_for_path (path_dest);
	if (!g_file_copy (file_src, file_dest,
			  G_FILE_COPY_OVERWRITE |
			  G_FILE_COPY_TARGET_DEFAULT_PERMS,
			  NULL, NULL, NULL, error))
		return FALSE;

	/* fix the origin */
	if (origin != NULL) {
		g_autoptr(AsStore) store = NULL;
		store = as_store_new ();
		if (!as_store_from_file (store, file_dest, NULL, NULL, error))
			return FALSE;
		as_store_set_origin (store, origin);
		if (!as_store_to_file (store, file_dest,
				       AS_NODE_TO_XML_FLAG_ADD_HEADER |
				       AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE,
				       NULL, error))
			return FALSE;
	}
	return TRUE;
}

/**
 * as_utils_install_filename:
 * @location: the #AsUtilsLocation, e.g. %AS_UTILS_LOCATION_CACHE
 * @filename: the full path of the file to install
 * @origin: the origin to use for the installation, or %NULL
 * @destdir: the destdir to use, or %NULL
 * @error: A #GError or %NULL
 *
 * Installs an AppData, MetaInfo, AppStream XML or AppStream Icon metadata file.
 *
 * Returns: %TRUE for success, %FALSE if error is set
 *
 * Since: 0.3.4
 **/
gboolean
as_utils_install_filename (AsUtilsLocation location,
			  const gchar *filename,
			  const gchar *origin,
			  const gchar *destdir,
			  GError **error)
{
	gboolean ret = FALSE;
	gchar *tmp;
	g_autofree gchar *basename = NULL;
	g_autofree gchar *path = NULL;

	/* default value */
	if (destdir == NULL)
		destdir = "";

	switch (as_app_guess_source_kind (filename)) {
	case AS_APP_SOURCE_KIND_APPSTREAM:
		if (g_strstr_len (filename, -1, ".yml.gz") != NULL) {
			path = g_build_filename (as_utils_location_get_prefix (location),
						 "app-info", "yaml", NULL);
			ret = as_utils_install_xml (filename, origin, path, destdir, error);
		} else {
			path = g_build_filename (as_utils_location_get_prefix (location),
						 "app-info", "xmls", NULL);
			ret = as_utils_install_xml (filename, origin, path, destdir, error);
		}
		break;
	case AS_APP_SOURCE_KIND_APPDATA:
	case AS_APP_SOURCE_KIND_METAINFO:
		if (location == AS_UTILS_LOCATION_CACHE) {
			g_set_error_literal (error,
					     AS_UTILS_ERROR,
					     AS_UTILS_ERROR_INVALID_TYPE,
					     "cached location unsupported for "
					     "MetaInfo and AppData files");
			return FALSE;
		}
		path = g_build_filename (as_utils_location_get_prefix (location),
					 "appdata", NULL);
		ret = as_utils_install_xml (filename, NULL, path, destdir, error);
		break;
	default:
		/* icons */
		if (origin != NULL) {
			ret = as_utils_install_icon (location, filename, origin, destdir, error);
			break;
		}
		basename = g_path_get_basename (filename);
		tmp = g_strstr_len (basename, -1, "-icons.tar.gz");
		if (tmp != NULL) {
			*tmp = '\0';
			ret = as_utils_install_icon (location, filename, basename, destdir, error);
			break;
		}

		/* unrecognised */
		g_set_error_literal (error,
				     AS_UTILS_ERROR,
				     AS_UTILS_ERROR_INVALID_TYPE,
				     "No idea how to process files of this type");
		break;
	}
	return ret;
}

/**
 * as_utils_search_token_valid:
 * @token: the search token
 *
 * Checks the search token if it is valid. Valid tokens are at least 3 chars in
 * length, not common words like "and", and do not contain markup.
 *
 * Returns: %TRUE is the search token was valid
 *
 * Since: 0.3.4
 **/
gboolean
as_utils_search_token_valid (const gchar *token)
{
	guint i;
	const gchar *blacklist[] = {
		"and", "the", "application", "for", "you", "your",
		"with", "can", "are", "from", "that", "use", "allows", "also",
		"this", "other", "all", "using", "has", "some", "like", "them",
		"well", "not", "using", "not", "but", "set", "its", "into",
		"such", "was", "they", "where", "want", "only", "about",
		"uses", "font", "features", "designed", "provides", "which",
		"many", "used", "org", "fonts", "open", "more", "based",
		"different", "including", "will", "multiple", "out", "have",
		"each", "when", "need", "most", "both", "their", "even",
		"way", "several", "been", "while", "very", "add", "under",
		"what", "those", "much", "either", "currently", "one",
		"support", "make", "over", "these", "there", "without", "etc",
		"main",
		NULL };
	if (strlen (token) < 3)
		return FALSE;
	if (g_strstr_len (token, -1, "<") != NULL)
		return FALSE;
	if (g_strstr_len (token, -1, ">") != NULL)
		return FALSE;
	if (g_strstr_len (token, -1, "(") != NULL)
		return FALSE;
	if (g_strstr_len (token, -1, ")") != NULL)
		return FALSE;
	for (i = 0; blacklist[i] != NULL; i++)  {
		if (g_strcmp0 (token, blacklist[i]) == 0)
			return FALSE;
	}
	return TRUE;
}

/**
 * as_utils_search_tokenize:
 * @search: the search string
 *
 * Splits up a string into tokens and returns tokens that are suitable for
 * searching. This includes taking out common words and casefolding the
 * returned search tokens.
 *
 * Returns: (transfer full): Valid tokens to search for, or %NULL for error
 *
 * Since: 0.3.4
 **/
gchar **
as_utils_search_tokenize (const gchar *search)
{
	gchar **values = NULL;
	guint i;
	guint idx = 0;
	g_auto(GStrv) tmp = NULL;

	/* only add keywords that are long enough */
	tmp = g_strsplit (search, " ", -1);
	values = g_new0 (gchar *, g_strv_length (tmp) + 1);
	for (i = 0; tmp[i] != NULL; i++) {
		if (!as_utils_search_token_valid (tmp[i]))
			continue;
		values[idx++] = g_utf8_casefold (tmp[i], -1);
	}
	if (idx == 0) {
		g_free (values);
		return NULL;
	}
	return values;
}

/**
 * as_utils_vercmp:
 * @version_a: the release version, e.g. 1.2.3
 * @version_b: the release version, e.g. 1.2.3.1
 *
 * Compares version numbers for sorting. This function cannot deal with version
 * strings that do not contain numbers, for instance "rev2706" or "1.2_alpha".
 *
 * Returns: -1 if a < b, +1 if a > b, 0 if they are equal, and %G_MAXINT on error
 *
 * Since: 0.3.5
 */
gint
as_utils_vercmp (const gchar *version_a, const gchar *version_b)
{
	gchar *endptr;
	gint64 ver_a;
	gint64 ver_b;
	guint i;
	guint longest_split;
	g_autofree gchar *str_a = NULL;
	g_autofree gchar *str_b = NULL;
	g_auto(GStrv) split_a = NULL;
	g_auto(GStrv) split_b = NULL;

	/* sanity check */
	if (version_a == NULL || version_b == NULL)
		return G_MAXINT;

	/* optimisation */
	if (g_strcmp0 (version_a, version_b) == 0)
		return 0;

	/* split into sections, and try to parse */
	str_a = as_utils_version_parse (version_a);
	str_b = as_utils_version_parse (version_b);
	split_a = g_strsplit (str_a, ".", -1);
	split_b = g_strsplit (str_b, ".", -1);
	longest_split = MAX (g_strv_length (split_a), g_strv_length (split_b));
	for (i = 0; i < longest_split; i++) {

		/* we lost or gained a dot */
		if (split_a[i] == NULL)
			return -1;
		if (split_b[i] == NULL)
			return 1;

		/* compare integers */
		ver_a = g_ascii_strtoll (split_a[i], &endptr, 10);
		if (endptr != NULL && endptr[0] != '\0')
			return G_MAXINT;
		if (ver_a < 0)
			return G_MAXINT;
		ver_b = g_ascii_strtoll (split_b[i], &endptr, 10);
		if (endptr != NULL && endptr[0] != '\0')
			return G_MAXINT;
		if (ver_b < 0)
			return G_MAXINT;
		if (ver_a < ver_b)
			return -1;
		if (ver_a > ver_b)
			return 1;
	}

	/* we really shouldn't get here */
	return 0;
}

/**
 * as_ptr_array_find_string:
 * @array: gchar* array
 * @value: string to find
 *
 * Finds a string in a pointer array.
 *
 * Returns: the const string, or %NULL if not found
 **/
const gchar *
as_ptr_array_find_string (GPtrArray *array, const gchar *value)
{
	const gchar *tmp;
	guint i;
	for (i = 0; i < array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		if (g_strcmp0 (tmp, value) == 0)
			return tmp;
	}
	return NULL;
}

/**
 * as_utils_guid_is_valid:
 * @guid: string to check
 *
 * Checks the source string is a valid string GUID descriptor.
 *
 * Returns: %TRUE if @guid was a valid GUID, %FALSE otherwise
 *
 * Since: 0.5.0
 **/
gboolean
as_utils_guid_is_valid (const gchar *guid)
{
	gint rc;
	uuid_t uu;
	if (guid == NULL)
		return FALSE;
	rc = uuid_parse (guid, uu);
	return rc == 0;
}

/**
 * as_utils_guid_from_string:
 * @str: A source string to use as a key
 *
 * Returns a GUID for a given string. This uses a hash and so even small
 * differences in the @str will produce radically different return values.
 *
 * The implementation is taken from RFC4122, Section 4.1.3; specifically
 * using a type-5 SHA-1 hash with a DNS namespace.
 * The same result can be obtained with this simple python program:
 *
 *    #!/usr/bin/python
 *    import uuid
 *    print uuid.uuid5(uuid.NAMESPACE_DNS, 'python.org')
 *
 * Returns: A new GUID, or %NULL if the string was invalid
 *
 * Since: 0.5.0
 **/
gchar *
as_utils_guid_from_string (const gchar *str)
{
	const gchar *namespace_id = "6ba7b810-9dad-11d1-80b4-00c04fd430c8";
	gchar guid_new[37]; /* 36 plus NUL */
	gsize digestlen = 20;
	guint8 hash[20];
	gint rc;
	uuid_t uu_namespace;
	uuid_t uu_new;
	g_autoptr(GChecksum) csum = NULL;

	/* invalid */
	if (str == NULL)
		return NULL;

	/* convert the namespace to binary */
	rc = uuid_parse (namespace_id, uu_namespace);
	g_assert (rc == 0);

	/* hash the namespace and then the string */
	csum = g_checksum_new (G_CHECKSUM_SHA1);
	g_checksum_update (csum, (guchar *) uu_namespace, 16);
	g_checksum_update (csum, (guchar *) str, (gssize) strlen (str));
	g_checksum_get_digest (csum, hash, &digestlen);

	/* copy most parts of the hash 1:1 */
	memcpy(uu_new, hash, 16);

	/* set specific bits according to Section 4.1.3 */
	uu_new[6] = (guint8) ((uu_new[6] & 0x0f) | (5 << 4));
	uu_new[8] = (guint8) ((uu_new[8] & 0x3f) | 0x80);

	/* return as a string */
	uuid_unparse (uu_new, guid_new);
	return g_strdup (guid_new);
}

/**
 * as_utils_version_from_uint32:
 * @val: A uint32le version number
 * @flags: flags used for formatting, e.g. %AS_VERSION_PARSE_FLAG_USE_TRIPLET
 *
 * Returns a dotted decimal version string from a 32 bit number.
 *
 * Returns: A version number, e.g. "1.0.3"
 *
 * Since: 0.5.2
 **/
gchar *
as_utils_version_from_uint32 (guint32 val, AsVersionParseFlag flags)
{
	if (flags & AS_VERSION_PARSE_FLAG_USE_TRIPLET) {
		return g_strdup_printf ("%u.%u.%u",
					(val >> 24) & 0xff,
					(val >> 16) & 0xff,
					val & 0xffff);
	}
	return g_strdup_printf ("%u.%u.%u.%u",
				(val >> 24) & 0xff,
				(val >> 16) & 0xff,
				(val >> 8) & 0xff,
				val & 0xff);
}

/**
 * as_utils_version_from_uint16:
 * @val: A uint16le version number
 * @flags: flags used for formatting, e.g. %AS_VERSION_PARSE_FLAG_USE_TRIPLET
 *
 * Returns a dotted decimal version string from a 16 bit number.
 *
 * Returns: A version number, e.g. "1.3"
 *
 * Since: 0.5.2
 **/
gchar *
as_utils_version_from_uint16 (guint16 val, AsVersionParseFlag flags)
{
	return g_strdup_printf ("%u.%u",
				(guint) (val >> 8) & 0xff,
				(guint) val & 0xff);
}

/**
 * as_utils_version_parse:
 * @version: A version number
 *
 * Returns a dotted decimal version string from a version string. The supported
 * formats are:
 *
 * - Dotted decimal, e.g. "1.2.3"
 * - Base 16, a hex number *with* a 0x prefix, e.g. "0x10203"
 * - Base 10, a string containing just [0-9], e.g. "66051"
 * - Date in YYYYMMDD format, e.g. 20150915
 *
 * Anything with a '.' or that doesn't match [0-9] or 0x[a-f,0-9] is considered
 * a string and returned without modification.
 *
 * Returns: A version number, e.g. "1.0.3"
 *
 * Since: 0.5.2
 */
gchar *
as_utils_version_parse (const gchar *version)
{
	gchar *endptr = NULL;
	guint64 tmp;
	guint base;
	guint i;

	/* already dotted decimal */
	if (g_strstr_len (version, -1, ".") != NULL)
		return g_strdup (version);

	/* is a date */
	if (g_str_has_prefix (version, "20") &&
	    strlen (version) == 8)
		return g_strdup (version);

	/* convert 0x prefixed strings to dotted decimal */
	if (g_str_has_prefix (version, "0x")) {
		version += 2;
		base = 16;
	} else {
		/* for non-numeric content, just return the string */
		for (i = 0; version[i] != '\0'; i++) {
			if (!g_ascii_isdigit (version[i]))
				return g_strdup (version);
		}
		base = 10;
	}

	/* convert */
	tmp = g_ascii_strtoull (version, &endptr, base);
	if (endptr != NULL && endptr[0] != '\0')
		return g_strdup (version);
	if (tmp == 0 || tmp < 0xff)
		return g_strdup (version);
	return as_utils_version_from_uint32 ((guint32) tmp, AS_VERSION_PARSE_FLAG_USE_TRIPLET);
}

/**
 * as_utils_string_replace:
 * @string: The #GString to operate on
 * @search: The text to search for
 * @replace: The text to use for substitutions
 *
 * Performs multiple search and replace operations on the given string.
 *
 * Returns: the number of replacements done, or 0 if @search is not found.
 *
 * Since: 0.5.11
 **/
guint
as_utils_string_replace (GString *string, const gchar *search, const gchar *replace)
{
	gchar *tmp;
	guint count = 0;
	guint search_idx = 0;
	guint replace_len;
	guint search_len;

	g_return_val_if_fail (string != NULL, 0);
	g_return_val_if_fail (search != NULL, 0);
	g_return_val_if_fail (replace != NULL, 0);

	/* nothing to do */
	if (string->len == 0)
		return 0;

	search_len = (guint) strlen (search);
	replace_len = (guint) strlen (replace);

	do {
		tmp = g_strstr_len (string->str + search_idx, -1, search);
		if (tmp == NULL)
			break;

		/* advance the counter in case @replace contains @search */
		search_idx = (guint) (tmp - string->str);

		/* reallocate the string if required */
		if (search_len > replace_len) {
			g_string_erase (string, search_idx,
					search_len - replace_len);
			memcpy (tmp, replace, replace_len);
		} else if (search_len < replace_len) {
			g_string_insert_len (string, search_idx, replace,
					     replace_len - search_len);
			/* we have to treat this specially as it could have
			 * been reallocated when the insertion happened */
			memcpy (string->str + search_idx, replace, replace_len);
		} else {
			/* just memcmp in the new string */
			memcpy (tmp, replace, replace_len);
		}
		search_idx += replace_len;
		count++;
	} while (TRUE);

	return count;
}

/**
 * as_utils_iso8601_to_datetime: (skip)
 * @iso_date: The ISO8601 date
 *
 * Converts an ISO8601 to a #GDateTime.
 *
 * Returns: a #GDateTime, or %NULL for error.
 *
 * Since: 0.6.1
 **/
GDateTime *
as_utils_iso8601_to_datetime (const gchar *iso_date)
{
	GTimeVal tv;
	guint dmy[] = {0, 0, 0};

	/* nothing set */
	if (iso_date == NULL || iso_date[0] == '\0')
		return NULL;

	/* try to parse complete ISO8601 date */
	if (g_strstr_len (iso_date, -1, " ") != NULL) {
		if (g_time_val_from_iso8601 (iso_date, &tv) && tv.tv_sec != 0)
			return g_date_time_new_from_timeval_utc (&tv);
	}

	/* g_time_val_from_iso8601() blows goats and won't
	 * accept a valid ISO8601 formatted date without a
	 * time value - try and parse this case */
	if (sscanf (iso_date, "%u-%u-%u", &dmy[0], &dmy[1], &dmy[2]) != 3)
		return NULL;

	/* create valid object */
	return g_date_time_new_utc ((gint) dmy[0], (gint) dmy[1], (gint) dmy[2], 0, 0, 0);
}
