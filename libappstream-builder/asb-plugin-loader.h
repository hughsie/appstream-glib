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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ASB_PLUGIN_LOADER_H
#define ASB_PLUGIN_LOADER_H

#include <glib-object.h>

#include "asb-plugin.h"

#define ASB_TYPE_PLUGIN_LOADER			(asb_plugin_loader_get_type())
#define ASB_PLUGIN_LOADER(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), ASB_TYPE_PLUGIN_LOADER, AsbPluginLoader))
#define ASB_PLUGIN_LOADER_CLASS(cls)		(G_TYPE_CHECK_CLASS_CAST((cls), ASB_TYPE_PLUGIN_LOADER, AsbPluginLoaderClass))
#define ASB_IS_PLUGIN_LOADER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), ASB_TYPE_PLUGIN_LOADER))
#define ASB_IS_PLUGIN_LOADER_CLASS(cls)		(G_TYPE_CHECK_CLASS_TYPE((cls), ASB_TYPE_PLUGIN_LOADER))
#define ASB_PLUGIN_LOADER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), ASB_TYPE_PLUGIN_LOADER, AsbPluginLoaderClass))

G_BEGIN_DECLS

typedef struct _AsbPluginLoader			AsbPluginLoader;
typedef struct _AsbPluginLoaderClass		AsbPluginLoaderClass;

struct _AsbPluginLoader
{
	GObject			parent;
};

struct _AsbPluginLoaderClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_asb_reserved1)	(void);
	void (*_asb_reserved2)	(void);
	void (*_asb_reserved3)	(void);
	void (*_asb_reserved4)	(void);
	void (*_asb_reserved5)	(void);
	void (*_asb_reserved6)	(void);
	void (*_asb_reserved7)	(void);
	void (*_asb_reserved8)	(void);
};

GType		 asb_plugin_loader_get_type	(void);
AsbPluginLoader	*asb_plugin_loader_new		(AsbContext		*ctx);

gboolean	 asb_plugin_loader_setup	(AsbPluginLoader	*plugin_loader,
						 GError			**error);
GPtrArray	*asb_plugin_loader_get_globs	(AsbPluginLoader	*plugin_loader);
GPtrArray	*asb_plugin_loader_get_plugins	(AsbPluginLoader	*plugin_loader);
void		 asb_plugin_loader_merge	(AsbPluginLoader	*plugin_loader,
						 GList			**apps);
gboolean	 asb_plugin_loader_process_app	(AsbPluginLoader	*plugin_loader,
						 AsbPackage		*pkg,
						 AsbApp			*app,
						 const gchar		*tmpdir,
						 GError			**error);
AsbPlugin	*asb_plugin_loader_match_fn	(AsbPluginLoader	*plugin_loader,
						 const gchar		*filename);

G_END_DECLS

#endif /* ASB_PLUGIN_LOADER_H */
