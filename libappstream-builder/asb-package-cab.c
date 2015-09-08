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

/**
 * SECTION:asb-package-cab
 * @short_description: Object representing a .CAB package file.
 * @stability: Unstable
 *
 * This object represents one .cab package file.
 */

#include "config.h"

#include "as-cleanup.h"
#include "asb-package-cab.h"
#include "asb-plugin.h"


G_DEFINE_TYPE (AsbPackageCab, asb_package_cab, ASB_TYPE_PACKAGE)

/**
 * asb_package_cab_init:
 **/
static void
asb_package_cab_init (AsbPackageCab *pkg)
{
}

/**
 * asb_package_cab_ensure_simple:
 **/
static gboolean
asb_package_cab_ensure_simple (AsbPackage *pkg, GError **error)
{
	g_autofree gchar *basename;
	gchar *tmp;

	/* get basename minus the .cab extension */
	basename = g_path_get_basename (asb_package_get_filename (pkg));
	tmp = g_strrstr (basename, ".cab");
	if (tmp != NULL)
		*tmp = '\0';

	/* maybe get a version too */
	tmp = g_strrstr (basename, "-");
	if (tmp == NULL) {
		asb_package_set_name (pkg, basename);
		asb_package_set_version (pkg, "unknown");
		asb_package_set_release (pkg, "unknown");
	} else {
		*tmp = '\0';
		asb_package_set_name (pkg, basename);
		asb_package_set_version (pkg, tmp + 1);
		asb_package_set_release (pkg, "unknown");
	}
	return TRUE;
}

/**
 * asb_package_cab_ensure_filelists:
 **/
static gboolean
asb_package_cab_ensure_filelists (AsbPackage *pkg, GError **error)
{
	const gchar *argv[4] = { "gcab", "--list", "fn", NULL };
	guint i;
	g_autofree gchar *output = NULL;
	g_autoptr(GPtrArray) files = NULL;
	g_auto(GStrv) lines = NULL;

	/* spawn sync */
	argv[2] = asb_package_get_filename (pkg);
	if (!g_spawn_sync (NULL, (gchar **) argv, NULL,
			   G_SPAWN_SEARCH_PATH,
			   NULL, NULL,
			   &output, NULL, NULL, error))
		return FALSE;

	/* parse output */
	files = g_ptr_array_new_with_free_func (g_free);
	lines = g_strsplit (output, "\n", -1);
	for (i = 0; lines[i] != NULL; i++) {
		if (lines[i][0] == '\0')
			continue;
		g_ptr_array_add (files, g_strdup (lines[i]));
	}

	/* save */
	g_ptr_array_add (files, NULL);
	asb_package_set_filelist (pkg, (gchar **) files->pdata);
	return TRUE;
}

/**
 * asb_package_cab_open:
 **/
static gboolean
asb_package_cab_open (AsbPackage *pkg, const gchar *filename, GError **error)
{
	/* read package stuff */
	if (!asb_package_cab_ensure_simple (pkg, error))
		return FALSE;
	if (!asb_package_cab_ensure_filelists (pkg, error))
		return FALSE;
	return TRUE;
}

/**
 * asb_package_cab_ensure:
 **/
static gboolean
asb_package_cab_ensure (AsbPackage *pkg,
			AsbPackageEnsureFlags flags,
			GError **error)
{
	if ((flags & ASB_PACKAGE_ENSURE_NEVRA) > 0) {
		if (!asb_package_cab_ensure_simple (pkg, error))
			return FALSE;
	}

	if ((flags & ASB_PACKAGE_ENSURE_FILES) > 0) {
		if (!asb_package_cab_ensure_filelists (pkg, error))
			return FALSE;
	}

	return TRUE;
}

/**
 * asb_package_cab_class_init:
 **/
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
