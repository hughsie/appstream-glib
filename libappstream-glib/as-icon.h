/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#if !defined (__APPSTREAM_GLIB_H_INSIDE__) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define AS_TYPE_ICON (as_icon_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsIcon, as_icon, AS, ICON, GObject)

struct _AsIconClass
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
 * AsIconError:
 * @AS_ICON_ERROR_FAILED:			Generic failure
 *
 * The error type.
 **/
typedef enum {
	AS_ICON_ERROR_FAILED,
	/*< private >*/
	AS_ICON_ERROR_LAST
} AsIconError;

/**
 * AsIconKind:
 * @AS_ICON_KIND_UNKNOWN:		Type invalid or not known
 * @AS_ICON_KIND_STOCK:			Stock icon or present in the generic icon theme
 * @AS_ICON_KIND_CACHED:		An icon shipped with the AppStream metadata
 * @AS_ICON_KIND_REMOTE:		An icon referenced by a remote URL
 * @AS_ICON_KIND_EMBEDDED:		An embedded Base64 icon
 * @AS_ICON_KIND_LOCAL:			An icon with absolute path and filename
 *
 * The icon type.
 **/
typedef enum {
	AS_ICON_KIND_UNKNOWN,		/* Since: 0.1.0 */
	AS_ICON_KIND_STOCK,		/* Since: 0.1.0 */
	AS_ICON_KIND_CACHED,		/* Since: 0.1.0 */
	AS_ICON_KIND_REMOTE,		/* Since: 0.1.0 */
	AS_ICON_KIND_EMBEDDED,		/* Since: 0.3.1 */
	AS_ICON_KIND_LOCAL,		/* Since: 0.3.1 */
	/*< private >*/
	AS_ICON_KIND_LAST
} AsIconKind;

/**
 * AsIconLoadFlags:
 * @AS_ICON_LOAD_FLAG_NONE:		No extra flags to use
 * @AS_ICON_LOAD_FLAG_SEARCH_SIZE:	Search first in a size-specific directory
 *
 * The flags to use when loading icons.
 **/
typedef enum {
	AS_ICON_LOAD_FLAG_NONE			= 0,	/* Since: 0.3.1 */
	AS_ICON_LOAD_FLAG_SEARCH_SIZE		= 1,	/* Since: 0.3.1 */
	/*< private >*/
	AS_ICON_LOAD_FLAG_LAST
} AsIconLoadFlags;

#define	AS_ICON_ERROR				as_icon_error_quark ()

AsIcon		*as_icon_new			(void);
GQuark		 as_icon_error_quark		(void);

/* helpers */
const gchar	*as_icon_kind_to_string		(AsIconKind	 icon_kind);
AsIconKind	 as_icon_kind_from_string	(const gchar	*icon_kind);

/* getters */
const gchar	*as_icon_get_name		(AsIcon		*icon);
const gchar	*as_icon_get_url		(AsIcon		*icon);
const gchar	*as_icon_get_filename		(AsIcon		*icon);
const gchar	*as_icon_get_prefix		(AsIcon		*icon);
guint		 as_icon_get_width		(AsIcon		*icon);
guint		 as_icon_get_height		(AsIcon		*icon);
guint		 as_icon_get_scale		(AsIcon		*icon);
AsIconKind	 as_icon_get_kind		(AsIcon		*icon);
GdkPixbuf	*as_icon_get_pixbuf		(AsIcon		*icon);

/* setters */
void		 as_icon_set_name		(AsIcon		*icon,
						 const gchar	*name);
void		 as_icon_set_url		(AsIcon		*icon,
						 const gchar	*url);
void		 as_icon_set_filename		(AsIcon		*icon,
						 const gchar	*filename);
void		 as_icon_set_prefix		(AsIcon		*icon,
						 const gchar	*prefix);
void		 as_icon_set_width		(AsIcon		*icon,
						 guint		 width);
void		 as_icon_set_height		(AsIcon		*icon,
						 guint		 height);
void		 as_icon_set_scale		(AsIcon		*icon,
						 guint		 scale);
void		 as_icon_set_kind		(AsIcon		*icon,
						 AsIconKind	 kind);
void		 as_icon_set_pixbuf		(AsIcon		*icon,
						 GdkPixbuf	*pixbuf);

/* object methods */
gboolean	 as_icon_load			(AsIcon		*icon,
						 AsIconLoadFlags flags,
						 GError		**error);
gboolean	 as_icon_convert_to_kind	(AsIcon		*icon,
						 AsIconKind	 kind,
						 GError		**error);

G_END_DECLS
