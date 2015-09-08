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

#ifndef ASB_PACKAGE_CAB_H
#define ASB_PACKAGE_CAB_H

#include <glib-object.h>

#include <stdarg.h>
#include <appstream-glib.h>

#include "asb-package.h"

G_BEGIN_DECLS

#define ASB_TYPE_PACKAGE_CAB (asb_package_cab_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsbPackageCab, asb_package_cab, ASB, PACKAGE_CAB, GObject)

struct _AsbPackageCabClass
{
	AsbPackageClass			parent_class;
};

AsbPackage	*asb_package_cab_new		(void);

G_END_DECLS

#endif /* ASB_PACKAGE_CAB_H */
