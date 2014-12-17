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
 * SECTION:asb-package
 * @short_description: Object representing a package file.
 * @stability: Unstable
 *
 * This object represents one package file.
 */

#include "config.h"

#include <limits.h>

#include "as-cleanup.h"
#include "asb-package.h"
#include "asb-plugin.h"

typedef struct _AsbPackagePrivate	AsbPackagePrivate;
struct _AsbPackagePrivate
{
	gboolean	 enabled;
	gchar		**filelist;
	gchar		**deps;
	gchar		*filename;
	gchar		*basename;
	gchar		*name;
	guint		 epoch;
	gchar		*version;
	gchar		*release;
	gchar		*arch;
	gchar		*url;
	gchar		*nevr;
	gchar		*nevra;
	gchar		*evr;
	gchar		*license;
	gchar		*vcs;
	gchar		*source_nevra;
	gchar		*source_pkgname;
	gsize		 log_written_len;
	GString		*log;
	GHashTable	*configs;
	GTimer		*timer;
	gdouble		 last_log;
	GPtrArray	*releases;
	GHashTable	*releases_hash;
	GMutex		 mutex_log;
};

G_DEFINE_TYPE_WITH_PRIVATE (AsbPackage, asb_package, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (asb_package_get_instance_private (o))

/**
 * asb_package_finalize:
 **/
static void
asb_package_finalize (GObject *object)
{
	AsbPackage *pkg = ASB_PACKAGE (object);
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);

	g_mutex_clear (&priv->mutex_log);
	g_strfreev (priv->filelist);
	g_strfreev (priv->deps);
	g_free (priv->filename);
	g_free (priv->basename);
	g_free (priv->name);
	g_free (priv->version);
	g_free (priv->release);
	g_free (priv->arch);
	g_free (priv->url);
	g_free (priv->nevr);
	g_free (priv->nevra);
	g_free (priv->evr);
	g_free (priv->license);
	g_free (priv->vcs);
	g_free (priv->source_nevra);
	g_free (priv->source_pkgname);
	g_string_free (priv->log, TRUE);
	g_timer_destroy (priv->timer);
	g_hash_table_unref (priv->configs);
	g_ptr_array_unref (priv->releases);
	g_hash_table_unref (priv->releases_hash);

	G_OBJECT_CLASS (asb_package_parent_class)->finalize (object);
}

/**
 * asb_package_init:
 **/
static void
asb_package_init (AsbPackage *pkg)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	priv->enabled = TRUE;
	priv->log = g_string_sized_new (1024);
	priv->timer = g_timer_new ();
	priv->configs = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	priv->releases = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->releases_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
						     g_free, (GDestroyNotify) g_object_unref);
	g_mutex_init (&priv->mutex_log);
}

/**
 * asb_package_get_enabled:
 * @pkg: A #AsbPackage
 *
 * Gets if the package is enabled.
 *
 * Returns: enabled status
 *
 * Since: 0.1.0
 **/
gboolean
asb_package_get_enabled (AsbPackage *pkg)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	return priv->enabled;
}

/**
 * asb_package_set_enabled:
 * @pkg: A #AsbPackage
 * @enabled: boolean
 *
 * Enables or disables the package.
 *
 * Since: 0.1.0
 **/
void
asb_package_set_enabled (AsbPackage *pkg, gboolean enabled)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	priv->enabled = enabled;
}

/**
 * asb_package_log_start:
 * @pkg: A #AsbPackage
 *
 * Starts the log timer.
 *
 * Since: 0.1.0
 **/
void
asb_package_log_start (AsbPackage *pkg)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	g_timer_reset (priv->timer);
}

/**
 * asb_package_log:
 * @pkg: A #AsbPackage
 * @log_level: a #AsbPackageLogLevel
 * @fmt: Format string
 * @...: varargs
 *
 * Logs a message.
 *
 * Since: 0.1.0
 **/
