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

#include "as-node.h"
#include "as-utils.h"

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
	GNode *root = NULL;
	GNode *tmp;
	GNode *tmp_c;
	GString *str = NULL;
	const gchar *tag;
	const gchar *tag_c;
	gboolean ret = TRUE;
	gchar *formatted = NULL;

	/* is this actually markup */
	if (g_strstr_len (markup, markup_len, "<") == NULL) {
		formatted = as_strndup (markup, markup_len);
		goto out;
	}

	/* load */
	root = as_node_from_xml (markup, markup_len, error);
	if (root == NULL) {
		ret = FALSE;
		goto out;
	}
	str = g_string_sized_new (markup_len);

	/* format */
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
					ret = FALSE;
					g_set_error (error,
						     AS_NODE_ERROR,
						     AS_NODE_ERROR_FAILED,
						     "Tag %s in %s invalid",
						     tag_c, tag);
					goto out;
				}
			}
		} else {
			/* only <p>, <ul> and <ol> is valid here */
			ret = FALSE;
			g_set_error (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_FAILED,
				     "Unknown tag '%s'", tag);
			goto out;
		}
	}

	/* success */
	if (str->len > 0)
		g_string_truncate (str, str->len - 1);
out:
	if (root != NULL)
		as_node_unref (root);
	if (str != NULL) {
		if (!ret)
			g_string_free (str, TRUE);
		else
			formatted = g_string_free (str, FALSE);
	}
	return formatted;
}
