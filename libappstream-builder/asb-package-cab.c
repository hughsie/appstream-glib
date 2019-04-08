/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:asb-package-cab
 * @short_description: Object representing a .CAB package file.
 * @stability: Unstable
 *
 * This object represents one .cab package file.
 */

#include "config.h"

#include "asb-package-cab.h"
#include "asb-plugin.h"


G_DEFINE_TYPE (AsbPackageCab, asb_package_cab, ASB_TYPE_PACKAGE)

static void
asb_package_cab_init (AsbPackageCab *pkg)
{
}

static gboolean
asb_package_cab_open (AsbPackage *pkg, const gchar *filename, GError **error)
{
	g_set_error (error,
		     AS_STORE_ERROR,
		     AS_STORE_ERROR_FAILED,
		     "Loading firmware is no longer supported, see fwupd");
	return FALSE;
}

static gboolean
asb_package_cab_ensure (AsbPackage *pkg,
			AsbPackageEnsureFlags flags,
			GError **error)
{
	g_set_error (error,
		     AS_STORE_ERROR,
		     AS_STORE_ERROR_FAILED,
		     "Loading firmware is no longer supported, see fwupd");
	return FALSE;
}

static void
asb_package_cab_class_init (AsbPackageCabClass *klass)
{
	AsbPackageClass *package_class = ASB_PACKAGE_CLASS (klass);
	package_class->open = asb_package_cab_open;
	package_class->ensure = asb_package_cab_ensure;
}

/**
 * asb_package_cab_new:
 *
 * Creates a new CAB package.
 *
 * Returns: a package
 *
 * Since: 0.3.5
 **/
AsbPackage *
asb_package_cab_new (void)
{
	AsbPackage *pkg;
	pkg = g_object_new (ASB_TYPE_PACKAGE_CAB, NULL);
	asb_package_set_kind (pkg, ASB_PACKAGE_KIND_FIRMWARE);
	return ASB_PACKAGE (pkg);
}
