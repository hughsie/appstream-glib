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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#if !defined (__APPSTREAM_GLIB_H) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#ifndef __AS_IMAGE_H
#define __AS_IMAGE_H

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define AS_TYPE_IMAGE		(as_image_get_type())
#define AS_IMAGE(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), AS_TYPE_IMAGE, AsImage))
#define AS_IMAGE_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), AS_TYPE_IMAGE, AsImageClass))
#define AS_IS_IMAGE(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), AS_TYPE_IMAGE))
#define AS_IS_IMAGE_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), AS_TYPE_IMAGE))
#define AS_IMAGE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), AS_TYPE_IMAGE, AsImageClass))

G_BEGIN_DECLS

typedef struct _AsImage		AsImage;
typedef struct _AsImageClass	AsImageClass;

struct _AsImage
{
	GObject			parent;
};

struct _AsImageClass
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
 * AsImageKind:
 * @AS_IMAGE_KIND_UNKNOWN:		Type invalid or not known
 * @AS_IMAGE_KIND_SOURCE:		The source image at full resolution
 * @AS_IMAGE_KIND_THUMBNAIL:		A thumbnail at reduced resolution
 *
 * The image type.
 **/
typedef enum {
	AS_IMAGE_KIND_UNKNOWN,
	AS_IMAGE_KIND_SOURCE,
	AS_IMAGE_KIND_THUMBNAIL,
	/*< private >*/
	AS_IMAGE_KIND_LAST
} AsImageKind;

/**
 * AsImageSaveFlag:
 * @AS_IMAGE_SAVE_FLAG_NONE:		No special flags set
 * @AS_IMAGE_SAVE_FLAG_PAD_16_9:	Pad with alpha to 16:9 aspect
 *
 * The flags used for saving images.
 **/
typedef enum {
	AS_IMAGE_SAVE_FLAG_NONE		= 0,	/* Since: 0.1.6 */
	AS_IMAGE_SAVE_FLAG_PAD_16_9	= 1,	/* Since: 0.1.6 */
	/*< private >*/
	AS_IMAGE_SAVE_FLAG_LAST
} AsImageSaveFlags;

GType		 as_image_get_type		(void);
AsImage		*as_image_new			(void);

/* helpers */
AsImageKind	 as_image_kind_from_string	(const gchar	*kind);
const gchar	*as_image_kind_to_string	(AsImageKind	 kind);

/* getters */
const gchar	*as_image_get_url		(AsImage	*image);
const gchar	*as_image_get_md5		(AsImage	*image);
guint		 as_image_get_width		(AsImage	*image);
guint		 as_image_get_height		(AsImage	*image);
AsImageKind	 as_image_get_kind		(AsImage	*image);
GdkPixbuf	*as_image_get_pixbuf		(AsImage	*image);

/* setters */
void		 as_image_set_url		(AsImage	*image,
						 const gchar	*url,
						 gssize		 url_len);
void		 as_image_set_width		(AsImage	*image,
						 guint		 width);
void		 as_image_set_height		(AsImage	*image,
						 guint		 height);
void		 as_image_set_kind		(AsImage	*image,
						 AsImageKind	 kind);
void		 as_image_set_pixbuf		(AsImage	*image,
						 GdkPixbuf	*pixbuf);

/* object methods */
gboolean	 as_image_load_filename		(AsImage	*image,
						 const gchar	*filename,
						 GError		**error);
GdkPixbuf	*as_image_save_pixbuf		(AsImage	*image,
						 guint		 width,
						 guint		 height,
						 AsImageSaveFlags flags);
gboolean	 as_image_save_filename		(AsImage	*image,
						 const gchar	*filename,
						 guint		 width,
						 guint		 height,
						 AsImageSaveFlags flags,
						 GError		**error);

G_END_DECLS

#endif /* __AS_IMAGE_H */
