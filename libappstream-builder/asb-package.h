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

#ifndef ASB_PACKAGE_H
#define ASB_PACKAGE_H

#include <glib-object.h>

#include <stdarg.h>
#include <appstream-glib.h>

#define ASB_TYPE_PACKAGE		(asb_package_get_type())
#define ASB_PACKAGE(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), ASB_TYPE_PACKAGE, AsbPackage))
#define ASB_PACKAGE_CLASS(cls)		(G_TYPE_CHECK_CLASS_CAST((cls), ASB_TYPE_PACKAGE, AsbPackageClass))
#define ASB_IS_PACKAGE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), ASB_TYPE_PACKAGE))
#define ASB_IS_PACKAGE_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), ASB_TYPE_PACKAGE))
#define ASB_PACKAGE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), ASB_TYPE_PACKAGE, AsbPackageClass))

G_BEGIN_DECLS

typedef struct _AsbPackage		AsbPackage;
typedef struct _AsbPackageClass		AsbPackageClass;

struct _AsbPackage
{
	GObject			parent;
};

typedef enum {
	ASB_PACKAGE_ENSURE_NONE		= 0,
	ASB_PACKAGE_ENSURE_NEVRA	= 1,
	ASB_PACKAGE_ENSURE_FILES	= 2,
	ASB_PACKAGE_ENSURE_RELEASES	= 4,
	ASB_PACKAGE_ENSURE_DEPS		= 8,
	ASB_PACKAGE_ENSURE_LICENSE	= 16,
	ASB_PACKAGE_ENSURE_URL		= 32,
	ASB_PACKAGE_ENSURE_SOURCE	= 64,
	ASB_PACKAGE_ENSURE_LAST
} AsbPackageEnsureFlags;

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
	/*< private >*/
	void (*_asb_reserved1)	(void);
	void (*_asb_reserved2)	(void);
	void (*_asb_reserved3)	(void);
	void (*_asb_reserved4)	(void);
	void (*_asb_reserved5)	(void);
	void (*_asb_reserved6)	(void);
	void (*_asb_reserved7)	(void);
	void (*_asb_reserved8)	(void);
};

typedef enum {
	ASB_PACKAGE_LOG_LEVEL_NONE,
	ASB_PACKAGE_LOG_LEVEL_DEBUG,
	ASB_PACKAGE_LOG_LEVEL_INFO,
	ASB_PACKAGE_LOG_LEVEL_WARNING,
	ASB_PACKAGE_LOG_LEVEL_LAST,
} AsbPackageLogLevel;

GType		 asb_package_get_type		(void);

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
gboolean	 asb_package_ensure		(AsbPackage	*pkg,
						 AsbPackageEnsureFlags flags,
						 GError		**error);
gboolean	 asb_package_explode		(AsbPackage	*pkg,
						 const gchar	*dir,
						 GPtrArray	*glob,
						 GError		**error);
const gchar	*asb_package_get_filename	(AsbPackage	*pkg);
const gchar	*asb_package_get_basename	(AsbPackage	*pkg);
const gchar	*asb_package_get_arch		(AsbPackage	*pkg);
const gchar	*asb_package_get_name		(AsbPackage	*pkg);
const gchar	*asb_package_get_nevr		(AsbPackage	*pkg);
const gchar	*asb_package_get_nevra		(AsbPackage	*pkg);
const gchar	*asb_package_get_evr		(AsbPackage	*pkg);
const gchar	*asb_package_get_url		(AsbPackage	*pkg);
const gchar	*asb_package_get_license	(AsbPackage	*pkg);
const gchar	*asb_package_get_source		(AsbPackage	*pkg);
const gchar	*asb_package_get_source_pkgname	(AsbPackage	*pkg);
void		 asb_package_set_name		(AsbPackage	*pkg,
						 const gchar	*name);
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
void		 asb_package_set_source		(AsbPackage	*pkg,
						 const gchar	*source);
void		 asb_package_set_source_pkgname	(AsbPackage	*pkg,
						 const gchar	*source_pkgname);
void		 asb_package_set_deps		(AsbPackage	*pkg,
						 gchar		**deps);
void		 asb_package_set_filelist	(AsbPackage	*pkg,
						 gchar		**filelist);
gchar		**asb_package_get_filelist	(AsbPackage	*pkg);
gchar		**asb_package_get_deps		(AsbPackage	*pkg);
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

#endif /* ASB_PACKAGE_H */
