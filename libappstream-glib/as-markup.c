/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2016 Richard Hughes <richard@hughsie.com>
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
 * SECTION:as-markup
 * @short_description: Functions for managing AppStream description markup
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * These functions are used internally to libappstream-glib, and some may be
 * useful to user-applications.
 */

#include "config.h"

#include <string.h>

#include "as-markup.h"
#include "as-node.h"
#include "as-utils.h"

/**
 * as_markup_import_simple:
 */
static gchar *
as_markup_import_simple (const gchar *text, GError **error)
{
	GString *str;
	guint i;
	g_auto(GStrv) lines = NULL;

	/* empty */
	if (text == NULL || text[0] == '\0')
		return NULL;

	/* just assume paragraphs */
	str = g_string_new ("<p>");
	lines = g_strsplit (text, "\n", -1);
	for (i = 0; lines[i] != NULL; i++) {
		g_autofree gchar *markup = NULL;
		if (lines[i][0] == '\0') {
			if (g_str_has_suffix (str->str, " "))
				g_string_truncate (str, str->len - 1);
			g_string_append (str, "</p><p>");
			continue;
		}
		markup = g_markup_escape_text (lines[i], -1);
		g_string_append (str, markup);
		g_string_append (str, " ");
	}
	if (g_str_has_suffix (str->str, " "))
		g_string_truncate (str, str->len - 1);
	g_string_append (str, "</p>");
	return g_string_free (str, FALSE);
}

/**
 * as_markup_import:
 * @text: the text to import.
 * @format: the #AsMarkupConvertFormat, e.g. %AS_MARKUP_CONVERT_FORMAT_SIMPLE
 * @error: A #GError or %NULL
 *
 * Imports text and converts to AppStream markup.
 *
 * Returns: (transfer full): appstream markup, or %NULL in event of an error
 *
 * Since: 0.5.11
 */
gchar *
as_markup_import (const gchar *text, AsMarkupConvertFormat format, GError **error)
{
	if (format == AS_MARKUP_CONVERT_FORMAT_SIMPLE)
		return as_markup_import_simple (text, error);
	g_set_error_literal (error,
			     AS_UTILS_ERROR,
			     AS_UTILS_ERROR_INVALID_TYPE,
			     "unknown comnversion kind");
	return NULL;
}

/**
 * as_markup_strsplit_words:
 * @text: the text to split.
 * @line_len: the maximum length of the output line
 *
 * Splits up a long line into an array of smaller strings, each being no longer
 * than @line_len. Words are not split.
 *
 * Returns: (transfer full): lines, or %NULL in event of an error
 *
 * Since: 0.3.5
 **/
gchar **
as_markup_strsplit_words (const gchar *text, guint line_len)
{
	GPtrArray *lines;
	guint i;
	g_autoptr(GString) curline = NULL;
	g_auto(GStrv) tokens = NULL;

	/* sanity check */
	if (text == NULL || text[0] == '\0')
		return NULL;
	if (line_len == 0)
		return NULL;

	lines = g_ptr_array_new ();
	curline = g_string_new ("");

	/* tokenize the string */
	tokens = g_strsplit (text, " ", -1);
	for (i = 0; tokens[i] != NULL; i++) {

		/* current line plus new token is okay */
		if (curline->len + strlen (tokens[i]) < line_len) {
			g_string_append_printf (curline, "%s ", tokens[i]);
			continue;
		}

		/* too long, so remove space, add newline and dump */
		if (curline->len > 0)
			g_string_truncate (curline, curline->len - 1);
		g_string_append (curline, "\n");
		g_ptr_array_add (lines, g_strdup (curline->str));
		g_string_truncate (curline, 0);
		g_string_append_printf (curline, "%s ", tokens[i]);

	}

	/* any incomplete line? */
	if (curline->len > 0) {
		g_string_truncate (curline, curline->len - 1);
		g_string_append (curline, "\n");
		g_ptr_array_add (lines, g_strdup (curline->str));
	}

	g_ptr_array_add (lines, NULL);
	return (gchar **) g_ptr_array_free (lines, FALSE);
}

/**
 * as_markup_render_para:
 **/