void
asb_package_log (AsbPackage *pkg,
		 AsbPackageLogLevel log_level,
		 const gchar *fmt, ...)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	va_list args;
	gdouble now;
	_cleanup_free_ gchar *tmp = NULL;

	g_mutex_lock (&priv->mutex_log);

	va_start (args, fmt);
	tmp = g_strdup_vprintf (fmt, args);
	va_end (args);
	if (g_getenv ("ASB_PROFILE") != NULL) {
		now = g_timer_elapsed (priv->timer, NULL) * 1000;
		g_string_append_printf (priv->log,
					"%05.0f\t+%05.0f\t",
					now, now - priv->last_log);
		priv->last_log = now;
	}
	switch (log_level) {
	case ASB_PACKAGE_LOG_LEVEL_INFO:
		g_debug ("INFO:    %s", tmp);
		g_string_append_printf (priv->log, "INFO:    %s\n", tmp);
		break;
	case ASB_PACKAGE_LOG_LEVEL_DEBUG:
		g_debug ("DEBUG:   %s", tmp);
		if (g_getenv ("ASB_PROFILE") != NULL)
			g_string_append_printf (priv->log, "DEBUG:   %s\n", tmp);
		break;
	case ASB_PACKAGE_LOG_LEVEL_WARNING:
		g_debug ("WARNING: %s", tmp);
		g_string_append_printf (priv->log, "WARNING: %s\n", tmp);
		break;
	default:
		g_debug ("%s", tmp);
		g_string_append_printf (priv->log, "%s\n", tmp);
		break;
	}
	g_mutex_unlock (&priv->mutex_log);
}

/**
 * asb_package_log_flush:
 * @pkg: A #AsbPackage
 * @error: A #GError or %NULL
 *
 * Flushes the log queue.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_package_log_flush (AsbPackage *pkg, GError **error)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	_cleanup_free_ gchar *logfile = NULL;
	_cleanup_free_ gchar *logdir_char = NULL;

	/* needs no update */
	if (priv->log_written_len == priv->log->len)
		return TRUE;

	/* don't write if unset */
	if (asb_package_get_config (pkg, "LogDir") == NULL)
		return TRUE;

	/* overwrite old log */
	logdir_char = g_strdup_printf ("%s/%c",
				       asb_package_get_config (pkg, "LogDir"),
				       g_ascii_tolower (priv->name[0]));
	if (!asb_utils_ensure_exists (logdir_char, error))
		return FALSE;
	priv->log_written_len = priv->log->len;
	logfile = g_strdup_printf ("%s/%s.log", logdir_char, priv->name);
	return g_file_set_contents (logfile, priv->log->str, -1, error);
}

/**
 * asb_package_get_filename:
 * @pkg: A #AsbPackage
 *
 * Gets the filename of the package.
 *
 * Returns: utf8 filename
 *
 * Since: 0.1.0
 **/
const gchar *
asb_package_get_filename (AsbPackage *pkg)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	return priv->filename;
}

/**
 * asb_package_get_basename:
 * @pkg: A #AsbPackage
 *
 * Gets the package basename.
 *
 * Returns: utf8 string
 *
 * Since: 0.1.0
 **/
const gchar *
asb_package_get_basename (AsbPackage *pkg)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	return priv->basename;
}

/**
 * asb_package_get_name:
 * @pkg: A #AsbPackage
 *
 * Gets the package name
 *
 * Returns: utf8 string
 *
 * Since: 0.1.0
 **/
const gchar *
asb_package_get_name (AsbPackage *pkg)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	return priv->name;
}

/**
 * asb_package_get_arch:
 * @pkg: A #AsbPackage
 *
 * Gets the package architecture
 *
 * Returns: utf8 string
 *
 * Since: 0.3.0
 **/
const gchar *
asb_package_get_arch (AsbPackage *pkg)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	return priv->arch;
}

/**
 * asb_package_get_url:
 * @pkg: A #AsbPackage
 *
 * Gets the package homepage URL
 *
 * Returns: utf8 string
 *
 * Since: 0.1.0
 **/
const gchar *
asb_package_get_url (AsbPackage *pkg)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	return priv->url;
}

/**
 * asb_package_get_license:
 * @pkg: A #AsbPackage
 *
 * Gets the package license.
 *
 * Returns: utf8 string
 *
 * Since: 0.1.0
 **/
const gchar *
asb_package_get_license (AsbPackage *pkg)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	return priv->license;
}

/**
 * asb_package_get_vcs:
 * @pkg: A #AsbPackage
 *
 * Gets the package version control system.
 *
 * Returns: utf8 string
 *
 * Since: 0.3.4
 **/
const gchar *
asb_package_get_vcs (AsbPackage *pkg)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	return priv->vcs;
}

/**
 * asb_package_get_source:
 * @pkg: A #AsbPackage
 *
 * Gets the package source nevra.
 *
 * Returns: utf8 string
 *
 * Since: 0.1.0
 **/
const gchar *
asb_package_get_source (AsbPackage *pkg)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	return priv->source_nevra;
}

/**
 * asb_package_get_source_pkgname:
 * @pkg: A #AsbPackage
 *
 * Gets the package source name.
 *
 * Returns: utf8 string
 *
 * Since: 0.2.4
 **/
