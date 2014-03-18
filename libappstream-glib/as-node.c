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

#include <glib.h>
#include <string.h>

#include "as-node-private.h"
#include "as-utils-private.h"

typedef struct
{
	gchar		*name;
	gchar		*cdata;
	gboolean	 cdata_escaped;
	GHashTable	*attributes;
} AsNodeData;

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
	return g_node_new (NULL);
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
	if (data->attributes != NULL)
		g_hash_table_unref (data->attributes);
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
	gchar **split = NULL;
	gchar *tmp = NULL;

	/* quick search */
	if (g_strstr_len (string->str, -1, search) == NULL)
		goto out;

	/* replace */
	split = g_strsplit (string->str, search, -1);
	tmp = g_strjoinv (replace, split);
	g_string_assign (string, tmp);
out:
	g_strfreev (split);
	g_free (tmp);
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
		g_string_append (xml, " ");
}

/**
 * as_node_get_attr_string:
 **/
static gchar *
as_node_get_attr_string (GHashTable *hash)
{
	const gchar *key;
	const gchar *value;
	GList *keys;
	GList *l;
	GString *str;

	/* nothing to get */
	if (hash == NULL)
		return g_strdup ("");

	str = g_string_new ("");
	keys = g_hash_table_get_keys (hash);
	for (l = keys; l != NULL; l = l->next) {
		key = l->data;
		value = g_hash_table_lookup (hash, key);
		g_string_append_printf (str, " %s=\"%s\"", key, value);
	}
	g_list_free (keys);
	return g_string_free (str, FALSE);
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
	guint depth = g_node_depth ((GNode *) n);
	gchar *attrs;

	/* root node */
	if (data == NULL) {
		for (c = n->children; c != NULL; c = c->next)
			as_node_to_xml_string (xml, depth_offset, c, flags);

	/* leaf node */
	} else if (n->children == NULL) {
		if ((flags & AS_NODE_TO_XML_FLAG_FORMAT_INDENT) > 0)
			as_node_add_padding (xml, depth - depth_offset);
		attrs = as_node_get_attr_string (data->attributes);
		if (data->cdata == NULL || data->cdata[0] == '\0') {
			g_string_append_printf (xml, "<%s%s/>",
						data->name, attrs);
		} else {
			as_node_cdata_to_escaped (data);
			g_string_append_printf (xml, "<%s%s>%s</%s>",
						data->name,
						attrs,
						data->cdata,
						data->name);
		}
		if ((flags & AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE) > 0)
			g_string_append (xml, "\n");
		g_free (attrs);

	/* node with children */
	} else {
		if ((flags & AS_NODE_TO_XML_FLAG_FORMAT_INDENT) > 0)
			as_node_add_padding (xml, depth - depth_offset);
		attrs = as_node_get_attr_string (data->attributes);
		g_string_append_printf (xml, "<%s%s>", data->name, attrs);
		if ((flags & AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE) > 0)
			g_string_append (xml, "\n");
		g_free (attrs);

		for (c = n->children; c != NULL; c = c->next)
			as_node_to_xml_string (xml, depth_offset, c, flags);

		if ((flags & AS_NODE_TO_XML_FLAG_FORMAT_INDENT) > 0)
			as_node_add_padding (xml, depth - depth_offset);
		g_string_append_printf (xml, "</%s>", data->name);
		if ((flags & AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE) > 0)
			g_string_append (xml, "\n");
	}
}

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
	guint depth_offset = g_node_depth ((GNode *) node) + 1;
	xml = g_string_new ("");
	if ((flags & AS_NODE_TO_XML_FLAG_ADD_HEADER) > 0)
		g_string_append (xml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	as_node_to_xml_string (xml, depth_offset, node, flags);
	return xml;
}

/**
 * as_node_start_element_cb:
 **/
