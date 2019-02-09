/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "asb-plugin.h"

G_BEGIN_DECLS

#define ASB_TYPE_PLUGIN_LOADER (asb_plugin_loader_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsbPluginLoader, asb_plugin_loader, ASB, PLUGIN_LOADER, GObject)

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

AsbPluginLoader	*asb_plugin_loader_new		(AsbContext		*ctx);

const gchar	*asb_plugin_loader_get_dir	(AsbPluginLoader	*plugin_loader);
void		 asb_plugin_loader_set_dir	(AsbPluginLoader	*plugin_loader,
						 const gchar		*plugin_dir);
gboolean	 asb_plugin_loader_setup	(AsbPluginLoader	*plugin_loader,
						 GError			**error);
GPtrArray	*asb_plugin_loader_get_globs	(AsbPluginLoader	*plugin_loader);
GPtrArray	*asb_plugin_loader_get_plugins	(AsbPluginLoader	*plugin_loader);
void		 asb_plugin_loader_merge	(AsbPluginLoader	*plugin_loader,
						 GList			*apps);
gboolean	 asb_plugin_loader_process_app	(AsbPluginLoader	*plugin_loader,
						 AsbPackage		*pkg,
						 AsbApp			*app,
						 const gchar		*tmpdir,
						 GError			**error);
AsbPlugin	*asb_plugin_loader_match_fn	(AsbPluginLoader	*plugin_loader,
						 const gchar		*filename);

G_END_DECLS
