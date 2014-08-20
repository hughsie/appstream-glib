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

#ifdef AS_BUILD_DEP11
#include <yaml.h>
#endif

#include "as-cleanup.h"
#include "as-node.h"
#include "as-yaml.h"

typedef enum {
	AS_YAML_NODE_KIND_UNKNOWN,
	AS_YAML_NODE_KIND_MAP,
	AS_YAML_NODE_KIND_SEQ,
	AS_YAML_NODE_KIND_KEY,
	AS_YAML_NODE_KIND_KEY_VALUE,
	AS_YAML_NODE_KIND_LAST
} AsYamlNodeKind;

typedef struct {
	gchar		*key;
	gchar		*value;
	AsYamlNodeKind	 kind;
} AsYamlNode;

/**
 * as_yaml_node_get_kind:
 **/
static AsYamlNodeKind
as_yaml_node_get_kind (GNode *node)
{
	AsYamlNode *ym;
	if (node == NULL)
		return AS_YAML_NODE_KIND_UNKNOWN;
	ym = node->data;
	if (ym == NULL)
		return AS_YAML_NODE_KIND_UNKNOWN;
	return ym->kind;
}

/**
 * as_yaml_node_get_key:
 **/
const gchar *
as_yaml_node_get_key (const GNode *node)
{
	AsYamlNode *ym;
	if (node == NULL)
		return NULL;
	ym = node->data;
	if (ym == NULL)
		return NULL;
	if (ym->key == NULL || ym->key[0] == '\0')
		return NULL;
	return ym->key;
}

/**
 * as_yaml_node_get_value:
 **/
const gchar *
as_yaml_node_get_value (const GNode *node)
{
	AsYamlNode *ym;
	if (node == NULL)
		return NULL;
	ym = node->data;
	if (ym == NULL)
		return NULL;
	if (ym->value == NULL || ym->value[0] == '\0')
		return NULL;
	return ym->value;
}

/**
 * as_yaml_node_get_value_as_int:
 **/
