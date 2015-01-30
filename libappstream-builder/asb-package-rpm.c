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
 * SECTION:asb-package-rpm
 * @short_description: Object representing a .RPM package file.
 * @stability: Unstable
 *
 * This object represents one .rpm package file.
 */

#include "config.h"

#include <limits.h>
#include <archive.h>
#include <archive_entry.h>

#include <rpm/rpmlib.h>
#include <rpm/rpmts.h>

#include "as-cleanup.h"
#include "asb-package-rpm.h"
#include "asb-plugin.h"

typedef struct _AsbPackageRpmPrivate	AsbPackageRpmPrivate;
struct _AsbPackageRpmPrivate
{
	Header		 h;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsbPackageRpm, asb_package_rpm, ASB_TYPE_PACKAGE)

#define GET_PRIVATE(o) (asb_package_rpm_get_instance_private (o))

/**
 * asb_package_rpm_finalize:
 **/
static void
asb_package_rpm_finalize (GObject *object)
{
	AsbPackageRpm *pkg = ASB_PACKAGE_RPM (object);
	AsbPackageRpmPrivate *priv = GET_PRIVATE (pkg);

	headerFree (priv->h);

	G_OBJECT_CLASS (asb_package_rpm_parent_class)->finalize (object);
}

/**
 * asb_package_rpm_init:
 **/
static void
asb_package_rpm_init (AsbPackageRpm *pkg)
{
}

/**
 * asb_package_rpm_set_license:
 **/
