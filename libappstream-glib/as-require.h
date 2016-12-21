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

#ifndef __AS_REQUIRE_H
#define __AS_REQUIRE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_REQUIRE (as_require_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsRequire, as_require, AS, REQUIRE, GObject)

struct _AsRequireClass
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
 * AsRequireKind:
 * @AS_REQUIRE_KIND_UNKNOWN:		Type invalid or not known
 * @AS_REQUIRE_KIND_ID:			Component ID
 * @AS_REQUIRE_KIND_FIRMWARE:		Device firmware version
 *
 * The require type.
 **/
typedef enum {
	AS_REQUIRE_KIND_UNKNOWN,
	AS_REQUIRE_KIND_ID,
	AS_REQUIRE_KIND_FIRMWARE,
	/*< private >*/
	AS_REQUIRE_KIND_LAST
} AsRequireKind;

/**
 * AsRequireCompare:
 * @AS_REQUIRE_COMPARE_UNKNOWN:			Comparison predicate invalid or not known
 * @AS_REQUIRE_COMPARE_EQ:			Equal to
 * @AS_REQUIRE_COMPARE_NE:			Not equal to
 * @AS_REQUIRE_COMPARE_LT:			Less than
 * @AS_REQUIRE_COMPARE_GT:			Greater than
 * @AS_REQUIRE_COMPARE_LE:			Less than or equal to
 * @AS_REQUIRE_COMPARE_GE:			Greater than or equal to
 *
 * The relational comparison type.
 **/
typedef enum {
	AS_REQUIRE_COMPARE_UNKNOWN,
	AS_REQUIRE_COMPARE_EQ,
	AS_REQUIRE_COMPARE_NE,
	AS_REQUIRE_COMPARE_LT,
	AS_REQUIRE_COMPARE_GT,
	AS_REQUIRE_COMPARE_LE,
	AS_REQUIRE_COMPARE_GE,
	/*< private >*/
	AS_REQUIRE_COMPARE_LAST
} AsRequireCompare;

AsRequire	*as_require_new			(void);

/* helpers */
AsRequireKind	 as_require_kind_from_string	(const gchar	*kind);
const gchar	*as_require_kind_to_string	(AsRequireKind	 kind);
AsRequireCompare as_require_compare_from_string	(const gchar	*compare);
const gchar	*as_require_compare_to_string	(AsRequireCompare compare);

/* getters */
AsRequireKind	 as_require_get_kind		(AsRequire	*require);
AsRequireCompare as_require_get_compare		(AsRequire	*require);
const gchar	*as_require_get_version		(AsRequire	*require);
const gchar	*as_require_get_value		(AsRequire	*require);

/* setters */
void		 as_require_set_kind		(AsRequire	*require,
						 AsRequireKind	 kind);
void		 as_require_set_compare		(AsRequire	*require,
						 AsRequireCompare compare);
void		 as_require_set_version		(AsRequire	*require,
						 const gchar	*version);
void		 as_require_set_value		(AsRequire	*require,
						 const gchar	*value);

/* object methods */
gboolean	 as_require_version_compare	(AsRequire	*require,
						 const gchar	*version,
						 GError		**error);

G_END_DECLS

#endif /* __AS_REQUIRE_H */
