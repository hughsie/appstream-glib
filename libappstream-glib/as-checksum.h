/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2015 Richard Hughes <richard@hughsie.com>
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

#ifndef __AS_CHECKSUM_H
#define __AS_CHECKSUM_H

#include <glib-object.h>

G_BEGIN_DECLS

#define AS_TYPE_CHECKSUM (as_checksum_get_type ())
G_DECLARE_DERIVABLE_TYPE (AsChecksum, as_checksum, AS, CHECKSUM, GObject)

struct _AsChecksumClass
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
 * AsChecksumTarget:
 * @AS_CHECKSUM_TARGET_UNKNOWN:			Unknown state
 * @AS_CHECKSUM_TARGET_CONTAINER:		Container file, e.g. `.cab`
 * @AS_CHECKSUM_TARGET_CONTENT:			Extracted file, e.g. `.bin`
 * @AS_CHECKSUM_TARGET_SIGNATURE:		Signature, e.g. `.asc` or `.cat`
 * @AS_CHECKSUM_TARGET_DEVICE:			Device-reported value
 *
 * The checksum target type.
 **/
typedef enum {
	AS_CHECKSUM_TARGET_UNKNOWN,		/* Since: 0.4.2 */
	AS_CHECKSUM_TARGET_CONTAINER,		/* Since: 0.4.2 */
	AS_CHECKSUM_TARGET_CONTENT,		/* Since: 0.4.2 */
	AS_CHECKSUM_TARGET_SIGNATURE,		/* Since: 0.7.9 */
	AS_CHECKSUM_TARGET_DEVICE,		/* Since: 0.7.15 */
	/*< private >*/
	AS_CHECKSUM_TARGET_LAST
} AsChecksumTarget;

AsChecksum	*as_checksum_new		(void);
AsChecksumTarget as_checksum_target_from_string	(const gchar	*target);
const gchar	*as_checksum_target_to_string	(AsChecksumTarget target);

/* getters */
const gchar	*as_checksum_get_filename	(AsChecksum	*checksum);
const gchar	*as_checksum_get_value		(AsChecksum	*checksum);
GChecksumType	 as_checksum_get_kind		(AsChecksum	*checksum);
AsChecksumTarget as_checksum_get_target		(AsChecksum	*checksum);

/* setters */
void		 as_checksum_set_filename	(AsChecksum	*checksum,
						 const gchar	*filename);
void		 as_checksum_set_value		(AsChecksum	*checksum,
						 const gchar	*value);
void		 as_checksum_set_kind		(AsChecksum	*checksum,
						 GChecksumType	 kind);
void		 as_checksum_set_target		(AsChecksum	*checksum,
						 AsChecksumTarget target);

G_END_DECLS

#endif /* __AS_CHECKSUM_H */
