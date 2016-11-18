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

#include "as-node.h"
#include "as-ref-string.h"
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
	AsRefString	*key;
	AsRefString	*value;
	AsYamlNodeKind	 kind;
} AsYamlNode;

const gchar *
as_yaml_node_get_key (const AsNode *node)
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

const gchar *
as_yaml_node_get_value (const AsNode *node)
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

gint
as_yaml_node_get_value_as_int (const AsNode *node)
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
	return (gint) value_tmp;
}

guint
as_yaml_node_get_value_as_uint (const AsNode *node)
{
	const gchar *tmp;
	gchar *endptr = NULL;
	guint64 value_tmp;

	tmp = as_yaml_node_get_value (node);
	if (tmp == NULL)
		return G_MAXUINT;
	value_tmp = g_ascii_strtoull (tmp, &endptr, 10);
	if (value_tmp == 0 && tmp == endptr)
		return G_MAXUINT;
	if (value_tmp > G_MAXUINT)
		return G_MAXUINT;
	return (guint) value_tmp;
}

static gboolean
as_node_yaml_destroy_node_cb (AsNode *node, gpointer data)
{
	AsYamlNode *ym = node->data;
	if (ym == NULL)
		return FALSE;
	if (ym->key != NULL)
		as_ref_string_unref (ym->key);
	if (ym->value != NULL)
		as_ref_string_unref (ym->value);
	g_slice_free (AsYamlNode, ym);
	return FALSE;
}

void
as_yaml_unref (AsNode *node)
{
	g_node_traverse (node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			 as_node_yaml_destroy_node_cb, NULL);
	g_node_destroy (node);
}

