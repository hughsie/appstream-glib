/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#if !defined (__APPSTREAM_GLIB_H_INSIDE__) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#include <gio/gio.h>
#include <glib-object.h>
#include <stdarg.h>

#include "as-tag.h"

G_BEGIN_DECLS

/**
 * AsNodeToXmlFlags:
 * @AS_NODE_TO_XML_FLAG_NONE:			No extra flags to use
 * @AS_NODE_TO_XML_FLAG_ADD_HEADER:		Add an XML header to the data
 * @AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE:	Split up children with a newline
 * @AS_NODE_TO_XML_FLAG_FORMAT_INDENT:		Indent the XML by child depth
 * @AS_NODE_TO_XML_FLAG_INCLUDE_SIBLINGS:	Include the siblings when converting
 * @AS_NODE_TO_XML_FLAG_SORT_CHILDREN:		Sort the tags by alphabetical order
 *
 * The flags for converting to XML.
 **/
typedef enum {
	AS_NODE_TO_XML_FLAG_NONE		= 0,	/* Since: 0.1.0 */
	AS_NODE_TO_XML_FLAG_ADD_HEADER		= 1,	/* Since: 0.1.0 */
	AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE	= 2,	/* Since: 0.1.0 */
	AS_NODE_TO_XML_FLAG_FORMAT_INDENT	= 4,	/* Since: 0.1.0 */
	AS_NODE_TO_XML_FLAG_INCLUDE_SIBLINGS	= 8,	/* Since: 0.1.4 */
	AS_NODE_TO_XML_FLAG_SORT_CHILDREN	= 16,	/* Since: 0.2.1 */
	/*< private >*/
	AS_NODE_TO_XML_FLAG_LAST
} AsNodeToXmlFlags;

/**
 * AsNodeFromXmlFlags:
 * @AS_NODE_FROM_XML_FLAG_NONE:			No extra flags to use
 * @AS_NODE_FROM_XML_FLAG_LITERAL_TEXT:		Treat the text as an exact string
 * @AS_NODE_FROM_XML_FLAG_KEEP_COMMENTS:	Retain comments in the XML file
 * @AS_NODE_FROM_XML_FLAG_ONLY_NATIVE_LANGS:	Only load native languages
 *
 * The flags for converting from XML.
 **/
typedef enum {
	AS_NODE_FROM_XML_FLAG_NONE		= 0,		/* Since: 0.1.0 */
	AS_NODE_FROM_XML_FLAG_LITERAL_TEXT	= 1 << 0,	/* Since: 0.1.3 */
	AS_NODE_FROM_XML_FLAG_KEEP_COMMENTS	= 1 << 1,	/* Since: 0.1.6 */
	AS_NODE_FROM_XML_FLAG_ONLY_NATIVE_LANGS	= 1 << 2,	/* Since: 0.6.5 */
	/*< private >*/
	AS_NODE_FROM_XML_FLAG_LAST
} AsNodeFromXmlFlags;

/**
 * AsNodeInsertFlags:
 * @AS_NODE_INSERT_FLAG_NONE:			No extra flags to use
 * @AS_NODE_INSERT_FLAG_PRE_ESCAPED:		The data is already XML escaped
 * @AS_NODE_INSERT_FLAG_SWAPPED:		The name and key should be swapped
 * @AS_NODE_INSERT_FLAG_NO_MARKUP:		Preformat the 'description' markup
 * @AS_NODE_INSERT_FLAG_DEDUPE_LANG:		No xml:lang keys where text matches 'C'
 * @AS_NODE_INSERT_FLAG_MARK_TRANSLATABLE:	Mark the tag name as translatable
 * @AS_NODE_INSERT_FLAG_BASE64_ENCODED:		The data is Base64 enoded
 *
 * The flags to use when inserting a node.
 **/
typedef enum {
	AS_NODE_INSERT_FLAG_NONE		= 0,	/* Since: 0.1.0 */
	AS_NODE_INSERT_FLAG_PRE_ESCAPED		= 1,	/* Since: 0.1.0 */
	AS_NODE_INSERT_FLAG_SWAPPED		= 2,	/* Since: 0.1.0 */
	AS_NODE_INSERT_FLAG_NO_MARKUP		= 4,	/* Since: 0.1.1 */
	AS_NODE_INSERT_FLAG_DEDUPE_LANG		= 8,	/* Since: 0.1.4 */
	AS_NODE_INSERT_FLAG_MARK_TRANSLATABLE	= 16,	/* Since: 0.2.1 */
	AS_NODE_INSERT_FLAG_BASE64_ENCODED	= 32,	/* Since: 0.3.1 */
	/*< private >*/
	AS_NODE_INSERT_FLAG_LAST
} AsNodeInsertFlags;

