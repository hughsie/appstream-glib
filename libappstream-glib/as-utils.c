/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
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
#include <libsoup/soup.h>

#include "as-app.h"
#include "as-cleanup.h"
#include "as-enums.h"
#include "as-node.h"
#include "as-resources.h"
#include "as-utils.h"
#include "as-utils-private.h"

/**
 * as_strndup:
 * @text: the text to copy.
 * @text_len: the length of @text, or -1 if @text is NULL terminated.
 *
 * Copies a string, with an optional length argument.
 *
 * Returns: (transfer full): a newly allocated %NULL terminated string
 *
 * Since: 0.1.0
 **/
gchar *
as_strndup (const gchar *text, gssize text_len)
{
	if (text_len < 0)
		return g_strdup (text);
	return g_strndup (text, text_len);
}

/**
 * as_markup_convert_simple:
 * @markup: the text to copy.
 * @markup_len: the length of @markup, or -1 if @markup is NULL terminated.
 * @error: A #GError or %NULL
 *
 * Converts an XML description into a printable form.
 *
 * Returns: (transfer full): a newly allocated %NULL terminated string
 *
 * Since: 0.1.0
 **/
gchar *
as_markup_convert_simple (const gchar *markup,
			  gssize markup_len,
			  GError **error)
{
	GNode *tmp;
	GNode *tmp_c;
	const gchar *tag;
	const gchar *tag_c;
	_cleanup_node_unref_ GNode *root = NULL;
	_cleanup_string_free_ GString *str = NULL;

	/* is this actually markup */
	if (g_strstr_len (markup, markup_len, "<") == NULL)
		return as_strndup (markup, markup_len);

	/* load */
	root = as_node_from_xml (markup,
				 markup_len,
				 AS_NODE_FROM_XML_FLAG_NONE,
				 error);
	if (root == NULL)
		return NULL;

	/* format */
	str = g_string_sized_new (markup_len);
	for (tmp = root->children; tmp != NULL; tmp = tmp->next) {

		tag = as_node_get_name (tmp);
		if (g_strcmp0 (tag, "p") == 0) {
			if (str->len > 0)
				g_string_append (str, "\n");
			g_string_append_printf (str, "%s\n", as_node_get_data (tmp));

		/* loop on the children */
		} else if (g_strcmp0 (tag, "ul") == 0 ||
			   g_strcmp0 (tag, "ol") == 0) {
			for (tmp_c = tmp->children; tmp_c != NULL; tmp_c = tmp_c->next) {
				tag_c = as_node_get_name (tmp_c);
				if (g_strcmp0 (tag_c, "li") == 0) {
					g_string_append_printf (str,
								" • %s\n",
								as_node_get_data (tmp_c));
				} else {
					/* only <li> is valid in lists */
					g_set_error (error,
						     AS_NODE_ERROR,
						     AS_NODE_ERROR_FAILED,
						     "Tag %s in %s invalid",
						     tag_c, tag);
					return FALSE;
				}
			}
		} else {
			/* only <p>, <ul> and <ol> is valid here */
			g_set_error (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_FAILED,
				     "Unknown tag '%s'", tag);
			return NULL;
		}
	}

	/* success */
	if (str->len > 0)
		g_string_truncate (str, str->len - 1);
	return g_strdup (str->str);
}

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
	_cleanup_bytes_unref_ GBytes *data = NULL;
	_cleanup_free_ gchar *key = NULL;

	/* load the readonly data section and look for the icon name */
	data = g_resource_lookup_data (as_get_resource (),
				       "/org/freedesktop/appstream-glib/as-stock-icons.txt",
				       G_RESOURCE_LOOKUP_FLAGS_NONE,
				       NULL);
	if (data == NULL)
		return FALSE;
	key = g_strdup_printf ("\n%s\n", name);
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
	_cleanup_bytes_unref_ GBytes *data = NULL;
	_cleanup_free_ gchar *key = NULL;

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
	guint i;
	_cleanup_bytes_unref_ GBytes *data = NULL;
	_cleanup_free_ gchar *key = NULL;
	_cleanup_strv_free_ gchar **split = NULL;

	/* load the readonly data section and look for the icon name */
	data = g_resource_lookup_data (as_get_resource (),
				       "/org/freedesktop/appstream-glib/as-blacklist-ids.txt",
				       G_RESOURCE_LOOKUP_FLAGS_NONE,
				       NULL);
	if (data == NULL)
		return FALSE;
	split = g_strsplit (g_bytes_get_data (data, NULL), "\n", -1);
	for (i = 0; split[i] != NULL; i++) {
		if (fnmatch (split[i], desktop_id, 0) == 0)
			return TRUE;
	}
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
	_cleanup_bytes_unref_ GBytes *data = NULL;
	_cleanup_free_ gchar *key = NULL;

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
	_cleanup_bytes_unref_ GBytes *data = NULL;
	_cleanup_free_ gchar *key = NULL;

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
	_cleanup_free_ gchar *last_literal = NULL;
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
 * Returns: (transfer full): array of strings
 *
 * Since: 0.1.5
 **/