const gchar *
asb_package_get_source_pkgname (AsbPackage *pkg)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	return priv->source_pkgname;
}

/**
 * asb_package_get_filelist:
 * @pkg: A #AsbPackage
 *
 * Gets the package filelist.
 *
 * Returns: (transfer none) (element-type utf8): filelist
 *
 * Since: 0.1.0
 **/
gchar **
asb_package_get_filelist (AsbPackage *pkg)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	return priv->filelist;
}

/**
 * asb_package_get_deps:
 * @pkg: A #AsbPackage
 *
 * Get the package dependancy list.
 *
 * Returns: (transfer none) (element-type utf8): deplist
 *
 * Since: 0.1.0
 **/
gchar **
asb_package_get_deps (AsbPackage *pkg)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	return priv->deps;
}

/**
 * asb_package_set_name:
 * @pkg: A #AsbPackage
 * @name: package name
 *
 * Sets the package name.
 *
 * Since: 0.1.0
 **/
void
asb_package_set_name (AsbPackage *pkg, const gchar *name)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	g_free (priv->name);
	priv->name = g_strdup (name);
}

/**
 * asb_package_set_version:
 * @pkg: A #AsbPackage
 * @version: package version
 *
 * Sets the package version.
 *
 * Since: 0.1.0
 **/
void
asb_package_set_version (AsbPackage *pkg, const gchar *version)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	g_free (priv->version);
	priv->version = g_strdup (version);
}

/**
 * asb_package_set_release:
 * @pkg: A #AsbPackage
 * @release: package release
 *
 * Sets the package release.
 *
 * Since: 0.1.0
 **/
void
asb_package_set_release (AsbPackage *pkg, const gchar *release)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	g_free (priv->release);
	priv->release = g_strdup (release);
}

/**
 * asb_package_set_arch:
 * @pkg: A #AsbPackage
 * @arch: package architecture
 *
 * Sets the package architecture.
 *
 * Since: 0.1.0
 **/
void
asb_package_set_arch (AsbPackage *pkg, const gchar *arch)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	g_free (priv->arch);
	priv->arch = g_strdup (arch);
}

/**
 * asb_package_set_epoch:
 * @pkg: A #AsbPackage
 * @epoch: epoch, or 0 for unset
 *
 * Sets the package epoch
 *
 * Since: 0.1.0
 **/
void
asb_package_set_epoch (AsbPackage *pkg, guint epoch)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	priv->epoch = epoch;
}

/**
 * asb_package_set_url:
 * @pkg: A #AsbPackage
 * @url: homepage URL
 *
 * Sets the package URL.
 *
 * Since: 0.1.0
 **/
void
asb_package_set_url (AsbPackage *pkg, const gchar *url)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	g_free (priv->url);
	priv->url = g_strdup (url);
}

/**
 * asb_package_set_license:
 * @pkg: A #AsbPackage
 * @license: license string
 *
 * Sets the package license.
 *
 * Since: 0.1.0
 **/
void
asb_package_set_license (AsbPackage *pkg, const gchar *license)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	g_free (priv->license);
	priv->license = g_strdup (license);
}

/**
 * asb_package_set_vcs:
 * @pkg: A #AsbPackage
 * @vcs: vcs string
 *
 * Sets the package version control system.
 *
 * Since: 0.3.4
 **/
void
asb_package_set_vcs (AsbPackage *pkg, const gchar *vcs)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	g_free (priv->vcs);
	priv->vcs = g_strdup (vcs);
}

/**
 * asb_package_set_source:
 * @pkg: A #AsbPackage
 * @source: source string, e.g. the srpm nevra
 *
 * Sets the package source name, which is usually the parent of a set of
 * subpackages.
 *
 * Since: 0.1.0
 **/
void
asb_package_set_source (AsbPackage *pkg, const gchar *source)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	g_free (priv->source_nevra);
	priv->source_nevra = g_strdup (source);
}

/**
 * asb_package_set_source_pkgname:
 * @pkg: A #AsbPackage
 * @source_pkgname: source string, e.g. the srpm name
 *
 * Sets the package source name, which is usually the parent of a set of
 * subpackages.
 *
 * Since: 0.2.4
 **/
void
asb_package_set_source_pkgname (AsbPackage *pkg, const gchar *source_pkgname)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	g_free (priv->source_pkgname);
	priv->source_pkgname = g_strdup (source_pkgname);
}

