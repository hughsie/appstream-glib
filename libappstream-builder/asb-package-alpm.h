/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Fabien Bourigault <bourigaultfabien@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include <stdarg.h>
#include <appstream-glib.h>

#include "asb-package.h"

G_BEGIN_DECLS

#define ASB_TYPE_PACKAGE_ALPM (asb_package_alpm_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsbPackageAlpm, asb_package_alpm, ASB, PACKAGE_ALPM, GObject)

struct _AsbPackageAlpmClass
{
	AsbPackageClass			parent_class;
};

AsbPackage	*asb_package_alpm_new		(void);

G_END_DECLS
