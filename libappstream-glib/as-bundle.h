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

#define AS_TYPE_BUNDLE (as_bundle_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsBundle, as_bundle, AS, BUNDLE, GObject)

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
 * @AS_BUNDLE_KIND_FLATPAK:		Flatpak application deployment
 * @AS_BUNDLE_KIND_SNAP:		Snap application deployment
 * @AS_BUNDLE_KIND_PACKAGE:		Package-based application deployment
 * @AS_BUNDLE_KIND_CABINET:		Cabinet firmware deployment
 * @AS_BUNDLE_KIND_APPIMAGE:		AppImage application bundle
 *
 * The bundle type.
 **/
typedef enum {
	AS_BUNDLE_KIND_UNKNOWN,			/* Since: 0.3.5 */
	AS_BUNDLE_KIND_LIMBA,			/* Since: 0.3.5 */
	AS_BUNDLE_KIND_FLATPAK,			/* Since: 0.5.15 */
	AS_BUNDLE_KIND_SNAP,			/* Since: 0.6.1 */
	AS_BUNDLE_KIND_PACKAGE,			/* Since: 0.6.1 */
	AS_BUNDLE_KIND_CABINET,			/* Since: 0.6.2 */
	AS_BUNDLE_KIND_APPIMAGE,		/* Since: 0.6.4 */
	/*< private >*/
	AS_BUNDLE_KIND_LAST
} AsBundleKind;

/* DEPRECATED */
#define AS_BUNDLE_KIND_XDG_APP	AS_BUNDLE_KIND_FLATPAK

AsBundle	*as_bundle_new			(void);

/* helpers */
AsBundleKind	 as_bundle_kind_from_string	(const gchar	*kind);
const gchar	*as_bundle_kind_to_string	(AsBundleKind	 kind);

/* getters */
const gchar	*as_bundle_get_id		(AsBundle	*bundle);
const gchar	*as_bundle_get_runtime		(AsBundle	*bundle);
const gchar	*as_bundle_get_sdk		(AsBundle	*bundle);
AsBundleKind	 as_bundle_get_kind		(AsBundle	*bundle);

/* setters */
void		 as_bundle_set_id		(AsBundle	*bundle,
						 const gchar	*id);
void		 as_bundle_set_runtime		(AsBundle	*bundle,
						 const gchar	*runtime);
void		 as_bundle_set_sdk		(AsBundle	*bundle,
						 const gchar	*sdk);
void		 as_bundle_set_kind		(AsBundle	*bundle,
						 AsBundleKind	 kind);

G_END_DECLS
