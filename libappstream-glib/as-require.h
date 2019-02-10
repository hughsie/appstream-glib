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
 * @AS_REQUIRE_KIND_HARDWARE:		Hardware device, typically a GUID
 * @AS_REQUIRE_KIND_MODALIAS:		Modalias string
 * @AS_REQUIRE_KIND_KERNEL:		Kernel version
 * @AS_REQUIRE_KIND_MEMORY:		Amount of installed memory in MiB
 *
 * The require type.
 **/
typedef enum {
	AS_REQUIRE_KIND_UNKNOWN,
	AS_REQUIRE_KIND_ID,
	AS_REQUIRE_KIND_FIRMWARE,
	AS_REQUIRE_KIND_HARDWARE,		/* Since: 0.7.4 */
	AS_REQUIRE_KIND_MODALIAS,		/* Since: 0.7.8 */
	AS_REQUIRE_KIND_KERNEL,			/* Since: 0.7.8 */
	AS_REQUIRE_KIND_MEMORY,			/* Since: 0.7.8 */
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
 * @AS_REQUIRE_COMPARE_GLOB:			Filename glob, e.g. `test*`
 * @AS_REQUIRE_COMPARE_REGEX:			A regular expression, e.g. `fw[0-255]`
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
	AS_REQUIRE_COMPARE_GLOB,
	AS_REQUIRE_COMPARE_REGEX,
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
gboolean	 as_require_equal		(AsRequire	*require1,
						 AsRequire	*require2);

G_END_DECLS
