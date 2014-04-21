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

#if !defined (__APPSTREAM_GLIB_PRIVATE_H) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#ifndef __AS_NODE_PRIVATE_H
#define __AS_NODE_PRIVATE_H

#include "as-node.h"

G_BEGIN_DECLS

gchar		*as_node_take_data		(const GNode	*node);
gchar		*as_node_take_attribute		(const GNode	*node,
						 const gchar	*key);
void		 as_node_set_name		(GNode		*node,
						 const gchar	*name);
void		 as_node_set_data		(GNode		*node,
						 const gchar	*cdata,
						 gssize		 cdata_len,
						 AsNodeInsertFlags insert_flags);
gint		 as_node_get_attribute_as_int	(const GNode	*node,
						 const gchar	*key);
void		 as_node_add_attribute		(GNode		*node,
						 const gchar	*key,
						 const gchar	*value,
						 gssize		 value_len);
gchar		*as_node_reflow_text		(const gchar	*text,
						 gssize		 text_len);

G_END_DECLS

#endif /* __AS_NODE_PRIVATE_H */