/**
 * asb_package_set_deps:
 * @pkg: A #AsbPackage
 * @deps: package deps
 *
 * Sets the package dependancies.
 *
 * Since: 0.1.0
 **/
void
asb_package_set_deps (AsbPackage *pkg, gchar **deps)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	g_strfreev (priv->deps);
	priv->deps = g_strdupv (deps);
}

/**
 * asb_package_set_filelist:
 * @pkg: A #AsbPackage
 * @filelist: package filelist
 *
 * Sets the package filelist.
 *
 * Since: 0.1.0
 **/
void
asb_package_set_filelist (AsbPackage *pkg, gchar **filelist)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	g_strfreev (priv->filelist);
	priv->filelist = g_strdupv (filelist);
}

/**
 * asb_package_get_nevr:
 * @pkg: A #AsbPackage
 *
 * Gets the package NEVR.
 *
 * Returns: utf8 string
 *
 * Since: 0.1.0
 **/
const gchar *
asb_package_get_nevr (AsbPackage *pkg)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	if (priv->nevr == NULL) {
		if (priv->epoch == 0) {
			priv->nevr = g_strdup_printf ("%s-%s-%s",
						      priv->name,
						      priv->version,
						      priv->release);
		} else {
			priv->nevr = g_strdup_printf ("%s-%i:%s-%s",
						      priv->name,
						      priv->epoch,
						      priv->version,
						      priv->release);
		}
	}
	return priv->nevr;
}

/**
 * asb_package_get_nevra:
 * @pkg: A #AsbPackage
 *
 * Gets the package NEVRA.
 *
 * Returns: utf8 string
 *
 * Since: 0.3.0
 **/
const gchar *
asb_package_get_nevra (AsbPackage *pkg)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	if (priv->nevra == NULL) {
		if (priv->epoch == 0) {
			priv->nevra = g_strdup_printf ("%s-%s-%s.%s",
						       priv->name,
						       priv->version,
						       priv->release,
						       priv->arch);
		} else {
			priv->nevra = g_strdup_printf ("%s-%i:%s-%s.%s",
						       priv->name,
						       priv->epoch,
						       priv->version,
						       priv->release,
						       priv->arch);
		}
	}
	return priv->nevra;
}

/**
 * asb_package_get_evr:
 * @pkg: A #AsbPackage
 *
 * Gets the package EVR.
 *
 * Returns: utf8 string
 *
 * Since: 0.1.0
 **/
const gchar *
asb_package_get_evr (AsbPackage *pkg)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	if (priv->evr == NULL) {
		if (priv->epoch == 0) {
			priv->evr = g_strdup_printf ("%s-%s",
						     priv->version,
						     priv->release);
		} else {
			priv->evr = g_strdup_printf ("%i:%s-%s",
						     priv->epoch,
						     priv->version,
						     priv->release);
		}
	}
	return priv->evr;
}

/**
 * asb_package_class_init:
 **/
static void
asb_package_class_init (AsbPackageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asb_package_finalize;
}

/**
 * asb_package_open:
 * @pkg: A #AsbPackage
 * @filename: package filename
 * @error: A #GError or %NULL
 *
 * Opens a package and parses the contents.
 * As little i/o should be done at this point, and implementations
 * should rely on asb_package_ensure() to set data.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_package_open (AsbPackage *pkg, const gchar *filename, GError **error)
{
	AsbPackageClass *klass = ASB_PACKAGE_GET_CLASS (pkg);
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);

	/* cache here */
	priv->filename = g_strdup (filename);
	priv->basename = g_path_get_basename (filename);

	/* call distro-specific method */
	if (klass->open != NULL)
		return klass->open (pkg, filename, error);
	return TRUE;
}

/**
 * asb_package_ensure:
 * @pkg: A #AsbPackage
 * @flags: #AsbPackageEnsureFlags
 * @error: A #GError or %NULL
 *
 * Ensures data exists.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.3.0
 **/
gboolean
asb_package_ensure (AsbPackage *pkg,
		    AsbPackageEnsureFlags flags,
		    GError **error)
{
	AsbPackageClass *klass = ASB_PACKAGE_GET_CLASS (pkg);
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);

	/* clear flags */
	if (priv->name != NULL)
		flags &= ~ASB_PACKAGE_ENSURE_NEVRA;
	if (priv->license != NULL)
		flags &= ~ASB_PACKAGE_ENSURE_LICENSE;
	if (priv->vcs != NULL)
		flags &= ~ASB_PACKAGE_ENSURE_VCS;
	if (priv->url != NULL)
		flags &= ~ASB_PACKAGE_ENSURE_URL;
	if (priv->source_pkgname != NULL)
		flags &= ~ASB_PACKAGE_ENSURE_SOURCE;
	if (priv->filelist != NULL)
		flags &= ~ASB_PACKAGE_ENSURE_FILES;
	if (priv->deps != NULL)
		flags &= ~ASB_PACKAGE_ENSURE_DEPS;
	if (priv->releases->len > 0)
		flags &= ~ASB_PACKAGE_ENSURE_RELEASES;

	/* call distro-specific method */
	if (klass->ensure != NULL)
		return klass->ensure (pkg, flags, error);
	return TRUE;
}