static void
as_markup_render_para (GString *str, AsMarkupConvertFormat format, const gchar *data)
{
	guint i;
	g_autofree gchar *tmp = NULL;
	g_auto(GStrv) spl = NULL;

	if (str->len > 0)
		g_string_append (str, "\n");
	switch (format) {
	case AS_MARKUP_CONVERT_FORMAT_SIMPLE:
		g_string_append_printf (str, "%s\n", data);
		break;
	case AS_MARKUP_CONVERT_FORMAT_APPSTREAM:
		tmp = g_markup_escape_text (data, -1);
		g_string_append_printf (str, "<p>%s</p>", tmp);
		break;
	case AS_MARKUP_CONVERT_FORMAT_MARKDOWN:
		/* break to 80 chars */
		spl = as_markup_strsplit_words (data, 80);
		for (i = 0; spl[i] != NULL; i++)
			g_string_append (str, spl[i]);
		break;
	default:
		break;
	}
}

/**
 * as_markup_render_li:
 **/
static void
as_markup_render_li (GString *str, AsMarkupConvertFormat format, const gchar *data)
{
	guint i;
	g_autofree gchar *tmp = NULL;
	g_auto(GStrv) spl = NULL;

	switch (format) {
	case AS_MARKUP_CONVERT_FORMAT_SIMPLE:
		g_string_append_printf (str, " • %s\n", data);
		break;
	case AS_MARKUP_CONVERT_FORMAT_APPSTREAM:
		tmp = g_markup_escape_text (data, -1);
		g_string_append_printf (str, "<li>%s</li>", tmp);
		break;
	case AS_MARKUP_CONVERT_FORMAT_MARKDOWN:
		/* break to 80 chars, leaving room for the dot/indent */
		spl = as_markup_strsplit_words (data, 80 - 3);
		g_string_append_printf (str, " * %s", spl[0]);
		for (i = 1; spl[i] != NULL; i++)
			g_string_append_printf (str, "   %s", spl[i]);
		break;
	default:
		break;
	}
}

/**
 * as_markup_render_ul_start:
 **/
static void
as_markup_render_ul_start (GString *str, AsMarkupConvertFormat format)
{
	switch (format) {
	case AS_MARKUP_CONVERT_FORMAT_APPSTREAM:
		g_string_append (str, "<ul>");
		break;
	default:
		break;
	}
}

/**
 * as_markup_render_ul_end:
 **/
static void
as_markup_render_ul_end (GString *str, AsMarkupConvertFormat format)
{
	switch (format) {
	case AS_MARKUP_CONVERT_FORMAT_APPSTREAM:
		g_string_append (str, "</ul>");
		break;
	default:
		break;
	}
}

/**
 * as_markup_validate:
 * @markup: the text to validate
 * @error: A #GError or %NULL
 *
 * Validates some markup.
 *
 * Returns: %TRUE if the appstream description was valid
 *
 * Since: 0.5.1
 **/
gboolean
as_markup_validate (const gchar *markup, GError **error)
{
	g_autofree gchar *tmp = NULL;
	tmp = as_markup_convert (markup, AS_MARKUP_CONVERT_FORMAT_NULL, error);
	return tmp != NULL;
}

/**
 * as_markup_convert_full:
 * @markup: the text to copy.
 * @format: the #AsMarkupConvertFormat, e.g. %AS_MARKUP_CONVERT_FORMAT_MARKDOWN
 * @flags: the #AsMarkupConvertFlag, e.g. %AS_MARKUP_CONVERT_FLAG_IGNORE_ERRORS
 * @error: A #GError or %NULL
 *
 * Converts an XML description into a printable form.
 *
 * Returns: (transfer full): a newly allocated %NULL terminated string
 *
 * Since: 0.3.5
 **/