static gboolean
as_yaml_to_string_cb (AsNode *node, gpointer data)
{
	AsYamlNode *ym = node->data;
	GString *str = (GString *) data;
	guint depth;
	guint i;

	depth = g_node_depth (node);
	if (depth >= 2) {
		for (i = 0; i < depth - 2; i++)
			g_string_append (str, " ");
	}
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

GString *
as_yaml_to_string (AsNode *node)
{
	GString *str = g_string_new ("");
	g_node_traverse (node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			 as_yaml_to_string_cb, str);
	return str;
}

#ifdef AS_BUILD_DEP11
static AsYamlNodeKind
as_yaml_node_get_kind (AsNode *node)
{
	AsYamlNode *ym;
	if (node == NULL)
		return AS_YAML_NODE_KIND_UNKNOWN;
	ym = node->data;
	if (ym == NULL)
		return AS_YAML_NODE_KIND_UNKNOWN;
	return ym->kind;
}

static AsYamlNode *
as_yaml_node_new (AsYamlNodeKind kind, const gchar *id)
{
	AsYamlNode *ym;
	ym = g_slice_new0 (AsYamlNode);
	ym->kind = kind;
	if (id != NULL)
		ym->key = as_ref_string_ref (id);
	return ym;
}

static gchar *
as_yaml_mark_to_str (yaml_mark_t *mark)
{
	return g_strdup_printf ("ln:%" G_GSIZE_FORMAT " col:%" G_GSIZE_FORMAT,
				mark->line + 1,
				mark->column + 1);
}

static gboolean
as_yaml_parser_error_to_gerror (yaml_parser_t *parser, GError **error)
{
	g_autofree gchar *problem_str = NULL;
	g_autofree gchar *context_str = NULL;

	switch (parser->error) {
	case YAML_MEMORY_ERROR:
		g_set_error_literal (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_INVALID_MARKUP,
				     "not enough memory for parsing");
		break;

	case YAML_READER_ERROR:
		if (parser->problem_value != -1) {
			g_set_error (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_INVALID_MARKUP,
				     "reader error: %s: #%X at %" G_GSIZE_FORMAT "",
				     parser->problem,
				     (guint) parser->problem_value,
				     parser->problem_offset);
		} else {
			g_set_error (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_INVALID_MARKUP,
				     "reader error: %s at %" G_GSIZE_FORMAT "",
				     parser->problem,
				     parser->problem_offset);
		}
		break;

	case YAML_SCANNER_ERROR:
		problem_str = as_yaml_mark_to_str (&parser->problem_mark);
		if (parser->context) {
			context_str = as_yaml_mark_to_str (&parser->context_mark);
			g_set_error (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_INVALID_MARKUP,
				     "scanner error: %s at %s, %s at %s",
				     parser->context,
				     context_str,
				     parser->problem,
				     problem_str);
		} else {
			g_set_error (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_INVALID_MARKUP,
				     "scanner error: %s at %s ",
				     parser->problem,
				     problem_str);
		}
		break;
	case YAML_PARSER_ERROR:
		problem_str = as_yaml_mark_to_str (&parser->problem_mark);
		if (parser->context) {
			context_str = as_yaml_mark_to_str (&parser->context_mark);
			g_set_error (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_INVALID_MARKUP,
				     "parser error: %s at %s, %s at %s",
				     parser->context,
				     context_str,
				     parser->problem,
				     problem_str);
		} else {
			g_set_error (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_INVALID_MARKUP,
				     "parser error: %s at %s ",
				     parser->problem,
				     problem_str);
		}
		break;
	case YAML_COMPOSER_ERROR:
		problem_str = as_yaml_mark_to_str (&parser->problem_mark);
		if (parser->context) {
			context_str = as_yaml_mark_to_str (&parser->context_mark);
			g_set_error (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_INVALID_MARKUP,
				     "composer error: %s at %s, %s at %s",
				     parser->context,
				     context_str,
				     parser->problem,
				     problem_str);
		} else {
			g_set_error (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_INVALID_MARKUP,
				     "composer error: %s at %s ",
				     parser->problem, problem_str);
		}
		break;
	default:
		/* can't happen */
		g_set_error_literal (error,
				     AS_NODE_ERROR,
				     AS_NODE_ERROR_INVALID_MARKUP,
				     "internal error");
		break;
	}
	return FALSE;
}

typedef struct {
	AsYamlFromFlags		 flags;
	const gchar * const	*locales;
	yaml_parser_t		*parser;
} AsYamlContext;

static gboolean
as_yaml_node_valid (AsYamlContext *ctx, AsNode *parent, const gchar *key)
{
	const gchar *sections[] = { "Name", "Summary", "Description", NULL };
	AsYamlNode *ym = parent->data;

	/* if native filtering enabled */
	if ((ctx->flags & AS_YAML_FROM_FLAG_ONLY_NATIVE_LANGS) == 0)
		return TRUE;

	/* filter by translatable sections */
	if (!g_strv_contains (sections, ym->key))
		return TRUE;

	/* non-native language */
	if (g_strv_contains (ctx->locales, key))
		return TRUE;

	return FALSE;
}

static gboolean
as_node_yaml_process_layer (AsYamlContext *ctx, AsNode *parent, GError **error)
{
	AsYamlNode *ym;
	AsNode *last_scalar = NULL;
	AsNode *new;
	const gchar *tmp;
	gboolean valid = TRUE;
	yaml_event_t event;
	AsRefString *map_str = as_ref_string_new_static ("{");
	AsRefString *seq_str = as_ref_string_new_static ("[");

	while (valid) {

		/* process event */
		if (!yaml_parser_parse (ctx->parser, &event))
			return as_yaml_parser_error_to_gerror (ctx->parser, error);

		switch (event.type) {
		case YAML_SCALAR_EVENT:
			tmp = (const gchar *) event.data.scalar.value;
			if (as_yaml_node_get_kind (parent) != AS_YAML_NODE_KIND_SEQ &&
			    last_scalar != NULL) {
				ym = last_scalar->data;
				if (ym->key != NULL)
					ym->value = as_ref_string_new_copy (tmp);
				ym->kind = AS_YAML_NODE_KIND_KEY_VALUE;
				last_scalar = NULL;
			} else {
				if (as_yaml_node_valid (ctx, parent, tmp)) {
					g_autoptr(AsRefString) rstr = as_ref_string_new_copy (tmp);
					ym = as_yaml_node_new (AS_YAML_NODE_KIND_KEY, rstr);
				} else {
					ym = as_yaml_node_new (AS_YAML_NODE_KIND_KEY, NULL);
				}
				last_scalar = g_node_append_data (parent, ym);
			}
			break;
		case YAML_MAPPING_START_EVENT:
			if (last_scalar == NULL) {
				ym = as_yaml_node_new (AS_YAML_NODE_KIND_MAP, map_str);
				new = g_node_append_data (parent, ym);
			} else {
				ym = last_scalar->data;
				ym->kind = AS_YAML_NODE_KIND_MAP;
				new = last_scalar;
			}
			if (!as_node_yaml_process_layer (ctx, new, error))
				return FALSE;
			last_scalar = NULL;
			break;
		case YAML_SEQUENCE_START_EVENT:
			if (last_scalar == NULL) {
				ym = as_yaml_node_new (AS_YAML_NODE_KIND_SEQ, seq_str);
				new = g_node_append_data (parent, ym);
			} else {
				ym = last_scalar->data;
				ym->kind = AS_YAML_NODE_KIND_SEQ;
				new = last_scalar;
			}
			if (!as_node_yaml_process_layer (ctx, new, error))
				return FALSE;
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
	return TRUE;
}

typedef yaml_parser_t* AsYamlParser;
G_DEFINE_AUTO_CLEANUP_FREE_FUNC(AsYamlParser, yaml_parser_delete, NULL);

#endif

AsNode *
as_yaml_from_data (const gchar *data,
		   gssize data_len,
		   AsYamlFromFlags flags,
		   GError **error)
{
	g_autoptr(AsYaml) node = NULL;
#ifdef AS_BUILD_DEP11
	AsYamlContext ctx;
	yaml_parser_t parser;
	g_auto(AsYamlParser) parser_cleanup = NULL;

	/* parse */
	if (!yaml_parser_initialize (&parser)) {
		as_yaml_parser_error_to_gerror (&parser, error);
		return NULL;
	}
	parser_cleanup = &parser;
	if (data_len < 0)
		data_len = (gssize) strlen (data);
	yaml_parser_set_input_string (&parser, (guchar *) data, (gsize) data_len);
	node = g_node_new (NULL);
	ctx.parser = &parser;
	ctx.flags = flags;
	ctx.locales = g_get_language_names ();
	if (!as_node_yaml_process_layer (&ctx, node, error))
		return NULL;
#else
	g_set_error_literal (error,
			     AS_NODE_ERROR,
			     AS_NODE_ERROR_NO_SUPPORT,
			     "No DEP-11 support, needs libyaml");
#endif
	return g_steal_pointer (&node);
}

#ifdef AS_BUILD_DEP11
static int
as_yaml_read_handler_cb (void *data,
			 unsigned char *buffer,
			 size_t size,
			 size_t *size_read)
{
	GInputStream *stream = G_INPUT_STREAM (data);
	gssize len = g_input_stream_read (stream,
					  buffer,
					  (gsize) size,
					  NULL,
					  NULL);
	if (len < 0)
		return 0;
	*size_read = (gsize) len;
	return 1;
}
#endif

AsNode *
as_yaml_from_file (GFile *file, AsYamlFromFlags flags, GCancellable *cancellable, GError **error)
{
	g_autoptr(AsYaml) node = NULL;
#ifdef AS_BUILD_DEP11
	const gchar *content_type = NULL;
	yaml_parser_t parser;
	g_auto(AsYamlParser) parser_cleanup = NULL;
	g_autofree gchar *data = NULL;
	g_autoptr(GConverter) conv = NULL;
	g_autoptr(GFileInfo) info = NULL;
	g_autoptr(GInputStream) file_stream = NULL;
	g_autoptr(GInputStream) stream_data = NULL;
	AsYamlContext ctx;

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
	if (!yaml_parser_initialize (&parser)) {
		as_yaml_parser_error_to_gerror (&parser, error);
		return NULL;
	}
	parser_cleanup = &parser;
	yaml_parser_set_input (&parser, as_yaml_read_handler_cb, stream_data);
	node = g_node_new (NULL);
	ctx.parser = &parser;
	ctx.flags = flags;
	ctx.locales = g_get_language_names ();
	if (!as_node_yaml_process_layer (&ctx, node, error))
		return NULL;
#else
	g_set_error_literal (error,
			     AS_NODE_ERROR,
			     AS_NODE_ERROR_NO_SUPPORT,
			     "No DEP-11 support, needs libyaml");
#endif
	return g_steal_pointer (&node);
}
