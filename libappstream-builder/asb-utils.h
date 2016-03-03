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

#ifndef __ASB_UTILS_H
#define __ASB_UTILS_H

#include <glib.h>
#include <appstream-glib.h>

G_BEGIN_DECLS

typedef struct	AsbGlobValue		AsbGlobValue;

gboolean	 asb_utils_rmtree			(const gchar	*directory,
							 GError		**error);
gboolean	 asb_utils_ensure_exists		(const gchar	*directory,
							 GError		**error);
gboolean	 asb_utils_ensure_exists_and_empty	(const gchar	*directory,
							 GError		**error);
gboolean	 asb_utils_write_archive_dir		(const gchar	*filename,
							 const gchar	*directory,
							 GError		**error);
gboolean	 asb_utils_explode			(const gchar	*filename,
							 const gchar	*dir,
							 GPtrArray	*glob,
							 GError		**error);
gchar		*asb_utils_get_cache_id_for_filename	(const gchar	*filename);

gchar		*asb_utils_get_builder_id		(void);

AsbGlobValue	*asb_glob_value_new			(const gchar	*glob,
							 const gchar	*value);
void		 asb_glob_value_free			(AsbGlobValue	*kv);
const gchar	*asb_glob_value_search			(GPtrArray	*array,
							 const gchar	*search);
GPtrArray	*asb_glob_value_array_new		(void);

G_END_DECLS

#endif /* __ASB_UTILS_H */
