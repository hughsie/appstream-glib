/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2016 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#if !defined (__APPSTREAM_GLIB_H_INSIDE__) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#include <glib.h>

G_BEGIN_DECLS

/**
 * AsMarkupConvertFormat:
 * @AS_MARKUP_CONVERT_FORMAT_SIMPLE:		UTF-8 text
 * @AS_MARKUP_CONVERT_FORMAT_MARKDOWN:		Markdown format
 * @AS_MARKUP_CONVERT_FORMAT_NULL:		No output
 * @AS_MARKUP_CONVERT_FORMAT_APPSTREAM:		AppStream (passthrough)
 * @AS_MARKUP_CONVERT_FORMAT_HTML:		HyperText Markup Language
 *
 * The format used when converting to or from AppStream descriptions.
 **/
typedef enum {
	AS_MARKUP_CONVERT_FORMAT_SIMPLE,
	AS_MARKUP_CONVERT_FORMAT_MARKDOWN,
	AS_MARKUP_CONVERT_FORMAT_NULL,		/* Since: 0.5.2 */
	AS_MARKUP_CONVERT_FORMAT_APPSTREAM,	/* Since: 0.5.2 */
	AS_MARKUP_CONVERT_FORMAT_HTML,		/* Since: 0.5.11 */
	/*< private >*/
	AS_MARKUP_CONVERT_FORMAT_LAST
} AsMarkupConvertFormat;

/**
 * AsMarkupConvertFlag:
 * @AS_MARKUP_CONVERT_FLAG_NONE:		No flags set
 * @AS_MARKUP_CONVERT_FLAG_IGNORE_ERRORS:	Ignore errors where possible
 *
 * The flags used when converting descriptions from AppStream-style.
 **/
typedef enum {
	AS_MARKUP_CONVERT_FLAG_NONE		= 0,
	AS_MARKUP_CONVERT_FLAG_IGNORE_ERRORS	= 1 << 0,
	/*< private >*/
	AS_MARKUP_CONVERT_FLAG_LAST
} AsMarkupConvertFlag;

gchar		*as_markup_convert_simple	(const gchar	*markup,
						 GError		**error);
gchar		*as_markup_convert		(const gchar	*markup,
						 AsMarkupConvertFormat format,
						 GError		**error);
gchar		*as_markup_convert_full		(const gchar	*markup,
						 AsMarkupConvertFormat format,
						 AsMarkupConvertFlag flags,
						 GError		**error);
gboolean	 as_markup_validate		(const gchar	*markup,
						 GError		**error);
gchar		**as_markup_strsplit_words	(const gchar	*text,
						 guint		 line_len);
gchar		*as_markup_import		(const gchar	*text,
						 AsMarkupConvertFormat format,
						 GError		**error);

G_END_DECLS
