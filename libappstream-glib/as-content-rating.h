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

#define AS_TYPE_CONTENT (as_content_rating_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsContentRating, as_content_rating, AS, CONTENT_RATING, GObject)

struct _AsContentRatingClass
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
 * AsContentRatingValue:
 * @AS_CONTENT_RATING_VALUE_UNKNOWN:		Unknown value
 * @AS_CONTENT_RATING_VALUE_NONE:		None
 * @AS_CONTENT_RATING_VALUE_MILD:		A small amount
 * @AS_CONTENT_RATING_VALUE_MODERATE:		A moderate amount
 * @AS_CONTENT_RATING_VALUE_INTENSE:		An intense amount
 *
 * The specified level of an content_rating rating ID.
 **/
typedef enum {
	AS_CONTENT_RATING_VALUE_UNKNOWN,
	AS_CONTENT_RATING_VALUE_NONE,
	AS_CONTENT_RATING_VALUE_MILD,
	AS_CONTENT_RATING_VALUE_MODERATE,
	AS_CONTENT_RATING_VALUE_INTENSE,
	/*< private >*/
	AS_CONTENT_RATING_VALUE_LAST
} AsContentRatingValue;

AsContentRating	*as_content_rating_new		(void);

/* helpers */
const gchar	*as_content_rating_value_to_string	(AsContentRatingValue	 value);
AsContentRatingValue	 as_content_rating_value_from_string	(const gchar	*value);

/* getters */
const gchar	*as_content_rating_get_kind	(AsContentRating	*content_rating);
guint		 as_content_rating_get_minimum_age (AsContentRating	*content_rating);
AsContentRatingValue as_content_rating_get_value (AsContentRating	*content_rating,
						 const gchar		*id);
void		 as_content_rating_add_attribute(AsContentRating	*content_rating,
						 const gchar		*id,
						 AsContentRatingValue	 value);

const gchar	**as_content_rating_get_rating_ids (AsContentRating	*content_rating);

guint		as_content_rating_attribute_to_csm_age (const gchar		*id,
							AsContentRatingValue 	 value);
const gchar	**as_content_rating_get_all_rating_ids (void);

/* setters */
void		 as_content_rating_set_kind	(AsContentRating	*content_rating,
						 const gchar		*kind);

G_END_DECLS