static void
asb_package_rpm_set_license (AsbPackage *pkg, const gchar *license)
{
	guint i;
	guint j;
	_cleanup_free_ gchar *new = NULL;
	_cleanup_strv_free_ gchar **tokens = NULL;
	struct {
		const gchar	*fedora;
		const gchar	*spdx;
	} convert[] =  {
		{ "AGPLv3",			"AGPL-3.0" },
		{ "AGPLv3+",			"AGPL-3.0" },
		{ "AGPLv3 with exceptions",	"AGPL-3.0" },
		{ "AGPLv3+ with exceptions",	"AGPL-3.0" },
		{ "Artistic 2.0",		"Artistic-2.0" },
		{ "Artistic",			"Artistic-1.0" },
		{ "Artistic clarified",		"Artistic-2.0" },
		{ "ASL 1.1",			"Apache-1.1" },
		{ "ASL 2.0",			"Apache-2.0" },
		{ "Boost",			"BSL-1.0" },
		{ "BSD",			"BSD-3-Clause" },
		{ "BSD with advertising",	"BSD-3-Clause" },
		{ "CC0",			"CC0-1.0" },
		{ "CC-BY",			"CC-BY-3.0" },
		{ "CC-BY-SA",			"CC-BY-SA-3.0" },
		{ "CDDL",			"CDDL-1.0" },
		{ "CeCILL-C",			"CECILL-C" },
		{ "CeCILL",			"CECILL-2.0" },
		{ "EPL",			"EPL-1.0" },
		{ "Free Art",			"ClArtistic" },
		{ "GFDL",			"GFDL-1.3" },
		{ "GPL+",			"GPL-1.0+" },
		{ "GPLv2",			"GPL-2.0" },
		{ "GPLv2+",			"GPL-2.0+" },
		{ "GPLV2",			"GPL-2.0" },
		{ "GPLv2 with exceptions",	"GPL-2.0-with-font-exception" },
		{ "GPLv2+ with exceptions",	"GPL-2.0-with-font-exception" },
		{ "GPLv3",			"GPL-3.0" },
		{ "GPLv3+",			"GPL-3.0+" },
		{ "GPLV3+",			"GPL-3.0+" },
		{ "GPLv3+ with exceptions",	"GPL-3.0+" },
		{ "GPLv3 with exceptions",	"GPL-3.0-with-GCC-exception" },
		{ "GPL+ with exceptions",	"GPL-2.0-with-font-exception" },
		{ "IBM",			"IPL-1.0" },
		{ "LGPL+",			"LGPL-2.1+" },
		{ "LGPLv2.1",			"LGPL-2.1" },
		{ "LGPLv2",			"LGPL-2.1" },
		{ "LGPLv2+",			"LGPL-2.1+" },
		{ "LGPLv2 with exceptions",	"LGPL-2.0" },
		{ "LGPLv2+ with exceptions",	"LGPL-2.0+" },
		{ "LGPLv3",			"LGPL-3.0" },
		{ "LGPLv3+",			"LGPL-3.0+" },
		{ "LPPL",			"LPPL-1.3c" },
		{ "MIT with advertising",	"MIT" },
		{ "MPLv1.0",			"MPL-1.0" },
		{ "MPLv1.1",			"MPL-1.1" },
		{ "MPLv2.0",			"MPL-2.0" },
		{ "Netscape",			"NPL-1.1" },
		{ "OFL",			"OFL-1.1" },
		{ "Python",			"Python-2.0" },
		{ "QPL",			"QPL-1.0" },
		{ "QPL with exceptions",	"QPL-1.0" },
		{ "SPL",			"SPL-1.0" },
		{ "zlib",			"Zlib" },
		{ "ZPLv2.0",			"ZPL-2.0" },
		{ "Unlicense",			"CC0-1.0" },
		{ NULL, NULL } };

	/* this isn't supposed to happen */
	if (license == NULL) {
		asb_package_log (pkg, ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "no license!");
		return;
	}

	/* tokenize the license string and try to convert the Fedora license
	 * string to a SPDX license the best we can */
	tokens = as_utils_spdx_license_tokenize (license);
	for (i = 0; tokens[i] != NULL; i++) {

		/* ignore */
		if (tokens[i][0] == '(' ||
		    tokens[i][0] == ')' ||
		    tokens[i][0] == '&' ||
		    tokens[i][0] == '|')
			continue;

		/* already SPDX */
		if (tokens[i][0] == '@')
			continue;

		/* convert */
		for (j = 0; convert[j].fedora != NULL; j++) {
			if (g_strcmp0 (tokens[i], convert[j].fedora) == 0) {
				g_free (tokens[i]);
				tokens[i] = g_strdup_printf ("@%s", convert[j].spdx);
				asb_package_log (pkg,
						 ASB_PACKAGE_LOG_LEVEL_DEBUG,
						 "Converting Fedora license "
						 "'%s' to SPDX '%s'",
						 convert[j].fedora,
						 convert[j].spdx);
				break;
			}
		}
		if (convert[j].fedora != NULL)
			continue;

		/* no matching SPDX entry */
		asb_package_log (pkg,
				 ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "Unable to currently map Fedora "
				 "license '%s' to SPDX", tokens[i]);
	}
	new = as_utils_spdx_license_detokenize (tokens);
	asb_package_set_license (pkg, new);
}

/**
 * asb_package_rpm_set_source:
 **/
static void
asb_package_rpm_set_source (AsbPackage *pkg, const gchar *source)
{
	gchar *tmp;
	_cleanup_free_ gchar *srcrpm = NULL;

	/* this isn't supposed to happen */
	if (source == NULL) {
		asb_package_log (pkg, ASB_PACKAGE_LOG_LEVEL_WARNING,
				 "no source!");
		return;
	}
	srcrpm = g_strdup (source);
	tmp = g_strstr_len (srcrpm, -1, ".src.rpm");
	if (tmp != NULL)
		*tmp = '\0';
	asb_package_set_source (pkg, srcrpm);

	/* get the srpm name */
	tmp = g_strrstr (srcrpm, "-");
	if (tmp != NULL)
		*tmp = '\0';
	tmp = g_strrstr (srcrpm, "-");
	if (tmp != NULL)
		*tmp = '\0';
	asb_package_set_source_pkgname (pkg, srcrpm);
}

/**
 * asb_package_rpm_ensure_nevra:
 **/
