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

#ifndef __AS_SCREENSHOT_H
#define __AS_SCREENSHOT_H

#include <glib-object.h>

#include "as-image.h"

G_BEGIN_DECLS

#define AS_TYPE_SCREENSHOT (as_screenshot_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsScreenshot, as_screenshot, AS, SCREENSHOT, GObject)

struct _AsScreenshotClass
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
 * AsScreenshotKind:
 * @AS_SCREENSHOT_KIND_UNKNOWN:		Type invalid or not known
 * @AS_SCREENSHOT_KIND_NORMAL:		Optional screenshot
 * @AS_SCREENSHOT_KIND_DEFAULT:		Screenshot to show by default
 *
 * The screenshot type.
 **/
typedef enum {
	AS_SCREENSHOT_KIND_UNKNOWN,
	AS_SCREENSHOT_KIND_NORMAL,
	AS_SCREENSHOT_KIND_DEFAULT,
	/*< private >*/
	AS_SCREENSHOT_KIND_LAST
} AsScreenshotKind;

AsScreenshot	*as_screenshot_new		(void);

/* helpers */
AsScreenshotKind as_screenshot_kind_from_string (const gchar	*kind);
const gchar	*as_screenshot_kind_to_string	(AsScreenshotKind kind);

/* getters */
AsScreenshotKind as_screenshot_get_kind		(AsScreenshot	*screenshot);
gint		 as_screenshot_get_priority	(AsScreenshot	*screenshot);
const gchar	*as_screenshot_get_caption	(AsScreenshot	*screenshot,
						 const gchar	*locale);
GPtrArray	*as_screenshot_get_images	(AsScreenshot	*screenshot);
AsImage		*as_screenshot_get_image	(AsScreenshot	*screenshot,
						 guint		 width,
						 guint		 height);
AsImage		*as_screenshot_get_source	(AsScreenshot	*screenshot);

/* setters */
void		 as_screenshot_set_kind		(AsScreenshot	*screenshot,
						 AsScreenshotKind kind);
void		 as_screenshot_set_priority	(AsScreenshot	*screenshot,
						 gint		 priority);
void		 as_screenshot_set_caption	(AsScreenshot	*screenshot,
						 const gchar	*locale,
						 const gchar	*caption);
void		 as_screenshot_add_image	(AsScreenshot	*screenshot,
						 AsImage	*image);
gboolean	 as_screenshot_equal		(AsScreenshot	*screenshot1,
						 AsScreenshot	*screenshot2);

G_END_DECLS

#endif /* __AS_SCREENSHOT_H */
