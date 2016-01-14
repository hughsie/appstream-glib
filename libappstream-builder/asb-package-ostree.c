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
 * SECTION:asb-package-ostree
 * @short_description: Object representing a os branch.
 * @stability: Unstable
 *
 * This object represents one ostree entry.
 */

#include "config.h"

#include <ostree.h>

#include "asb-package-ostree.h"
#include "asb-plugin.h"

typedef struct
{
	OstreeRepo		*repo;
	gchar			*repodir;
} AsbPackageOstreePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AsbPackageOstree, asb_package_ostree, ASB_TYPE_PACKAGE)

#define GET_PRIVATE(o) (asb_package_ostree_get_instance_private (o))

static gboolean asb_package_ostree_ensure_nevra (AsbPackage *pkg, GError **error);

/**
 * asb_package_ostree_finalize:
 **/
static void
asb_package_ostree_finalize (GObject *object)
{
	AsbPackageOstree *pkg = ASB_PACKAGE_OSTREE (object);
	AsbPackageOstreePrivate *priv = GET_PRIVATE (pkg);

	g_free (priv->repodir);
	g_object_unref (priv->repo);

	G_OBJECT_CLASS (asb_package_ostree_parent_class)->finalize (object);
}

/**
 * asb_package_ostree_init:
 **/
static void
asb_package_ostree_init (AsbPackageOstree *pkg_ostree)
{
}

/**
 * asb_package_ostree_set_repodir:
 **/
void
asb_package_ostree_set_repodir (AsbPackageOstree *pkg_ostree,
				const gchar *repodir)
{
	AsbPackageOstreePrivate *priv = GET_PRIVATE (pkg_ostree);
	priv->repodir = g_strdup (repodir);
}

/**
 * asb_package_ostree_open:
 **/
static gboolean
asb_package_ostree_open (AsbPackage *pkg, const gchar *filename, GError **error)
{
	AsbPackageOstree *pkg_ostree = ASB_PACKAGE_OSTREE (pkg);
	AsbPackageOstreePrivate *priv = GET_PRIVATE (pkg_ostree);
	g_autoptr(GFile) file = NULL;

	if (!asb_package_ostree_ensure_nevra (pkg, error))
		return FALSE;

	/* create the OstreeRepo */
	file = g_file_new_for_path (priv->repodir);
	priv->repo = ostree_repo_new (file);

	return TRUE;
}

/**
 * asb_package_ostree_build_filelist:
 **/
static gboolean
asb_package_ostree_build_filelist (GPtrArray *array, GFile *file, GError **error)
{
	g_autoptr(GFileEnumerator) enumerator = NULL;

	/* iter on children */
	enumerator = g_file_enumerate_children (file, "standard::*",
						G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
						NULL, error);
	if (enumerator == NULL)
		return FALSE;
	do {
		g_autofree gchar *path = NULL;
		g_autoptr(GFileInfo) info = NULL;

		info = g_file_enumerator_next_file (enumerator, NULL, error);
		if (info == NULL) {
			if (*error != NULL)
				return FALSE;
			break;
		}

		/* recurse if directory */
		path = g_file_get_path (file);
		g_ptr_array_add (array, g_build_filename (path, g_file_info_get_name (info), NULL));
		if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
			g_autoptr(GFile) child = NULL;
			child = g_file_get_child (g_file_enumerator_get_container (enumerator),
						  g_file_info_get_name (info));
			if (!asb_package_ostree_build_filelist (array, child, error))
				return FALSE;
		}
	} while (TRUE);
	return TRUE;
}

/**
 * asb_package_ostree_ensure_files:
 **/
static gboolean
asb_package_ostree_ensure_files (AsbPackage *pkg, GError **error)
{
	AsbPackageOstree *pkg_ostree = ASB_PACKAGE_OSTREE (pkg);
	AsbPackageOstreePrivate *priv = GET_PRIVATE (pkg_ostree);
	const gchar *rev;
	g_autoptr(GFile) root = NULL;
	g_autoptr(GPtrArray) array = NULL;

	/* get the filelist */
	if (!ostree_repo_open (priv->repo, NULL, error))
		return FALSE;
	rev = asb_package_get_source (pkg);
	if (!ostree_repo_read_commit (priv->repo, rev, &root, NULL, NULL, error))
		return FALSE;

	/* build an array */
	array = g_ptr_array_new_with_free_func (g_free);
	if (!asb_package_ostree_build_filelist (array, root, error))
		return FALSE;
	g_ptr_array_add (array, NULL);
	asb_package_set_filelist (pkg, (gchar **) array->pdata);

	return TRUE;
}


