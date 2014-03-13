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

//#if !defined (__APPSTREAM_GLIB_H) && !defined (AS_COMPILATION)
//#error "Only <appstream-glib.h> can be included directly."
//#endif

#ifndef __AS_NODE_H
#define __AS_NODE_H

#include <glib-object.h>
#include <stdarg.h>

G_BEGIN_DECLS

typedef enum {
	AS_NODE_TO_XML_FLAG_NONE		= 0,
	AS_NODE_TO_XML_FLAG_ADD_HEADER		= 1,
	AS_NODE_TO_XML_FLAG_FORMAT_MULTILINE	= 2,
	AS_NODE_TO_XML_FLAG_FORMAT_INDENT	= 4,
	AS_NODE_TO_XML_FLAG_LAST
} AsNodeToXmlFlags;

typedef enum {
	AS_NODE_INSERT_FLAG_NONE		= 0,	/* 'bar & baz > foo' */
	AS_NODE_INSERT_FLAG_PRE_ESCAPED		= 1,	/* 'bar &amp; baz &lt; foo' */
	AS_NODE_INSERT_FLAG_SWAPPED		= 2,
	AS_NODE_INSERT_FLAG_LAST
} AsNodeInsertFlags;

typedef enum {
	AS_NODE_ERROR_FAILED,
	AS_NODE_ERROR_LAST
} AsNodeError;

#define	AS_NODE_ERROR				as_node_error_quark ()

GNode		*as_node_new			(void);
GQuark		 as_node_error_quark		(void);
void		 as_node_unref			(GNode		*node);

const gchar	*as_node_get_name		(const GNode	*node);
const gchar	*as_node_get_data		(const GNode	*node);
const gchar	*as_node_get_attribute		(const GNode	*node,
						 const gchar	*key);
gint		 as_node_get_attribute_as_int	(const GNode	*node,
						 const gchar	*key);
GHashTable	*as_node_get_localized		(const GNode	*node,
						 const gchar	*key);
GHashTable	*as_node_get_localized_unwrap	(const GNode	*node,
						 GError		**error);

GString		*as_node_to_xml			(const GNode	*node,
						 AsNodeToXmlFlags flags);
GNode		*as_node_from_xml		(const gchar	*data,
						 gssize		 data_len,
						 GError		**error)
						 G_GNUC_WARN_UNUSED_RESULT;

GNode		*as_node_find			(GNode		*root,
						 const gchar	*path)
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

G_END_DECLS

#endif /* __AS_NODE_H */

