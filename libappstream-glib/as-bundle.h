/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
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

#ifndef __AS_BUNDLE_H
#define __AS_BUNDLE_H

#include <glib-object.h>

#define AS_TYPE_BUNDLE			(as_bundle_get_type())
#define AS_BUNDLE(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), AS_TYPE_BUNDLE, AsBundle))
#define AS_BUNDLE_CLASS(cls)		(G_TYPE_CHECK_CLASS_CAST((cls), AS_TYPE_BUNDLE, AsBundleClass))
#define AS_IS_BUNDLE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), AS_TYPE_BUNDLE))
#define AS_IS_BUNDLE_CLASS(cls)		(G_TYPE_CHECK_CLASS_TYPE((cls), AS_TYPE_BUNDLE))
#define AS_BUNDLE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), AS_TYPE_BUNDLE, AsBundleClass))

G_BEGIN_DECLS

typedef struct _AsBundle		AsBundle;
typedef struct _AsBundleClass	AsBundleClass;

struct _AsBundle
{
	GObject			parent;
};

struct _AsBundleClass
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
 * AsBundleKind:
 * @AS_BUNDLE_KIND_UNKNOWN:		Type invalid or not known
 * @AS_BUNDLE_KIND_LIMBA:		Limba application bundle
 *
 * The bundle type.
 **/
typedef enum {
	AS_BUNDLE_KIND_UNKNOWN,
	AS_BUNDLE_KIND_LIMBA,
	/*< private >*/
	AS_BUNDLE_KIND_LAST
} AsBundleKind;

GType		 as_bundle_get_type		(void);
AsBundle	*as_bundle_new			(void);

/* helpers */
AsBundleKind	 as_bundle_kind_from_string	(const gchar	*kind);
const gchar	*as_bundle_kind_to_string	(AsBundleKind	 kind);

/* getters */
const gchar	*as_bundle_get_id		(AsBundle	*bundle);
AsBundleKind	 as_bundle_get_kind		(AsBundle	*bundle);

/* setters */
void		 as_bundle_set_id		(AsBundle	*bundle,
						 const gchar	*id,
						 gssize		 id_len);
void		 as_bundle_set_kind		(AsBundle	*bundle,
						 AsBundleKind	 kind);

G_END_DECLS

#endif /* __AS_BUNDLE_H */
