/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#if !defined (__APPSTREAM_GLIB_H_INSIDE__) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

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
 * @AS_TRANSLATION_KIND_QT:			QT translation system
 *
 * The translation type.
 **/
typedef enum {
	AS_TRANSLATION_KIND_UNKNOWN,		/* Since: 0.5.7 */
	AS_TRANSLATION_KIND_GETTEXT,		/* Since: 0.5.7 */
	AS_TRANSLATION_KIND_QT,			/* Since: 0.5.8 */
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