gchar *
as_markup_convert_full (const gchar *markup,
			AsMarkupConvertFormat format,
			AsMarkupConvertFlag flags,
			GError **error)
{
	GNode *tmp;
	GNode *tmp_c;
	const gchar *tag;
	const gchar *tag_c;
	g_autoptr(AsNode) root = NULL;
	g_autoptr(GError) error_local = NULL;
	g_autoptr(GString) str = NULL;

	/* is this actually markup */
	if (g_strstr_len (markup, -1, "<") == NULL)
		return g_strdup (markup);

	/* load */
	root = as_node_from_xml (markup, AS_NODE_FROM_XML_FLAG_NONE, &error_local);
	if (root == NULL) {

		/* truncate to the last tag and try again */
		if (flags & AS_MARKUP_CONVERT_FLAG_IGNORE_ERRORS) {
			gchar *found;
			g_autofree gchar *markup_new = NULL;
			markup_new = g_strdup (markup);
			found = g_strrstr (markup_new, "<");
			g_assert (found != NULL);
			*found = '\0';
			return as_markup_convert_full (markup_new, format, flags, error);
		}

		/* just return error */
		g_propagate_error (error, error_local);
		error_local = NULL;
		return NULL;
	}

	/* format */
	str = g_string_new ("");
	for (tmp = root->children; tmp != NULL; tmp = tmp->next) {

		tag = as_node_get_name (tmp);
		if (g_strcmp0 (tag, "p") == 0) {
			as_markup_render_para (str, format, as_node_get_data (tmp));
			continue;
		}

		/* loop on the children */
		if (g_strcmp0 (tag, "ul") == 0 ||
		    g_strcmp0 (tag, "ol") == 0) {
			as_markup_render_ul_start (str, format);
			for (tmp_c = tmp->children; tmp_c != NULL; tmp_c = tmp_c->next) {
				tag_c = as_node_get_name (tmp_c);
				if (g_strcmp0 (tag_c, "li") == 0) {
					as_markup_render_li (str, format,
							     as_node_get_data (tmp_c));
					continue;
				}

				/* just abort the list */
				if (flags & AS_MARKUP_CONVERT_FLAG_IGNORE_ERRORS)
					break;

				/* only <li> is valid in lists */
				g_set_error (error,
					     AS_NODE_ERROR,
					     AS_NODE_ERROR_FAILED,
					     "Tag %s in %s invalid",
					     tag_c, tag);
				return NULL;
			}
			as_markup_render_ul_end (str, format);
			continue;
		}

		/* just try again */
		if (flags & AS_MARKUP_CONVERT_FLAG_IGNORE_ERRORS)
			continue;

		/* only <p>, <ul> and <ol> is valid here */
		g_set_error (error,
			     AS_NODE_ERROR,
			     AS_NODE_ERROR_FAILED,
			     "Unknown tag '%s'", tag);
		return NULL;
	}

	/* success */
	switch (format) {
	case AS_MARKUP_CONVERT_FORMAT_SIMPLE:
	case AS_MARKUP_CONVERT_FORMAT_MARKDOWN:
		if (str->len > 0)
			g_string_truncate (str, str->len - 1);
		break;
	default:
		break;
	}
	return g_strdup (str->str);
}

/**
 * as_markup_convert:
 * @markup: the text to copy.
 * @format: the #AsMarkupConvertFormat, e.g. %AS_MARKUP_CONVERT_FORMAT_MARKDOWN
 * @error: A #GError or %NULL
 *
 * Converts an XML description into a printable form.
 *
 * Returns: (transfer full): a newly allocated %NULL terminated string
 *
 * Since: 0.3.5
 **/
gchar *
as_markup_convert (const gchar *markup,
		   AsMarkupConvertFormat format, GError **error)
{
	return as_markup_convert_full (markup, format,
				       AS_MARKUP_CONVERT_FLAG_NONE,
				       error);
}

/**
 * as_markup_convert_simple:
 * @markup: the text to copy.
 * @error: A #GError or %NULL
 *
 * Converts an XML description into a printable form.
 *
 * Returns: (transfer full): a newly allocated %NULL terminated string
 *
 * Since: 0.1.0
 **/
gchar *
as_markup_convert_simple (const gchar *markup, GError **error)
{
	return as_markup_convert_full (markup,
				       AS_MARKUP_CONVERT_FORMAT_SIMPLE,
				       AS_MARKUP_CONVERT_FLAG_NONE,
				       error);
}
