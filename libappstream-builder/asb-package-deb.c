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
 * SECTION:asb-package-deb
 * @short_description: Object representing a .DEB package file.
 * @stability: Unstable
 *
 * This object represents one .deb package file.
 */

#include "config.h"

#include "asb-package-deb.h"
#include "asb-plugin.h"


G_DEFINE_TYPE (AsbPackageDeb, asb_package_deb, ASB_TYPE_PACKAGE)

/**
 * asb_package_deb_init:
 **/
static void
asb_package_deb_init (AsbPackageDeb *pkg)
{
}

/**
 * asb_package_deb_ensure_simple:
 **/
static gboolean
asb_package_deb_ensure_simple (AsbPackage *pkg, GError **error)
{
	const gchar *argv[4] = { "dpkg", "--field", "fn", NULL };
	gchar *tmp;
	guint i;
	guint j;
	g_autofree gchar *output = NULL;
	g_auto(GStrv) lines = NULL;

	/* spawn sync */
	argv[2] = asb_package_get_filename (pkg);
	if (!g_spawn_sync (NULL, (gchar **) argv, NULL,
			   G_SPAWN_SEARCH_PATH,
			   NULL, NULL,
			   &output, NULL, NULL, error))
		return FALSE;

	/* parse output */
	lines = g_strsplit (output, "\n", -1);
	for (i = 0; lines[i] != NULL; i++) {
		if (g_str_has_prefix (lines[i], "Package: ")) {
			asb_package_set_name (pkg, lines[i] + 9);
			continue;
		}
		if (g_str_has_prefix (lines[i], "Source: ")) {
			asb_package_set_source (pkg, lines[i] + 8);
			continue;
		}
		if (g_str_has_prefix (lines[i], "Version: ")) {
			g_auto(GStrv) vr = NULL;
			vr = g_strsplit (lines[i] + 9, "-", 2);
			tmp = g_strstr_len (vr[0], -1, ":");
			if (tmp == NULL) {
				asb_package_set_version (pkg, vr[0]);
			} else {
				*tmp = '\0';
				j = g_ascii_strtoll (vr[0], NULL, 10);
				asb_package_set_epoch (pkg, j);
				asb_package_set_version (pkg, tmp + 1);
			}
			if (vr[1] != NULL) {
				asb_package_set_release (pkg, vr[1]);
			} else {
				/* packages don't actually have to have a
				 * release value like rpm; in this case fake
				 * something plausible */
				asb_package_set_release (pkg, "0");
			}
			continue;
		}
		if (g_str_has_prefix (lines[i], "Depends: ")) {
			g_auto(GStrv) vr = NULL;
			vr = g_strsplit (lines[i] + 9, ", ", -1);
			for (j = 0; vr[j] != NULL; j++) {
				tmp = g_strstr_len (vr[j], -1, " ");
				if (tmp != NULL)
					*tmp = '\0';
				asb_package_add_dep (pkg, vr[j]);
			}
			continue;
		}
	}
	return TRUE;
}

/**
 * asb_package_deb_ensure_filelists:
 **/
static gboolean
asb_package_deb_ensure_filelists (AsbPackage *pkg, GError **error)
{
	const gchar *argv[4] = { "dpkg", "--contents", "fn", NULL };
	const gchar *fn;
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
		fn = g_strrstr (lines[i], " ");
		if (fn == NULL)
			continue;
		/* ignore directories */
		if (g_str_has_suffix (fn, "/"))
			continue;
		g_ptr_array_add (files, g_strdup (fn + 2));
	}

	/* save */
	g_ptr_array_add (files, NULL);
	asb_package_set_filelist (pkg, (gchar **) files->pdata);
	return TRUE;
}

/**
 * asb_package_deb_open:
 **/
static gboolean
asb_package_deb_open (AsbPackage *pkg, const gchar *filename, GError **error)
{
	/* read package stuff */
	if (!asb_package_deb_ensure_simple (pkg, error))
		return FALSE;
	if (!asb_package_deb_ensure_filelists (pkg, error))
		return FALSE;
	return TRUE;
}

/**
 * asb_package_deb_explode:
 **/
static gboolean
asb_package_deb_explode (AsbPackage *pkg,
			 const gchar *dir,
			 GPtrArray *glob,
			 GError **error)
{
	guint i;
	const gchar *data_names[] = { "data.tar.xz",
				      "data.tar.bz2",
				      "data.tar.gz",
				      "data.tar.lzma",
				      "data.tar",
				      NULL };

	/* first decompress the main deb */
	if (!asb_utils_explode (asb_package_get_filename (pkg),
				dir, NULL, error))
		return FALSE;

	/* then decompress the data file */
	for (i = 0; data_names[i] != NULL; i++) {
		g_autofree gchar *data_fn = NULL;
		data_fn = g_build_filename (dir, data_names[i], NULL);
		if (g_file_test (data_fn, G_FILE_TEST_EXISTS)) {
			if (!asb_utils_explode (data_fn, dir, glob, error))
				return FALSE;
		}
	}
	return TRUE;
}

/**
 * asb_package_deb_class_init:
 **/
static void
asb_package_deb_class_init (AsbPackageDebClass *klass)
{
	AsbPackageClass *package_class = ASB_PACKAGE_CLASS (klass);
	package_class->open = asb_package_deb_open;
	package_class->explode = asb_package_deb_explode;
}

/**
 * asb_package_deb_new:
 *
 * Creates a new DEB package.
 *
 * Returns: a package
 *
 * Since: 0.1.0
 **/
AsbPackage *
asb_package_deb_new (void)
{
	AsbPackage *pkg;
	pkg = g_object_new (ASB_TYPE_PACKAGE_DEB, NULL);
	return ASB_PACKAGE (pkg);
}
