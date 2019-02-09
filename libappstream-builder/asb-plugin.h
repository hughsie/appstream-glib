/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>
#include <gmodule.h>
#include <gio/gio.h>
#include <appstream-glib.h>

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
	ASB_PLUGIN_ERROR_IGNORE,
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
							 GList		*apps);
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
GList		*asb_plugin_process			(AsbPlugin	*plugin,
							 AsbPackage	*pkg,
							 const gchar	*tmpdir,
							 GError		**error);
void		 asb_plugin_add_globs			(AsbPlugin	*plugin,
							 GPtrArray	*globs);
void		 asb_plugin_merge			(AsbPlugin	*plugin,
							 GList		*list);
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
gboolean	 asb_plugin_match_glob			(const gchar	*glob,
							 const gchar	*value);

G_END_DECLS
