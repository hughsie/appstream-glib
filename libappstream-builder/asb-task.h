/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "asb-package.h"
#include "asb-context.h"

G_BEGIN_DECLS

#define ASB_TYPE_TASK (asb_task_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsbTask, asb_task, ASB, TASK, GObject)

struct _AsbTaskClass
{
	GObjectClass		 parent_class;
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

AsbTask		*asb_task_new			(AsbContext	*ctx);
gboolean	 asb_task_process		(AsbTask	*task,
						 GError		**error);
void		 asb_task_set_package		(AsbTask	*task,
						 AsbPackage	*pkg);

G_END_DECLS
