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

#define AS_TYPE_SUGGEST (as_suggest_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsSuggest, as_suggest, AS, SUGGEST, GObject)

struct _AsSuggestClass
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
 * AsSuggestKind:
 * @AS_SUGGEST_KIND_UNKNOWN:		Type invalid or not known
 * @AS_SUGGEST_KIND_UPSTREAM:		Upstream-specified suggestion
 * @AS_SUGGEST_KIND_HEURISTIC:		Suggestion from a heuristic
 *
 * The suggest type.
 **/
typedef enum {
	AS_SUGGEST_KIND_UNKNOWN,
	AS_SUGGEST_KIND_UPSTREAM,
	AS_SUGGEST_KIND_HEURISTIC,
	/*< private >*/
	AS_SUGGEST_KIND_LAST
} AsSuggestKind;

AsSuggest	*as_suggest_new			(void);

/* helpers */
AsSuggestKind	 as_suggest_kind_from_string	(const gchar	*kind);
const gchar	*as_suggest_kind_to_string	(AsSuggestKind	 kind);

/* getters */
AsSuggestKind	 as_suggest_get_kind		(AsSuggest	*suggest);
GPtrArray	*as_suggest_get_ids		(AsSuggest	*suggest);

/* setters */
void		 as_suggest_set_kind		(AsSuggest	*suggest,
						 AsSuggestKind	 kind);
void		 as_suggest_add_id		(AsSuggest	*suggest,
						 const gchar	*id);

G_END_DECLS
