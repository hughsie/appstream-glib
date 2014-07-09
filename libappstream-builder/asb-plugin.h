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

#ifndef __ASB_PLUGIN_H
#define __ASB_PLUGIN_H

#include <glib-object.h>
#include <gmodule.h>
#include <gio/gio.h>
#include <appstream-glib.h>

#include "as-cleanup.h"

#include "asb-app.h"
#include "asb-context.h"
#include "asb-package.h"
#include "asb-utils.h"

G_BEGIN_DECLS

typedef struct	AsbPluginPrivate	AsbPluginPrivate;
typedef struct	AsbPlugin		AsbPlugin;

struct AsbPlugin {
	GModule			*module;
	gboolean		 enabled;
	gchar			*name;
	AsbPluginPrivate	*priv;
	AsbContext		*ctx;
};

typedef enum {
	ASB_PLUGIN_ERROR_FAILED,
	ASB_PLUGIN_ERROR_NOT_SUPPORTED,
	ASB_PLUGIN_ERROR_LAST
} AsbPluginError;

/* helpers */
#define	ASB_PLUGIN_ERROR				1
#define	ASB_PLUGIN_GET_PRIVATE(x)			g_new0 (x,1)
#define	ASB_PLUGIN(x)					((AsbPlugin *) x);

typedef const gchar	*(*AsbPluginGetNameFunc)	(void);
typedef void		 (*AsbPluginFunc)		(AsbPlugin	*plugin);
typedef void		 (*AsbPluginGetGlobsFunc)	(AsbPlugin	*plugin,
							 GPtrArray	*array);
typedef void		 (*AsbPluginMergeFunc)		(AsbPlugin	*plugin,
							 GList		**apps);
typedef gboolean	 (*AsbPluginCheckFilenameFunc)	(AsbPlugin	*plugin,
							 const gchar	*filename);
typedef GList		*(*AsbPluginProcessFunc)	(AsbPlugin	*plugin,
							 AsbPackage 	*pkg,
							 const gchar	*tmp_dir,
							 GError		**error);
typedef gboolean	 (*AsbPluginProcessAppFunc)	(AsbPlugin	*plugin,
							 AsbPackage 	*pkg,
							 AsbApp 	*app,
							 const gchar	*tmpdir,
							 GError		**error);

const gchar	*asb_plugin_get_name			(void);
void		 asb_plugin_initialize			(AsbPlugin	*plugin);
void		 asb_plugin_destroy			(AsbPlugin	*plugin);
void		 asb_plugin_set_enabled			(AsbPlugin	*plugin,
							 gboolean	 enabled);
GList		*asb_plugin_process			(AsbPlugin	*plugin,
							 AsbPackage	*pkg,
							 const gchar	*tmpdir,
							 GError		**error);
void		 asb_plugin_add_globs			(AsbPlugin	*plugin,
							 GPtrArray	*globs);
void		 asb_plugin_merge			(AsbPlugin	*plugin,
							 GList		**list);
gboolean	 asb_plugin_process_app			(AsbPlugin	*plugin,
							 AsbPackage	*pkg,
							 AsbApp		*app,
							 const gchar	*tmp_dir,
							 GError		**error);
gboolean	 asb_plugin_check_filename		(AsbPlugin	*plugin,
							 const gchar	*filename);
void		 asb_plugin_add_app			(GList		**list,
							 AsApp		*app);
void		 asb_plugin_add_glob			(GPtrArray	*array,
							 const gchar	*glob);

G_END_DECLS

#endif /* __ASB_PLUGIN_H */
