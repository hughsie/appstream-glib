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


/**
 * AsYamlFromFlags:
 * @AS_YAML_FROM_FLAG_NONE:			No extra flags to use
 * @AS_YAML_FROM_FLAG_ONLY_NATIVE_LANGS:	Only load native languages
 *
 * The flags for converting from XML.
 **/
typedef enum {
	AS_YAML_FROM_FLAG_NONE			= 0,		/* Since: 0.1.0 */
	AS_YAML_FROM_FLAG_ONLY_NATIVE_LANGS	= 1 << 0,	/* Since: 0.6.6 */
	/*< private >*/
	AS_YAML_FROM_FLAG_LAST
} AsYamlFromFlags;

typedef GNode AsYaml;

void		 as_yaml_unref			(AsYaml		*node);
GString		*as_yaml_to_string		(AsYaml		*node);
AsYaml		*as_yaml_from_data		(const gchar	*data,
						 gssize		 data_len,
						 AsYamlFromFlags flags,
						 GError		**error);
AsYaml		*as_yaml_from_file		(GFile		*file,
						 AsYamlFromFlags flags,
						 GCancellable	*cancellable,
						 GError		**error);
const gchar	*as_yaml_node_get_key		(const AsYaml	*node);
const gchar	*as_yaml_node_get_value		(const AsYaml	*node);
gint		 as_yaml_node_get_value_as_int	(const AsYaml	*node);
guint		 as_yaml_node_get_value_as_uint	(const AsYaml	*node);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(AsYaml, as_yaml_unref)

G_END_DECLS

#endif /* __AS_YAML_H */