static gboolean
asb_package_rpm_ensure_nevra (AsbPackage *pkg, GError **error)
{
	AsbPackageRpm *pkg_rpm = ASB_PACKAGE_RPM (pkg);
	AsbPackageRpmPrivate *priv = GET_PRIVATE (pkg_rpm);
	rpmtd td;

	td = rpmtdNew ();
	headerGet (priv->h, RPMTAG_NAME, td, HEADERGET_MINMEM);
	asb_package_set_name (pkg, rpmtdGetString (td));
	headerGet (priv->h, RPMTAG_VERSION, td, HEADERGET_MINMEM);
	asb_package_set_version (pkg, rpmtdGetString (td));
	headerGet (priv->h, RPMTAG_RELEASE, td, HEADERGET_MINMEM);
	asb_package_set_release (pkg, rpmtdGetString (td));
	headerGet (priv->h, RPMTAG_ARCH, td, HEADERGET_MINMEM);
	asb_package_set_arch (pkg, rpmtdGetString (td));
	headerGet (priv->h, RPMTAG_EPOCH, td, HEADERGET_MINMEM);
	asb_package_set_epoch (pkg, rpmtdGetNumber (td));
	rpmtdFree (td);
	return TRUE;
}

/**
 * asb_package_rpm_ensure_source:
 **/
static gboolean
asb_package_rpm_ensure_source (AsbPackage *pkg, GError **error)
{
	AsbPackageRpm *pkg_rpm = ASB_PACKAGE_RPM (pkg);
	AsbPackageRpmPrivate *priv = GET_PRIVATE (pkg_rpm);
	rpmtd td;

	td = rpmtdNew ();
	headerGet (priv->h, RPMTAG_SOURCERPM, td, HEADERGET_MINMEM);
	asb_package_rpm_set_source (pkg, rpmtdGetString (td));
	rpmtdFree (td);
	return TRUE;
}

/**
 * asb_package_rpm_ensure_url:
 **/
static gboolean
asb_package_rpm_ensure_url (AsbPackage *pkg, GError **error)
{
	AsbPackageRpm *pkg_rpm = ASB_PACKAGE_RPM (pkg);
	AsbPackageRpmPrivate *priv = GET_PRIVATE (pkg_rpm);
	rpmtd td;

	td = rpmtdNew ();
	headerGet (priv->h, RPMTAG_URL, td, HEADERGET_MINMEM);
	asb_package_set_url (pkg, rpmtdGetString (td));
	rpmtdFree (td);
	return TRUE;
}

/**
 * asb_package_rpm_ensure_vcs:
 **/
static gboolean
asb_package_rpm_ensure_vcs (AsbPackage *pkg, GError **error)
{
	AsbPackageRpm *pkg_rpm = ASB_PACKAGE_RPM (pkg);
	AsbPackageRpmPrivate *priv = GET_PRIVATE (pkg_rpm);
	rpmtd td;

	td = rpmtdNew ();
	headerGet (priv->h, RPMTAG_VCS, td, HEADERGET_MINMEM);
	asb_package_set_vcs (pkg, rpmtdGetString (td));
	rpmtdFree (td);
	return TRUE;
}

/**
 * asb_package_rpm_ensure_license:
 **/
static gboolean
asb_package_rpm_ensure_license (AsbPackage *pkg, GError **error)
{
	AsbPackageRpm *pkg_rpm = ASB_PACKAGE_RPM (pkg);
	AsbPackageRpmPrivate *priv = GET_PRIVATE (pkg_rpm);
	rpmtd td;

	td = rpmtdNew ();
	headerGet (priv->h, RPMTAG_LICENSE, td, HEADERGET_MINMEM);
	asb_package_rpm_set_license (pkg, rpmtdGetString (td));
	rpmtdFree (td);
	return TRUE;
}

/**
 * asb_package_rpm_add_release:
 **/
