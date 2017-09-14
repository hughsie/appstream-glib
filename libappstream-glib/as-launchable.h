/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
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

#ifndef __AS_LAUNCHABLE_H
#define __AS_LAUNCHABLE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_LAUNCHABLE (as_launchable_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsLaunchable, as_launchable, AS, LAUNCHABLE, GObject)

struct _AsLaunchableClass
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
 * AsLaunchableKind:
 * @AS_LAUNCHABLE_KIND_UNKNOWN:		Type invalid or not known
 * @AS_LAUNCHABLE_KIND_DESKTOP_ID:	A desktop ID
 *
 * The launchable type.
 **/
typedef enum {
	AS_LAUNCHABLE_KIND_UNKNOWN,
	AS_LAUNCHABLE_KIND_DESKTOP_ID,		/* Since: 0.6.13 */
	AS_LAUNCHABLE_KIND_SERVICE,		/* Since: 0.11.2 */
	AS_LAUNCHABLE_KIND_COCKPIT_MANIFEST,    /* Since: 0.11.4 */
	/*< private >*/
	AS_LAUNCHABLE_KIND_LAST
} AsLaunchableKind;

AsLaunchable	*as_launchable_new		(void);

/* helpers */
AsLaunchableKind as_launchable_kind_from_string	(const gchar		*kind);
const gchar	*as_launchable_kind_to_string	(AsLaunchableKind	 kind);

/* getters */
const gchar	*as_launchable_get_value	(AsLaunchable		*launchable);
AsLaunchableKind as_launchable_get_kind		(AsLaunchable		*launchable);

/* setters */
void		 as_launchable_set_value	(AsLaunchable		*launchable,
						 const gchar		*value);
void		 as_launchable_set_kind		(AsLaunchable		*launchable,
						 AsLaunchableKind	 kind);

G_END_DECLS

#endif /* __AS_LAUNCHABLE_H */
