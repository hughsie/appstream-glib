/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Fabien Bourigault <bourigaultfabien@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:asb-package-alpm
 * @short_description: Object representing a .tar.xz (pacman) package file.
 * @stability: Unstable
 *
 * This object represents one .tar.xz (pacman) package file.
 */

#include "config.h"

#include <limits.h>

#include <alpm.h>

#include "asb-package-alpm.h"
#include "asb-plugin.h"

typedef struct
{
	alpm_handle_t	*handle;
	alpm_pkg_t	*package;
} AsbPackageAlpmPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsbPackageAlpm, asb_package_alpm, ASB_TYPE_PACKAGE)

#define GET_PRIVATE(o) (asb_package_alpm_get_instance_private (o))

static void
asb_package_alpm_finalize (GObject *object)
{
	AsbPackageAlpm *pkg = ASB_PACKAGE_ALPM (object);
	AsbPackageAlpmPrivate *priv = GET_PRIVATE (pkg);

	/* TODO: handle errors */
	alpm_pkg_free (priv->package);
	alpm_release (priv->handle);

	G_OBJECT_CLASS (asb_package_alpm_parent_class)->finalize (object);
}

static void
asb_package_alpm_init (AsbPackageAlpm *pkg)
{
}

static GPtrArray *
asb_package_alpm_list_to_array (alpm_list_t *list)
{
	alpm_list_t *current;
	GPtrArray *array = g_ptr_array_new_with_free_func (g_free);

	for (current = list; current; current = alpm_list_next (current)) {
		g_ptr_array_add (array, g_strdup (current->data));
	}
	g_ptr_array_add (array, NULL);

	return array;
}

static gboolean
asb_package_alpm_ensure_license (AsbPackage *pkg, GError **error)
{
	AsbPackageAlpm *pkg_alpm = ASB_PACKAGE_ALPM (pkg);
	AsbPackageAlpmPrivate *priv = GET_PRIVATE (pkg_alpm);
	
	alpm_list_t *alpm_licenses;
	GPtrArray *licenses;
	gchar *license;

	alpm_licenses = alpm_pkg_get_licenses (priv->package);
	licenses = asb_package_alpm_list_to_array (alpm_licenses);
	/* TODO: translate licenses to SPDX licenses (makes licenses clickable
	 * is GNOME Software) */
	license = g_strjoinv (" AND ", (gchar **)(licenses->pdata));

	asb_package_set_license (pkg, license);

	g_ptr_array_free (licenses, TRUE);
	return TRUE;
}

static void
asb_package_alpm_ensure_version (AsbPackage *pkg, GError **error)
{
	AsbPackageAlpm *pkg_alpm = ASB_PACKAGE_ALPM (pkg);
	AsbPackageAlpmPrivate *priv = GET_PRIVATE (pkg_alpm);

	g_auto(GStrv) split = NULL;

	split = g_strsplit (alpm_pkg_get_version (priv->package), ":-", 3);

	/* epoch:version:release */
	if (g_strv_length (split) == 3) {
		asb_package_set_epoch (pkg, (guint) g_ascii_strtoull (split[0], NULL, 0));
		asb_package_set_version (pkg, split[1]);
		asb_package_set_release (pkg, split[2]);
	} else {/* version:release */
		asb_package_set_version (pkg, split[0]);
		asb_package_set_release (pkg, split[1]);
	}
}

#if 0
static gboolean
asb_package_alpm_ensure_releases (AsbPackage *pkg, GError **error)
{
	AsbPackageAlpm *pkg_alpm = ASB_PACKAGE_ALPM (pkg);
	AsbPackageAlpmPrivate *priv = GET_PRIVATE (pkg_alpm);
	return TRUE;
}
#endif

static gboolean
asb_package_alpm_ensure_depends (AsbPackage *pkg, GError **error)
{
	AsbPackageAlpm *pkg_alpm = ASB_PACKAGE_ALPM (pkg);
	AsbPackageAlpmPrivate *priv = GET_PRIVATE (pkg_alpm);
	alpm_list_t *alpm_depends;
	alpm_list_t *current;

	alpm_depends = alpm_pkg_get_depends (priv->package);
	for (current = alpm_depends; current; current = alpm_list_next (current))
		asb_package_add_dep (pkg, current->data);

	return TRUE;
}