static void
asb_package_rpm_add_release (AsbPackage *pkg,
			     guint64 timestamp,
			     const gchar *name,
			     const gchar *text)
{
	AsRelease *release;
	const gchar *version;
	gchar *tmp;
	gchar *vr;
	_cleanup_free_ gchar *name_dup = NULL;

	/* get last string chunk */
	name_dup = g_strchomp (g_strdup (name));
	vr = g_strrstr (name_dup, " ");
	if (vr == NULL)
		return;

	/* get last string chunk */
	version = vr + 1;

	/* ignore version-less dashed email address, e.g.
	 * 'Fedora Release Engineering <rel-eng@lists.fedoraproject.org>' */
	if (g_strstr_len (version, -1, "@") != NULL ||
	    g_strstr_len (version, -1, "<") != NULL ||
	    g_strstr_len (version, -1, ">") != NULL)
		return;

	tmp = g_strrstr_len (version, -1, "-");
	if (tmp != NULL)
		*tmp = '\0';

	/* remove any epoch */
	tmp = g_strstr_len (version, -1, ":");
	if (tmp != NULL)
		version = tmp + 1;

	/* is version already in the database */
	release = asb_package_get_release (pkg, version);
	if (release != NULL) {
		/* use the earlier timestamp to ignore auto-rebuilds with just
		 * a bumped release */
		if (timestamp < as_release_get_timestamp (release))
			as_release_set_timestamp (release, timestamp);
	} else {
		release = as_release_new ();
		as_release_set_version (release, version, -1);
		as_release_set_timestamp (release, timestamp);
		asb_package_add_release (pkg, version, release);
		g_object_unref (release);
	}
}

/**
 * asb_package_rpm_ensure_releases:
 **/
static gboolean
asb_package_rpm_ensure_releases (AsbPackage *pkg, GError **error)
{
	AsbPackageRpm *pkg_rpm = ASB_PACKAGE_RPM (pkg);
	AsbPackageRpmPrivate *priv = GET_PRIVATE (pkg_rpm);
	guint i;
	rpmtd td[3] = { NULL, NULL, NULL };

	/* read out the file list */
	for (i = 0; i < 3; i++)
		td[i] = rpmtdNew ();
	/* get the ChangeLog info */
	headerGet (priv->h, RPMTAG_CHANGELOGTIME, td[0], HEADERGET_MINMEM);
	headerGet (priv->h, RPMTAG_CHANGELOGNAME, td[1], HEADERGET_MINMEM);
	headerGet (priv->h, RPMTAG_CHANGELOGTEXT, td[2], HEADERGET_MINMEM);
	while (rpmtdNext (td[0]) != -1 &&
	       rpmtdNext (td[1]) != -1 &&
	       rpmtdNext (td[2]) != -1) {
		asb_package_rpm_add_release (pkg,
					     rpmtdGetNumber (td[0]),
					     rpmtdGetString (td[1]),
					     rpmtdGetString (td[2]));
	}
	for (i = 0; i < 3; i++) {
		rpmtdFreeData (td[i]);
		rpmtdFree (td[i]);
	}
	return TRUE;
}

/**
 * asb_package_rpm_ensure_deps:
 **/
static gboolean
asb_package_rpm_ensure_deps (AsbPackage *pkg, GError **error)
{
	AsbPackageRpm *pkg_rpm = ASB_PACKAGE_RPM (pkg);
	AsbPackageRpmPrivate *priv = GET_PRIVATE (pkg_rpm);
	const gchar *dep;
	gboolean ret = TRUE;
	gchar *tmp;
	gint rc;
	rpmtd td = NULL;

	/* read out the dep list */
	td = rpmtdNew ();
	rc = headerGet (priv->h, RPMTAG_REQUIRENAME, td, HEADERGET_MINMEM);
	if (!rc) {
		ret = FALSE;
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "Failed to read list of requires %s",
			     asb_package_get_filename (pkg));
		goto out;
	}
	while (rpmtdNext (td) != -1) {
		_cleanup_free_ gchar *dep_no_qual = NULL;
		dep = rpmtdGetString (td);
		if (g_str_has_prefix (dep, "rpmlib"))
			continue;
		if (g_strcmp0 (dep, "/bin/sh") == 0)
			continue;
		dep_no_qual = g_strdup (dep);
		tmp = g_strstr_len (dep_no_qual, -1, "(");
		if (tmp != NULL)
			*tmp = '\0';
		asb_package_add_dep (pkg, dep_no_qual);
	}