gint
as_yaml_node_get_value_as_int (const GNode *node)
{
	const gchar *tmp;
	gchar *endptr = NULL;
	gint64 value_tmp;

	tmp = as_yaml_node_get_value (node);
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
 * as_node_yaml_destroy_node_cb:
 **/
static gboolean
as_node_yaml_destroy_node_cb (GNode *node, gpointer data)
{
	AsYamlNode *ym = node->data;
	if (ym == NULL)
		return FALSE;
	g_free (ym->key);
	g_free (ym->value);
	g_slice_free (AsYamlNode, ym);
	return FALSE;
}

/**
 * as_yaml_unref:
 **/
void
as_yaml_unref (GNode *node)
{
	g_node_traverse (node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			 as_node_yaml_destroy_node_cb, NULL);
	g_node_destroy (node);
}

/**
 * as_yaml_to_string_cb:
 **/
static gboolean
as_yaml_to_string_cb (GNode *node, gpointer data)
{
	AsYamlNode *ym = node->data;
	GString *str = (GString *) data;
	gint depth;
	gint i;

	depth = g_node_depth (node);
	for (i = 0; i < depth - 2; i++)
		g_string_append (str, " ");
	if (ym == NULL)
		return FALSE;
	switch (ym->kind) {
	case AS_YAML_NODE_KIND_MAP:
		g_string_append (str, "[MAP]");
		break;
	case AS_YAML_NODE_KIND_SEQ:
		g_string_append (str, "[SEQ]");
		break;
	case AS_YAML_NODE_KIND_KEY:
		g_string_append (str, "[KEY]");
		break;
	case AS_YAML_NODE_KIND_KEY_VALUE:
		g_string_append (str, "[KVL]");
		break;
	case AS_YAML_NODE_KIND_UNKNOWN:
	default:
		g_string_append (str, "???: ");
		break;
	}

	if (ym->value != NULL) {
		g_string_append_printf (str, "%s=%s\n", ym->key, ym->value);
		return FALSE;
	}
	g_string_append_printf (str, "%s\n", ym->key);
	return FALSE;
}

/**
 * as_yaml_to_string:
 **/
GString *
as_yaml_to_string (GNode *node)
{
	GString *str = g_string_new ("");
	g_node_traverse (node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			 as_yaml_to_string_cb, str);
	return str;
}

#if AS_BUILD_DEP11
/**
 * as_yaml_node_new:
 **/
static AsYamlNode *
as_yaml_node_new (AsYamlNodeKind kind, const gchar *id)
{
	AsYamlNode *ym;
	ym = g_slice_new0 (AsYamlNode);
	ym->kind = kind;
	ym->key = g_strdup (id);
	return ym;
}

/**
 * as_node_yaml_process_layer:
 **/
static void
as_node_yaml_process_layer (yaml_parser_t *parser, GNode *parent)
{
	AsYamlNode *ym;
	GNode *last_scalar = NULL;
	GNode *new;
	const gchar *tmp;
	gboolean valid = TRUE;
	yaml_event_t event;

	while (valid) {
		yaml_parser_parse (parser, &event);
		switch (event.type) {
		case YAML_SCALAR_EVENT:
			tmp = (const gchar *) event.data.scalar.value;
			if (as_yaml_node_get_kind (parent) != AS_YAML_NODE_KIND_SEQ &&
			    last_scalar != NULL) {
				ym = last_scalar->data;
				ym->value = g_strdup (tmp);
				ym->kind = AS_YAML_NODE_KIND_KEY_VALUE;
				last_scalar = NULL;
			} else {
				ym = as_yaml_node_new (AS_YAML_NODE_KIND_KEY, tmp);
				last_scalar = g_node_append_data (parent, ym);
			}
			break;
		case YAML_MAPPING_START_EVENT:
			if (last_scalar == NULL) {
				ym = as_yaml_node_new (AS_YAML_NODE_KIND_MAP, "{");
				new = g_node_append_data (parent, ym);
			} else {
				ym = last_scalar->data;
				ym->kind = AS_YAML_NODE_KIND_MAP;
				new = last_scalar;
			}
			as_node_yaml_process_layer (parser, new);
			last_scalar = NULL;
			break;
		case YAML_SEQUENCE_START_EVENT:
			if (last_scalar == NULL) {
				ym = as_yaml_node_new (AS_YAML_NODE_KIND_SEQ, "[");
				new = g_node_append_data (parent, ym);
			} else {
				ym = last_scalar->data;
				ym->kind = AS_YAML_NODE_KIND_SEQ;
				new = last_scalar;
			}
			as_node_yaml_process_layer (parser, new);
			last_scalar = NULL;
			break;
		case YAML_STREAM_START_EVENT:
			break;
		case YAML_MAPPING_END_EVENT:
		case YAML_SEQUENCE_END_EVENT:
		case YAML_STREAM_END_EVENT:
			valid = FALSE;
			break;
		default:
			break;
		}
		yaml_event_delete (&event);
	}
}
#endif

/**
 * as_yaml_from_data:
 **/
GNode *
as_yaml_from_data (const gchar *data, gssize data_len, GError **error)
{
	GNode *node = NULL;
#if AS_BUILD_DEP11
	yaml_parser_t parser;

	/* parse */
	yaml_parser_initialize (&parser);
	if (data_len < 0)
		data_len = strlen (data);
	yaml_parser_set_input_string (&parser, (guchar *) data, data_len);
	node = g_node_new (NULL);
	as_node_yaml_process_layer (&parser, node);
	yaml_parser_delete (&parser);
#else
	g_set_error_literal (error,
			     AS_NODE_ERROR,
			     AS_NODE_ERROR_NO_SUPPORT,
			     "No DEP-11 support, needs libyaml");
#endif
	return node;
}

#if AS_BUILD_DEP11
/**
 * as_yaml_read_handler_cb:
 **/
static int
as_yaml_read_handler_cb (void *data,
			 unsigned char *buffer,
			 size_t size,
			 size_t *size_read)
{
	GInputStream *stream = G_INPUT_STREAM (data);
	*size_read = g_input_stream_read (stream, buffer, size, NULL, NULL);
	return 1;
}
#endif

/**
 * as_yaml_from_file:
 **/
GNode *
as_yaml_from_file (GFile *file, GCancellable *cancellable, GError **error)
{
	GNode *node = NULL;
#if AS_BUILD_DEP11
	const gchar *content_type = NULL;
	yaml_parser_t parser;
	_cleanup_free_ gchar *data = NULL;
	_cleanup_object_unref_ GConverter *conv = NULL;
	_cleanup_object_unref_ GFileInfo *info = NULL;
	_cleanup_object_unref_ GInputStream *file_stream = NULL;
	_cleanup_object_unref_ GInputStream *stream_data = NULL;

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
	} else if (g_strcmp0 (content_type, "application/x-yaml") == 0) {
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
	yaml_parser_initialize (&parser);
	yaml_parser_set_input (&parser, as_yaml_read_handler_cb, stream_data);
	node = g_node_new (NULL);
	as_node_yaml_process_layer (&parser, node);
	yaml_parser_delete (&parser);
#else
	g_set_error_literal (error,
			     AS_NODE_ERROR,
			     AS_NODE_ERROR_NO_SUPPORT,
			     "No DEP-11 support, needs libyaml");
#endif
	return node;
}
