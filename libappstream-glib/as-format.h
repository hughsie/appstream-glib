/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#if !defined (__APPSTREAM_GLIB_H_INSIDE__) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_FORMAT (as_format_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsFormat, as_format, AS, FORMAT, GObject)

struct _AsFormatClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
	void (*_as_reserved7)	(void);
	void (*_as_reserved8)	(void);
};

/**
 * AsFormatKind:
 * @AS_FORMAT_KIND_UNKNOWN:		Not formatd from a file
 * @AS_FORMAT_KIND_APPSTREAM:		AppStream file
 * @AS_FORMAT_KIND_DESKTOP:		Desktop file
 * @AS_FORMAT_KIND_APPDATA:		AppData file
 * @AS_FORMAT_KIND_METAINFO:		MetaInfo file
 *
 * The format kind.
 **/
typedef enum {
	AS_FORMAT_KIND_UNKNOWN,		/* Since: 0.6.9 */
	AS_FORMAT_KIND_APPSTREAM,	/* Since: 0.6.9 */
	AS_FORMAT_KIND_DESKTOP,		/* Since: 0.6.9 */
	AS_FORMAT_KIND_APPDATA,		/* Since: 0.6.9 */
	AS_FORMAT_KIND_METAINFO,	/* Since: 0.6.9 */
	/*< private >*/
	AS_FORMAT_KIND_LAST
} AsFormatKind;

AsFormat	*as_format_new			(void);

/* helpers */
AsFormatKind	 as_format_kind_from_string	(const gchar	*kind);
const gchar	*as_format_kind_to_string	(AsFormatKind	 kind);
gboolean	 as_format_equal		(AsFormat	*format1,
						 AsFormat	*format2);
AsFormatKind	 as_format_guess_kind		(const gchar	*filename);

/* getters */
const gchar	*as_format_get_filename		(AsFormat	*format);
AsFormatKind	 as_format_get_kind		(AsFormat	*format);

/* setters */
void		 as_format_set_filename		(AsFormat	*format,
						 const gchar	*filename);
void		 as_format_set_kind		(AsFormat	*format,
						 AsFormatKind	 kind);

G_END_DECLS