static void
as_node_start_element_cb (GMarkupParseContext *context,
			 const gchar         *element_name,
			 const gchar        **attribute_names,
			 const gchar        **attribute_values,
			 gpointer             user_data,
			 GError             **error)
{
	GNode **current = (GNode **) user_data;
	AsNodeData *data;
	guint i;

	/* create the new node data */
	data = g_slice_new0 (AsNodeData);
	data->name = g_strdup (element_name);
	if (g_strv_length ((gchar **) attribute_names) > 0) {
		data->attributes = g_hash_table_new_full (g_str_hash,
							  g_str_equal,
							  g_free,
							  g_free);
	}
	for (i = 0; attribute_names[i] != NULL; i++) {
		g_hash_table_insert (data->attributes,
				     g_strdup (attribute_names[i]),
				     g_strdup (attribute_values[i]));
	}

	/* add the node to the DOM */
	*current = g_node_append_data (*current, data);
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
	GNode **current = (GNode **) user_data;
	(*current) = (*current)->parent;
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
	GNode **current = (GNode **) user_data;
	AsNodeData *data;
	guint i;
	gchar **split;

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

	/* save cdata */
	data = (*current)->data;

	/* create as it's now required */
	if (g_strstr_len (text, text_len, "\n") == NULL) {
		data->cdata = g_strndup (text, text_len);

	/* split up into lines and add each with spaces stripped */
	} else {
		GString *tmp;
		tmp = g_string_sized_new (text_len + 1);
		split = g_strsplit (text, "\n", -1);
		for (i = 0; split[i] != NULL; i++) {
			g_strstrip (split[i]);
			if (split[i][0] == '\0')
				continue;
			g_string_append_printf (tmp, "%s ", split[i]);
		}

		/* remove trailing space */
		if (tmp->len > 1 && tmp->str[tmp->len - 1] == ' ')
			g_string_truncate (tmp, tmp->len - 1);
		data->cdata = g_string_free (tmp, FALSE);
		g_strfreev (split);
	}
}

/**
 * as_node_from_xml: (skip)
 * @data: XML data
 * @data_len: Length of @data, or -1 if NULL terminated
 * @flags: #AsNodeFromXmlFlags, e.g. %AS_NODE_FROM_XML_FLAG_NONE
 * @error: A #GError or %NULL
 *
 * Parses XML data into a DOM tree.
 *
 * Returns: (transfer full): A populated #GNode tree
 *
 * Since: 0.1.0
 **/
GNode *
as_node_from_xml (const gchar *data,
		  gssize data_len,
		  AsNodeFromXmlFlags flags,
		  GError **error)
{
	GError *error_local = NULL;
	GMarkupParseContext *ctx;
	GNode *root;
	GNode *current;
	gboolean ret;
	const GMarkupParser parser = {
		as_node_start_element_cb,
		as_node_end_element_cb,
		as_node_text_cb,
		NULL,
		NULL };

	g_return_val_if_fail (data != NULL, FALSE);

	current = root = g_node_new (NULL);
	ctx = g_markup_parse_context_new (&parser,
					  G_MARKUP_PREFIX_ERROR_POSITION,
					  &current,
					  NULL);
	ret = g_markup_parse_context_parse (ctx,
					    data,
					    data_len,
					    &error_local);
	if (!ret) {
		g_set_error_literal (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_FAILED,
				     error_local->message);
		g_error_free (error_local);
		as_node_unref (root);
		root = NULL;
		goto out;
	}

	/* more opening than closing */
	if (root != current) {
		g_set_error_literal (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_FAILED,
				     "Mismatched XML");
		as_node_unref (root);
		root = NULL;
		goto out;
	}
out:
	g_markup_parse_context_free (ctx);
	return root;
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
 * Returns: (transfer full): A populated #GNode tree
 *
 * Since: 0.1.0
 **/
GNode *
as_node_from_file (GFile *file,
		   AsNodeFromXmlFlags flags,
		   GCancellable *cancellable,
		   GError **error)
{
	GConverter *conv = NULL;
	GError *error_local = NULL;
	GFileInfo *info = NULL;
	GInputStream *file_stream = NULL;
	GInputStream *stream_data = NULL;
	GMarkupParseContext *ctx = NULL;
	GNode *current;
	GNode *root = NULL;
	const gchar *content_type = NULL;
	gboolean ret = TRUE;
	gchar *data = NULL;
	gsize chunk_size = 32 * 1024;
	gssize len;
	const GMarkupParser parser = {
		as_node_start_element_cb,
		as_node_end_element_cb,
		as_node_text_cb,
		NULL,
		NULL };

	/* what kind of file is this */
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				  G_FILE_QUERY_INFO_NONE,
				  cancellable,
				  error);
	if (info == NULL) {
		ret = FALSE;
		goto out;
	}

	/* decompress if required */
	file_stream = G_INPUT_STREAM (g_file_read (file, cancellable, error));
	if (file_stream == NULL) {
		ret = FALSE;
		goto out;
	}
	content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
	if (g_strcmp0 (content_type, "application/gzip") == 0 ||
	    g_strcmp0 (content_type, "application/x-gzip") == 0) {
		conv = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
		stream_data = g_converter_input_stream_new (file_stream, conv);
	} else if (g_strcmp0 (content_type, "application/xml") == 0) {
		stream_data = g_object_ref (file_stream);
	} else {
		ret = FALSE;
		g_set_error (error,
			     AS_NODE_ERROR,
			     AS_NODE_ERROR_FAILED,
			     "cannot process file of type %s",
			     content_type);
		goto out;
	}

	/* parse */
	current = root = g_node_new (NULL);
	ctx = g_markup_parse_context_new (&parser,
					  G_MARKUP_PREFIX_ERROR_POSITION,
					  &current,
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
			root = NULL;
			goto out;
		}
	}
	if (len < 0) {
		as_node_unref (root);
		root = NULL;
		goto out;
	}

	/* more opening than closing */
	if (root != current) {
		g_set_error_literal (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_FAILED,
				     "Mismatched XML");
		as_node_unref (root);
		root = NULL;
		goto out;
	}