out:
	rpmtdFreeData (td);
	rpmtdFree (td);
	return ret;
}

/**
 * asb_package_rpm_ensure_filelists:
 **/
static gboolean
asb_package_rpm_ensure_filelists (AsbPackage *pkg, GError **error)
{
	AsbPackageRpm *pkg_rpm = ASB_PACKAGE_RPM (pkg);
	AsbPackageRpmPrivate *priv = GET_PRIVATE (pkg_rpm);
	gboolean ret = TRUE;
	gint rc;
	guint i;
	rpmtd td[3] = { NULL, NULL, NULL };
	_cleanup_free_ const gchar **dirnames = NULL;
	_cleanup_free_ gint32 *dirindex = NULL;
	_cleanup_strv_free_ gchar **filelist = NULL;

	/* is a virtual package with no files */
	if (!headerIsEntry (priv->h, RPMTAG_DIRINDEXES))
		return TRUE;

	/* read out the file list */
	for (i = 0; i < 3; i++)
		td[i] = rpmtdNew ();
	rc = headerGet (priv->h, RPMTAG_DIRNAMES, td[0], HEADERGET_MINMEM);
	if (rc)
		rc = headerGet (priv->h, RPMTAG_BASENAMES, td[1], HEADERGET_MINMEM);
	if (rc)
		rc = headerGet (priv->h, RPMTAG_DIRINDEXES, td[2], HEADERGET_MINMEM);
	if (!rc) {
		ret = FALSE;
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "Failed to read package file list %s",
			     asb_package_get_filename (pkg));
		goto out;
	}
	i = 0;
	dirnames = g_new0 (const gchar *, rpmtdCount (td[0]) + 1);
	while (rpmtdNext (td[0]) != -1)
		dirnames[i++] = rpmtdGetString (td[0]);
	i = 0;
	dirindex = g_new0 (gint32, rpmtdCount (td[2]) + 1);
	while (rpmtdNext (td[2]) != -1)
		dirindex[i++] = rpmtdGetNumber (td[2]);
	i = 0;
	filelist = g_new0 (gchar *, rpmtdCount (td[1]) + 1);
	while (rpmtdNext (td[1]) != -1) {
		filelist[i] = g_build_filename (dirnames[dirindex[i]],
						rpmtdGetString (td[1]),
						NULL);
		i++;
	}
	asb_package_set_filelist (pkg, filelist);
out:
	for (i = 0; i < 3; i++) {
		rpmtdFreeData (td[i]);
		rpmtdFree (td[i]);
	}
	return ret;
}

/**
 * asb_package_rpm_strerror:
 **/
static const gchar *
asb_package_rpm_strerror (rpmRC rc)
{
	const gchar *str;
	switch (rc) {
	case RPMRC_OK:
		str = "Generic success";
		break;
	case RPMRC_NOTFOUND:
		str = "Generic not found";
		break;
	case RPMRC_FAIL:
		str = "Generic failure";
		break;
	case RPMRC_NOTTRUSTED:
		str = "Signature is OK, but key is not trusted";
		break;
	case RPMRC_NOKEY:
		str = "Public key is unavailable";
		break;
	default:
		str = "unknown";
		break;
	}
	return str;
}

/**
 * asb_package_rpm_open:
 **/
