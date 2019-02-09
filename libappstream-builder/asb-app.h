/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <stdarg.h>
#include <glib-object.h>
#include <appstream-glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "asb-package.h"

G_BEGIN_DECLS

#define ASB_TYPE_APP (asb_app_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsbApp, asb_app, ASB, APP, GObject)

struct _AsbAppClass
{
	AsAppClass		parent_class;
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

/**
 * AsbAppSaveFlags:
 * @ASB_APP_SAVE_FLAG_NONE:		Nothing to do
 * @ASB_APP_SAVE_FLAG_ICONS:		Save icons to disk
 * @ASB_APP_SAVE_FLAG_SCREENSHOTS:	Save screenshots to disk
 *
 * The flags to use when saving resources.
 **/
typedef enum {
	ASB_APP_SAVE_FLAG_NONE,
	ASB_APP_SAVE_FLAG_ICONS		= 1,	/* Since: 0.3.2 */
	ASB_APP_SAVE_FLAG_SCREENSHOTS	= 2,	/* Since: 0.3.2 */
	/*< private >*/
	ASB_APP_SAVE_FLAG_LAST,
} AsbAppSaveFlags;

AsbApp		*asb_app_new			(AsbPackage	*pkg,
						 const gchar	*id);
void		 asb_app_set_hidpi_enabled	(AsbApp		*app,
						 gboolean	 hidpi_enabled);
void		 asb_app_set_package		(AsbApp		*app,
						 AsbPackage	*pkg);
AsbPackage	*asb_app_get_package		(AsbApp		*app);
gboolean	 asb_app_save_resources		(AsbApp		*app,
						 AsbAppSaveFlags save_flags,
						 GError		**error);


G_END_DECLS