/**
 * asb_package_ostree_ensure_nevra:
 **/
static gboolean
asb_package_ostree_ensure_nevra (AsbPackage *pkg, GError **error)
{
	g_auto(GStrv) split = NULL;

	/* split up 'app/org.gnome.GEdit/x86_64/master' */
	split = g_strsplit (asb_package_get_source (pkg), "/", -1);
	if (g_strv_length (split) != 4) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "invalid ref name %s",
			     asb_package_get_source (pkg));
		return FALSE;
	}
	asb_package_set_release (pkg, split[0]);
	asb_package_set_name (pkg, split[1]);
	asb_package_set_version (pkg, split[3]);
	asb_package_set_arch (pkg, split[2]);
	return TRUE;
}

/**
 * asb_package_ostree_ensure:
 **/
static gboolean
asb_package_ostree_ensure (AsbPackage *pkg,
			   AsbPackageEnsureFlags flags,
			   GError **error)
{
	if ((flags & ASB_PACKAGE_ENSURE_NEVRA) > 0) {
		if (!asb_package_ostree_ensure_nevra (pkg, error))
			return FALSE;
	}

	if ((flags & ASB_PACKAGE_ENSURE_FILES) > 0) {
		if (!asb_package_ostree_ensure_files (pkg, error))
			return FALSE;
	}

	return TRUE;
}

/**
 * asb_package_ostree_compare:
 **/
static gint
asb_package_ostree_compare (AsbPackage *pkg1, AsbPackage *pkg2)
{
	return g_strcmp0 (asb_package_get_name (pkg1),
			  asb_package_get_name (pkg2));
}

/**
 * asb_package_ostree_explode:
 **/
static gboolean
asb_package_ostree_explode (AsbPackage *pkg, const gchar *dir,
			    GPtrArray *glob, GError **error)
{
	AsbPackageOstree *pkg_ostree = ASB_PACKAGE_OSTREE (pkg);
	AsbPackageOstreePrivate *priv = GET_PRIVATE (pkg_ostree);
	const gchar *commit;
	g_autofree gchar *resolved_commit = NULL;
	g_autoptr(GFileInfo) file_info = NULL;
	g_autoptr(GFile) root = NULL;
	g_autoptr(GFile) target = NULL;

	/* extract root */
	commit = asb_package_get_source (pkg);
	if (!ostree_repo_resolve_rev (priv->repo, commit, FALSE,
				      &resolved_commit, error))
		return FALSE;
	if (!ostree_repo_read_commit (priv->repo, resolved_commit,
				      &root, NULL, NULL, error))
		return FALSE;
	file_info = g_file_query_info (root, "standard::*",
				       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				       NULL, error);
	if (file_info == NULL)
		return FALSE;
	target = g_file_new_for_path (dir);
	if (!ostree_repo_checkout_tree (priv->repo, OSTREE_REPO_CHECKOUT_MODE_USER,
					OSTREE_REPO_CHECKOUT_OVERWRITE_UNION_FILES,
					target, OSTREE_REPO_FILE (root),
					file_info, NULL, error))
		return FALSE;
	return TRUE;
}

/**
 * asb_package_ostree_class_init:
 **/
static void
asb_package_ostree_class_init (AsbPackageOstreeClass *klass)
{
	AsbPackageClass *package_class = ASB_PACKAGE_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = asb_package_ostree_finalize;
	package_class->open = asb_package_ostree_open;
	package_class->ensure = asb_package_ostree_ensure;
	package_class->compare = asb_package_ostree_compare;
	package_class->explode = asb_package_ostree_explode;
}

/**
 * asb_package_ostree_new:
 *
 * Creates a new OSTREE package.
 *
 * Returns: a package
 *
 * Since: 0.3.5
 **/
AsbPackage *
asb_package_ostree_new (void)
{
	AsbPackage *pkg;
	pkg = g_object_new (ASB_TYPE_PACKAGE_OSTREE, NULL);
	asb_package_set_kind (pkg, ASB_PACKAGE_KIND_BUNDLE);
	return ASB_PACKAGE (pkg);
}