gchar **
as_utils_spdx_license_tokenize (const gchar *license)
{
	guint i;
	AsUtilsSpdxHelper helper;

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
 * Returns: (transfer full): string
 *
 * Since: 0.2.5
 **/
gchar *
as_utils_spdx_license_detokenize (gchar **license_tokens)
{
	GString *tmp;
	guint i;

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
	_cleanup_strv_free_ gchar **tokens = NULL;

	tokens = as_utils_spdx_license_tokenize (license);
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
 * as_utils_check_url_exists:
 * @url: the URL to check.
 * @timeout: the timeout in seconds.
 * @error: A #GError or %NULL
 *
 * Checks to see if a URL is reachable.
 *
 * Returns: %TRUE if the URL was reachable and pointed to a non-zero-length file.
 *
 * Since: 0.1.5
 **/
gboolean
as_utils_check_url_exists (const gchar *url, guint timeout, GError **error)
{
	_cleanup_object_unref_ SoupMessage *msg = NULL;
	_cleanup_object_unref_ SoupSession *session = NULL;
	_cleanup_uri_unref_ SoupURI *base_uri = NULL;

	/* GET file */
	base_uri = soup_uri_new (url);
	if (base_uri == NULL) {
		g_set_error_literal (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_FAILED,
				     "URL not valid");
		return FALSE;
	}
	msg = soup_message_new_from_uri (SOUP_METHOD_GET, base_uri);
	if (msg == NULL) {
		g_set_error_literal (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_FAILED,
				     "Failed to setup message");
		return FALSE;
	}
	session = soup_session_sync_new_with_options (SOUP_SESSION_USER_AGENT,
						      "libappstream-glib",
						      SOUP_SESSION_TIMEOUT,
						      timeout,
						      NULL);
	if (session == NULL) {
		g_set_error_literal (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_FAILED,
				     "Failed to set up networking");
		return FALSE;
	}

	/* send sync */
	if (soup_session_send_message (session, msg) != SOUP_STATUS_OK) {
		g_set_error_literal (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_FAILED,
				     msg->reason_phrase);
		return FALSE;
	}

	/* check if it's a zero sized file */
	if (msg->response_body->length == 0) {
		g_set_error (error,
			     AS_NODE_ERROR,
			     AS_NODE_ERROR_FAILED,
			     "Returned a zero length file");
		return FALSE;
	}
	return TRUE;
}

/**
 * as_pixbuf_blur_private:
 **/
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
	_cleanup_free_ guchar *div_kernel_size = NULL;
	_cleanup_object_unref_ GdkPixbuf *tmp = NULL;

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
	_cleanup_object_unref_ GdkPixbuf *blurred = NULL;

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
			p_src_row[0] = interpolate_value (p_src_row[0],
							  p_blurred_row[0],
							  amount);
			p_src_row[1] = interpolate_value (p_src_row[1],
							  p_blurred_row[1],
							  amount);
			p_src_row[2] = interpolate_value (p_src_row[2],
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
					"scalable",
					"48x48",
					"32x32",
					"24x24",
					"16x16",
					NULL };
	const gchar *sizes_hi_dpi[] = { "128x128",
					"256x256",
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

	/* fallback */
	if (destdir == NULL)
		destdir = "";

	/* is this an absolute path */
	if (search[0] == '/') {
		_cleanup_free_ gchar *tmp = NULL;
		tmp = g_build_filename (destdir, search, NULL);
		if (!g_file_test (tmp, G_FILE_TEST_EXISTS)) {
			g_set_error (error,
				     AS_APP_ERROR,
				     AS_APP_ERROR_FAILED,
				     "specified icon '%s' does not exist",
				     search);
			return NULL;
		}
		return g_strdup (tmp);
	}

	/* icon theme apps */
	sizes = flags & AS_UTILS_FIND_ICON_HI_DPI ? sizes_hi_dpi : sizes_lo_dpi;
	for (k = 0; theme_dirs[k] != NULL; k++) {
		for (i = 0; sizes[i] != NULL; i++) {
			for (m = 0; types[m] != NULL; m++) {
				for (j = 0; supported_ext[j] != NULL; j++) {
					_cleanup_free_ gchar *tmp = NULL;
					tmp = g_strdup_printf ("%s/usr/share/icons/"
							       "%s/%s/%s/%s%s",
							       destdir,
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
			_cleanup_free_ gchar *tmp = NULL;
			tmp = g_strdup_printf ("%s/usr/share/%s/%s%s",
					       destdir,
					       pixmap_dirs[i],
					       search,
					       supported_ext[j]);
			if (g_file_test (tmp, G_FILE_TEST_EXISTS))
				return g_strdup (tmp);
		}
	}

	/* failed */
	g_set_error (error,
		     AS_APP_ERROR,
		     AS_APP_ERROR_FAILED,
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

/**
 * as_utils_get_string_overlap_prefix:
 */
static gchar *
as_utils_get_string_overlap_prefix (const gchar *s1, const gchar *s2)
{
	guint i;
	for (i = 0; s1[i] != '\0' && s2[i] != '\0'; i++) {
		if (s1[i] != s2[i])
			break;
	}
	if (i == 0)
		return NULL;
	if (s1[i - 1] == '-' || s1[i - 1] == '.')
		i--;
	return g_strndup (s1, i);
}

/**
 * as_utils_get_string_overlap_suffix:
 */
static gchar *
as_utils_get_string_overlap_suffix (const gchar *s1, const gchar *s2)
{
	guint i;
	guint len1 = strlen (s1);
	guint len2 = strlen (s2);
	for (i = 0; i <= len1 && i <= len2; i++) {
		if (s1[len1 - i] != s2[len2 - i])
			break;
	}
	if (i <= 1)
		return NULL;
	return g_strdup (&s1[len1 - i + 1]);
}

/**
 * as_utils_get_string_overlap:
 * @s1: A string.
 * @s2: Another string
 *
 * Return a prefix and sufffix that is common to both strings.
 *
 * Returns: (transfer full): a newly allocated %NULL terminated string, or %NULL
 *
 * Since: 0.3.1
 */
gchar *
as_utils_get_string_overlap (const gchar *s1, const gchar *s2)
{
	_cleanup_free_ gchar *prefix = NULL;
	_cleanup_free_ gchar *suffix = NULL;

	g_return_val_if_fail (s1 != NULL, NULL);
	g_return_val_if_fail (s2 != NULL, NULL);

	/* same? */
	if (g_strcmp0 (s1, s2) == 0)
		return g_strdup (s1);

	prefix = as_utils_get_string_overlap_prefix (s1, s2);
	suffix = as_utils_get_string_overlap_suffix (s1, s2);
	if (prefix == NULL && suffix == NULL)
		return NULL;
	if (prefix != NULL && suffix == NULL)
		return g_strdup (prefix);
	if (prefix == NULL && suffix != NULL)
		return g_strdup (suffix);
	return g_strdup_printf ("%s%s", prefix, suffix);
}
