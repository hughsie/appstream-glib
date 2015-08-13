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

/**
 * SECTION:as-node
 * @short_description: A simple DOM parser
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * These helper functions allow parsing to and from #AsApp's and the AppStream
 * XML representation. This parser is UTF-8 safe, but not very fast, and parsers
 * like expat should be used if full XML specification adherence is required.
 *
 * See also: #AsApp
 */

#include "config.h"

#include <glib.h>
#include <string.h>

#include "as-cleanup.h"
#include "as-node-private.h"
#include "as-utils-private.h"

typedef struct
{
	GList		*attrs;
	gchar		*name;		/* only used if tag == AS_TAG_UNKNOWN */
	gchar		*cdata;
	gboolean	 cdata_escaped;
	AsTag		 tag;
} AsNodeData;

typedef struct {
	const gchar	*key;
	gchar		*value;
} AsNodeAttr;

/**
 * as_node_new: (skip)
 *
 * Creates a new empty tree whicah can have nodes appended to it.
 *
 * Returns: (transfer full): a new empty tree
 *
 * Since: 0.1.0
 **/
GNode *
as_node_new (void)
{
	AsNodeData *data;
	data = g_slice_new0 (AsNodeData);
	data->tag = AS_TAG_LAST;
	return g_node_new (data);
}

/**
 * as_node_attr_free:
 **/
static void
as_node_attr_free (AsNodeAttr *attr)
{
	g_free (attr->value);
	g_slice_free (AsNodeAttr, attr);
}

/**
 * as_node_attr_insert:
 **/
static AsNodeAttr *
as_node_attr_insert (AsNodeData *data, const gchar *key, const gchar *value)
{
	AsNodeAttr *attr;
	attr = g_slice_new0 (AsNodeAttr);
	attr->key = g_intern_string (key);
	attr->value = g_strdup (value);
	data->attrs = g_list_prepend (data->attrs, attr);
	return attr;
}

/**
 * as_node_attr_find:
 **/
static AsNodeAttr *
as_node_attr_find (AsNodeData *data, const gchar *key)
{
	AsNodeAttr *attr;
	GList *l;

	for (l = data->attrs; l != NULL; l = l->next) {
		attr = l->data;
		if (g_strcmp0 (attr->key, key) == 0)
			return attr;
	}
	return NULL;
}

/**
 * as_node_attr_lookup:
 **/
static const gchar *
as_node_attr_lookup (AsNodeData *data, const gchar *key)
{
	AsNodeAttr *attr;
	attr = as_node_attr_find (data, key);
	if (attr != NULL)
		return attr->value;
	return NULL;
}

/**
 * as_node_destroy_node_cb:
 **/
static gboolean
as_node_destroy_node_cb (GNode *node, gpointer user_data)
{
	AsNodeData *data = node->data;
	if (data == NULL)
		return FALSE;
	g_free (data->name);
	g_free (data->cdata);
	g_list_free_full (data->attrs, (GDestroyNotify) as_node_attr_free);
	g_slice_free (AsNodeData, data);
	return FALSE;
}

/**
 * as_node_unref:
 * @node: a #GNode.
 *
 * Deallocates all notes in the tree.
 *
 * Since: 0.1.0
 **/
void
as_node_unref (GNode *node)
{
	g_node_traverse (node,
			 G_PRE_ORDER,
			 G_TRAVERSE_ALL,
			 -1,
			 as_node_destroy_node_cb,
			 NULL);
	g_node_destroy (node);
}

/**
 * as_node_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.1.0
 **/
GQuark
as_node_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AsNodeError");
	return quark;
}

/**
 * as_node_string_replace:
 **/
static void
as_node_string_replace (GString *string, const gchar *search, const gchar *replace)
{
	_cleanup_free_ gchar *tmp = NULL;
	_cleanup_strv_free_ gchar **split = NULL;

	/* quick search */
	if (g_strstr_len (string->str, -1, search) == NULL)
		return;

	/* replace */
	split = g_strsplit (string->str, search, -1);
	tmp = g_strjoinv (replace, split);
	g_string_assign (string, tmp);
}

/**
 * as_node_string_replace_inplace:
 **/
static void
as_node_string_replace_inplace (gchar *text,
				const gchar *search,
				gchar replace)
{
	const gchar *start = text;
	gchar *tmp;
	gsize len;
	gsize len_escaped = 0;

	while ((tmp = g_strstr_len (start, -1, search)) != NULL) {
		*tmp = replace;
		len = strlen (tmp);
		if (len_escaped == 0)
			len_escaped = strlen (search);
		memcpy (tmp + 1,
			tmp + len_escaped,
			(len - len_escaped) + 1);
		start = tmp + 1;
	}
}

/**
 * as_node_cdata_to_raw:
 **/
static void
as_node_cdata_to_raw (AsNodeData *data)
{
	if (!data->cdata_escaped)
		return;
	as_node_string_replace_inplace (data->cdata, "&amp;", '&');
	as_node_string_replace_inplace (data->cdata, "&lt;", '<');
	as_node_string_replace_inplace (data->cdata, "&gt;", '>');
	data->cdata_escaped = FALSE;
}

/**
 * as_node_cdata_to_escaped:
 **/
static void
as_node_cdata_to_escaped (AsNodeData *data)
{
	GString *str;
	if (data->cdata_escaped)
		return;
	str = g_string_new (data->cdata);
	g_free (data->cdata);
	as_node_string_replace (str, "&", "&amp;");
	as_node_string_replace (str, "<", "&lt;");
	as_node_string_replace (str, ">", "&gt;");
	data->cdata = g_string_free (str, FALSE);
	data->cdata_escaped = TRUE;
}

/**
 * as_node_add_padding:
 **/
static void
as_node_add_padding (GString *xml, guint depth)
{
	guint i;
	for (i = 0; i < depth; i++)
		g_string_append (xml, "  ");
}

/**
 * as_node_get_attr_string:
 **/
static gchar *
as_node_get_attr_string (AsNodeData *data)
{
	AsNodeAttr *attr;
	GList *l;
	GString *str;

	str = g_string_new ("");
	for (l = data->attrs; l != NULL; l = l->next) {
		attr = l->data;
		if (g_strcmp0 (attr->key, "@comment") == 0 ||
		    g_strcmp0 (attr->key, "@comment-tmp") == 0)
			continue;
		g_string_append_printf (str, " %s=\"%s\"",
					attr->key, attr->value);
	}
	return g_string_free (str, FALSE);
}

/**
 * as_tag_data_get_name:
 **/
static const gchar *
as_tag_data_get_name (AsNodeData *data)
{
	if (data->name == NULL)
		return as_tag_to_string (data->tag);
	return data->name;
}

/**
 * as_node_data_set_name:
 **/
static void
as_node_data_set_name (AsNodeData *data, const gchar *name, AsNodeInsertFlags flags)
{
	if ((flags & AS_NODE_INSERT_FLAG_MARK_TRANSLATABLE) == 0) {
		/* only store the name if the tag is not recognised */
		data->tag = as_tag_from_string (name);
		if (data->tag == AS_TAG_UNKNOWN)
			data->name = g_strdup (name);
	} else {
		/* always store the translated tag */
		data->name = g_strdup_printf ("_%s", name);
	}
}