out:
	if (info != NULL)
		g_object_unref (info);
	if (stream_data != NULL)
		g_object_unref (stream_data);
	if (conv != NULL)
		g_object_unref (conv);
	if (file_stream != NULL)
		g_object_unref (file_stream);
	g_free (data);
	if (ctx != NULL)
		g_markup_parse_context_free (ctx);
	return root;
}

/**
 * as_node_get_child_node:
 **/
static GNode *
as_node_get_child_node (const GNode *root, const gchar *name)
{
	AsNodeData *data;
	GNode *node;

	/* invalid */
	if (name == NULL || name[0] == '\0')
		return NULL;

	/* find a node called name */
	for (node = root->children; node != NULL; node = node->next) {
		data = node->data;
		if (data == NULL)
			return NULL;
		if (g_strcmp0 (data->name, name) == 0)
			return node;
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
	return ((AsNodeData *) node->data)->name;
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
 *
 * Gets a node attribute, e.g. 34
 *
 * Return value: integer value
 *
 * Since: 0.1.0
 **/
gint
as_node_get_attribute_as_int (const GNode *node, const gchar *key)
{
	const gchar *tmp;
	gchar *endptr = NULL;
	guint64 value_tmp;
	guint value = G_MAXUINT;

	tmp = as_node_get_attribute (node, key);
	if (tmp == NULL)
		goto out;
	value_tmp = g_ascii_strtoll (tmp, &endptr, 10);
	if (value_tmp == 0 && tmp == endptr)
		goto out;
	if (value_tmp > G_MAXINT)
		goto out;
	value = value_tmp;
out:
	return value;
}

/**
 * as_node_get_attribute:
 * @node: a #GNode
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
	if (data->attributes == NULL)
		return NULL;
	return g_hash_table_lookup (data->attributes, key);
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
	gchar **split;
	GNode *node = root;
	guint i;

	g_return_val_if_fail (path != NULL, NULL);

	split = g_strsplit (path, "/", -1);
	for (i = 0; split[i] != NULL; i++) {
		node = as_node_get_child_node (node, split[i]);
		if (node == NULL)
			goto out;
	}
out:
	g_strfreev (split);
	return node;
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
 * Returns: (transfer full): A populated #GNode
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

	data = g_slice_new0 (AsNodeData);
	data->name = g_strdup (name);
	if (cdata != NULL)
		data->cdata = g_strdup (cdata);
	data->cdata_escaped = insert_flags & AS_NODE_INSERT_FLAG_PRE_ESCAPED;
	data->attributes = g_hash_table_new_full (g_str_hash,
						  g_str_equal,
						  g_free,
						  g_free);

	/* process the attrs valist */
	va_start (args, insert_flags);
	for (i = 0;; i++) {
		key = va_arg (args, const gchar *);
		if (key == NULL)
			break;
		value = va_arg (args, const gchar *);
		if (value == NULL)
			break;
		g_hash_table_insert (data->attributes,
				     g_strdup (key), g_strdup (value));
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
	const gchar *key;
	const gchar *value;
	AsNodeData *data;
	GList *l;
	GList *list;

	list = g_hash_table_get_keys (localized);
	list = g_list_sort (list, as_node_list_sort_cb);
	for (l = list; l != NULL; l = l->next) {
		key = l->data;
		value = g_hash_table_lookup (localized, key);
		data = g_slice_new (AsNodeData);
		data->name = g_strdup (name);
		data->cdata = g_strdup (value);
		data->cdata_escaped = insert_flags & AS_NODE_INSERT_FLAG_PRE_ESCAPED;
		data->attributes = g_hash_table_new_full (g_str_hash,
							  g_str_equal,
							  g_free,
							  g_free);
		if (g_strcmp0 (key, "C") != 0) {
			g_hash_table_insert (data->attributes,
					     g_strdup ("xml:lang"),
					     g_strdup (key));
		}
		g_node_insert_data (parent, -1, data);
	}
	g_list_free (list);
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

	list = g_hash_table_get_keys (hash);
	list = g_list_sort (list, as_node_list_sort_cb);
	for (l = list; l != NULL; l = l->next) {
		key = l->data;
		value = g_hash_table_lookup (hash, key);
		data = g_slice_new (AsNodeData);
		data->name = g_strdup (name);
		data->cdata = g_strdup (!swapped ? value : key);
		data->cdata_escaped = insert_flags & AS_NODE_INSERT_FLAG_PRE_ESCAPED;
		data->attributes = g_hash_table_new_full (g_str_hash,
							  g_str_equal,
							  g_free,
							  g_free);
		if (!swapped) {
			if (key != NULL && key[0] != '\0') {
				g_hash_table_insert (data->attributes,
						     g_strdup (attr_key),
						     g_strdup (key));
			}
		} else {
			if (value != NULL && value[0] != '\0') {
				g_hash_table_insert (data->attributes,
						     g_strdup (attr_key),
						     g_strdup (value));
			}
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
	tmp = as_node_get_child_node (node, key);
	if (tmp == NULL)
		goto out;
	data_unlocalized = as_node_get_data (tmp);

	/* find a node called name */
	hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	for (tmp = node->children; tmp != NULL; tmp = tmp->next) {
		data = tmp->data;
		if (data == NULL)
			continue;
		if (data->cdata == NULL)
			continue;
		if (g_strcmp0 (data->name, key) != 0)
			continue;
		if (data->attributes != NULL)
			xml_lang = g_hash_table_lookup (data->attributes, "xml:lang");
		else
			xml_lang = NULL;

		/* avoid storing identical strings */
		data_localized = data->cdata;
		if (xml_lang != NULL && g_strcmp0 (data_unlocalized, data_localized) == 0)
			continue;
		g_hash_table_insert (hash,
				     g_strdup (xml_lang != NULL ? xml_lang : "C"),
				     (gpointer) data_localized);
	}
out:
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
	GHashTable *hash;
	const gchar *tmp = NULL;

	hash = as_node_get_localized (node, key);
	if (hash == NULL)
		goto out;
	tmp = as_hash_lookup_by_locale (hash, NULL);
	if (tmp == NULL)
		goto out;
out:
	if (hash != NULL)
		g_hash_table_unref (hash);
	return tmp;
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
	const gchar *xml_lang;
	GList *keys;
	GList *l;
	GString *str;

	keys = g_hash_table_get_keys (hash);
	for (l = keys; l != NULL; l = l->next) {
		xml_lang = l->data;
		str = g_hash_table_lookup (hash, xml_lang);
		if (is_start)
			g_string_append_printf (str, "<%s>", data);
		else
			g_string_append_printf (str, "</%s>", data);
	}
	g_list_free (keys);
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

	/* only set if attrs exist */
	if (data->attributes != NULL)
		xml_lang = g_hash_table_lookup (data->attributes, "xml:lang");

	/* get locale */
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
 * as_node_get_localized_unwrap:
 * @node: a #GNode.
 * @error: A #GError or %NULL.
 *
 * Denormalize AppData data like this:
 *
 * <description>
 *  <p>Hi</p>
 *  <p xml:lang="pl">Czesc</p>
 *  <ul>
 *   <li>First</li>
 *   <li xml:lang="pl">Pierwszy</li>
 *  </ul>
 * </description>
 *
 * into a hash that contains:
 *
 * "C"  ->  "<p>Hi</p><ul><li>First</li></ul>"
 * "pl" ->  "<p>Czesc</p><ul><li>Pierwszy</li></ul>"
 *
 * Returns: (transfer full): a hash table of data
 *
 * Since: 0.1.0
 **/
GHashTable *
as_node_get_localized_unwrap (const GNode *node, GError **error)
{
	GNode *tmp;
	GNode *tmp_c;
	AsNodeData *data;
	AsNodeData *data_c;
	const gchar *xml_lang;
	GHashTable *hash;
	GString *str;
	GList *keys = NULL;
	GList *l;
	GHashTable *results = NULL;

	hash = g_hash_table_new_full (g_str_hash, g_str_equal,
				      g_free, (GDestroyNotify) as_node_string_free);
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

				/* we can only add local text for languages
				 * already added in paragraph text */
				str = as_node_denorm_get_str_for_lang (hash,
								      data_c,
								      FALSE);
				if (str == NULL) {
					g_set_error (error,
						     AS_NODE_ERROR,
						     AS_NODE_ERROR_FAILED,
						     "Cannot add locale for <%s>",
						     data_c->name);
					goto out;
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
						     AS_NODE_ERROR_FAILED,
						     "Tag %s in %s invalid",
						     data_c->name, data->name);
					goto out;
				}
			}
			as_node_denorm_add_to_langs (hash, data->name, FALSE);
		} else {
			/* only <p>, <ul> and <ol> is valid here */
			g_set_error (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_FAILED,
				     "Unknown tag '%s'",
				     data->name);
			goto out;
		}
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
out:
	g_list_free (keys);
	g_hash_table_unref (hash);
	return results;
}
