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

#define ASB_TYPE_PACKAGE_RPM (asb_package_rpm_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsbPackageRpm, asb_package_rpm, ASB, PACKAGE_RPM, GObject)

struct _AsbPackageRpmClass
{
	AsbPackageClass			parent_class;
};

AsbPackage	*asb_package_rpm_new		(void);

G_END_DECLS