static gboolean
asb_package_alpm_ensure_filelists (AsbPackage *pkg, GError **error)
{
	AsbPackageAlpm *pkg_alpm = ASB_PACKAGE_ALPM (pkg);
	AsbPackageAlpmPrivate *priv = GET_PRIVATE (pkg_alpm);

	alpm_filelist_t *pkgfiles = alpm_pkg_get_files (priv->package);
	guint i;
	GPtrArray *filelist = g_ptr_array_new_with_free_func (g_free);

	for (i = 0; i < pkgfiles->count; i++) {
		const alpm_file_t *file = pkgfiles->files + i;
		g_ptr_array_add (filelist, g_strconcat ("/", file->name, NULL));
	}
	g_ptr_array_add (filelist, NULL);

	asb_package_set_filelist (pkg, (gchar **)(filelist->pdata));

	g_ptr_array_free (filelist, TRUE);
	return TRUE;
}

static gboolean
asb_package_alpm_open (AsbPackage *pkg, const gchar *filename, GError **error)
{
	AsbPackageAlpm *pkg_alpm = ASB_PACKAGE_ALPM (pkg);
	AsbPackageAlpmPrivate *priv = GET_PRIVATE (pkg_alpm);

	alpm_errno_t alpm_error;

	/* initialize the alpm library */
	priv->handle = alpm_initialize ("/", "/tmp", &alpm_error);
	if (priv->handle == NULL) {
		g_set_error (error,
		             ASB_PLUGIN_ERROR,
		             ASB_PLUGIN_ERROR_FAILED,
		             "libalpm initialization failed %s (%u) for %s",
		             alpm_strerror (alpm_error),
		             alpm_error,
		             filename);
		return FALSE;
	}

	/* open the package */
	if (alpm_pkg_load (priv->handle, filename, TRUE, 0, &priv->package) == -1) {
		g_set_error (error,
		             ASB_PLUGIN_ERROR,
		             ASB_PLUGIN_ERROR_FAILED,
		             "Failed to load package %s : %s (%u)",
		             filename,
		             alpm_strerror (alpm_errno (priv->handle)),
		             alpm_errno (priv->handle));
		alpm_release (priv->handle);
		return FALSE;
	}

	asb_package_set_name (pkg, alpm_pkg_get_name (priv->package));
	asb_package_set_url (pkg, alpm_pkg_get_url (priv->package));
	asb_package_set_arch (pkg, alpm_pkg_get_arch (priv->package));
	asb_package_alpm_ensure_version (pkg, error);

	return TRUE;
}

static gint
asb_package_alpm_compare (AsbPackage *pkg1, AsbPackage *pkg2)
{
	AsbPackageAlpm *pkg_alpm1 = ASB_PACKAGE_ALPM (pkg1);
	AsbPackageAlpmPrivate *priv1 = GET_PRIVATE (pkg_alpm1);

	AsbPackageAlpm *pkg_alpm2 = ASB_PACKAGE_ALPM (pkg2);
	AsbPackageAlpmPrivate *priv2 = GET_PRIVATE (pkg_alpm2);

	const gchar *pkg1_version = alpm_pkg_get_version (priv1->package);
	const gchar *pkg2_version = alpm_pkg_get_version (priv2->package);
	
	return alpm_pkg_vercmp (pkg1_version, pkg2_version);
}

static gboolean
asb_package_alpm_ensure (AsbPackage *pkg,
			 AsbPackageEnsureFlags flags,
			 GError **error)
{
	if ((flags & ASB_PACKAGE_ENSURE_DEPS) > 0) {
		if (!asb_package_alpm_ensure_depends (pkg, error))
			return FALSE;
	}
	if ((flags & ASB_PACKAGE_ENSURE_FILES) > 0) {
		if (!asb_package_alpm_ensure_filelists (pkg, error))
			return FALSE;
	}
	if ((flags & ASB_PACKAGE_ENSURE_LICENSE) > 0) {
		if (!asb_package_alpm_ensure_license (pkg, error))
			return FALSE;
	}
	return TRUE;
}

static void
asb_package_alpm_class_init (AsbPackageAlpmClass *klass)
{
	AsbPackageClass *package_class = ASB_PACKAGE_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = asb_package_alpm_finalize;
	package_class->open = asb_package_alpm_open;
	package_class->ensure = asb_package_alpm_ensure;
	package_class->compare = asb_package_alpm_compare;
}

AsbPackage *
asb_package_alpm_new (void)
{
	AsbPackage *pkg;
	pkg = g_object_new (ASB_TYPE_PACKAGE_ALPM, NULL);
	return ASB_PACKAGE (pkg);
}
