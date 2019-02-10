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

#include <glib.h>

G_BEGIN_DECLS

typedef gchar AsRefString;

/**
 * AsRefStringDebugFlags:
 * @AS_REF_STRING_DEBUG_NONE:			No detailed debugging
 * @AS_REF_STRING_DEBUG_DEDUPED:		Show detailed dedupe stats
 * @AS_REF_STRING_DEBUG_DUPES:			Show detailed duplication stats
 *
 * The debug type flags.
 **/
typedef enum {
	AS_REF_STRING_DEBUG_NONE	= 0,		/* Since: 0.6.16 */
	AS_REF_STRING_DEBUG_DEDUPED	= 1 << 0,	/* Since: 0.6.16 */
	AS_REF_STRING_DEBUG_DUPES	= 1 << 1,	/* Since: 0.6.16 */
	/*< private >*/
	AS_REF_STRING_DEBUG_LAST
} AsRefStringDebugFlags;

#define as_ref_string_new_static(o)			(AsRefString *) (("\xff\xff\xff\xff" o) + 4)

AsRefString	*as_ref_string_new			(const gchar	*str);
AsRefString	*as_ref_string_new_with_length		(const gchar	*str,
							 gsize		 len);
AsRefString	*as_ref_string_ref			(AsRefString	*rstr);
AsRefString	*as_ref_string_unref			(AsRefString	*rstr);
void		 as_ref_string_assign			(AsRefString	**rstr_ptr,
							 AsRefString	*rstr);
void		 as_ref_string_assign_safe		(AsRefString	**rstr_ptr,
							 const gchar	*str);
gchar		*as_ref_string_debug			(AsRefStringDebugFlags flags);
void		 as_ref_string_debug_start		(void);
void		 as_ref_string_debug_stop		(void);

/* deprecated */
AsRefString	*as_ref_string_new_copy			(const gchar	*str);
AsRefString	*as_ref_string_new_copy_with_length	(const gchar	*str,
							 gsize		 len);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(AsRefString, as_ref_string_unref)

G_END_DECLS
