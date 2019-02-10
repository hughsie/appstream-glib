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
