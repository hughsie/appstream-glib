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

G_BEGIN_DECLS

#define ASB_TYPE_PACKAGE (asb_package_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsbPackage, asb_package, ASB, PACKAGE, GObject)

#define ASB_PACKAGE_ENSURE_NONE		(0u)
#define ASB_PACKAGE_ENSURE_NEVRA	(1u << 0)
#define ASB_PACKAGE_ENSURE_FILES	(1u << 1)
#define ASB_PACKAGE_ENSURE_RELEASES	(1u << 2)
#define ASB_PACKAGE_ENSURE_DEPS		(1u << 3)
#define ASB_PACKAGE_ENSURE_LICENSE	(1u << 4)
#define ASB_PACKAGE_ENSURE_URL		(1u << 5)
#define ASB_PACKAGE_ENSURE_SOURCE	(1u << 6)
#define ASB_PACKAGE_ENSURE_VCS		(1u << 7)
typedef guint64 AsbPackageEnsureFlags;

struct _AsbPackageClass
{
	GObjectClass		parent_class;
	gboolean		 (*open)	(AsbPackage	*pkg,
						 const gchar	*filename,
						 GError		**error);
	gboolean		 (*ensure)	(AsbPackage	*pkg,
						 AsbPackageEnsureFlags flags,
						 GError		**error);
	gboolean		 (*explode)	(AsbPackage	*pkg,
						 const gchar	*dir,
						 GPtrArray	*glob,
						 GError		**error);
	gint			 (*compare)	(AsbPackage	*pkg1,
						 AsbPackage	*pkg2);
	gboolean		 (*close)	(AsbPackage	*pkg,
						 GError		**error);
	/*< private >*/
	void (*_asb_reserved1)	(void);
	void (*_asb_reserved2)	(void);
	void (*_asb_reserved3)	(void);
	void (*_asb_reserved4)	(void);
	void (*_asb_reserved5)	(void);
	void (*_asb_reserved6)	(void);
	void (*_asb_reserved7)	(void);
};

typedef enum {
	ASB_PACKAGE_LOG_LEVEL_NONE,
	ASB_PACKAGE_LOG_LEVEL_DEBUG,
	ASB_PACKAGE_LOG_LEVEL_INFO,
	ASB_PACKAGE_LOG_LEVEL_WARNING,
	ASB_PACKAGE_LOG_LEVEL_LAST,
} AsbPackageLogLevel;

typedef enum {
	ASB_PACKAGE_KIND_DEFAULT,
	ASB_PACKAGE_KIND_BUNDLE,
	ASB_PACKAGE_KIND_FIRMWARE,
	ASB_PACKAGE_KIND_LAST
} AsbPackageKind;

void		 asb_package_log_start		(AsbPackage	*pkg);
void		 asb_package_log		(AsbPackage	*pkg,
						 AsbPackageLogLevel log_level,
						 const gchar	*fmt,
						 ...)
						 G_GNUC_PRINTF (3, 4);
gboolean	 asb_package_log_flush		(AsbPackage	*pkg,
						 GError		**error);
gboolean	 asb_package_open		(AsbPackage	*pkg,
						 const gchar	*filename,
						 GError		**error);
gboolean	 asb_package_close		(AsbPackage	*pkg,
						 GError		**error);
gboolean	 asb_package_ensure		(AsbPackage	*pkg,
						 AsbPackageEnsureFlags flags,
						 GError		**error);
void		 asb_package_clear		(AsbPackage	*pkg,
						 AsbPackageEnsureFlags flags);
gboolean	 asb_package_explode		(AsbPackage	*pkg,
						 const gchar	*dir,
						 GPtrArray	*glob,
						 GError		**error);
AsbPackageKind	 asb_package_get_kind		(AsbPackage	*pkg);
guint		 asb_package_get_epoch		(AsbPackage	*pkg);
const gchar	*asb_package_get_filename	(AsbPackage	*pkg);
const gchar	*asb_package_get_basename	(AsbPackage	*pkg);
const gchar	*asb_package_get_arch		(AsbPackage	*pkg);
const gchar	*asb_package_get_name		(AsbPackage	*pkg);
const gchar	*asb_package_get_version	(AsbPackage	*pkg);
const gchar	*asb_package_get_release_str	(AsbPackage	*pkg);
const gchar	*asb_package_get_nevr		(AsbPackage	*pkg);
const gchar	*asb_package_get_nevra		(AsbPackage	*pkg);
const gchar	*asb_package_get_evr		(AsbPackage	*pkg);
const gchar	*asb_package_get_url		(AsbPackage	*pkg);
const gchar	*asb_package_get_vcs		(AsbPackage	*pkg);
const gchar	*asb_package_get_license	(AsbPackage	*pkg);
const gchar	*asb_package_get_source		(AsbPackage	*pkg);
const gchar	*asb_package_get_source_pkgname	(AsbPackage	*pkg);
void		 asb_package_set_kind		(AsbPackage	*pkg,
						 AsbPackageKind	 kind);
void		 asb_package_set_name		(AsbPackage	*pkg,
						 const gchar	*name);
void		 asb_package_set_filename	(AsbPackage	*pkg,
						 const gchar	*filename);
void		 asb_package_set_version	(AsbPackage	*pkg,
						 const gchar	*version);
void		 asb_package_set_release	(AsbPackage	*pkg,
						 const gchar	*release);
void		 asb_package_set_arch		(AsbPackage	*pkg,
						 const gchar	*arch);
void		 asb_package_set_epoch		(AsbPackage	*pkg,
						 guint		 epoch);
void		 asb_package_set_url		(AsbPackage	*pkg,
						 const gchar	*url);
void		 asb_package_set_license	(AsbPackage	*pkg,
						 const gchar	*license);
void		 asb_package_set_vcs		(AsbPackage	*pkg,
						 const gchar	*vcs);
void		 asb_package_set_source		(AsbPackage	*pkg,
						 const gchar	*source);
void		 asb_package_set_source_pkgname	(AsbPackage	*pkg,
						 const gchar	*source_pkgname);
void		 asb_package_add_dep		(AsbPackage	*pkg,
						 const gchar	*dep);
void		 asb_package_set_filelist	(AsbPackage	*pkg,
						 gchar		**filelist);
gchar		**asb_package_get_filelist	(AsbPackage	*pkg);
GPtrArray	*asb_package_get_deps		(AsbPackage	*pkg);
GPtrArray	*asb_package_get_releases	(AsbPackage	*pkg);
void		 asb_package_set_config		(AsbPackage	*pkg,
						 const gchar	*key,
						 const gchar	*value);
const gchar	*asb_package_get_config		(AsbPackage	*pkg,
						 const gchar	*key);
gint		 asb_package_compare		(AsbPackage	*pkg1,
						 AsbPackage	*pkg2);
gboolean	 asb_package_get_enabled	(AsbPackage	*pkg);
void		 asb_package_set_enabled	(AsbPackage	*pkg,
						 gboolean	 enabled);
AsRelease	*asb_package_get_release	(AsbPackage	*pkg,
						 const gchar	*version);
void		 asb_package_add_release	(AsbPackage	*pkg,
						 const gchar	*version,
						 AsRelease	*release);
AsbPackage	*asb_package_new		(void);

G_END_DECLS