/**
 * asb_package_explode:
 * @pkg: A #AsbPackage
 * @dir: directory to explode into
 * @glob: (element-type utf8): the glob list, or %NULL
 * @error: A #GError or %NULL
 *
 * Decompresses a package into a directory, optionally using a glob list.
 *
 * Returns: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.1.0
 **/
gboolean
asb_package_explode (AsbPackage *pkg,
		     const gchar *dir,
		     GPtrArray *glob,
		     GError **error)
{
	AsbPackageClass *klass = ASB_PACKAGE_GET_CLASS (pkg);
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	if (klass->explode != NULL)
		return klass->explode (pkg, dir, glob, error);
	return asb_utils_explode (priv->filename, dir, glob, error);
}

/**
 * asb_package_set_config:
 * @pkg: A #AsbPackage
 * @key: utf8 string
 * @value: utf8 string
 *
 * Sets a config attribute on a package.
 *
 * Since: 0.1.0
 **/
void
asb_package_set_config (AsbPackage *pkg, const gchar *key, const gchar *value)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	g_hash_table_insert (priv->configs, g_strdup (key), g_strdup (value));
}

/**
 * asb_package_get_config:
 * @pkg: A #AsbPackage
 * @key: utf8 string
 *
 * Gets a config attribute from a package.
 *
 * Returns: utf8 string
 *
 * Since: 0.1.0
 **/
const gchar *
asb_package_get_config (AsbPackage *pkg, const gchar *key)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	return g_hash_table_lookup (priv->configs, key);
}

/**
 * asb_package_get_releases:
 * @pkg: A #AsbPackage
 *
 * Gets the releases of the package.
 *
 * Returns: (transfer none) (element-type AsRelease): the release data
 *
 * Since: 0.1.0
 **/
GPtrArray *
asb_package_get_releases (AsbPackage *pkg)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	return priv->releases;
}

/**
 * asb_package_compare:
 * @pkg1: A #AsbPackage
 * @pkg2: A #AsbPackage
 *
 * Compares one package with another.
 *
 * Returns: -1 for <, 0 for the same and +1 for >
 *
 * Since: 0.1.0
 **/
gint
asb_package_compare (AsbPackage *pkg1, AsbPackage *pkg2)
{
	AsbPackageClass *klass = ASB_PACKAGE_GET_CLASS (pkg1);
	if (klass->compare != NULL)
		return klass->compare (pkg1, pkg2);
	return 0;
}

/**
 * asb_package_get_release:
 * @pkg: A #AsbPackage
 * @version: package version
 *
 * Gets the release for a specific version.
 *
 * Returns: (transfer none): an #AsRelease, or %NULL for not found
 *
 * Since: 0.1.0
 **/
AsRelease *
asb_package_get_release	(AsbPackage *pkg, const gchar *version)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	return g_hash_table_lookup (priv->releases_hash, version);
}

/**
 * asb_package_add_release:
 * @pkg: A #AsbPackage
 * @version: a package version
 * @release: a package release
 *
 * Adds a (downstream) release to a package.
 *
 * Since: 0.1.0
 **/
void
asb_package_add_release	(AsbPackage *pkg,
			 const gchar *version,
			 AsRelease *release)
{
	AsbPackagePrivate *priv = GET_PRIVATE (pkg);
	g_hash_table_insert (priv->releases_hash,
			     g_strdup (version),
			     g_object_ref (release));
	g_ptr_array_add (priv->releases, g_object_ref (release));
}

/**
 * asb_package_new:
 *
 * Creates a new %AsbPackage.
 *
 * You don't need to use this function unless you want a memory-backed package
 * for testing purposes.
 *
 * Returns: a package
 *
 * Since: 0.2.2
 **/
AsbPackage *
asb_package_new (void)
{
	AsbPackage *pkg;
	pkg = g_object_new (ASB_TYPE_PACKAGE, NULL);
	return ASB_PACKAGE (pkg);
}
