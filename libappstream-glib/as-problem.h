/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#if !defined (__APPSTREAM_GLIB_H_INSIDE__) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_PROBLEM (as_problem_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsProblem, as_problem, AS, PROBLEM, GObject)

struct _AsProblemClass
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
 * AsProblemKind:
 * @AS_PROBLEM_KIND_UNKNOWN:			Type invalid or not known
 * @AS_PROBLEM_KIND_TAG_DUPLICATED:		A tag is duplicated
 * @AS_PROBLEM_KIND_TAG_MISSING:		A required tag is missing
 * @AS_PROBLEM_KIND_TAG_INVALID:		A tag value is invalid
 * @AS_PROBLEM_KIND_ATTRIBUTE_MISSING:		A required attribute is missing
 * @AS_PROBLEM_KIND_ATTRIBUTE_INVALID:		An attribute is invalid
 * @AS_PROBLEM_KIND_MARKUP_INVALID:		The XML markup is invalid
 * @AS_PROBLEM_KIND_STYLE_INCORRECT:		Style guidelines are incorrect
 * @AS_PROBLEM_KIND_TRANSLATIONS_REQUIRED:	Translations are required
 * @AS_PROBLEM_KIND_DUPLICATE_DATA:		Some data is duplicated
 * @AS_PROBLEM_KIND_VALUE_MISSING:		A value is required
 * @AS_PROBLEM_KIND_URL_NOT_FOUND:		The URL is not found
 * @AS_PROBLEM_KIND_FILE_INVALID:		The file is invalid
 * @AS_PROBLEM_KIND_ASPECT_RATIO_INCORRECT:	The image aspect ratio is wrong
 * @AS_PROBLEM_KIND_RESOLUTION_INCORRECT:	The image resolution is wrong
 *
 * The problem type.
 **/
typedef enum {
	AS_PROBLEM_KIND_UNKNOWN,
	AS_PROBLEM_KIND_TAG_DUPLICATED,
	AS_PROBLEM_KIND_TAG_MISSING,
	AS_PROBLEM_KIND_TAG_INVALID,
	AS_PROBLEM_KIND_ATTRIBUTE_MISSING,
	AS_PROBLEM_KIND_ATTRIBUTE_INVALID,
	AS_PROBLEM_KIND_MARKUP_INVALID,
	AS_PROBLEM_KIND_STYLE_INCORRECT,
	AS_PROBLEM_KIND_TRANSLATIONS_REQUIRED,
	AS_PROBLEM_KIND_DUPLICATE_DATA,
	AS_PROBLEM_KIND_VALUE_MISSING,
	AS_PROBLEM_KIND_URL_NOT_FOUND,
	AS_PROBLEM_KIND_FILE_INVALID,
	AS_PROBLEM_KIND_ASPECT_RATIO_INCORRECT,
	AS_PROBLEM_KIND_RESOLUTION_INCORRECT,
	/*< private >*/
	AS_PROBLEM_KIND_LAST
} AsProblemKind;

AsProblem	*as_problem_new			(void);

/* helpers */
const gchar	*as_problem_kind_to_string	(AsProblemKind	 kind);

/* getters */
AsProblemKind	 as_problem_get_kind		(AsProblem	*problem);
guint		 as_problem_get_line_number	(AsProblem	*problem);
const gchar	*as_problem_get_message		(AsProblem	*problem);

/* setters */
void		 as_problem_set_kind		(AsProblem	*problem,
						 AsProblemKind	 kind);
void		 as_problem_set_line_number	(AsProblem	*problem,
						 guint		 line_number);
void		 as_problem_set_message		(AsProblem	*problem,
						 const gchar	*message);

G_END_DECLS