/**
 * AsNodeError:
 * @AS_NODE_ERROR_FAILED:			Generic failure
 * @AS_NODE_ERROR_INVALID_MARKUP:		XML markup was invalid
 * @AS_NODE_ERROR_NO_SUPPORT:			No support for parsing
 *
 * The error type.
 **/
typedef enum {
	AS_NODE_ERROR_FAILED,
	AS_NODE_ERROR_INVALID_MARKUP,		/* Since: 0.2.4 */
	AS_NODE_ERROR_NO_SUPPORT,		/* Since: 0.3.0 */
	/*< private >*/
	AS_NODE_ERROR_LAST
} AsNodeError;

#define	AS_NODE_ERROR				as_node_error_quark ()

typedef GNode AsNode;

GNode		*as_node_new			(void);
GQuark		 as_node_error_quark		(void);
void		 as_node_unref			(GNode		*node);

const gchar	*as_node_get_name		(const GNode	*node);
const gchar	*as_node_get_data		(const GNode	*node);
const gchar	*as_node_get_comment		(const GNode	*node);
AsTag		 as_node_get_tag		(const GNode	*node);
const gchar	*as_node_get_attribute		(const GNode	*node,
						 const gchar	*key);
gint		 as_node_get_attribute_as_int	(const GNode	*node,
						 const gchar	*key);
guint		 as_node_get_attribute_as_uint	(const GNode	*node,
						 const gchar	*key);
GHashTable	*as_node_get_localized		(const GNode	*node,
						 const gchar	*key);
const gchar	*as_node_get_localized_best	(const GNode	*node,
						 const gchar	*key);
GHashTable	*as_node_get_localized_unwrap	(const GNode	*node,
						 GError		**error);

void		 as_node_set_name		(GNode		*node,
						 const gchar	*name);
void		 as_node_set_data		(GNode		*node,
						 const gchar	*cdata,
						 AsNodeInsertFlags insert_flags);
void		 as_node_set_comment		(GNode		*node,
						 const gchar	*comment);
void		 as_node_add_attribute		(GNode		*node,
						 const gchar	*key,
						 const gchar	*value);
void		 as_node_add_attribute_as_int	(GNode		*node,
						 const gchar	*key,
						 gint		 value);
void		 as_node_add_attribute_as_uint	(GNode		*node,
						 const gchar	*key,
						 guint		 value);
void		 as_node_remove_attribute	(GNode		*node,
						 const gchar	*key);

GString		*as_node_to_xml			(const GNode	*node,
						 AsNodeToXmlFlags flags);
GNode		*as_node_from_xml		(const gchar	*data,
						 AsNodeFromXmlFlags flags,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
GNode		*as_node_from_bytes		(GBytes		*bytes,
						 AsNodeFromXmlFlags flags,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
GNode		*as_node_from_file		(GFile		*file,
						 AsNodeFromXmlFlags flags,
						 GCancellable	*cancellable,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;
gboolean	 as_node_to_file		(const GNode	*root,
						 GFile		*file,
						 AsNodeToXmlFlags flags,
						 GCancellable	*cancellable,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;

GNode		*as_node_find			(GNode		*root,
						 const gchar	*path)
						 G_GNUC_WARN_UNUSED_RESULT;
GNode		*as_node_find_with_attribute	(GNode		*root,
						 const gchar	*path,
						 const gchar	*attr_key,
						 const gchar	*attr_value)
						 G_GNUC_WARN_UNUSED_RESULT;

GNode		*as_node_insert			(GNode		*parent,
						 const gchar	*name,
						 const gchar	*cdata,
						 AsNodeInsertFlags insert_flags,
						 ...)
						 G_GNUC_NULL_TERMINATED;
void		 as_node_insert_localized	(GNode		*parent,
						 const gchar	*name,
						 GHashTable	*localized,
						 AsNodeInsertFlags insert_flags);
void		 as_node_insert_hash		(GNode		*parent,
						 const gchar	*name,
						 const gchar	*attr_key,
						 GHashTable	*hash,
						 AsNodeInsertFlags insert_flags);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(AsNode, as_node_unref)

G_END_DECLS
