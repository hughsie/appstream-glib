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

#ifndef ASB_APP_H
#define ASB_APP_H

#include <stdarg.h>
#include <glib-object.h>
#include <appstream-glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "asb-package.h"

#define ASB_TYPE_APP		(asb_app_get_type())
#define ASB_APP(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), ASB_TYPE_APP, AsbApp))
#define ASB_APP_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), ASB_TYPE_APP, AsbAppClass))
#define ASB_IS_APP(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), ASB_TYPE_APP))
#define ASB_IS_APP_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), ASB_TYPE_APP))
#define ASB_APP_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), ASB_TYPE_APP, AsbAppClass))

G_BEGIN_DECLS

typedef struct _AsbApp		AsbApp;
typedef struct _AsbAppClass	AsbAppClass;

struct _AsbApp
{
	AsApp			parent;
};

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

GType		 asb_app_get_type		(void);


AsbApp		*asb_app_new			(AsbPackage	*pkg,
						 const gchar	*id);
void		 asb_app_set_hidpi_enabled	(AsbApp		*app,
						 gboolean	 hidpi_enabled);
void		 asb_app_add_requires_appdata	(AsbApp		*app,
						 const gchar	*fmt,
						 ...)
						 G_GNUC_PRINTF(2,3);
void		 asb_app_set_requires_appdata	(AsbApp		*app,
						 gboolean	 requires_appdata);
void		 asb_app_add_pixbuf		(AsbApp		*app,
						 GdkPixbuf	*pixbuf);

GPtrArray	*asb_app_get_requires_appdata	(AsbApp		*app);
AsbPackage	*asb_app_get_package		(AsbApp		*app);

gboolean	 asb_app_save_resources		(AsbApp		*app,
						 AsbAppSaveFlags save_flags,
						 GError		**error);


G_END_DECLS

#endif /* ASB_APP_H */
