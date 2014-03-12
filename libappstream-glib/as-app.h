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

#ifndef AS_APP_H
#define AS_APP_H

#include <glib-object.h>

#define AS_TYPE_APP		(as_app_get_type())
#define AS_APP(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), AS_TYPE_APP, AsApp))
#define AS_APP_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), AS_TYPE_APP, AsAppClass))
#define AS_IS_APP(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), AS_TYPE_APP))
#define AS_IS_APP_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), AS_TYPE_APP))
#define AS_APP_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), AS_TYPE_APP, AsAppClass))

G_BEGIN_DECLS

typedef struct _AsApp		AsApp;
typedef struct _AsAppClass	AsAppClass;

struct _AsApp
{
	GObject			parent;
};

struct _AsAppClass
{
	GObjectClass		parent_class;
};

GType		 as_app_get_type		(void);
AsApp		*as_app_new			(void);

const gchar	*as_app_get_id			(AsApp		*app);

void		 as_app_set_id			(AsApp		*app,
						 const gchar	*id);

G_END_DECLS

#endif /* AS_APP_H */
