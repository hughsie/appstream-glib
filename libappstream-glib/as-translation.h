/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
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

#ifndef __AS_TRANSLATION_H
#define __AS_TRANSLATION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_TRANSLATION (as_translation_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsTranslation, as_translation, AS, TRANSLATION, GObject)

struct _AsTranslationClass
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
 * AsTranslationKind:
 * @AS_TRANSLATION_KIND_UNKNOWN:		Type invalid or not known
 * @AS_TRANSLATION_KIND_GETTEXT:		Gettext translation system
 *
 * The translation type.
 **/
typedef enum {
	AS_TRANSLATION_KIND_UNKNOWN,
	AS_TRANSLATION_KIND_GETTEXT,
	/*< private >*/
	AS_TRANSLATION_KIND_LAST
} AsTranslationKind;

AsTranslation	*as_translation_new			(void);

/* helpers */
AsTranslationKind as_translation_kind_from_string	(const gchar	*kind);
const gchar	*as_translation_kind_to_string		(AsTranslationKind kind);

/* getters */
const gchar	*as_translation_get_id			(AsTranslation	*translation);
AsTranslationKind as_translation_get_kind		(AsTranslation	*translation);

/* setters */
void		 as_translation_set_id			(AsTranslation	*translation,
							 const gchar	*id);
void		 as_translation_set_kind		(AsTranslation	*translation,
							 AsTranslationKind kind);

G_END_DECLS

#endif /* __AS_TRANSLATION_H */
