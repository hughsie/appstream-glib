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

#ifndef __AS_REF_STRING_H
#define __AS_REF_STRING_H

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

#define as_ref_string_new_static(o)			(("\xff\xff\xff\xff" o) + 4)

AsRefString	*as_ref_string_new			(const gchar	*str);
AsRefString	*as_ref_string_new_with_length		(const gchar	*str,
							 gsize		 len);
AsRefString	*as_ref_string_new_copy			(const gchar	*str);
AsRefString	*as_ref_string_new_copy_with_length	(const gchar	*str,
							 gsize		 len);
AsRefString	*as_ref_string_ref			(AsRefString	*rstr);
AsRefString	*as_ref_string_unref			(AsRefString	*rstr);
void		 as_ref_string_assign			(AsRefString	**rstr_ptr,
							 AsRefString	*rstr);
void		 as_ref_string_assign_safe		(AsRefString	**rstr_ptr,
							 const gchar	*str);
gchar		*as_ref_string_debug			(AsRefStringDebugFlags flags);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(AsRefString, as_ref_string_unref)

G_END_DECLS

#endif /* __AS_REF_STRING_H */