static gboolean
asb_package_rpm_open (AsbPackage *pkg, const gchar *filename, GError **error)
{
	AsbPackageRpm *pkg_rpm = ASB_PACKAGE_RPM (pkg);
	AsbPackageRpmPrivate *priv = GET_PRIVATE (pkg_rpm);
	FD_t fd;
	gboolean ret = TRUE;
	rpmRC rc;
	rpmts ts;

	/* open the file */
	ts = rpmtsCreate ();
	rpmtsSetVSFlags (ts, _RPMVSF_NODIGESTS | _RPMVSF_NOSIGNATURES);
	fd = Fopen (filename, "r");
	if (fd <= 0) {
		ret = FALSE;
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "Failed to open package %s", filename);
		goto out;
	}

	/* create package */
	rc = rpmReadPackageFile (ts, fd, filename, &priv->h);
	if (rc == RPMRC_FAIL) {
		ret = FALSE;
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "Failed to read package %s: %s",
			     filename, asb_package_rpm_strerror (rc));
		goto out;
	}

	/* read package stuff */
	ret = asb_package_rpm_ensure_nevra (pkg, error);
	if (!ret)
		goto out;
out:
	rpmtsFree (ts);
	Fclose (fd);
	return ret;
}

/**
 * asb_package_rpm_ensure:
 **/
static gboolean
asb_package_rpm_ensure (AsbPackage *pkg,
			AsbPackageEnsureFlags flags,
			GError **error)
{
	if ((flags & ASB_PACKAGE_ENSURE_NEVRA) > 0) {
		if (!asb_package_rpm_ensure_nevra (pkg, error))
			return FALSE;
	}
	if ((flags & ASB_PACKAGE_ENSURE_DEPS) > 0) {
		if (!asb_package_rpm_ensure_deps (pkg, error))
			return FALSE;
	}
	if ((flags & ASB_PACKAGE_ENSURE_RELEASES) > 0) {
		if (!asb_package_rpm_ensure_releases (pkg, error))
			return FALSE;
	}
	if ((flags & ASB_PACKAGE_ENSURE_FILES) > 0) {
		if (!asb_package_rpm_ensure_filelists (pkg, error))
			return FALSE;
	}
	if ((flags & ASB_PACKAGE_ENSURE_LICENSE) > 0) {
		if (!asb_package_rpm_ensure_license (pkg, error))
			return FALSE;
	}
	if ((flags & ASB_PACKAGE_ENSURE_URL) > 0) {
		if (!asb_package_rpm_ensure_url (pkg, error))
			return FALSE;
	}
	if ((flags & ASB_PACKAGE_ENSURE_SOURCE) > 0) {
		if (!asb_package_rpm_ensure_source (pkg, error))
			return FALSE;
	}
	if ((flags & ASB_PACKAGE_ENSURE_VCS) > 0) {
		if (!asb_package_rpm_ensure_vcs (pkg, error))
			return FALSE;
	}
	return TRUE;
}

/**
 * asb_package_rpm_compare:
 **/
static gint
asb_package_rpm_compare (AsbPackage *pkg1, AsbPackage *pkg2)
{
	return rpmvercmp (asb_package_get_evr (pkg1),
			  asb_package_get_evr (pkg2));
}

/**
 * asb_package_rpm_class_init:
 **/
static void
asb_package_rpm_class_init (AsbPackageRpmClass *klass)
{
	AsbPackageClass *package_class = ASB_PACKAGE_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = asb_package_rpm_finalize;
	package_class->open = asb_package_rpm_open;
	package_class->ensure = asb_package_rpm_ensure;
	package_class->compare = asb_package_rpm_compare;
}

/**
 * asb_package_rpm_init_cb:
 **/
static gpointer
asb_package_rpm_init_cb (gpointer user_data)
{
	rpmReadConfigFiles (NULL, NULL);
	return NULL;
}

/**
 * asb_package_rpm_new:
 *
 * Creates a new RPM package.
 *
 * Returns: a package
 *
 * Since: 0.1.0
 **/
AsbPackage *
asb_package_rpm_new (void)
{
	AsbPackage *pkg;
	static GOnce rpm_init = G_ONCE_INIT;
	g_once (&rpm_init, asb_package_rpm_init_cb, NULL);
	pkg = g_object_new (ASB_TYPE_PACKAGE_RPM, NULL);
	return ASB_PACKAGE (pkg);
}
