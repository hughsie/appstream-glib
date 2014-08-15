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

#ifndef ASB_PANEL_H
#define ASB_PANEL_H

#include <glib-object.h>

#define ASB_TYPE_PANEL			(asb_panel_get_type())
#define ASB_PANEL(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), ASB_TYPE_PANEL, AsbPanel))
#define ASB_PANEL_CLASS(cls)		(G_TYPE_CHECK_CLASS_CAST((cls), ASB_TYPE_PANEL, AsbPanelClass))
#define ASB_IS_PANEL(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), ASB_TYPE_PANEL))
#define ASB_IS_PANEL_CLASS(cls)		(G_TYPE_CHECK_CLASS_TYPE((cls), ASB_TYPE_PANEL))
#define ASB_PANEL_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), ASB_TYPE_PANEL, AsbPanelClass))

G_BEGIN_DECLS

typedef struct _AsbPanel		AsbPanel;
typedef struct _AsbPanelClass		AsbPanelClass;

struct _AsbPanel
{
	GObject				 parent;
};

struct _AsbPanelClass
{
	GObjectClass			 parent_class;
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

GType		 asb_panel_get_type		(void);

AsbPanel	*asb_panel_new			(void);
void		 asb_panel_remove		(AsbPanel	*panel);
void		 asb_panel_set_title		(AsbPanel	*panel,
						 const gchar	*title);
void		 asb_panel_set_status		(AsbPanel	*panel,
						 const gchar	*fmt,
						 ...)
						 G_GNUC_PRINTF(2,3);
void		 asb_panel_set_job_number	(AsbPanel	*panel,
						 guint		 job_number);
void		 asb_panel_set_job_total	(AsbPanel	*panel,
						 guint		 job_total);

G_END_DECLS

#endif /* ASB_PANEL_H */
