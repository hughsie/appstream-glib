/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#if !defined (__APPSTREAM_GLIB_H_INSIDE__) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

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
 * @AS_LAUNCHABLE_KIND_SERVICE:		A system service
 * @AS_LAUNCHABLE_KIND_COCKPIT_MANIFEST: A manifest run by the cockpit project
 * @AS_LAUNCHABLE_KIND_URL:		A web-app
 *
 * The launchable type.
 **/
typedef enum {
	AS_LAUNCHABLE_KIND_UNKNOWN,
	AS_LAUNCHABLE_KIND_DESKTOP_ID,		/* Since: 0.6.13 */
	AS_LAUNCHABLE_KIND_SERVICE,		/* Since: 0.7.3 */
	AS_LAUNCHABLE_KIND_COCKPIT_MANIFEST,	/* Since: 0.7.3 */
	AS_LAUNCHABLE_KIND_URL,			/* Since: 0.7.3 */
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
