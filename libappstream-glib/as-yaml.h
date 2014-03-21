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

#if !defined (__APPSTREAM_GLIB_H) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#ifndef __AS_YAML_H
#define __AS_YAML_H

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

void		 as_yaml_unref			(GNode		*node);
GString		*as_yaml_to_string		(GNode		*node);
GNode		*as_yaml_from_data		(const gchar	*data,
						 gssize		 data_len,
						 GError		**error);
GNode		*as_yaml_from_file		(GFile		*file,
						 GCancellable	*cancellable,
						 GError		**error);
const gchar	*as_yaml_node_get_key		(const GNode	*node);
const gchar	*as_yaml_node_get_value		(const GNode	*node);
gint		 as_yaml_node_get_value_as_int	(const GNode	*node);

G_END_DECLS

#endif /* __AS_YAML_H */

