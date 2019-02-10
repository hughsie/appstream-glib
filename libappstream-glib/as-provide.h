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

#define AS_TYPE_PROVIDE (as_provide_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsProvide, as_provide, AS, PROVIDE, GObject)

struct _AsProvideClass
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
 * AsProvideKind:
 * @AS_PROVIDE_KIND_UNKNOWN:		Type invalid or not known
 * @AS_PROVIDE_KIND_LIBRARY:		A library file
 * @AS_PROVIDE_KIND_BINARY:		A binary file
 * @AS_PROVIDE_KIND_FONT:		A font file
 * @AS_PROVIDE_KIND_MODALIAS:		A hardware modalias
 * @AS_PROVIDE_KIND_FIRMWARE_RUNTIME:	A runtime-loadable firmware file
 * @AS_PROVIDE_KIND_PYTHON2:		A Python 2 module
 * @AS_PROVIDE_KIND_PYTHON3:		A Python 3 module
 * @AS_PROVIDE_KIND_DBUS_SESSION:	A D-Bus session service
 * @AS_PROVIDE_KIND_DBUS_SYSTEM:	A D-Bus system service
 * @AS_PROVIDE_KIND_FIRMWARE_FLASHED:	A flashed firmware GUID
 * @AS_PROVIDE_KIND_ID:			An AppStream component ID
 *
 * The provide type.
 **/
typedef enum {
	AS_PROVIDE_KIND_UNKNOWN,
	AS_PROVIDE_KIND_LIBRARY,
	AS_PROVIDE_KIND_BINARY,
	AS_PROVIDE_KIND_FONT,
	AS_PROVIDE_KIND_MODALIAS,
	AS_PROVIDE_KIND_FIRMWARE_RUNTIME,
	AS_PROVIDE_KIND_PYTHON2,
	AS_PROVIDE_KIND_PYTHON3,
	AS_PROVIDE_KIND_DBUS_SESSION,		/* Since: 0.1.7 */
	AS_PROVIDE_KIND_DBUS_SYSTEM,		/* Since: 0.2.4 */
	AS_PROVIDE_KIND_FIRMWARE_FLASHED,	/* Since: 0.5.0 */
	AS_PROVIDE_KIND_ID,			/* Since: 0.7.1 */
	/*< private >*/
	AS_PROVIDE_KIND_LAST
} AsProvideKind;

AsProvide	*as_provide_new			(void);

/* helpers */
AsProvideKind	 as_provide_kind_from_string	(const gchar	*kind);
const gchar	*as_provide_kind_to_string	(AsProvideKind	 kind);

/* getters */
const gchar	*as_provide_get_value		(AsProvide	*provide);
AsProvideKind	 as_provide_get_kind		(AsProvide	*provide);

/* setters */
void		 as_provide_set_value		(AsProvide	*provide,
						 const gchar	*value);
void		 as_provide_set_kind		(AsProvide	*provide,
						 AsProvideKind	 kind);

G_END_DECLS
