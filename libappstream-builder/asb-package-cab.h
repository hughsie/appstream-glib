/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

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
