/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

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
gboolean	 asb_utils_optimize_png			(const gchar	*filename,
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