/**
 * as_node_sort_children:
 **/
static void
as_node_sort_children (GNode *first)
{
	AsNodeData *d1;
	AsNodeData *d2;
	GNode *child;
	gpointer tmp;

	d1 = (AsNodeData *) first->data;
	for (child = first->next; child != NULL; child = child->next) {
		d2 = (AsNodeData *) child->data;
		if (g_strcmp0 (as_tag_data_get_name (d1),
			       as_tag_data_get_name (d2)) > 0) {
			tmp = child->data;
			child->data = first->data;
			first->data = tmp;
			tmp = child->children;
			child->children = first->children;
			first->children = tmp;
			as_node_sort_children (first);
		}
	}
	if (first->next != NULL)
		as_node_sort_children (first->next);
}

/**
 * as_node_to_xml_string:
 **/
static void
as_node_to_xml_string (GString *xml,
		       guint depth_offset,
		       const GNode *n,
		       AsNodeToXmlFlags flags)
{
	AsNodeData *data = n->data;
	GNode *c;
	const gchar *tag_str;
	const gchar *comment;
	guint depth = g_node_depth ((GNode *) n);
	gchar *attrs;

	/* comment */
	comment = as_node_get_comment (n);
	if (comment != NULL) {
		guint i;
		_cleanup_strv_free_ gchar **split = NULL;

		/* do not put additional spacing for the root node */
		if (depth_offset < g_node_depth ((GNode *) n) &&
		    (flags & AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE) > 0)
			g_string_append (xml, "\n");
		if ((flags & AS_NODE_TO_XML_FLAG_FORMAT_INDENT) > 0)
			as_node_add_padding (xml, depth - depth_offset);

		/* add each comment section */
		split = g_strsplit (comment, "<&>", -1);
		for (i = 0; split[i] != NULL; i++) {
			g_string_append_printf (xml, "<!--%s-->", split[i]);
			if ((flags & AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE) > 0)
				g_string_append (xml, "\n");
		}
	}

	/* root node */
	if (data == NULL || as_node_get_tag (n) == AS_TAG_LAST) {
		if ((flags & AS_NODE_TO_XML_FLAG_SORT_CHILDREN) > 0)
			as_node_sort_children (n->children);
		for (c = n->children; c != NULL; c = c->next)
			as_node_to_xml_string (xml, depth_offset, c, flags);

	/* leaf node */
	} else if (n->children == NULL) {
		if ((flags & AS_NODE_TO_XML_FLAG_FORMAT_INDENT) > 0)
			as_node_add_padding (xml, depth - depth_offset);
		attrs = as_node_get_attr_string (data);
		tag_str = as_tag_data_get_name (data);
		if (data->cdata == NULL || data->cdata[0] == '\0') {
			g_string_append_printf (xml, "<%s%s/>",
						tag_str, attrs);
		} else {
			as_node_cdata_to_escaped (data);
			g_string_append_printf (xml, "<%s%s>%s</%s>",
						tag_str,
						attrs,
						data->cdata,
						tag_str);
		}
		if ((flags & AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE) > 0)
			g_string_append (xml, "\n");
		g_free (attrs);

	/* node with children */
	} else {
		if ((flags & AS_NODE_TO_XML_FLAG_FORMAT_INDENT) > 0)
			as_node_add_padding (xml, depth - depth_offset);
		attrs = as_node_get_attr_string (data);
		tag_str = as_tag_data_get_name (data);
		g_string_append_printf (xml, "<%s%s>", tag_str, attrs);
		if ((flags & AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE) > 0)
			g_string_append (xml, "\n");
		g_free (attrs);
		if ((flags & AS_NODE_TO_XML_FLAG_SORT_CHILDREN) > 0)
			as_node_sort_children (n->children);
		for (c = n->children; c != NULL; c = c->next)
			as_node_to_xml_string (xml, depth_offset, c, flags);

		if ((flags & AS_NODE_TO_XML_FLAG_FORMAT_INDENT) > 0)
			as_node_add_padding (xml, depth - depth_offset);
		g_string_append_printf (xml, "</%s>", tag_str);
		if ((flags & AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE) > 0)
			g_string_append (xml, "\n");
	}
}

/**
 * as_node_reflow_text:
 * @text: XML text data
 * @text_len: length of @text
 *
 * Converts pretty-formatted source text into a format suitable for AppStream.
 * This might include joining paragraphs, supressing newlines or doing other
 * sanity checks to the text.
 *
 * Returns: (transfer full): a new string
 *
 * Since: 0.1.4
 **/
gchar *
as_node_reflow_text (const gchar *text, gssize text_len)
{
	GString *tmp;
	guint i;
	guint newline_count = 0;
	_cleanup_strv_free_ gchar **split = NULL;

	/* split the text into lines */
	tmp = g_string_sized_new (text_len + 1);
	split = g_strsplit (text, "\n", -1);
	for (i = 0; split[i] != NULL; i++) {

		/* remove leading and trailing whitespace */
		g_strstrip (split[i]);

		/* if this is a blank line we end the paragraph mode
		 * and swallow the newline. If we see exactly two
		 * newlines in sequence then do a paragraph break */
		if (split[i][0] == '\0') {
			newline_count++;
			continue;
		}

		/* if the line just before this one was not a newline
		 * then seporate the words with a space */
		if (newline_count == 1 && tmp->len > 0)
			g_string_append (tmp, " ");

		/* if we had more than one newline in sequence add a paragraph
		 * break */
		if (newline_count > 1)
			g_string_append (tmp, "\n\n");

		/* add the actual stripped text */
		g_string_append (tmp, split[i]);

		/* this last section was paragraph */
		newline_count = 1;
	}
	return g_string_free (tmp, FALSE);
}

typedef struct {
	GNode			*current;
	AsNodeFromXmlFlags	 flags;
} AsNodeToXmlHelper;

/**
 * as_node_to_xml:
 * @node: a #GNode.
 * @flags: the AsNodeToXmlFlags, e.g. %AS_NODE_INSERT_FLAG_PRE_ESCAPED.
 *
 * Converts a node and it's children to XML.
 *
 * Returns: (transfer full): a #GString
 *
 * Since: 0.1.0
 **/
