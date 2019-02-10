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

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define AS_TYPE_IMAGE (as_image_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsImage, as_image, AS, IMAGE, GObject)

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
 * AsImageSaveFlags:
 * @AS_IMAGE_SAVE_FLAG_NONE:		No special flags set
 * @AS_IMAGE_SAVE_FLAG_PAD_16_9:	Pad with alpha to 16:9 aspect
 * @AS_IMAGE_SAVE_FLAG_SHARPEN:		Sharpen the image to clarify detail
 * @AS_IMAGE_SAVE_FLAG_BLUR:		Blur the image to clear detail
 *
 * The flags used for saving images.
 **/
typedef enum {
	AS_IMAGE_SAVE_FLAG_NONE		= 0,	/* Since: 0.1.6 */
	AS_IMAGE_SAVE_FLAG_PAD_16_9	= 1,	/* Since: 0.1.6 */
	AS_IMAGE_SAVE_FLAG_SHARPEN	= 2,	/* Since: 0.3.2 */
	AS_IMAGE_SAVE_FLAG_BLUR		= 4,	/* Since: 0.3.2 */
	/*< private >*/
	AS_IMAGE_SAVE_FLAG_LAST
} AsImageSaveFlags;

/**
 * AsImageLoadFlags:
 * @AS_IMAGE_LOAD_FLAG_NONE:		No special flags set
 * @AS_IMAGE_LOAD_FLAG_SHARPEN:		Sharpen the resulting image
 * @AS_IMAGE_LOAD_FLAG_SET_BASENAME:	Set the image basename
 * @AS_IMAGE_LOAD_FLAG_SET_CHECKSUM:	Set the image checksum
 * @AS_IMAGE_LOAD_FLAG_ONLY_SUPPORTED:	Only load supported formats like PNG and JPG
 * @AS_IMAGE_LOAD_FLAG_ALWAYS_RESIZE:	Always resize the source icon to the perfect size
 *
 * The flags used for loading images.
 **/
typedef enum {
	AS_IMAGE_LOAD_FLAG_NONE		= 0,	/* Since: 0.5.6 */
	AS_IMAGE_LOAD_FLAG_SHARPEN	= 1,	/* Since: 0.5.6 */
	AS_IMAGE_LOAD_FLAG_SET_BASENAME	= 2,	/* Since: 0.5.6 */
	AS_IMAGE_LOAD_FLAG_SET_CHECKSUM	= 4,	/* Since: 0.5.6 */
	AS_IMAGE_LOAD_FLAG_ONLY_SUPPORTED = 8,	/* Since: 0.5.6 */
	AS_IMAGE_LOAD_FLAG_ALWAYS_RESIZE = 16,	/* Since: 0.7.7 */
	/*< private >*/
	AS_IMAGE_LOAD_FLAG_LAST
} AsImageLoadFlags;

/**
 * AsImageAlphaFlags:
 * @AS_IMAGE_ALPHA_FLAG_NONE:		No padding detected
 * @AS_IMAGE_ALPHA_FLAG_TOP:		Padding detected at the image top
 * @AS_IMAGE_ALPHA_FLAG_BOTTOM:		Padding detected at the image bottom
 * @AS_IMAGE_ALPHA_FLAG_LEFT:		Padding detected at the image left side
 * @AS_IMAGE_ALPHA_FLAG_RIGHT:		Padding detected at the image right side
 * @AS_IMAGE_ALPHA_FLAG_INTERNAL:	Internal alpha cut out areas detected
 *
 * The flags used for reporting the alpha cutouts in the image.
 **/
#define AS_IMAGE_ALPHA_FLAG_NONE	(0u)		/* Since: 0.2.2 */
#define AS_IMAGE_ALPHA_FLAG_TOP		(1u << 0)	/* Since: 0.2.2 */
#define AS_IMAGE_ALPHA_FLAG_BOTTOM	(1u << 1)	/* Since: 0.2.2 */
#define AS_IMAGE_ALPHA_FLAG_LEFT	(1u << 2)	/* Since: 0.2.2 */
#define AS_IMAGE_ALPHA_FLAG_RIGHT	(1u << 3)	/* Since: 0.2.2 */
#define AS_IMAGE_ALPHA_FLAG_INTERNAL	(1u << 4)	/* Since: 0.2.2 */
typedef guint AsImageAlphaFlags;

/* some useful constants */
#define AS_IMAGE_LARGE_HEIGHT		423	/* Since: 0.2.2 */
#define AS_IMAGE_LARGE_WIDTH		752	/* Since: 0.2.2 */
#define AS_IMAGE_NORMAL_HEIGHT		351	/* Since: 0.2.2 */
#define AS_IMAGE_NORMAL_WIDTH		624	/* Since: 0.2.2 */
#define AS_IMAGE_THUMBNAIL_HEIGHT	63	/* Since: 0.2.2 */
#define AS_IMAGE_THUMBNAIL_WIDTH 	112	/* Since: 0.2.2 */

AsImage		*as_image_new			(void);

/* helpers */
AsImageKind	 as_image_kind_from_string	(const gchar	*kind);
const gchar	*as_image_kind_to_string	(AsImageKind	 kind);

/* getters */
const gchar	*as_image_get_url		(AsImage	*image);
const gchar	*as_image_get_md5		(AsImage	*image);
const gchar	*as_image_get_basename		(AsImage	*image);
const gchar	*as_image_get_locale		(AsImage	*image);
guint		 as_image_get_width		(AsImage	*image);
guint		 as_image_get_height		(AsImage	*image);
AsImageKind	 as_image_get_kind		(AsImage	*image);
GdkPixbuf	*as_image_get_pixbuf		(AsImage	*image);

/* setters */
void		 as_image_set_url		(AsImage	*image,
						 const gchar	*url);
void		 as_image_set_basename		(AsImage	*image,
						 const gchar	*basename);
void		 as_image_set_locale		(AsImage	*image,
						 const gchar	*locale);
void		 as_image_set_width		(AsImage	*image,
						 guint		 width);
void		 as_image_set_height		(AsImage	*image,
						 guint		 height);
void		 as_image_set_kind		(AsImage	*image,
						 AsImageKind	 kind);
void		 as_image_set_pixbuf		(AsImage	*image,
						 GdkPixbuf	*pixbuf);

/* object methods */
AsImageAlphaFlags as_image_get_alpha_flags	(AsImage	*image);
gboolean	 as_image_load_filename		(AsImage	*image,
						 const gchar	*filename,
						 GError		**error);
gboolean	 as_image_load_filename_full	(AsImage	*image,
						 const gchar	*filename,
						 guint		 dest_size,
						 guint		 src_size_min,
						 AsImageLoadFlags flags,
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
gboolean	 as_image_equal			(AsImage	*image1,
						 AsImage	*image2);

G_END_DECLS