GString *
as_node_to_xml (const GNode *node, AsNodeToXmlFlags flags)
{
	GString *xml;
	const GNode *l;
	guint depth_offset;

	g_return_val_if_fail (node != NULL, NULL);

	depth_offset = g_node_depth ((GNode *) node) + 1;
	xml = g_string_new ("");
	if ((flags & AS_NODE_TO_XML_FLAG_ADD_HEADER) > 0)
		g_string_append (xml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	if ((flags & AS_NODE_TO_XML_FLAG_INCLUDE_SIBLINGS) > 0) {
		for (l = node; l != NULL; l = l->next)
			as_node_to_xml_string (xml, depth_offset, l, flags);
	} else {
		as_node_to_xml_string (xml, depth_offset, node, flags);
	}
	return xml;
}

/**
 * as_node_start_element_cb:
 **/
static void
as_node_start_element_cb (GMarkupParseContext *context,
			  const gchar *element_name,
			  const gchar  **attribute_names,
			  const gchar **attribute_values,
			  gpointer user_data,
			  GError **error)
{
	AsNodeToXmlHelper *helper = (AsNodeToXmlHelper *) user_data;
	AsNodeData *data;
	GNode *current;
	gchar *tmp;
	guint i;

	/* create the new node data */
	data = g_slice_new0 (AsNodeData);
	as_node_data_set_name (data, element_name, AS_NODE_INSERT_FLAG_NONE);
	for (i = 0; attribute_names[i] != NULL; i++) {
		as_node_attr_insert (data,
				     attribute_names[i],
				     attribute_values[i]);
	}

	/* add the node to the DOM */
	current = g_node_append_data (helper->current, data);

	/* transfer the ownership of the comment to the new child */
	tmp = as_node_take_attribute (helper->current, "@comment-tmp");
	if (tmp != NULL) {
		as_node_add_attribute (current, "@comment", tmp);
		g_free (tmp);
	}

	/* the child is now the node to be processed */
	helper->current = current;
}

/**
 * as_node_end_element_cb:
 **/
static void
as_node_end_element_cb (GMarkupParseContext *context,
			const gchar         *element_name,
			gpointer             user_data,
			GError             **error)
{
	AsNodeToXmlHelper *helper = (AsNodeToXmlHelper *) user_data;
	helper->current = helper->current->parent;
}

/**
 * as_node_text_cb:
 **/
static void
as_node_text_cb (GMarkupParseContext *context,
		 const gchar         *text,
		 gsize                text_len,
		 gpointer             user_data,
		 GError             **error)
{
	AsNodeToXmlHelper *helper = (AsNodeToXmlHelper *) user_data;
	AsNodeData *data;
	guint i;

	/* no data */
	if (text_len == 0)
		return;

	/* all whitespace? */
	for (i = 0; i < text_len; i++) {
		if (!g_ascii_isspace(text[i]))
			break;
	}
	if (i >= text_len)
		return;

	/* split up into lines and add each with spaces stripped */
	data = helper->current->data;
	if (data->cdata != NULL) {
		g_set_error (error,
			     AS_NODE_ERROR,
			     AS_NODE_ERROR_INVALID_MARKUP,
			     "<%s> already set '%s' and tried to replace with '%s'",
			     as_tag_data_get_name (data),
			     data->cdata, text);
		return;
	}
	if ((helper->flags & AS_NODE_FROM_XML_FLAG_LITERAL_TEXT) > 0) {
		data->cdata = g_strndup (text, text_len);
	} else {
		data->cdata = as_node_reflow_text (text, text_len);
	}
}

/**
 * as_node_passthrough_cb:
 **/
static void
as_node_passthrough_cb (GMarkupParseContext *context,
			const gchar         *passthrough_text,
			gsize                passthrough_len,
			gpointer             user_data,
			GError             **error)
{
	AsNodeToXmlHelper *helper = (AsNodeToXmlHelper *) user_data;
	const gchar *existing;
	const gchar *tmp;
	gchar *found;
	_cleanup_free_ gchar *text = NULL;

	/* only keep comments when told to */
	if ((helper->flags & AS_NODE_FROM_XML_FLAG_KEEP_COMMENTS) == 0)
		return;

	/* xml header */
	if (g_strstr_len (passthrough_text, passthrough_len, "<?xml") != NULL)
		return;

	/* get stripped comment without '<!--' and '-->' */
	text = g_strndup (passthrough_text, passthrough_len);
	if (!g_str_has_prefix (text, "<!--")) {
		g_warning ("Unexpected input: %s", text);
		return;
	}
	found = g_strrstr (text, "-->");
	if (found != NULL)
		*found = '\0';
	tmp = text + 4;
	if ((helper->flags & AS_NODE_FROM_XML_FLAG_LITERAL_TEXT) == 0)
		tmp = g_strstrip ((gchar *) tmp);
	if (tmp == NULL || tmp[0] == '\0')
		return;

	/* append together comments */
	existing = as_node_get_attribute (helper->current, "@comment-tmp");
	if (existing == NULL) {
		as_node_add_attribute (helper->current, "@comment-tmp", tmp);
	} else {
		_cleanup_free_ gchar *join = NULL;
		join = g_strdup_printf ("%s<&>%s", existing, tmp);
		as_node_add_attribute (helper->current, "@comment-tmp", join);
	}
}

/**
 * as_node_from_xml: (skip)
 * @data: XML data
 * @flags: #AsNodeFromXmlFlags, e.g. %AS_NODE_FROM_XML_FLAG_NONE
 * @error: A #GError or %NULL
 *
 * Parses XML data into a DOM tree.
 *
 * Returns: (transfer none): A populated #GNode tree
 *
 * Since: 0.1.0
 **/
GNode *
as_node_from_xml (const gchar *data,
		  AsNodeFromXmlFlags flags,
		  GError **error)
{
	AsNodeToXmlHelper helper;
	GNode *root = NULL;
	gboolean ret;
	_cleanup_error_free_ GError *error_local = NULL;
	_cleanup_markup_parse_context_unref_ GMarkupParseContext *ctx = NULL;
	const GMarkupParser parser = {
		as_node_start_element_cb,
		as_node_end_element_cb,
		as_node_text_cb,
		as_node_passthrough_cb,
		NULL };

	g_return_val_if_fail (data != NULL, FALSE);

	root = as_node_new ();
	helper.flags = flags;
	helper.current = root;
	ctx = g_markup_parse_context_new (&parser,
					  G_MARKUP_PREFIX_ERROR_POSITION,
					  &helper,
					  NULL);
	ret = g_markup_parse_context_parse (ctx,
					    data,
					    -1,
					    &error_local);
	if (!ret) {
		g_set_error_literal (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_FAILED,
				     error_local->message);
		as_node_unref (root);
		return NULL;
	}

	/* more opening than closing */
	if (root != helper.current) {
		g_set_error_literal (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_FAILED,
				     "Mismatched XML");
		as_node_unref (root);
		return NULL;
	}
	return root;
}

/**
 * as_node_to_file: (skip)
 * @root: A populated #GNode tree
 * @file: a #GFile
 * @flags: #AsNodeToXmlFlags, e.g. %AS_NODE_TO_XML_FLAG_NONE
 * @cancellable: A #GCancellable, or %NULL
 * @error: A #GError or %NULL
 *
 * Exports a DOM tree to an XML file.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.2.0
 **/
gboolean
as_node_to_file (const GNode *root,
		 GFile *file,
		 AsNodeToXmlFlags flags,
		 GCancellable *cancellable,
		 GError **error)
{
	_cleanup_string_free_ GString *xml = NULL;
	xml = as_node_to_xml (root, flags);
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
 * as_node_from_file: (skip)
 * @file: file
 * @flags: #AsNodeFromXmlFlags, e.g. %AS_NODE_FROM_XML_FLAG_NONE
 * @cancellable: A #GCancellable, or %NULL
 * @error: A #GError or %NULL
 *
 * Parses an XML file into a DOM tree.
 *
 * Returns: (transfer none): A populated #GNode tree
 *
 * Since: 0.1.0
 **/
GNode *
as_node_from_file (GFile *file,
		   AsNodeFromXmlFlags flags,
		   GCancellable *cancellable,
		   GError **error)
{
	AsNodeToXmlHelper helper;
	GError *error_local = NULL;
	GNode *root = NULL;
	const gchar *content_type = NULL;
	gboolean ret = TRUE;
	gsize chunk_size = 32 * 1024;
	gssize len;
	_cleanup_free_ gchar *data = NULL;
	_cleanup_markup_parse_context_unref_ GMarkupParseContext *ctx = NULL;
	_cleanup_object_unref_ GConverter *conv = NULL;
	_cleanup_object_unref_ GFileInfo *info = NULL;
	_cleanup_object_unref_ GInputStream *file_stream = NULL;
	_cleanup_object_unref_ GInputStream *stream_data = NULL;
	const GMarkupParser parser = {
		as_node_start_element_cb,
		as_node_end_element_cb,
		as_node_text_cb,
		as_node_passthrough_cb,
		NULL };

	/* what kind of file is this */
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				  G_FILE_QUERY_INFO_NONE,
				  cancellable,
				  error);
	if (info == NULL)
		return NULL;

	/* decompress if required */
	file_stream = G_INPUT_STREAM (g_file_read (file, cancellable, error));
	if (file_stream == NULL)
		return NULL;
	content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
	if (g_strcmp0 (content_type, "application/gzip") == 0 ||
	    g_strcmp0 (content_type, "application/x-gzip") == 0) {
		conv = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
		stream_data = g_converter_input_stream_new (file_stream, conv);
	} else if (g_strcmp0 (content_type, "application/xml") == 0) {
		stream_data = g_object_ref (file_stream);
	} else {
		g_set_error (error,
			     AS_NODE_ERROR,
			     AS_NODE_ERROR_FAILED,
			     "cannot process file of type %s",
			     content_type);
		return NULL;
	}

	/* parse */
	root = as_node_new ();
	helper.flags = flags;
	helper.current = root;
	ctx = g_markup_parse_context_new (&parser,
					  G_MARKUP_PREFIX_ERROR_POSITION,
					  &helper,
					  NULL);

	data = g_malloc (chunk_size);
	while ((len = g_input_stream_read (stream_data,
					   data,
					   chunk_size,
					   cancellable,
					   error)) > 0) {
		ret = g_markup_parse_context_parse (ctx,
						    data,
						    len,
						    &error_local);
		if (!ret) {
			g_set_error_literal (error,
					     AS_NODE_ERROR,
					     AS_NODE_ERROR_FAILED,
					     error_local->message);
			g_error_free (error_local);
			as_node_unref (root);
			return NULL;
		}
	}
	if (len < 0) {
		as_node_unref (root);
		return NULL;
	}

	/* more opening than closing */
	if (root != helper.current) {
		g_set_error_literal (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_FAILED,
				     "Mismatched XML");
		as_node_unref (root);
		return NULL;
	}
	return root;
}

/**
 * as_node_get_child_node:
 **/
static GNode *
as_node_get_child_node (const GNode *root, const gchar *name,
			const gchar *attr_key, const gchar *attr_value)
{
	AsNodeData *data;
	GNode *node;

	/* invalid */
	if (root == NULL)
		return NULL;
	if (name == NULL || name[0] == '\0')
		return NULL;

	/* find a node called name */
	for (node = root->children; node != NULL; node = node->next) {
		data = node->data;
		if (data == NULL)
			return NULL;
		if (g_strcmp0 (as_tag_data_get_name (data), name) == 0) {
			if (attr_key != NULL) {
				if (g_strcmp0 (as_node_get_attribute (node, attr_key),
					       attr_value) != 0) {
					continue;
				}
			}
			return node;
		}
	}
	return NULL;
}

/**
 * as_node_get_name:
 * @node: a #GNode
 *
 * Gets the node name, e.g. "body"
 *
 * Return value: string value
 *
 * Since: 0.1.0
 **/
const gchar *
as_node_get_name (const GNode *node)
{
	g_return_val_if_fail (node != NULL, NULL);
	if (node->data == NULL)
		return NULL;
	return as_tag_data_get_name ((AsNodeData *) node->data);
}

/**
 * as_node_set_name: (skip)
 * @node: a #GNode
 * @name: the new name
 *
 * Sets the node name, e.g. "body"
 *
 * Since: 0.1.4
 **/
void
as_node_set_name (GNode *node, const gchar *name)
{
	AsNodeData *data;

	g_return_if_fail (node != NULL);

	if (node->data == NULL)
		return;
	data = node->data;
	if (data == NULL)
		return;

	/* overwrite */
	g_free (data->name);
	data->name = NULL;
	as_node_data_set_name (data, name, AS_NODE_INSERT_FLAG_NONE);
}

/**
 * as_node_get_data:
 * @node: a #GNode
 *
 * Gets the node data, e.g. "paragraph text"
 *
 * Return value: string value
 *
 * Since: 0.1.0
 **/
const gchar *
as_node_get_data (const GNode *node)
{
	AsNodeData *data;
	g_return_val_if_fail (node != NULL, NULL);
	if (node->data == NULL)
		return NULL;
	data = (AsNodeData *) node->data;
	if (data->cdata == NULL || data->cdata[0] == '\0')
		return NULL;
	as_node_cdata_to_raw (data);
	return data->cdata;
}

/**
 * as_node_get_comment:
 * @node: a #GNode
 *
 * Gets the node data, e.g. "Copyright 2014 Richard Hughes"
 *
 * Return value: string value, or %NULL
 *
 * Since: 0.1.6
 **/
const gchar *
as_node_get_comment (const GNode *node)
{
	return as_node_get_attribute (node, "@comment");
}

/**
 * as_node_get_tag:
 * @node: a #GNode
 *
 * Gets the node tag enum.
 *
 * Return value: #AsTag, e.g. %AS_TAG_PKGNAME
 *
 * Since: 0.1.2
 **/
AsTag
as_node_get_tag (const GNode *node)
{
	AsNodeData *data;
	const gchar *tmp;

	g_return_val_if_fail (node != NULL, AS_TAG_UNKNOWN);

	data = (AsNodeData *) node->data;
	if (data == NULL)
		return AS_TAG_UNKNOWN;

	/* try to match with a fallback */
	if (data->tag == AS_TAG_UNKNOWN) {
		tmp = as_tag_data_get_name (data);
		return as_tag_from_string_full (tmp, AS_TAG_FLAG_USE_FALLBACKS);
	}

	return data->tag;
}

/**
 * as_node_set_data: (skip)
 * @node: a #GNode
 * @cdata: new data
 * @insert_flags: any %AsNodeInsertFlags.
 *
 * Sets new data on a node.
 *
 * Since: 0.1.1
 **/
void
as_node_set_data (GNode *node,
		  const gchar *cdata,
		  AsNodeInsertFlags insert_flags)
{
	AsNodeData *data;

	g_return_if_fail (node != NULL);

	if (node->data == NULL)
		return;

	data = (AsNodeData *) node->data;
	g_free (data->cdata);
	data->cdata = g_strdup (cdata);
	data->cdata_escaped = insert_flags & AS_NODE_INSERT_FLAG_PRE_ESCAPED;
}

/**
 * as_node_set_comment: (skip)
 * @node: a #GNode
 * @comment: new comment
 *
 * Sets new comment for the node.
 *
 * Since: 0.1.6
 **/
void
as_node_set_comment (GNode *node, const gchar *comment)
{
	as_node_add_attribute (node, "@comment", comment);
}

/**
 * as_node_take_data:
 * @node: a #GNode
 *
 * Gets the node data, e.g. "paragraph text"
 *
 * Return value: string value
 *
 * Since: 0.1.0
 **/
gchar *
as_node_take_data (const GNode *node)
{
	AsNodeData *data;
	gchar *tmp;

	if (node->data == NULL)
		return NULL;
	data = (AsNodeData *) node->data;
	if (data->cdata == NULL || data->cdata[0] == '\0')
		return NULL;
	as_node_cdata_to_raw (data);
	tmp = data->cdata;
	data->cdata = NULL;
	return tmp;
}

/**
 * as_node_get_attribute_as_int:
 * @node: a #GNode
 * @key: the attribute key
 *
 * Gets a node attribute, e.g. 34
 *
 * Return value: integer value, or %G_MAXINT for error
 *
 * Since: 0.1.0
 **/
gint
as_node_get_attribute_as_int (const GNode *node, const gchar *key)
{
	const gchar *tmp;
	gchar *endptr = NULL;
	gint64 value_tmp;

	tmp = as_node_get_attribute (node, key);
	if (tmp == NULL)
		return G_MAXINT;
	value_tmp = g_ascii_strtoll (tmp, &endptr, 10);
	if (value_tmp == 0 && tmp == endptr)
		return G_MAXINT;
	if (value_tmp > G_MAXINT || value_tmp < G_MININT)
		return G_MAXINT;
	return value_tmp;
}

/**
 * as_node_get_attribute:
 * @node: a #GNode
 * @key: the attribute key
 *
 * Gets a node attribute, e.g. "false"
 *
 * Return value: string value
 *
 * Since: 0.1.0
 **/
const gchar *
as_node_get_attribute (const GNode *node, const gchar *key)
{
	AsNodeData *data;

	g_return_val_if_fail (node != NULL, NULL);
	g_return_val_if_fail (key != NULL, NULL);

	if (node->data == NULL)
		return NULL;
	data = (AsNodeData *) node->data;
	return as_node_attr_lookup (data, key);
}

/**
 * as_node_take_attribute: (skip)
 * @node: a #GNode
 * @key: the attribute key
 *
 * Gets a node attribute, e.g. "false"
 *
 * Return value: string value
 *
 * Since: 0.1.2
 **/
gchar *
as_node_take_attribute (const GNode *node, const gchar *key)
{
	AsNodeAttr *attr;
	AsNodeData *data;
	gchar *tmp;

	g_return_val_if_fail (node != NULL, NULL);
	g_return_val_if_fail (key != NULL, NULL);

	if (node->data == NULL)
		return NULL;
	data = (AsNodeData *) node->data;
	attr = as_node_attr_find (data, key);
	if (attr == NULL)
		return NULL;
	tmp = attr->value;
	attr->value = NULL;
	return tmp;
}

/**
 * as_node_remove_attribute: (skip)
 * @node: a #GNode
 * @key: the attribute key
 *
 * Removes a node attribute, e.g. "type"
 *
 * Since: 0.2.0
 **/
void
as_node_remove_attribute (GNode *node, const gchar *key)
{
	AsNodeAttr *attr;
	AsNodeData *data;

	g_return_if_fail (node != NULL);
	g_return_if_fail (key != NULL);

	if (node->data == NULL)
		return;
	data = (AsNodeData *) node->data;
	attr = as_node_attr_find (data, key);
	if (attr == NULL)
		return;
	data->attrs = g_list_remove_all (data->attrs, attr);
	as_node_attr_free (attr);
}

/**
 * as_node_add_attribute: (skip)
 * @node: a #GNode
 * @key: the attribute key
 * @value: new data
 *
 * Adds a new attribute to a node.
 *
 * Since: 0.1.1
 **/
void
as_node_add_attribute (GNode *node,
		       const gchar *key,
		       const gchar *value)
{
	AsNodeData *data;
	AsNodeAttr *attr;

	g_return_if_fail (node != NULL);
	g_return_if_fail (key != NULL);

	if (node->data == NULL)
		return;
	data = (AsNodeData *) node->data;
	attr = as_node_attr_insert (data, key, NULL);
	attr->value = g_strdup (value);
}

/**
 * as_node_add_attribute_as_int: (skip)
 * @node: a #GNode
 * @key: the attribute key
 * @value: new data
 *
 * Adds a new attribute to a node.
 *
 * Since: 0.3.1
 **/
void
as_node_add_attribute_as_int (GNode *node, const gchar *key, gint value)
{
	_cleanup_free_ gchar *tmp = g_strdup_printf ("%i", value);
	as_node_add_attribute (node, key, tmp);
}

/**
 * as_node_find: (skip)
 * @root: a root node, or %NULL
 * @path: a path in the DOM, e.g. "html/body"
 *
 * Gets a node from the DOM tree.
 *
 * Return value: A #GNode, or %NULL if not found
 *
 * Since: 0.1.0
 **/
GNode *
as_node_find (GNode *root, const gchar *path)
{
	GNode *node = root;
	guint i;
	_cleanup_strv_free_ gchar **split = NULL;

	g_return_val_if_fail (path != NULL, NULL);

	split = g_strsplit (path, "/", -1);
	for (i = 0; split[i] != NULL; i++) {
		node = as_node_get_child_node (node, split[i], NULL, NULL);
		if (node == NULL)
			return NULL;
	}
	return node;
}

/**
 * as_node_find_with_attribute: (skip)
 * @root: a root node, or %NULL
 * @path: a path in the DOM, e.g. "html/body"
 * @attr_key: the attribute key
 * @attr_value: the attribute value
 *
 * Gets a node from the DOM tree with a specified attribute.
 *
 * Return value: A #GNode, or %NULL if not found
 *
 * Since: 0.1.0
 **/
GNode *
as_node_find_with_attribute (GNode *root, const gchar *path,
			     const gchar *attr_key, const gchar *attr_value)
{
	GNode *node = root;
	guint i;
	_cleanup_strv_free_ gchar **split = NULL;

	g_return_val_if_fail (path != NULL, NULL);

	split = g_strsplit (path, "/", -1);
	for (i = 0; split[i] != NULL; i++) {
		/* only check the last element */
		if (split[i + 1] == NULL) {
			node = as_node_get_child_node (node, split[i],
						       attr_key, attr_value);
			if (node == NULL)
				return NULL;
		} else {
			node = as_node_get_child_node (node, split[i], NULL, NULL);
			if (node == NULL)
				return NULL;
		}
	}
	return node;
}

/**
 * as_node_insert_line_breaks:
 **/
static gchar *
as_node_insert_line_breaks (const gchar *text, guint break_len)
{
	GString *str;
	guint i;
	guint new_len;

	/* allocate long enough for the string, plus the extra newlines */
	new_len = strlen (text) * (break_len + 1) / break_len;
	str = g_string_new_len (NULL, new_len + 2);
	g_string_append (str, "\n");
	g_string_append (str, text);

	/* insert a newline every break length */
	for (i = break_len + 1; i < str->len; i += break_len + 1)
		g_string_insert (str, i, "\n");
	g_string_append (str, "\n");
	return g_string_free (str, FALSE);
}

/**
 * as_node_insert: (skip)
 * @parent: a parent #GNode.
 * @name: the tag name, e.g. "id".
 * @cdata: the tag data, or %NULL, e.g. "org.gnome.Software.desktop".
 * @insert_flags: any %AsNodeInsertFlags.
 * @...: any attributes to add to the node, terminated by %NULL
 *
 * Inserts a node into the DOM.
 *
 * Returns: (transfer none): A populated #GNode
 *
 * Since: 0.1.0
 **/
GNode *
as_node_insert (GNode *parent,
		const gchar *name,
		const gchar *cdata,
		AsNodeInsertFlags insert_flags,
		...)
{
	const gchar *key;
	const gchar *value;
	AsNodeData *data;
	guint i;
	va_list args;

	g_return_val_if_fail (name != NULL, NULL);

	data = g_slice_new0 (AsNodeData);
	as_node_data_set_name (data, name, insert_flags);
	if (cdata != NULL) {
		if (insert_flags & AS_NODE_INSERT_FLAG_BASE64_ENCODED)
			data->cdata = as_node_insert_line_breaks (cdata, 76);
		else
			data->cdata = g_strdup (cdata);
	}
	data->cdata_escaped = insert_flags & AS_NODE_INSERT_FLAG_PRE_ESCAPED;

	/* process the attrs valist */
	va_start (args, insert_flags);
	for (i = 0;; i++) {
		key = va_arg (args, const gchar *);
		if (key == NULL)
			break;
		value = va_arg (args, const gchar *);
		if (value == NULL)
			break;
		as_node_attr_insert (data, key, value);
	}
	va_end (args);

	return g_node_insert_data (parent, -1, data);
}

/**
 * as_node_list_sort_cb:
 **/
static gint
as_node_list_sort_cb (gconstpointer a, gconstpointer b)
{
	return g_strcmp0 ((const gchar *) a, (const gchar *) b);
}

/**
 * as_node_insert_localized:
 * @parent: a parent #GNode.
 * @name: the tag name, e.g. "id".
 * @localized: the hash table of data, with the locale as the key.
 * @insert_flags: any %AsNodeInsertFlags.
 *
 * Inserts a localized key into the DOM.
 *
 * Since: 0.1.0
 **/
void
as_node_insert_localized (GNode *parent,
			  const gchar *name,
			  GHashTable *localized,
			  AsNodeInsertFlags insert_flags)
{
	AsNodeData *data;
	GList *l;
	const gchar *key;
	const gchar *value;
	const gchar *value_c;
	_cleanup_list_free_ GList *list = NULL;

	g_return_if_fail (name != NULL);

	/* add the untranslated value first */
	value_c = g_hash_table_lookup (localized, "C");
	if (value_c == NULL)
		return;
	data = g_slice_new0 (AsNodeData);
	as_node_data_set_name (data, name, insert_flags);
	if (insert_flags & AS_NODE_INSERT_FLAG_NO_MARKUP) {
		data->cdata = as_markup_convert_simple (value_c, NULL);
		data->cdata_escaped = FALSE;
	} else {
		data->cdata = g_strdup (value_c);
		data->cdata_escaped = insert_flags & AS_NODE_INSERT_FLAG_PRE_ESCAPED;
	}
	g_node_insert_data (parent, -1, data);

	/* add the other localized values */
	list = g_hash_table_get_keys (localized);
	list = g_list_sort (list, as_node_list_sort_cb);
	for (l = list; l != NULL; l = l->next) {
		key = l->data;
		if (g_strcmp0 (key, "C") == 0)
			continue;
		value = g_hash_table_lookup (localized, key);
		if ((insert_flags & AS_NODE_INSERT_FLAG_DEDUPE_LANG) > 0 &&
		    g_strcmp0 (value_c, value) == 0)
			continue;
		data = g_slice_new0 (AsNodeData);
		as_node_attr_insert (data, "xml:lang", key);
		as_node_data_set_name (data, name, insert_flags);
		if (insert_flags & AS_NODE_INSERT_FLAG_NO_MARKUP) {
			data->cdata = as_markup_convert_simple (value, NULL);
			data->cdata_escaped = FALSE;
		} else {
			data->cdata = g_strdup (value);
			data->cdata_escaped = insert_flags & AS_NODE_INSERT_FLAG_PRE_ESCAPED;
		}
		g_node_insert_data (parent, -1, data);
	}
}

/**
 * as_node_insert_hash:
 * @parent: a parent #GNode.
 * @name: the tag name, e.g. "id".
 * @attr_key: the key to use as the attribute in the XML, e.g. "key".
 * @hash: the hash table with the key as the key to use in the XML.
 * @insert_flags: any %AsNodeInsertFlags.
 *
 * Inserts a hash table of data into the DOM.
 *
 * Since: 0.1.0
 **/
void
as_node_insert_hash (GNode *parent,
		     const gchar *name,
		     const gchar *attr_key,
		     GHashTable *hash,
		     AsNodeInsertFlags insert_flags)
{
	AsNodeData *data;
	GList *l;
	GList *list;
	const gchar *key;
	const gchar *value;
	gboolean swapped = (insert_flags & AS_NODE_INSERT_FLAG_SWAPPED) > 0;

	g_return_if_fail (name != NULL);

	list = g_hash_table_get_keys (hash);
	list = g_list_sort (list, as_node_list_sort_cb);
	for (l = list; l != NULL; l = l->next) {
		key = l->data;
		value = g_hash_table_lookup (hash, key);
		data = g_slice_new0 (AsNodeData);
		as_node_data_set_name (data, name, insert_flags);
		data->cdata = g_strdup (!swapped ? value : key);
		data->cdata_escaped = insert_flags & AS_NODE_INSERT_FLAG_PRE_ESCAPED;
		if (!swapped) {
			if (key != NULL && key[0] != '\0')
				as_node_attr_insert (data, attr_key, key);
		} else {
			if (value != NULL && value[0] != '\0')
				as_node_attr_insert (data, attr_key, value);
		}
		g_node_insert_data (parent, -1, data);
	}
	g_list_free (list);
}

/**
 * as_node_get_localized:
 * @node: a #GNode
 * @key: the key to use, e.g. "copyright"
 *
 * Extracts localized values from the DOM tree
 *
 * Return value: (transfer full): A hash table with the locale (e.g. en_GB) as the key
 *
 * Since: 0.1.0
 **/
GHashTable *
as_node_get_localized (const GNode *node, const gchar *key)
{
	AsNodeData *data;
	const gchar *xml_lang;
	const gchar *data_unlocalized;
	const gchar *data_localized;
	GHashTable *hash = NULL;
	GNode *tmp;

	/* does it exist? */
	tmp = as_node_get_child_node (node, key, NULL, NULL);
	if (tmp == NULL)
		return NULL;
	data_unlocalized = as_node_get_data (tmp);

	/* find a node called name */
	hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	for (tmp = node->children; tmp != NULL; tmp = tmp->next) {
		data = tmp->data;
		if (data == NULL)
			continue;
		if (data->cdata == NULL)
			continue;
		if (g_strcmp0 (as_tag_data_get_name (data), key) != 0)
			continue;
		xml_lang = as_node_attr_lookup (data, "xml:lang");

		/* avoid storing identical strings */
		data_localized = data->cdata;
		if (xml_lang != NULL && g_strcmp0 (data_unlocalized, data_localized) == 0)
			continue;
		g_hash_table_insert (hash,
				     g_strdup (xml_lang != NULL ? xml_lang : "C"),
				     (gpointer) data_localized);
	}
	return hash;
}

/**
 * as_node_get_localized_best:
 * @node: a #GNode.
 * @key: the tag name.
 *
 * Gets the 'best' locale version of a specific data value.
 *
 * Returns: the string value, or %NULL if there was no data
 *
 * Since: 0.1.0
 **/
const gchar *
as_node_get_localized_best (const GNode *node, const gchar *key)
{
	_cleanup_hashtable_unref_ GHashTable *hash = NULL;
	hash = as_node_get_localized (node, key);
	if (hash == NULL)
		return NULL;
	return as_hash_lookup_by_locale (hash, NULL);
}

/**
 * as_node_string_free:
 **/
static void
as_node_string_free (GString *string)
{
	g_string_free (string, TRUE);
}

/**
 * as_node_denorm_add_to_langs:
 **/
static void
as_node_denorm_add_to_langs (GHashTable *hash,
			     const gchar *data,
			     gboolean is_start)
{
	GList *l;
	GString *str;
	const gchar *xml_lang;
	_cleanup_list_free_ GList *keys = NULL;

	keys = g_hash_table_get_keys (hash);
	for (l = keys; l != NULL; l = l->next) {
		xml_lang = l->data;
		str = g_hash_table_lookup (hash, xml_lang);
		if (is_start)
			g_string_append_printf (str, "<%s>", data);
		else
			g_string_append_printf (str, "</%s>", data);
	}
}

/**
 * as_node_denorm_get_str_for_lang:
 **/
static GString *
as_node_denorm_get_str_for_lang (GHashTable *hash,
				AsNodeData *data,
				gboolean allow_new_locales)
{
	const gchar *xml_lang = NULL;
	GString *str;

	/* get locale */
	xml_lang = as_node_attr_lookup (data, "xml:lang");
	if (xml_lang == NULL)
		xml_lang = "C";
	str = g_hash_table_lookup (hash, xml_lang);
	if (str == NULL && allow_new_locales) {
		str = g_string_new ("");
		g_hash_table_insert (hash, g_strdup (xml_lang), str);
	}
	return str;
}

/**
 * as_node_get_localized_unwrap_type_li:
 *
 * Denormalize AppData data where each <li> element is translated:
 *
 * |[
 * <description>
 *  <p>Hi</p>
 *  <p xml:lang="pl">Czesc</p>
 *  <ul>
 *   <li>First</li>
 *   <li xml:lang="pl">Pierwszy</li>
 *  </ul>
 * </description>
 * ]|
 **/
static gboolean
as_node_get_localized_unwrap_type_li (const GNode *node,
				      GHashTable *hash,
				      GError **error)
{
	GNode *tmp;
	GNode *tmp_c;
	AsNodeData *data;
	AsNodeData *data_c;
	GString *str;

	for (tmp = node->children; tmp != NULL; tmp = tmp->next) {
		data = tmp->data;

		/* append to existing string, adding the locale if it's not
		 * already present */
		if (g_strcmp0 (data->name, "p") == 0) {
			str = as_node_denorm_get_str_for_lang (hash, data, TRUE);
			as_node_cdata_to_escaped (data);
			g_string_append_printf (str, "<p>%s</p>",
						data->cdata);

		/* loop on the children */
		} else if (g_strcmp0 (data->name, "ul") == 0 ||
			   g_strcmp0 (data->name, "ol") == 0) {
			as_node_denorm_add_to_langs (hash, data->name, TRUE);
			for (tmp_c = tmp->children; tmp_c != NULL; tmp_c = tmp_c->next) {
				data_c = tmp_c->data;

				/* handle new locales that have list
				 * translations but no paragraph translations */
				str = as_node_denorm_get_str_for_lang (hash,
								       data_c,
								       FALSE);
				if (str == NULL) {
					str = as_node_denorm_get_str_for_lang (hash,
									       data_c,
									       TRUE);
					g_string_append_printf (str, "<%s>",
								data->name);
				}
				if (g_strcmp0 (data_c->name, "li") == 0) {
					as_node_cdata_to_escaped (data_c);
					g_string_append_printf (str,
								"<li>%s</li>",
								data_c->cdata);
				} else {
					/* only <li> is valid in lists */
					g_set_error (error,
						     AS_NODE_ERROR,
						     AS_NODE_ERROR_INVALID_MARKUP,
						     "Tag %s in %s invalid",
						     data_c->name, data->name);
					return FALSE;
				}
			}
			as_node_denorm_add_to_langs (hash, data->name, FALSE);
		} else {
			/* only <p>, <ul> and <ol> is valid here */
			g_set_error (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_INVALID_MARKUP,
				     "Unknown tag '%s'",
				     data->name);
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * as_node_get_localized_unwrap_type_ul:
 *
 * Denormalize AppData data where the parent <ul> is translated:
 *
 * |[
 * <description>
 *  <p>Hi</p>
 *  <p xml:lang="pl">Czesc</p>
 *  <ul xml:lang="pl">
 *   <li>First</li>
 *  </ul>
 *  <ul xml:lang="pl">
 *   <li>Pierwszy</li>
 *  </ul>
 * </description>
 * ]|
 **/
static gboolean
as_node_get_localized_unwrap_type_ul (const GNode *node,
				      GHashTable *hash,
				      GError **error)
{
	GNode *tmp;
	GNode *tmp_c;
	AsNodeData *data;
	AsNodeData *data_c;
	GString *str;

	for (tmp = node->children; tmp != NULL; tmp = tmp->next) {
		data = tmp->data;

		/* append to existing string, adding the locale if it's not
		 * already present */
		if (g_strcmp0 (data->name, "p") == 0) {
			str = as_node_denorm_get_str_for_lang (hash, data, TRUE);
			as_node_cdata_to_escaped (data);
			g_string_append_printf (str, "<p>%s</p>",
						data->cdata);

		/* loop on the children */
		} else if (g_strcmp0 (data->name, "ul") == 0 ||
			   g_strcmp0 (data->name, "ol") == 0) {
			str = as_node_denorm_get_str_for_lang (hash, data, TRUE);
			g_string_append_printf (str, "<%s>", data->name);
			for (tmp_c = tmp->children; tmp_c != NULL; tmp_c = tmp_c->next) {
				data_c = tmp_c->data;
				if (g_strcmp0 (data_c->name, "li") == 0) {
					as_node_cdata_to_escaped (data_c);
					g_string_append_printf (str,
								"<li>%s</li>",
								data_c->cdata);
				} else {
					/* only <li> is valid in lists */
					g_set_error (error,
						     AS_NODE_ERROR,
						     AS_NODE_ERROR_INVALID_MARKUP,
						     "Tag %s in %s invalid",
						     data_c->name, data->name);
					return FALSE;
				}
			}
			g_string_append_printf (str, "</%s>", data->name);
		} else {
			/* only <p>, <ul> and <ol> is valid here */
			g_set_error (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_INVALID_MARKUP,
				     "Unknown tag '%s'",
				     data->name);
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * as_node_get_localized_unwrap:
 * @node: a #GNode.
 * @error: A #GError or %NULL.
 *
 * Denormalize AppData data like this:
 *
 * |[
 * <description>
 *  <p>Hi</p>
 *  <p xml:lang="pl">Czesc</p>
 *  <ul>
 *   <li>First</li>
 *   <li xml:lang="pl">Pierwszy</li>
 *  </ul>
 * </description>
 * ]|
 *
 * into a hash that contains:
 *
 * |[
 * "C"  ->  "<p>Hi</p><ul><li>First</li></ul>"
 * "pl" ->  "<p>Czesc</p><ul><li>Pierwszy</li></ul>"
 * ]|
 *
 * Returns: (transfer full): a hash table of data
 *
 * Since: 0.1.0
 **/
GHashTable *
as_node_get_localized_unwrap (const GNode *node, GError **error)
{
	AsNodeData *data;
	GHashTable *results;
	GList *l;
	GNode *tmp;
	GString *str;
	const gchar *xml_lang;
	gboolean is_li_translated = TRUE;
	_cleanup_hashtable_unref_ GHashTable *hash = NULL;
	_cleanup_list_free_ GList *keys = NULL;

	g_return_val_if_fail (node != NULL, NULL);

	/* work out what kind of normalization this is */
	xml_lang = as_node_get_attribute (node, "xml:lang");
	if (xml_lang != NULL && node->children != NULL) {
		str = as_node_to_xml (node->children, AS_NODE_TO_XML_FLAG_NONE);
		results = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
		g_hash_table_insert (results,
				     g_strdup (xml_lang),
				     g_strdup (str->str));
		g_string_free (str, TRUE);
		return results;
	}
	for (tmp = node->children; tmp != NULL; tmp = tmp->next) {
		data = tmp->data;
		if (g_strcmp0 (data->name, "ul") == 0 ||
		    g_strcmp0 (data->name, "ol") == 0) {
			if (as_node_attr_lookup (data, "xml:lang") != NULL) {
				is_li_translated = FALSE;
				break;
			}
		}
	}

	/* unwrap it to a hash */
	hash = g_hash_table_new_full (g_str_hash, g_str_equal,
				      g_free, (GDestroyNotify) as_node_string_free);
	if (is_li_translated) {
		if (!as_node_get_localized_unwrap_type_li (node, hash, error))
			return NULL;
	} else {
		if (!as_node_get_localized_unwrap_type_ul (node, hash, error))
			return NULL;
	}

	/* copy into a hash table of the correct size */
	results = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	keys = g_hash_table_get_keys (hash);
	for (l = keys; l != NULL; l = l->next) {
		xml_lang = l->data;
		str = g_hash_table_lookup (hash, xml_lang);
		g_hash_table_insert (results,
				     g_strdup (xml_lang),
				     g_strdup (str->str));
	}
	return results;
}

/* helper struct */
struct _AsNodeContext {
	AsAppSourceKind	 source_kind;
	AsAppSourceKind	 output;
	gdouble		 version;
};

/**
 * as_node_context_new: (skip)
 *
 * Creates a new node context with default values.
 *
 * Returns: a #AsNodeContext
 *
 * Since: 0.3.6
 **/
AsNodeContext *
as_node_context_new (void)
{
	AsNodeContext *ctx;
	ctx = g_new0 (AsNodeContext, 1);
	ctx->version = 0.f;
	ctx->source_kind = AS_APP_SOURCE_KIND_APPSTREAM;
	ctx->output = AS_APP_SOURCE_KIND_UNKNOWN;
	return ctx;
}

/**
 * as_node_context_get_version: (skip)
 * @ctx: a #AsNodeContext.
 *
 * Gets the AppStream API version used when parsing or inserting nodes.
 *
 * Returns: version number
 *
 * Since: 0.3.6
 **/
gdouble
as_node_context_get_version (AsNodeContext *ctx)
{
	return ctx->version;
}

/**
 * as_node_context_set_version: (skip)
 * @ctx: a #AsNodeContext.
 * @version: an API version number to target.
 *
 * Sets the AppStream API version used when parsing or inserting nodes.
 *
 * Since: 0.3.6
 **/
void
as_node_context_set_version (AsNodeContext *ctx, gdouble version)
{
	ctx->version = version;
}

/**
 * as_node_context_get_source_kind: (skip)
 * @ctx: a #AsNodeContext.
 *
 * Gets the AppStream API source_kind used when parsing nodes.
 *
 * Returns: source_kind number
 *
 * Since: 0.3.6
 **/
AsAppSourceKind
as_node_context_get_source_kind (AsNodeContext *ctx)
{
	return ctx->source_kind;
}

/**
 * as_node_context_set_source_kind: (skip)
 * @ctx: a #AsNodeContext.
 * @source_kind: an API source_kind number to target.
 *
 * Sets the AppStream API source_kind used when parsing nodes.
 *
 * Since: 0.3.6
 **/
void
as_node_context_set_source_kind (AsNodeContext *ctx, AsAppSourceKind source_kind)
{
	ctx->source_kind = source_kind;
}

/**
 * as_node_context_get_output: (skip)
 * @ctx: a #AsNodeContext.
 *
 * Gets the AppStream API destination kind used when inserting nodes.
 *
 * Returns: output format, e.g. %AS_APP_SOURCE_KIND_APPDATA
 *
 * Since: 0.3.6
 **/
AsAppSourceKind
as_node_context_get_output (AsNodeContext *ctx)
{
	return ctx->output;
}

/**
 * as_node_context_set_output: (skip)
 * @ctx: a #AsNodeContext.
 * @output: an output kind, e.g. %AS_APP_SOURCE_KIND_APPDATA
 *
 * Sets the AppStream API destination kind used when inserting nodes.
 *
 * Since: 0.3.6
 **/
void
as_node_context_set_output (AsNodeContext *ctx, AsAppSourceKind output)
{
	ctx->output = output;
}
